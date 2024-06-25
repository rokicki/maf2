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
$Log: maf_jm.h $
Revision 1.12  2010/01/30 22:33:58Z  Alun
added methods and members to optimise difficult reductions.
Many method parameters changed because of new style Node_Manager interface
Revision 1.11  2009/09/12 18:48:36Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.10  2008/11/02 19:00:56Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.11  2008/11/02 20:00:55Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.10  2008/10/02 08:57:34Z  Alun
pruning flag added
Revision 1.9  2008/09/27 10:12:21Z  Alun
Final version built using Indexer.
Revision 1.7  2007/12/20 23:25:45Z  Alun
*/
#pragma once
#ifndef MAF_JM_INCLUDED
#define MAF_JM_INCLUDED 1

#ifndef MAF_RM_INCLUDED
#include "maf_rm.h"
#endif

#ifndef MAFQUEUE_INCLUDED
#include "mafqueue.h"
#endif

#ifndef MAF_ONT_INCLUDED
#include "maf_ont.h"
#endif

//Classes defined in this header file
class Rewriter_Machine::Job_Manager;
class Equation_Queue;

//Classes and types defined elsewhere
#ifndef NODE_STATUS_INCLUDED
#include "node_status.h"
#endif
//enum Node_Status; //Can't do this, although it is clearly sensible.
class Node_Equation;
class Node_Overlap;
class Optimisation_Request;

struct Equation_Stats
{
  Node_Count nr_equations;
  Node_Count nr_visible;
  Node_Count nr_adopted;
  Node_Count nr_expanded;
  Word_Length shortest_new_lhs;
  Equation_Stats() :
    nr_equations(0),
    nr_visible(0),
    nr_adopted(0),
    shortest_new_lhs(INVALID_LENGTH)
  {}
};

class Rewriter_Machine::Job_Manager
{
  public:

    class Overlap_Queue
    {
      private:
        Node_Overlap *head;
        Node_Overlap **tail;
        Node_Count count;
        Rewriter_Machine &rm;
      public:
        Overlap_Queue(Rewriter_Machine & rm_) :
          head(0),
          tail(&head),
          count(0),
          rm(rm_)
        {}
        ~Overlap_Queue();
        void add(Node_Overlap *);
        Node_Overlap * use();
        void weed();
        Node_Count length() const
        {
          return count;
        }
    };
  private:
    /* priority is a negative number, with -1 for the most urgent tasks,
        -2, for the next most urgent and so on */
    MAF &maf;
    int new_priority;
    bool overloaded;
    bool pruning;
    bool optimising;
    bool dm_broken;
    unsigned weed;
    Node_List bad_inverse;
    Node_List bad_difference;
    Sortable_Node_List partial_reductions;
    Node_Equation *pending_head;
    Node_Equation *pending_tail;
    Node_Count pending_count;
    Node_Equation *urgent_pending_head;
    Node_Equation *urgent_pending_tail;
    Optimisation_Request *optimisation_head;
    Optimisation_Request *optimisation_tail;
    Element_Count optimisation_count;
    Node_Count urgent_pending_count;
    Overlap_Queue total_overlaps;
    Overlap_Queue pending_overlaps;
//    By_Left_Size_Node_Tree unconjugated;
    Equation_Queue unconjugated;
    Equation_Queue undifferenced;
//    Equation_Queue dubious;
    Equation_Queue uncorrected;
    Equation_Queue rhs_pending;
    Equation_Queue oversized;
//    Equation_Queue removed;
    By_Left_Size_Node_Tree_Queued dubious;
    By_Size_Node_Tree_Queued removed;
    Equation_Stats stats;
  public:
    Rewriter_Machine & rm;
    Job_Manager(Rewriter_Machine & rm_);
    ~Job_Manager();
    // queries
    const Equation_Stats & status() const
    {
      return stats;
    }

    bool work_pending() const;
    // commands
    void cancel_jobs(Equation_Handle e);
    void change_pool_count(int delta)
    {
      stats.nr_equations += delta;
    }
    void equate_nodes(Node_Handle first,Node_Handle second,
                      const Derivation &d,Ordinal difference = -1,bool urgent = true);
    void schedule_difference_correction(Node_Handle node);
    void schedule_inverse_check(Node_Handle node);
    void schedule_jobs(Equation_Handle e,Node_Status new_state);
    void schedule_optimise(const Word & word);
    void schedule_overlap(Equation_Handle e1,Equation_Handle e2,
                          Word_Length offset,bool is_total);
    void schedule_partial_reduction_check(Node_Handle node,Ordinal g);
    void update_machine(unsigned flags);
    void divert_differences();
    void urgent_deductions();
  private:
    void check_differences(Equation_Queue & queue,unsigned flags,int priority);
    void correct_differences(unsigned flags,int priority);
    void check_inverses(unsigned flags,int priority);
    void check_dubious_reductions(unsigned flags,int priority);
    void check_partial_reductions(unsigned flags,int priority);
    void recheck_partial_reductions(unsigned flags,int priority);
    void check_pool(unsigned flags,int priority);
    void conjugate(unsigned flags,int priority);
    void deduce(unsigned flags,int priority);
    void make_room_for_more_jobs();
    void optimise(unsigned flags,int priority);
    void overlaps(Overlap_Queue * queue,unsigned flags,int priority);
    bool perform_job(unsigned flags);
    void pre_correct(unsigned flags,int priority);
    void remove(unsigned flags,int priority);
    void rhs_corrections(unsigned flags,int priority);
    void set_priority(int priority)
    {
      if (priority > new_priority)
        new_priority = priority;
    }
    void update_inner(unsigned flags,int priority)
    {
      /* update_inner() is used by the worker functions above to
         see if they should break off what they are doing to
         allow something more urgent to be done. */
      while (new_priority > priority)
        if (!perform_job(flags))
          break;
    }
    void weed_jobs();

};

#endif
