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
$Log: maf_nm.h $
Revision 1.14  2010/06/10 13:58:08Z  Alun
All tabs removed again
Revision 1.13  2010/06/03 11:14:59Z  Alun
Lots of changes required by decision to use Node_ID rather than Node *
in data members of Node to save space in 64-bit version.
Revision 1.12  2009/11/09 20:56:05Z  Alun
Comment and name changes to try to improve readability.
Revision 1.11  2009/09/12 18:48:36Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.10  2008/12/28 23:08:28Z  Alun
g_height() method added. tc renamed to ntc to detect parts of code needing
to be changed after support for sparse nodes was added
Revision 1.9  2008/09/27 10:01:50Z  Alun
Final version built using Indexer.
Revision 1.6  2007/11/15 22:58:12Z  Alun
*/
#pragma once
#ifndef MAF_NM_INCLUDED
#define MAF_NM_INCLUDED 1

#ifndef MAF_INCLUDED
#include "maf.h"
#endif

#ifndef CERTIFICATE_INCLUDED
#include "certificate.h"
#endif

#ifndef NODEBASE_INCLUDED
#include "nodebase.h"
#endif

// Classes defined in this header file
class Node_Manager;
class Overlap_Filter;

/* Node_Manager manages the web of Node elements for Rewriter_Machine
   Several modules interact with this directly, rather than with the
   whole Rewriter_Machine.
*/

// Types referred to but defined elsewhere
class Node;
class Rewriter_Machine;
class MAF;
class Derivation;
class Equation_Word;
class Node_List;
class Block_Manager;
class Transition_Check;

/* During KB completion we may have equations of very different sizes and
   ages, and want to consider overlaps between them in a way that we hope
   makes early success more likely.
   We do this by restricting which overlaps we consider, according to
   various rules based on the size and status of the overlapping equations.
   For any particular overlap we want to be able to answer questions such
   as: "Must we consider this overlap now?" and "Has this overlap been
   considered already?". Note that it is perfectly reasonable to consider
   overlaps more than once, since the same overlap may give rise to different
   equations at different times, either because the RHSes of one or
   other equation have changed (though this does not really apply since MAF
   never considers such overlaps to have been considered before), or because
   the resulting equation involves reducible words whose reductions may
   change over time.

   Our answer to these questions may also vary according to what we are
   trying to do at the moment. For example it can happen that a phase of
   KB completion needs to be aborted because it has been decided that our
   current limits are inappropriate, and under these circumstances the
   rest of the phase is abbreviated as much as possible.

   The Overlap_Filter is a class that is a record of the "contract"
   Rewriter_Machine is applying on the current pass of the Knuth Bendix
   procedure. Under the terms of the contract Rewriter_Machine is obliged
   to ensure that certain overlaps have been considered, but can choose, or
   not as it pleases, to consider certain other overlaps. We also remember
   the contract from the previous path to help us to decide what has already
   been done. On each pass of the procedure the terms of the contract are
   widened to include more and more potential overlaps.
   This contract is fairly flexible: typically we can imagine that initially,
   when an overlap comes into existence, it will be between LHS that are
   completely outside the scope of the current contract. At some point the
   contract will be widened, so far that Rewriter_Machine is allowed to
   consider the overlap, but later on we may not be certain whether or not
   it in fact did so. Still later on the terms of the contract will be such
   that the overlap must be guaranteed to have been considered.

   Given equations ab=d bc=e, the overlap is abc and the equation is dc=ae

   MAF is only absolutely obliged to consider an overlap when all the
   following criteria apply:

   1) |abc| <= overlap_limit
   2) The id timestamp of the other partner in the overlap is
      no greater than the specified maximum which will be set for
      expanded_timestamp for the equation being expanded
   3) The equation arising from the overlap is, prior to normalisation,
      no larger than the current keep_limit.
   4) Either the other partner in the overlap is an expanded equation or
      the MSF_SELECTIVE_PROBE strategy flag is not set.
   5) The expanded_timestamp and the id of the other equation don't prove
      that these criteria were already fulfilled in an earlier pass.

   In all other cases consideration of an overlap is optional, and whether
   or not the overlap is considered will depend on a combination of
   the value of the ost member of the passed to find_candidate,
   various strategy flags, and properties of the equations themselves that
   may mark them out for special consideration.
   In fact, unless we are looking for word-differences, the contract will
   be a lot simpler than this : MAF generally won't look at optional overlaps,
   only those it is obliged to.
*/
class Overlap_Filter
{
  public:
    Total_Length overlap_limit; /* If the overlap length is no greater than
                                   this it should be considered. */
    Total_Length check_limit;   /* If, prior to reduction, the size of the
                                   equation resulting from the overlap is no
                                   greater than this, it should be considered.
                                */
    Total_Length keep_limit;    /* If, prior to reduction, the size of the
                                   equation is no greater than this, the
                                   Rewriter machine promises not to discard
                                   this equation */
    Total_Length vital_limit;   /* If, prior to reduction, the size of the
                                   equation is no greater than this, the
                                   equation must be considered */
    Total_Length forbidden_limit; /* If the overlap length is no more than
                                     this it may be considered */
    Word_Length probe_depth;
    bool filtered;                /* set to true if should_consider() ever
                                     returned false */
  public:
    Overlap_Filter() :
      overlap_limit(0),
      check_limit(0),
      keep_limit(0),
      vital_limit(0),
      forbidden_limit(0),
      probe_depth(0),
      filtered(false)
    {}
    void set(Total_Length overlap_limit_,
             Total_Length check_limit_,
             Total_Length keep_limit_,
             Total_Length forbidden_limit_,
             Word_Length probe_depth_)
    {
      overlap_limit = overlap_limit_;
      check_limit = check_limit_;
      keep_limit = keep_limit_;
      forbidden_limit = forbidden_limit_;
      if (forbidden_limit < overlap_limit)
        forbidden_limit = overlap_limit;
      probe_depth = probe_depth_;
      filtered = false;
    }
    void set_vital_limit(Total_Length vital_limit_)
    {
      vital_limit = vital_limit_;
    }
    void set_filtered()
    {
      filtered = true;
    }
    bool has_filtered() const
    {
      return filtered;
    }
};

class Node_Manager : private Nodes
{
  friend class Node;
  friend class Rewriter_Machine;
  friend class Block_Manager;
  public:
    struct Node_Stats
    {
      Node_Count ntc[2];
      Node_Count nc[5];
    };
  private:
    Node_Reference root;
    ID last_id;
    ID last_removed_id;
    Certificate current;
    Node_Stats stats;
    Block_Manager &bm;
    bool confluent;
    bool in_destructor;
    // Constructor destructor
    Node_Manager(Rewriter_Machine &rm_);
    ~Node_Manager();
  public:
    const Ordinal nr_generators;
    Rewriter_Machine &rm;
    MAF & maf;
    const Presentation_Data &pd;
    Overlap_Filter old_overlap_filter;
    Overlap_Filter overlap_filter;
    //Queries
    const Alphabet & alphabet() const
    {
      return maf.alphabet;
    }
    /* Because the Rewriter_Machine changes over time reduction information
       can become invalid. Certificates are used to check whether a word
       that is supposedly reduced or has a state actually does so. */
    Certificate_Validity check_certificate(const Certificate & other) const
    {
      return current.check(other);
    }
    const Certificate & current_certificate() const
    {
      return current;
    }
    Node_Reference get_state(const Ordinal *values,Word_Length end,
                             Word_Length start = 0) const;
#ifdef MAFNODE_INCLUDED
    Node_Reference new_state(State s,Ordinal g) const
    {
      Ordinal child_start,child_end;
      valid_children(&child_start,&child_end,s->last_letter());
      if (g >= child_start && g < child_end)
        return s->transition(*this,g);
      // we will get here if this is a coset system and the requested
      // transition is of type hg,gh,g_H,_H_H or _Hh.
      // Only gg,hh,h_H and _Hg are valid
      // it would make sense to return 0 if g is not a valid child
      // for the time being we will revert to the transition from the root.
      return root->transition(*this,g);
    }
    State node_find(Node_ID id) const
    {
      return Node::find_node(*this,id);
    }
    State fast_node_find(Node_ID id) const
    {
      return Node::fast_find_node(*this,id);
    }
#endif
    bool has_filtered() const
    {
      return overlap_filter.has_filtered();
    }
    Word_Length height() const;
    Word_Length g_height() const;
    bool in_tree_as_subword(const Equation_Word & lhs) const;
    Ordinal inverse(Ordinal g) const
    {
      return maf.inverse(g);
    }
    bool is_confluent() const
    {
      return confluent;
    }
    Ordinal safe_inverse(Ordinal g) const
    {
      return g < 0 ? g : maf.inverse(g);
    }
    Node_Reference start() const
    {
      return root;
    }
    void valid_children(Ordinal * start,Ordinal * end,Ordinal rvalue) const
    {
      pd.right_multipliers(start,end,rvalue);
    }
    void valid_prefixes(Ordinal * start,Ordinal * end,Ordinal lvalue) const
    {
      pd.left_multipliers(start,end,lvalue);
    }
    void validate_tree() const;
    // Commands
    void check_transitions(const Equation_Word & lhs,
                           Word_Length new_prefix_length,
                           bool rhs_changed_ = true,
                           bool publish_overlaps = true,
                           bool new_reduction = false);
    Node_Reference construct_equation(Node_Handle prefix,Ordinal rvalue,
                                      unsigned short eq_flags,
                                      Node_Handle rhs_node,
                                      Node_Handle primary);
    void inspect(Node_Reference node,bool check_height);
    Node_Reference node_get();
    void purge();
    void set_overlap_limits(Total_Length overlap_limit,
                            Total_Length attempt_limit,
                            Total_Length keep_limit,
                            Total_Length forbidden_limit,
                            Word_Length probe_depth)
    {
      old_overlap_filter = overlap_filter;
      overlap_filter.set(overlap_limit,attempt_limit,keep_limit,
                         forbidden_limit,probe_depth);
    }
    void set_vital_limit(Total_Length vital_limit)
    {
      if (pd.is_short)
        overlap_filter.set_vital_limit(vital_limit);
    }

    void equate_nodes(Node_Handle first,Node_Handle second,
                      const Derivation &d,Ordinal difference = -1,
                      bool create_now = false,bool force_insert = false);
    void update_machine();

  private:
    void check_transitions_inner(Transition_Check & tc,Node_Reference nr,
                                 Word_Length offset);
    void destroy_tree();
    bool in_tree_as_subword_inner(const Equation_Word & word,Node_Reference nr) const;
    void node_free(Node * node);
    void remove_node(Node_Handle nh);
    /* statistics */
    void count_in(Language language)
    {
      stats.nc[language]++;
    }
    void count_out(Language language)
    {
      stats.nc[language]--;
    }
    /* publish/revoke methods are called internally to announce various types
       of change to the tree */
    void publish_partial_reduction(Node_Handle node,Ordinal g);
    void publish_bad_difference(Node_Handle node);
    void publish_bad_inverse(Node_Handle node);
    void publish_bad_rhs(Equation_Handle e);
    void publish_dubious_rhs(Equation_Handle e,bool rhs_bad = false);
    void publish_transition(Boolean final)
    {
      stats.ntc[final ? 1 : 0]++;
    }
    void revoke_transition(Boolean final)
    {
      stats.ntc[final ? 1 : 0]--;
    }
};


#endif
