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
$Log: maf_dt.h $
Revision 1.12  2010/04/10 19:05:30Z  Alun
Lots of changes due to new style Node_Manager interface.
grow_wd() method changed to return an FSA_Simple instance rather than
a Difference_Tracker so that don't need to copy it again
Revision 1.11  2009/09/13 18:47:16Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.10  2008/10/22 07:13:04Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.10  2008/10/22 08:13:04Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
*/
#pragma once
#ifndef MAF_DT_INCLUDED
#define MAF_DT_INCLUDED 1
#ifndef MAFNODE_INCLUDED
#include "mafnode.h"
#endif
#ifndef LTFSA_INCLUDED
#include "ltfsa.h"
#endif
#ifndef MAF_WE_INCLUDED
#include "maf_we.h"
#endif
#ifndef MAF_EL_INCLUDED
#include "maf_el.h"
#endif

/* The Difference_Tracker class is used by Rewriter_Machine to keep track of
   the word-differences known so far and the shortest equations that give rise
   to them. It also does most of the work of building word-difference
   machines (and in fact is one).

   When a Difference_Tracker has undergone repeated modification due to changes
   to a Rewriter_Machine it will usually contain some errors - it may not
   associate the best equation with a word-difference or transition, or it may
   contain states that are not actual word-differences. It maintains two
   kinds of status information about this : valid() advises the caller
   whether the tracker should be rebuilt, and dm_changed()  advises the caller
   whether a difference machine built from the tracker would be different
   from one previously built using the same flags.
*/

const unsigned DTC_PRIMARY_STATE = 1;
const unsigned DTC_STATE = 2;
const unsigned DTC_PRIMARY_TRANSITION = 4;
const unsigned DTC_TRUE_TRANSITION = 8;
const unsigned DTC_COMPUTED_TRANSITION = 16;
const unsigned DTC_EQUATION = 32;
const unsigned DTC_PRIMARY_EQUATION = 64;
const unsigned DTC_INTEREST_EQUATION = 128;

class Difference_Tracker : public LTFSA
{
  public:
    struct Status
    {
      State_Count nr_differences;
      State_Count nr_primary_differences;
      State_Count nr_tertiary_differences;
      State_Count nr_recent_changes;
      State_Count nr_recent_primary_changes;
      State_Count nr_computed_transitions;
      State_Count nr_proved_transitions;
      State_Count nr_recent_transitions;
      Total_Length max_difference_eqn;  //largest size of equation having first occurrence of difference
      Total_Length max_primary_difference_eqn;
      Total_Length max_interesting_eqn;  //largest size of equation having first occurrence of transition
      Word_Length max_difference_length;//longest word-difference
      Status() :
        nr_differences(0),
        nr_primary_differences(0),
        nr_tertiary_differences(0),
        nr_recent_changes(0),
        nr_recent_primary_changes(0),
        nr_computed_transitions(0),
        nr_proved_transitions(0),
        nr_recent_transitions(0),
        max_difference_eqn(0),
        max_primary_difference_eqn(0),
        max_difference_length(0),
        max_interesting_eqn(0)
      {}
    };
  private:
    struct Pending_Pair
    {
      Pending_Pair * next;
      State_ID si1;
      State_ID si2;
      Pending_Pair(State_ID si1_,State_ID si2_) :
        si1(si1_),
        si2(si2_),
        next(0)
      {}
    };
    Node_Manager & nm;
    const Alphabet & full_alphabet;
    Array_Of<Node_ID> state_equations;
    Array_Of<Node_ID> state_primaries;
    Array_Of<Node_ID> state_hwords;
    Array_Of<State_ID> merge_map;
    Array_Of<State_ID> merge_next;
    Array_Of<State_ID> merge_prev;
    Array_Of<Word_Length> distance;
    State_List holes;
    Status stats;
    State_Count count;
    Transition_ID * reverse_product;
    Total_Length limit;
    Ordinal nr_generators; // N.B. This is not necessarily the same as
                           // nm.nr_generators.
    unsigned char changes;
    unsigned char recent_changes;
    Pending_Pair * pending_pair;
    Pending_Pair ** pending_tail;
    short closed:1;
    short interest_wrong:1;
    short ok:1;
    short max_changed:1;
  public:
    const Status &dt_stats;
  private:
    Difference_Tracker(Node_Manager &nm_,State_Count hash_size,
                       bool cached);
  public:
    static Difference_Tracker * create(Node_Manager &nm_,State_Count hash_size,bool cached = true)
    {
      return new Difference_Tracker(nm_,hash_size,cached);
    }
    ~Difference_Tracker();
    // queries
    /* dm_changed() returns true if the word-difference machine that
       would be built using the specified flags has changed since the
       Difference_Tracker was closed() */
    bool dm_changed(unsigned gwd_flags,bool recent = false) const;
    Total_Length interest_limit() const
    {
      /* returns the size of the longest equation which shows that a possible
         transition between word-differences is required.
         This is at least the maximum size of the equations associated
         with a difference machine state, and is possibly a lot more. */
      return this ? stats.max_interesting_eqn : 0;
    }
    bool import_difference_machine(const FSA_Simple & dm);
    bool is_accepted_equation(const Working_Equation & we) const
    {
      if (we.lhs_word().value(0) >= nr_generators)
        return true;
      return product_accepted(we.lhs_word(),we.rhs_word());
    }
    bool is_valid(bool check_interest) const
    {
      /* returns false if it is advisable to rebuild the Difference_Tracker
         due to inaccuracies that may be present */
      return ok && !(interest_wrong && check_interest);
    }
    const Status & status() const
    {
      return stats;
    }
    Total_Length vital_eqn_limit() const
    {
      /* returns the size of the longest equation (in a geodesic ordering)
         which could affect the reduction of words of the form Xwy where
         x,y are generators and w is a word-difference */
      return this ? stats.max_difference_length*2+2 : 0;
    }
    // commands
    void clear_changes(bool recent = true)
    {
      if (recent)
        recent_changes = 0;
      else
        changes = 0;
    }
    void close(bool invalidate = false)
    {
      if (!closed)
      {
        closed = true;
        changes = 0;
      }
      if (invalidate)
        ok = false;
    }
    void compute_transitions(unsigned gwd_flags);
    FSA_Simple *grow_wd(unsigned gwd_flags);
    bool is_inverse_complete();
    void set_limit(Total_Length limit_)
    {
      limit = limit_;
    }
    unsigned learn_equation(Working_Equation & we,Equation_Handle e,unsigned flags);
    void remove_differences(Equation_Handle e);
    bool relabel_difference(Node_Handle s,Node_Flags flags);
    void scan_all_differences();
    State_Count take_changes(unsigned gwd_flags = 0)
    {
      State_Count answer(stats.nr_recent_changes);
      if (gwd_flags & GWD_PRIMARY_ONLY)
      {
        answer += stats.nr_recent_primary_changes;
        stats.nr_recent_primary_changes = 0;
      }
      if (gwd_flags & GWD_KNOWN_TRANSITIONS)
      {
        answer += stats.nr_recent_transitions;
        stats.nr_recent_transitions = 0;
      }
      stats.nr_recent_changes = 0;
      return answer;
    }
    bool take_max_changed()
    {
      bool retcode = max_changed!=0;
      max_changed = 0;
      return retcode;
    }
    bool is_interesting_equation(const Working_Equation & we,unsigned ae_flags);
    Node_Reference nm_state(State_ID si) const
    {
      Node_ID answer = 0;
      LTFSA::get_state_key(&answer,si);
      return Node_Reference(nm,answer);
    }
  private:
    friend class Filtered_Word_Difference_Machine;
    void detach_all();
    State_ID check_difference_label(State_ID si,Equation_Word &ew0,bool needed);
    State_ID create_difference(Equation_Word &ew0,Word_Length defining_length,
                               State_ID si);
    State_ID create_transition(State_ID si,Transition_ID ti,unsigned flags);
    State_ID filtered_new_state(State_ID initial_state,
                                Transition_ID symbol_nr,
                                bool buffer,
                                unsigned gwd_flags) const;
    State_ID find_state(Node_Handle s,bool insert = true,bool map = true);
    unsigned learn_initial_difference(State_ID * si,
                                      Node_Handle h_is,
                                      Node_Handle g_is,
                                      Equation_Handle e,const Word &new_lhs,
                                      unsigned flags);
    unsigned setup_difference(State_ID nsi,Equation_Handle e,const Word &new_lhs,unsigned flags);

    Node_Reference get_equation(State_ID si,Transition_ID ti) const
    {
      return Node_Reference(nm,(Node_ID) get_transition_label(si,ti));
    }
    void flag_changes(unsigned char flags)
    {
      changes |= flags;
      recent_changes |= flags;
    }
    void label_states(Difference_Tracker & other,State_ID si = -1);
    unsigned learn_transition(State_ID * si,int * distance,
                              Ordinal lvalue,Ordinal rvalue,
                              Equation_Handle e,const Word & new_lhs,
                              int position,unsigned flags,Total_Length g_length);
    void merge_pair(State_ID si1,State_ID si2,const Derivation &d,State_ID * buffer);
    void merge_states(State_ID si1,State_ID si2,const Derivation &d);
    Node_Reference nm_difference(Equation_Word * ew1,const Equation_Word &ew0,Transition_ID ti) const;
    bool valid_transition(State_ID si,Transition_ID ti,State_ID nsi) const;
    void queue_pair(State_ID si1,State_ID si2);
    void set_equation(State_ID si,Transition_ID ti,Node_Handle e)
    {
      Node_ID nid(e);
      set_transition_label(si,ti,nid);
      if (!e.is_null())
        e->attach(nm);
    }
    void set_state_key(State_ID si,Node_ID nid)
    {
      LTFSA::set_state_key(si,&nid,sizeof(Node_ID));
    }
};

#endif
