/*
  Copyright 2008,2009,2010 Alun Williams
  This file is part of MAF.
  MAF is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  MAF is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with MAF.  If not, see <http://www.gnu.org/licenses/>.
*/


/*
$Log: maf_rm.h $
Revision 1.21  2010/06/10 13:58:09Z  Alun
All tabs removed again
Revision 1.20  2010/05/08 22:57:00Z  Alun
optimise() method added. Many other changes due to new style Node_Manager
interface
Revision 1.19  2009/11/10 20:51:33Z  Alun
Various name changes and set_initial_special_limit() added
Revision 1.18  2009/09/13 15:17:02Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.17  2009/09/01 23:37:15Z  Alun_Williams
Member favour_differences added. const attribute removed from is_interesting_equation
Revision 1.16  2009/08/23 18:56:53Z  Alun_Williams
subgroup_generators member removed as it is never actually used now
Revision 1.15  2008/12/26 21:33:20Z  Alun
Some new flags. coset_extras() method added for dealing with normal subgroup
coset systems
Revision 1.14  2008/10/27 22:08:16Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.13  2008/09/29 22:41:20Z  Alun
Switch to using Hash+Collection_Manager as replacement for Indexer.
Currently this is about 10% slower, but this is a more flexible
architecture.
Revision 1.7  2007/12/20 23:25:45Z  Alun
*/
#pragma once
#ifndef MAF_RM_INCLUDED
#define MAF_RM_INCLUDED 1
#ifndef MAF_INCLUDED
#include "maf.h"
#endif
#ifndef MAF_EW_INCLUDED
#include "maf_ew.h"
#endif
#ifndef EQUATION_INCLUDED
#include "equation.h"
#endif

class Node;
typedef Node Equation_Node;
typedef const Node * State;
#ifndef NODE_STATUS_INCLUDED
#include "node_status.h"
#endif

// Classes referred to and defined elsewhere
class Difference_Tracker;
class Equation;
class Hash;
class Keyed_FSA;
class MAF;
class Multi_Node_List;
class Presentation;
class Presentation_Data;
class Ordered_Node_Tree;
class Node_Manager;
class Simple_Equation;
class Working_Equation;
class FSA_Simple;
class Equation_Word;
class Word_Acceptor;
class Difference_Machine;
class Diff_Reduce;
class Word_DB;
class Equation_Manager;
class Equation_DB;

//Classes defined in this header
class Rewriter_Machine;
class Equation_Word_Reducer;

/* Values for generator_properties.
   Left cancellation means from xa=xb we are allowed to deduce a=b
   Right cancellation means from ax=bx we are allowed to deduce a=b
   Left inverse means that wg=Idword for some word w
   Right inverse means that gv=Idword for some word v
   If we have a left and a right inverse then they are equal
   since we have w=wgv=v
   Left and right inverse implies cancellation is OK on same side,
   but cancellation does not imply existence of an inverse.
   (Except that in a finite monoid, if all generators are both left and
    right cancellable, then the monoid is a group)
 */
const unsigned GF_LEFT_CANCELS = 1;
const unsigned GF_RIGHT_CANCELS = 2;
const unsigned GF_LEFT_INVERSE = 4;
const unsigned GF_RIGHT_INVERSE = 8;
const unsigned GF_TRIVIAL = 16;
const unsigned GF_REDUNDANT = 32; /* generator is reducible */
const unsigned GF_DIFFICULT = 64; /* we are having problems conjugating
                                     this generator */
const unsigned GF_COSET_REDUCIBLE = 128;
class Equation_Word_Reducer : private Equation_Word
{
   // The point of this class is to get us an equation_word that
   // lives for a long time, so as to cut down on heap churn,
   // but which we cannot see inside, so that we are not tempted to
   // assume its data stays valid.
  public:
    Equation_Word_Reducer(Node_Manager &nm_) :
      Equation_Word(nm_,(Word_Length) 0)
    {}
    bool reduce(Word * rword,const Word & word,unsigned flags = 0,const FSA * = 0);
};

class Strong_Diff_Reduce : public Word_Reducer
{
  private:
    Rewriter_Machine * rm;
    Difference_Tracker * dt;
    Diff_Reduce * dr0;
    Diff_Reduce * dr1;
    Diff_Reduce * dr2;
  public:
    Strong_Diff_Reduce(Rewriter_Machine *rm_);
    ~Strong_Diff_Reduce();
    unsigned reduce(Word * rword,const Word & word,unsigned flags=0,const FSA * wa = 0);
};

/* flags for update_machine() */
const unsigned UM_RECHECK_PARTIAL = 1;
const unsigned UM_NO_REPEAT_PARTIAL = 2;
const unsigned UM_CHECK_POOL = 4;
const unsigned UM_PRUNE = 8;
const unsigned UM_CHECK_OVERSIZED = 16;

/* flags for check_differences() */
const unsigned CD_CHECK_INTEREST = 1;
const unsigned CD_STABILISE = 2;
const unsigned CD_KEEP = 4;
const unsigned CD_EXPLORE = 8;
const unsigned CD_UPDATE = 16;
const unsigned CD_FORCE_REBUILD = 32;

typedef unsigned long Progress_Count;

class Rewriter_Machine
{
    friend class Strong_Diff_Reduce;
    friend class Node_Manager;
    friend class Working_Equation;
    friend class Equation_Manager;
    friend Node_Reference Equation_Word::get_node(Word_Length length,bool lhs,
                                                  Word_Length * new_prefix_length);
  public:
    struct Status
    {
      State_Count difference_delta;
      Node_Count equation_delta;
      Total_Length auto_expand_limit; // size up to which equation is
                                      // automatically added to expand lists
      Total_Length visible_limit;     // size up to which equation is
                                      // automatically inserted in tree
      Total_Length initial_special_limit;     // controls special overlaps
      Total_Length special_limit;
      Total_Length pool_limit;        // lowest size of pooled primary
      Word_Length best_pool_rhs;
      Word_Length best_pool_lhs;
      Total_Length best_pool_lhs_total;
      Total_Length best_pool_rhs_total;
      Total_Length discard_limit;  // size above which equations are
                                   // discarded
      Total_Length upper_consider_limit;
      Total_Length max_expanded; // maximum equation size to be expanded in current pass.
      Word_Length lhs_limit; // longest lhs size of equations automatically expanded
      Word_Length max_expanded_lhs; // longest lhs size actually expanded
      Word_Length max_primary_lhs;// longest lhs that should be on right
      Word_Length total_increment; // 1 or 2 depending on presence of odd length axioms.
      Total_Length shortest_recent;
      ID start_id;
      bool approved_complete;
      bool complete;
      bool just_ticked;
      bool pool_full;
      bool primary_complete;
      bool g_complete;
      bool h_complete;
      bool no_coset_pool;
      bool coset_complete;
      bool visibility_correct;
      bool relaxed;
      bool resumed;
      bool some_discarded;
      bool some_ignored;
      bool started;
      bool starting;
      bool vital_attained;
      bool want_differences;
      bool favour_differences;
      bool want_overlaps;
      bool want_secondaries;
      bool hidden_differences;
      int priority_status;
      Progress_Count status_count;
      Progress_Count ticked; // set to status_count periodically
      Progress_Count eticked; // set to status_count periodically
      Progress_Count last_difference; // set to status_count when the word-difference machine changes
      Progress_Count last_coset_equation; // set to status_count when a coset equation is found
      Progress_Count first_timeout;
      Progress_Count normal_timeout;
      Progress_Count timeout;
      Progress_Count last_explore;
      Progress_Count explore_time;
      Progress_Count expand_time;
      Progress_Count update_time; // set to status_count when update_machine() is called
      Progress_Count last_save_time; // set to status_count when provisional output is created
    };

  public:
    /* pd,maf,and container members must be first because constructors
       for nm,em and jm use them */
    const Presentation_Data &pd;
    MAF &maf;
    Container &container;
    class Job_Manager;
    friend class Job_Manager;
  private:
    Node_Manager &nm;
    Equation_Manager &em;
    Job_Manager &jm;
    Difference_Tracker *dt;
    Equation_Word_Reducer &word_reducer;
    Status stats;
    Node_List expand_list;
    Node_List re_expand_list;
    Ordered_Node_Tree *expand_ont;
    Equation_DB * pool;
    Hash * derivation_db;
    const Ordinal nr_generators;
    Ordinal nr_good_generators;
    Ordinal nr_trivial_generators;
    Ordinal nr_coset_reducible_generators;
    unsigned rm_state;
    unsigned char * generator_properties;
  public:
    /* Constructors /Destructors */
    Rewriter_Machine(MAF & maf_);
    APIMETHOD ~Rewriter_Machine();
    /* Query methods */
    const Alphabet & alphabet() const
    {
      return maf.alphabet;
    }
    operator const Node_Manager &() const
    {
      return nm;
    }
    operator Node_Manager &()
    {
      return nm;
    }
    /* Information about generators */
    bool difficult(Ordinal g) const
    {
      return (generator_properties[g] & GF_DIFFICULT)!=0;
    }
    Ordinal generator_count() const
    {
      return nr_generators;
    }
    Ordinal inverse(Ordinal g) const
    {
      return maf.inverse(g);
    }
    void invert(Word * inverse_word,const Word & word,Word_Length length = WHOLE_WORD) const
    {
      maf.Presentation::invert(inverse_word,word,length);
    }
    bool invertible(Ordinal g) const
    {
      return (generator_properties[g] & GF_LEFT_INVERSE+GF_RIGHT_INVERSE)==
             GF_LEFT_INVERSE+GF_RIGHT_INVERSE;
    }
    bool presentation_trivial() const
    {
      return nr_trivial_generators == nr_generators;
    }
    bool redundant(Ordinal g) const
    {
      return (generator_properties[g] & GF_REDUNDANT)!=0;
    }
    bool left_cancels(Ordinal g) const
    {
      return pd.has_cancellation || generator_properties[g] & GF_LEFT_CANCELS;
    }
    bool right_cancels(Ordinal g) const
    {
      return pd.has_cancellation || generator_properties[g] & GF_RIGHT_CANCELS;
    }
    /* Information about the Rewriter_Machine */
    Status & status() {return stats;}
    Word_Length height() const;
    Node_Count node_count(bool include_equations = false) const;
    Node_Count primary_count() const;
    /* Word functions */
    bool reduce(Word * rword,const Word & word,unsigned flags = 0) const
    {
      return word_reducer.reduce(rword,word,flags);
    }
    Total_Length interest_limit() const;
    bool is_interesting_equation(const Working_Equation & we,unsigned ae_flags);
    bool is_accepted_equation(const Working_Equation & we) const;
    bool need_partial_reduction_checks() const;
    // Derivation stuff
    State_ID get_derivation_id(Node_ID eid,bool insert);
    /* Commands */
    /* Methods for modified Knuth Bendix algorithm */
    bool add_axiom(const Word & lhs,const Word & rhs,unsigned flags);
    bool add_correction(const Ordinal_Word & lhs,const Ordinal_Word & rhs,bool is_primary = false);
    bool check_differences(unsigned ae_flags);
    bool container_status(unsigned level,int gap,const char * control,...) __attribute__((format(printf,4,5)));
    bool critical_status(unsigned level,int gap,const char * control,...) __attribute__((format(printf,4,5)));
    bool short_status(int priority,unsigned level,int gap,const char * control,Variadic_Arguments & va);
    // dm_valid() returns true if changes to the difference tracker
    // since the last call to grow_wd() would cause the specified difference
    // machine to be different.
    bool dm_valid(unsigned gwd_flags);
    /* expand_machine() return values
      -1 create provisional output and exit
       0 confluent
       1 not confluent but suggest attempt to build automatic structure
       2 create provisional output and resume
    */
    int expand_machine(bool limit = false);
    bool explore_acceptor(const FSA * word_acceptor,const FSA * dm2,
                          bool is_finite,Progress_Count allow = 0);
    void explore_dm(bool repeat);

    APIMETHOD FSA_Simple * grow_wd(unsigned ae_flags,unsigned gwd_flags);
    void import_difference_machine(const FSA_Simple & dm);
    bool is_dm_changing(bool include_history = true,
                        unsigned gwd_flags = 0,
                        bool force_tick = false);
    Node_Status learn_differences(Equation_Handle e);
    unsigned learn_differences(Working_Equation & we);

    Total_Length overlap_size(State e1,
                              State e2,
                              Word_Length offset) const;
    bool overlap_interesting(State e1,
                             State e2,
                             Word_Length offset);
    void prune();
    void remove_differences(Equation_Handle e);
    void set_difficult(Ordinal g) const
    {
      generator_properties[g] |= GF_DIFFICULT;
    }
    void set_validation_fsa(FSA_Simple * fsa);
    void set_timeout(Progress_Count timeout)
    {
      if (timeout > stats.first_timeout)
        stats.first_timeout = timeout;
      else
        stats.first_timeout = stats.first_timeout/2 + timeout/2;
      if (stats.first_timeout > 60)
        stats.first_timeout = 60;
    }
    void restart();
    void schedule_optimise(const Word & word);
    void start(); // to be called once before beginning KB expansion
                  // allows for one type initialisations that cannot be
                  // done until axioms have been added
    void update_machine(unsigned flags = 0);
  private:

    /* Methods for equation nodes */
    int add_equation(Working_Equation *we,unsigned flags = 0);
    void add_to_schedule(Equation_Handle e);
    bool schedule_expand(Multi_Node_List & fresh,
                         Multi_Node_List & old,
                         Equation_Handle e,bool force);
    bool check_interest(Equation_Handle e,State initial_state,Ordinal lvalue,Ordinal rvalue);
    void clear_differences(Node_List * equation_list);
    bool conjugate(Equation_Handle e);
    void schedule_right_conjugation(Equation_Handle e);
    void coset_extras(Equation_Handle e);
    int check_node(State node);
    int consider(Equation_Handle e1,Equation_Handle e2,Word_Length offset,
                 const Simple_Equation & eq1,
                 const Simple_Equation & eq2,
                 unsigned flags = 0);
    bool add_to_reduced_words(Sorted_Word_List &reduced_swl,
                              Word_DB &next_wdb,
                              Ordinal_Word * rword);

    void consider(Word_DB &current_wdb);
    Node_Status choose_status(Equation_Handle e,Node_Status recommendation);
    bool build_allowed(bool log);
    bool examine(unsigned rm_state);
    void expand(Equation_Handle e1,Node_Count done,unsigned ost);
    void hide_equations(FSA_Simple * coset_wa);
    void improve_dubious(Equation_Handle e);
    void improve_rhs(Equation_Handle e);
    void late_differences();
    bool prepare_differences(Equation_Handle e);
    void purge();
    bool relabel_difference(Node_Handle nh,Node_Flags flags,Node_Count to_do);
    void kill_node(Node * node);
    void reset_pool();
    void schedule(unsigned rm_state);
    void set_initial_special_limit();
    bool status(int priority,unsigned level,int gap,const char * control,...) __attribute__((format(printf,5,6)));
    bool add_to_pool(Element_ID *id,Working_Equation *we,unsigned flags);
    void optimise(const Word & word);
    void add_to_tree(Working_Equation * we,unsigned flags,Total_Length was_length);
};

#endif
