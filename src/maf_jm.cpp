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


// $Log: maf_jm.cpp $
// Revision 1.19  2010/06/10 13:57:38Z  Alun
// All tabs removed again
// Revision 1.18  2010/05/29 19:59:39Z  Alun
// Various changes required by new style Node_Manager interface
// New job added for optimisation of slow reductions (but this does not
// always work well yet)
// Priority of dealing with removed equations has been lowered
// Revision 1.17  2009/11/08 23:38:52Z  Alun
// normalised_limit member renamed
// Revision 1.16  2009/09/13 18:57:40Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.15  2009/09/01 12:08:10Z  Alun
// long comments updated
// Revision 1.14  2008/10/22 15:54:22Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.16  2008/10/22 16:54:22Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.15  2008/10/12 23:26:08Z  Alun
// Should not have AE_INSERT on AE_SECONDARY equations as this can cause endless
// insertion of new equations
// Revision 1.14  2008/10/02 08:59:24Z  Alun
// pruning flag added, so that processing of removed equations can take account
// of this if necessary
// Revision 1.13  2008/09/29 22:40:58Z  Alun
// Switch to using Hash+Collection_Manager as replacement for Indexer.
// Currently this is about 10% slower, but this is a more flexible
// architecture.
// Revision 1.7  2007/12/20 23:25:42Z  Alun
//

/* Any equation that is added to a Rewriter_Machine instance may have a large
   number of consequences, which require us to add or remove other equations.

   For example equations may have been deleted because a prefix of their
   LHS is now reducible, or when adding equations we may have noted
   other possible equations that could be added, or we may find that two
   distinct words have the same reduced inverse and are therefore equal.

   To prevent recursion getting out of hand the Rewriter_Machine::Job_Manager
   class allows lists to be built containing information about changes to be
   made to the Rewriter_Machine instance when convenient. I call each such
   change a "job".

   The Job_Manager::update_machine() method is used to process some or all of
   these deferred consequences in a sensible order: the Rewriter_Machine
   instance can call Job_Manager::update_machine() when it wants this to be
   done.

   There are two main ways to create jobs: in the first place we may have
   discovered an explicit equation. For example from ax=b we can deduce
   bX=a. When such equations are discovered the equate_nodes() method can
   be called to request that the appropriate equation will be added at some
   later point.

   Jobs are also created implicitly through the schedule_jobs() method.
   Each equation has a status that indicates what processing is still
   required for it, and for most of the state an equation can be in there
   is a queue of equations with that status. For each such queue
   there is a method that extracts equations from the queue, performs the
   appropriate processing, and puts the equation in the next appropriate
   queue.

   If an equation's state changes in some way, cancel_jobs() is used to
   remove it from its current queue, and another call to schedule_jobs() is
   made to assign the equation to a new queue.

   Each kind of job is associated with a priority level. update_machine()
   will always perform jobs in descending order of priority, which
   it does by calling update_inner(), which in turn calls perform_job()
   with successively lower priorities until it is told it has finished.

   perform_job() calls the worker function that has been assigned that
   priority, telling it its priority. Each worker function must call
   update_inner() after each change it makes, to allow a more urgent task
   to be run if necessary. perform_job() returns false when the priority
   is below that of any kind of job.

   update_machine() just calls update_inner() with priority -999, so
   all appropriate outstanding work gets done.

   This implementation means that additional kinds of job can readily be
   added if necessary, and that the individual methods don't care what their
   priority is - all that needs to be changed is the association between
   priority and worker method in perform_job().

   We give difference calculations a higher priority than conjugation to
   ensure that we at least start calculating word-differences early on.
   This will hopefully reduce the number of secondary equations that
   are created, since fewer equations will be "interesting". This does
   mean that methods that compute word-differences may have to create
   more equations themselves, and consequently the difference tracker will
   have a few more errors.
   Note that equations are forced to be conjugated before they are differenced
   anyway. So currently these operations proceed roughly in parallel.
*/
#include "maf_jm.h"
#include "mafnode.h"
#include "maf_wdb.h"
#include "container.h"
#include "maf_we.h"

const int Priority_Optimise = -1;
const int Priority_Urgent_Deduce = -2;
const int Priority_Pre_Correct = -3;
const int Priority_Dubious = -4;
const int Priority_Total_Overlap = -5;
const int Priority_Correct = -6;
const int Priority_Partial = -7;
const int Priority_Remove = -8;
const int Priority_Deduce = -9;
const int Priority_Inverse = -10;
const int Priority_Bad_Difference = -11;
const int Priority_Difference = -12;
const int Priority_Overlap = -13;
const int Priority_Conjugate = -14;
const int Priority_Pool = -15;
const int Priority_Recheck_Partial = -16;

class Node_Equation
{
  friend class Rewriter_Machine::Job_Manager;
  private:
    Node_Equation * next;
    Node_ID first;
    Node_ID second;
    Ordinal transition;
    Derivation derivation;
  public:
    Node_Equation(Node_Manager &nm,Node_Handle first_,Node_Handle second_,
                  const Derivation & derivation_,
                  Ordinal transition_) :
      first(first_),
      second(second_),
      derivation(derivation_),
      transition(transition_),
      next(0)
    {
      first_->attach(nm);
      second_->attach(nm);
    }

    void release(Node_Manager &nm)
    {
      Node::fast_find_node(nm,first)->detach(nm,first);
      Node::fast_find_node(nm,second)->detach(nm,second);
      delete this;
    }
};

class Node_Overlap
{
  friend class Rewriter_Machine::Job_Manager;
  friend class Rewriter_Machine::Job_Manager::Overlap_Queue;
  private:
    Node_Overlap * next;
    Node_ID first;
    Node_ID second;
    Word_Length offset;
  public:
    Node_Overlap(Node_Manager &nm,Node_Handle first_,Node_Handle second_,
                 Word_Length offset_) :
      first(first_),
      second(second_),
      offset(offset_),
      next(0)
    {
      first_->attach(nm);
      second_->attach(nm);
    }

    void release(Node_Manager &nm)
    {
      Node::fast_find_node(nm,first)->detach(nm,first);
      Node::fast_find_node(nm,second)->detach(nm,second);
      delete this;
    }
};

class Optimisation_Request
{
  friend class Rewriter_Machine::Job_Manager;
  private:
    Optimisation_Request * next;
    Ordinal_Word word;
  public:
    Optimisation_Request(const Word &word_) :
      word(word_),
      next(0)
    {}

    void release()
    {
      delete this;
    }
};

/**/

Rewriter_Machine::Job_Manager::Job_Manager(Rewriter_Machine & rm_) :
  rm(rm_),
  maf(rm_.maf),
  removed(rm_.nm),
  uncorrected(rm_.nm),
  rhs_pending(rm_.nm),
  unconjugated(rm_.nm),
  undifferenced(rm_.nm),
  oversized(rm_.nm),
  dubious(rm_.nm),
  total_overlaps(rm_),
  pending_overlaps(rm_),
  partial_reductions(rm_.nm),
  pending_head(0),
  pending_tail(0),
  pending_count(0),
  urgent_pending_head(0),
  urgent_pending_tail(0),
  urgent_pending_count(0),
  optimisation_head(0),
  optimisation_tail(0),
  optimisation_count(0),
  new_priority(-999),
  weed(0),
  pruning(false),
  overloaded(false),
  optimising(false),
  dm_broken(false)
{
}

/**/

Rewriter_Machine::Job_Manager::~Job_Manager()
{
  Node_Reference e;
  while (partial_reductions.use(&e))
    e->detach(rm.nm,e);
  while (bad_inverse.use(&e,rm.nm))
    e->detach(rm.nm,e);
  while (removed.use(&e))
    e->detach(rm.nm,e);
  while (bad_difference.use(&e,rm.nm))
    e->detach(rm.nm,e);
  while (pending_head)
  {
    Node_Equation * current = pending_head;
    pending_head = current->next;
    current->release(rm.nm);
  }
  while (urgent_pending_head)
  {
    Node_Equation * current = urgent_pending_head;
    urgent_pending_head = current->next;
    current->release(rm.nm);
  }
  while (optimisation_head)
  {
    Optimisation_Request * current = optimisation_head;
    optimisation_head = current->next;
    current->release();
  }
}

/**/

void Rewriter_Machine::Job_Manager::equate_nodes(Node_Handle first,Node_Handle second,
                                                 const Derivation &d,
                                                 Ordinal difference,bool urgent)
{
  if (!maf.options.no_deductions)
  {
    Node_Equation * new_equation = new Node_Equation(rm.nm,first,second,d,difference);
    State s;
    if (urgent &&
        (difference==PADDING_SYMBOL ||
         second->length(rm.nm) <= first->length(rm.nm)+1 ||
         first->transition(rm.nm,difference)->reduced_length(rm.nm) >=
         second->length(rm.nm) ||
         rm.pd.is_coset_system &&
         (s = Node::find_node(rm.nm,d.right_conjugate()))!=0 &&
         s->first_letter(rm.nm) == rm.pd.coset_symbol))
    {
      if (urgent_pending_head)
        urgent_pending_tail->next = new_equation;
      else
        urgent_pending_head = new_equation;
      urgent_pending_tail = new_equation;
//      if (!urgent_pending_head)
//        urgent_pending_tail = new_equation;
//      new_equation->next = urgent_pending_head;
//      urgent_pending_head = new_equation;
      urgent_pending_count++;
      set_priority(Priority_Urgent_Deduce);
    }
    else
    {
      if (pending_head)
        pending_tail->next = new_equation;
      else
        pending_head = new_equation;
      pending_tail = new_equation;
//    if (!pending_head)
//      pending_tail = new_equation;
//    new_equation->next = pending_head;
//    pending_head = new_equation;
      pending_count++;
      set_priority(Priority_Deduce);
    }
  }
}

/**/

void Rewriter_Machine::Job_Manager::schedule_optimise(const Word & word)
{
  if (!maf.options.no_equations && !optimising)
  {
    Optimisation_Request * new_or = new Optimisation_Request(word);
    if (optimisation_head)
      optimisation_tail->next = new_or;
    else
      optimisation_head = new_or;
    optimisation_tail = new_or;
    optimisation_count++;
    set_priority(Priority_Optimise);
  }
}

void Rewriter_Machine::Job_Manager::Overlap_Queue::add(Node_Overlap * new_overlap)
{
  if (new_overlap)
  {
    *tail = new_overlap;
    tail = &new_overlap->next;
    *tail = 0; // just in case
    count++;
  }
}

Node_Overlap * Rewriter_Machine::Job_Manager::Overlap_Queue::use()
{
  Node_Overlap * answer = head;
  if (answer)
  {
    head = answer->next;
    count--;
  }
  if (!head)
    tail = &head;
  return answer;
}

/**/

Rewriter_Machine::Job_Manager::Overlap_Queue::~Overlap_Queue()
{
  Node_Overlap * nov;
  while ((nov = use())!=0)
    nov->release(rm);
}

void Rewriter_Machine::Job_Manager::Overlap_Queue::weed()
{
  tail = &head;
  Node_Count done = 0;
  Node_Count to_do = count;
  int gap = 2;
  while (*tail)
  {
    Node_Overlap * current = *tail;
    Node_Overlap * next = current->next;
    Node_Manager &nm = rm;
    if (Node::fast_find_node(nm,current->first)->flagged(NF_REMOVED) ||
        Node::fast_find_node(nm,current->second)->flagged(NF_REMOVED))
    {
      count--;
      current->release(rm);
      *tail = next;
    }
    else
      tail = &current->next;

    if (!(char) ++done && rm.critical_status(2,gap,
                                             "Weeding overlap list (" FMT_NC " of " FMT_NC ")\n",
                                              done,to_do))
      gap = 1;
  }
}

/**/

void Rewriter_Machine::Job_Manager::make_room_for_more_jobs()
{
  if (!maf.options.no_throttle)
  {
    if (weed > 100 &&
        (partial_reductions.length() > 50000 ||
         total_overlaps.length() > 50000 ||
         pending_overlaps.length() > 50000))
      weed_jobs();

    if (partial_reductions.length() > 50000 ||
        total_overlaps.length() > 50000 ||
        pending_overlaps.length() > 50000)
      overloaded = true;
  }
}

void Rewriter_Machine::Job_Manager::schedule_overlap(Node_Handle first,
                                                     Node_Handle second,
                                                     Word_Length offset,
                                                     bool is_total)
{
  /* This method is called whenever the node manager notices a potential
     overlap between an existing and a new equation. If we consider
     the overlap to be interesting it is processed from within
     update_machine() and is therefore considered earlier than it would be
     otherwise. */
  if (rm.stats.want_overlaps)
  {
    if (is_total)
    {
      total_overlaps.add(new Node_Overlap(rm.nm,first,second,offset));
      set_priority(Priority_Total_Overlap);
    }
    else if (!overloaded && rm.overlap_interesting(first,second,offset))
    {
      make_room_for_more_jobs();
      pending_overlaps.add(new Node_Overlap(rm.nm,first,second,offset));
      set_priority(Priority_Overlap);
    }
  }
}

/**/

void Rewriter_Machine::Job_Manager::schedule_partial_reduction_check(Node_Handle node,
                                                                     Ordinal g)
{
  /* This method is called whenever a link to a shorter equation is created.
     In such cases there is definitely not a primary equation at the point
     in question. However it is possible that the prefix is in fact reducible
     but this has not been noticed yet because a relevant overlap has not
     yet been considered. So whenever we create such a link we schedule a
     check for the relevant overlap as well.

     In the case where a generator X has been eliminated and replaced by word
     w, one would think
     that it might be worth considering the equation axX=a at nodes ending
     in X, since although there is no overlap, the first reduction axw=a
     might well contain an overlap different from the overlaps in xw=IdWord.
     However, this does not seem to be worth doing usually, not because we
     wouldn't learn something new, but because the lhs is sometimes very
     hard to reduce and the resulting equation is very big.
  */

  make_room_for_more_jobs();
  if (!maf.aborting && maf.options.partial_reductions && !overloaded &&
      node->irreducible_length() < rm.stats.visible_limit+8)
  {
    Ordinal ig = maf.inverse(g);

    if (ig != INVALID_SYMBOL && !rm.difficult(g) &&
        ig != node->last_letter() &&
        !rm.redundant(g) &&
        (!rm.redundant(ig) || maf.options.partial_reductions & 4) &&
        (!maf.options.no_h || g < rm.pd.coset_symbol ||
         maf.options.partial_reductions & 2))
    {
      node->attach(rm.nm);
      partial_reductions.add_state(node,g);
      set_priority(Priority_Partial);
    }
  }
}

/**/

void Rewriter_Machine::Job_Manager::schedule_inverse_check(Node_Handle node)
{
  if (!node->flagged(NF_BAD_INVERSE) && !node->is_final())
  {
    node->attach(rm.nm)->set_flags(NF_BAD_INVERSE);
    bad_inverse.add_state(node);
    set_priority(Priority_Inverse);
  }
}

/**/

void Rewriter_Machine::Job_Manager::schedule_difference_correction(Node_Handle node)
{
  unsigned flags = node->get_flags() & NF_DIFFERENCES;
  node->attach(rm.nm)->clear_flags(NF_DIFFERENCES);
  bad_difference.add_state(node,flags);
  set_priority(Priority_Bad_Difference);
}

/**/

void Rewriter_Machine::Job_Manager::schedule_jobs(Node_Handle e,Node_Status new_status)
{
#ifdef DEBUG
  if (e->flagged(NF_REMOVED))
    MAF_INTERNAL_ERROR(maf.container,
                       ("Double removal of equation %p in Job_Manager::schedule_jobs()\n",e));
#endif
  stats.nr_equations++;
  stats.nr_visible++;
  e->node(rm.nm).set_status(rm.nm,new_status);
  switch (new_status)
  {
    case NS_Expanded_Equation:
      rm.add_to_schedule(e);
    case NS_Adopted_Equation:
      stats.nr_adopted++;
      break;
    case NS_Unconjugated_Equation:
      if (e->fast_is_primary())
      {
        /* We remember shortest new LHS length to stop partial_reductions
           list growing unnecessarily. In theory we should only do this
           when a truly new equation is added - here we will also get
           called on correct RHSes. But it makes no difference since if
           an RHS has been corrected the shortest_new_lhs value is already
           no more than the LHS length */
        Word_Length l = e->lhs_length(rm.nm);
        if (l < stats.shortest_new_lhs)
          stats.shortest_new_lhs = l;
      }
      unconjugated.add(e);
      set_priority(Priority_Conjugate);
      break;
    case NS_Undifferenced_Equation:
      undifferenced.add(e);
      set_priority(Priority_Difference);
      break;
    case NS_Oversized_Equation:
      oversized.add(e);
      break;
    case NS_Weak_Secondary_Equation:
      break;
    case NS_Dubious_Uncorrected_Equation:
      if (e->flagged(EQ_INTERESTING+EQ_HAS_DIFFERENCES+EQ_HAS_PRIMARY_DIFFERENCES))
        rm.remove_differences(e);
    case NS_Dubious_Unconjugated_Equation:
    case NS_Dubious_Undifferenced_Equation:
    case NS_Dubious_Secondary_Equation:
      dubious.add(e);
      set_priority(Priority_Dubious);
      break;
    case NS_Uncorrected_Equation:
      weed++;
      if (e->flagged(EQ_INTERESTING+EQ_HAS_DIFFERENCES+EQ_HAS_PRIMARY_DIFFERENCES))
        rm.remove_differences(e);
      uncorrected.add(e);
      set_priority(Priority_Pre_Correct);
      break;
    case NS_Correction_Pending_Equation:
      rhs_pending.add(e);
      set_priority(Priority_Correct);
      break;
    case NS_Removed_Equation:
      if (e->flagged(EQ_INTERESTING+EQ_HAS_DIFFERENCES+EQ_HAS_PRIMARY_DIFFERENCES))
        rm.remove_differences(e);
      stats.nr_visible--;
      weed++;
      if (!e->flagged(EQ_REDUNDANT))
      {
        removed.add(e);
        set_priority(Priority_Remove);
      }
      else
        stats.nr_equations--;
      /* I used to call equate_nodes() for EQ_REDUNDANT equations here,
         but this is no longer necessary. Such an equation is certainly
         secondary, and the equation we get from eliminating it is either
         already in the tree or in the pool. The call to equate_nodes()
         that used to be here can increase memory usage signficantly
         since the offending nodes could not be freed until the
         deduction was processed. */
      break;
    case NS_Irreducible:       // List all the remaining cases to show
    case NS_Removed_Node:      // we have thought about them
    case NS_Invalid:
    default:
      MAF_INTERNAL_ERROR(maf.container,
                         ("Equation has unexpected status %d in Job_Manager::schedule_jobs()\n",new_status));
  }
}

/**/

void Rewriter_Machine::Job_Manager::cancel_jobs(Node_Handle e)
{
  /* This method is called when a change to the Rewriter_Machine
     means that an equation has to be moved or removed */
  stats.nr_equations--;
  stats.nr_visible--;
  if (e->flagged(EQ_QUEUED))
  {
    switch (e->status())
    {
      case NS_Unconjugated_Equation:
        unconjugated.remove(e);
        break;
      case NS_Undifferenced_Equation:
        undifferenced.remove(e);
        break;
      case NS_Oversized_Equation:
        oversized.remove(e);
        break;
      case NS_Weak_Secondary_Equation:
        break;
      case NS_Dubious_Uncorrected_Equation:
      case NS_Dubious_Unconjugated_Equation:
      case NS_Dubious_Undifferenced_Equation:
      case NS_Dubious_Secondary_Equation:
        dubious.remove(e);
        break;
      case NS_Uncorrected_Equation:
        uncorrected.remove(e);
        break;
      case NS_Correction_Pending_Equation:
        rhs_pending.remove(e);
        break;

      case NS_Irreducible:       // List all the remaining cases to show
      case NS_Expanded_Equation: // we have thought about them
      case NS_Adopted_Equation:
      case NS_Removed_Equation:
      case NS_Removed_Node:
      case NS_Invalid:
      default:
        MAF_INTERNAL_ERROR(maf.container,
                           ("Equation has unexpected status %d in"
                            " Job_Manager::cancel_jobs()\n",e->status()));
    }
  }
  else
  {
    if (e->is_adopted())
      stats.nr_adopted--;
    if (e->flagged(EQ_EXPANDED) && rm.expand_ont)
    {
      e->node(rm.nm).clear_flags(EQ_EXPANDED);
      rm.expand_ont->remove(e);
    }
  }
}

/**/

void Rewriter_Machine::Job_Manager::update_machine(unsigned flags)
{
  overloaded = false;
  dm_broken = false;
  if (flags & UM_RECHECK_PARTIAL)
    set_priority(Priority_Recheck_Partial);
  if (flags & UM_CHECK_POOL)
    set_priority(Priority_Pool);

  update_inner(flags,-999);
  if (flags & UM_CHECK_OVERSIZED && rm.pool)
    set_priority(Priority_Difference);
  update_inner(flags,-999);
  overloaded = false;
  if (dm_broken)
    rm.check_differences(CD_CHECK_INTEREST|CD_FORCE_REBUILD);
}

/**/

bool Rewriter_Machine::Job_Manager::perform_job(unsigned flags)
{
  /* The priority of these methods is not cast in stone.
     However deduce() should have higher priority than conjugate().
  */
  switch (new_priority)
  {
    case Priority_Optimise:
      optimise(flags,new_priority);
      break;
    case Priority_Urgent_Deduce:
    case Priority_Deduce:
      deduce(flags,new_priority);
      break;
    case Priority_Dubious:
      check_dubious_reductions(flags,new_priority);
      break;
    case Priority_Inverse:
      check_inverses(flags,new_priority);
      break;
    case Priority_Pre_Correct:
      pre_correct(flags,new_priority);
      break;
    case Priority_Remove:
      remove(flags,new_priority);
      break;
    case Priority_Correct:
      rhs_corrections(flags,new_priority);
      break;
    case Priority_Bad_Difference:
      correct_differences(flags,new_priority);
      break;
    case Priority_Difference:
      check_differences(undifferenced,flags,new_priority);
      if (flags & UM_CHECK_OVERSIZED && oversized.length())
        check_differences(oversized,flags,new_priority);
      break;
    case Priority_Total_Overlap:
      if (total_overlaps.length())
        overlaps(&total_overlaps,flags,new_priority);
      break;
    case Priority_Overlap:
      if (pending_overlaps.length())
        overlaps(&pending_overlaps,flags,new_priority);
      break;
    case Priority_Partial:
      if (partial_reductions.length())
        check_partial_reductions(flags,new_priority);
      break;
    case Priority_Conjugate:
      conjugate(flags,new_priority);
      break;
    case Priority_Pool:
      if (flags & UM_CHECK_POOL)
        check_pool(flags,new_priority);
      break;
    case Priority_Recheck_Partial:
      if (flags & UM_RECHECK_PARTIAL)
        recheck_partial_reductions(flags,new_priority);
      break;
    default:
      return false;
  }
  // when the task has completed, reduce the priority to allow
  // the next task to run.
  new_priority--;
  return true;
}

/**/

void Rewriter_Machine::Job_Manager::optimise(unsigned flags,int priority)
{
  Node_Count done = 0;
  int gap = 3;
  while (optimisation_head)
  {
    Optimisation_Request * current = optimisation_head;
    optimisation_head = current->next;
    optimisation_count--;
    optimising = true;
    rm.optimise(current->word);
    optimising = false;
    ++done;
    update_inner(flags,priority);
    Node_Count to_do = optimisation_count;
    if (to_do && rm.status(priority,2,gap,
                           "Optimisation reductions (" FMT_NC " of " FMT_NC " to do)\n",
                           to_do,done+to_do))
      gap = 1;
    current->release();
  }
}

/**/

void Rewriter_Machine::Job_Manager::deduce(unsigned flags,int priority)
{
  Node_Count done = 0;
  Node_Count added = 0;
  int gap = 3;
  bool non_urgent = priority == Priority_Deduce;
  while (non_urgent ? pending_head : urgent_pending_head)
  {
    Node_Equation * current;
    unsigned ae_flags = 0;
    if (non_urgent)
    {
      current = pending_head;
      pending_head = current->next;
      pending_count--;
    }
    else
    {
      current = urgent_pending_head;
      urgent_pending_head = current->next;
      urgent_pending_count--;
      ae_flags |= AE_NO_UNBALANCE|AE_NO_REPEAT;
    }
    if (current->transition != PADDING_SYMBOL || rm.stats.want_secondaries)
      ae_flags |= AE_KEEP_LHS;
    Node_Reference  nr1(rm.nm,current->first);
    Node_Reference  nr2(rm.nm,current->second);
    if (!nr1->flagged(NF_REMOVED) && !nr2->flagged(NF_REMOVED))
    {
      if (current->transition != PADDING_SYMBOL)
      {
        if (rm.redundant(current->transition))
          ae_flags |= AE_DISCARDABLE;
        else if (!non_urgent)
          ae_flags |= AE_INSERT_DESIRABLE;
      }
      else
      {
        if (current->derivation.get_type() == BDT_Reverse)
          ae_flags |= AE_INSERT;
        else if (!nr1->is_final() && !nr2->is_final())
          ae_flags |= AE_HIGHLY_DESIRABLE;
      }
    }
    else
      ae_flags |= AE_DISCARDABLE;
    if (current->transition == PADDING_SYMBOL)
      ae_flags |= AE_PRE_CANCEL;
    if (current->derivation.get_type() == BDT_Known)
      ae_flags |= AE_KEEP_FAILED;
    ++done;
    for (;;)
    {
      Working_Equation we(rm.nm,nr1,nr2,current->derivation,
                          current->transition);

      int i = rm.add_equation(&we,ae_flags|AE_NO_REPEAT);
      if (i)
        ++added;
      update_inner(flags,priority);
      if (i != 1 || we.balanced != 1 || ae_flags & AE_NO_REPEAT)
        break;
    }

    Node_Count to_do = non_urgent ? pending_count : urgent_pending_count;
    if (to_do && rm.status(priority,2,gap,
                           "Processing pending deductions (" FMT_NC " of " FMT_NC " to do)"
                           " (" FMT_NC " new equations found)\n",to_do,done+to_do,
                           added))
      gap = 1;
    current->release(rm.nm);
  }
}

/**/

void Rewriter_Machine::Job_Manager::check_inverses(unsigned flags,int priority)
{
  Node_Reference nr;
  Node_Count done = 0;
  int gap = 3;

  while (bad_inverse.use(&nr,rm.nm))
  {
    Node & node = nr->node(rm.nm);
    ++done;
    if (!node.is_final())
    {
      node.clear_flags(NF_BAD_INVERSE);
      node.set_inverse(rm.nm,nr);
      update_inner(flags,priority);
      Node_Count to_do = bad_inverse.length();
      if (to_do &&
          rm.status(priority,2,gap,
                    "Calculating inverses (" FMT_NC " of " FMT_NC " to do)\n",
                    to_do,done+to_do))
        gap = 1;
    }
    node.detach(rm.nm,nr);
  }
}

/**/

void Rewriter_Machine::Job_Manager::correct_differences(unsigned flags,int priority)
{
  if (bad_difference.length())
  {
    Node_Reference nr;
    Node_Count done = 0;
    int gap = 3;
    int old_flags;

    while (bad_difference.use(&nr,rm.nm,&old_flags))
    {
      if (!rm.relabel_difference(nr,Node_Flags(old_flags),
                                 bad_difference.length()))
        dm_broken = true;
      nr->detach(rm.nm,nr);
      ++done;
      update_inner(flags,priority);
      Node_Count to_do = bad_difference.length();
      if (to_do && rm.status(priority,2,gap,
                             "Correcting differences (" FMT_NC " of " FMT_NC " to do)\n",
                             to_do,done+to_do))
        gap = 1;
    }
  }
}

/**/

void Rewriter_Machine::Job_Manager::pre_correct(unsigned flags,int priority)
{
  /* The purpose of this method is to create all the new right hand
     sides of equations with reducible RHSes. We do this before actually
     creating the equations because we first deal with the overlaps
     from eliminated equations. But by creating the RHSes we will need later
     we have the best chance of avoiding a bad decision about pooling a
     replaced equation. This is only relevant to non-shortlex systems,
     because we would never pool such an equation anyway in the shortlex case.

     If we find the new RHS is no longer than the old one we do the correction
     at once.
  */
  if (!rm.pd.is_short && uncorrected.length())
  {
    Node_Count done = 0;
    Node_Reference e;
    int gap = 3;
    Multi_Node_List mnl(rm.nm,rm.height());

    while (uncorrected.use(&e))
    {
      Equation_Word ew(rm.nm,e->fast_reduced_node(rm.nm));
      bool failed = false;
      ew.reduce(0,&failed);
      if (!maf.options.no_early_repair &&
          (ew.fast_length() <= e->reduced_length(rm.nm) ||
           ew.fast_length() <= e->lhs_length(rm.nm)))
      {
        rm.improve_rhs(e);
        update_inner(flags,priority);
      }
      else
      {
        if (!failed && ew.fast_length() < rm.height() &&
            (!rm.pd.is_coset_system || ew.first_letter() < rm.pd.coset_symbol))
          ew.realise_state();
        mnl.add_state(e);
      }
      e->detach(rm.nm,e);
      ++done;
      Node_Count to_do = uncorrected.length();
      if (to_do && rm.status(priority,2,gap,
                             "Pre-creating new RHSes (" FMT_NC " of " FMT_NC " to do)\n",
                             to_do,done + to_do))
        gap = 1;
    }
    Node_List temp_list;
    mnl.collapse(&temp_list);
    done = 0;

    while (temp_list.use(&e,rm.nm))
    {
      if (e->status() == NS_Uncorrected_Equation)
      {
        cancel_jobs(e);
        schedule_jobs(e,NS_Correction_Pending_Equation);
      }
      done++;
      Node_Count to_do = temp_list.length();
      if (rm.status(priority,2,gap,
                    "Rebuilding bad RHS list (" FMT_NC " of " FMT_NC " to do)\n",
                    to_do,done + to_do))
        gap = 1;
    }
  }
}

/**/

void Rewriter_Machine::Job_Manager::weed_jobs()
{
  Node_Count to_do = partial_reductions.length();
  if (to_do)
  {
    int g;
    Node_List keep;
    Node_Count done = 0;
    int gap = 3;
    Node_Reference s;
    while (partial_reductions.use(&s,&g))
    {
      if ((!s->is_final() || !s->parent(rm.nm)->is_final()) &&
          !rm.redundant(Ordinal(g)))
        keep.add_state(s,g);
      else
        s->detach(rm.nm,s);
      if (!(char) ++done &&
          rm.critical_status(2,gap,"Weeding partial reduction list (" FMT_NC
                                   " of " FMT_NC ")\n",done,to_do))
        gap = 1;
    }
    partial_reductions.merge(&keep);
  }
  total_overlaps.weed();
  pending_overlaps.weed();
  weed = 0;
}

/**/

void Rewriter_Machine::Job_Manager::remove(unsigned flags,int priority)
{
  /* It is not clear to me whether processing removed equations before or after
     ones with reducible RHS is more useful. But on the basis that any new
     equations created from  removed kill off assumed L0 words, whereas most
     from uncorrected merely tweak reductions for words we know are reducible
     then perhaps removed is more important. However, improvement
     of an RHS can easily result in the equation becoming unbalanced, so that
     the prefix becomes reducible. */
  Node_Reference e;
  Node_Count done = 0;
  Node_Count replaced = 0;
  int gap = 3;
  while (removed.use(&e))
  {
    stats.nr_equations--;
    Working_Equation we(rm.nm,e,maf.options.log_flags & LOG_DERIVATIONS ?
                        e->raw_reduced_node(rm.nm) : e->fast_reduced_node(rm.nm),
                        Derivation(BDT_Known,e));
    unsigned ae_flags = AE_KEEP_FAILED|AE_NO_UNBALANCE|AE_NO_REPEAT;
    if (rm.add_equation(&we,ae_flags) > 0)
      replaced++;
    update_inner(flags,priority);
    e->detach(rm.nm,e);
    ++done;
    Node_Count to_do = removed.length();
    if (to_do && rm.status(priority,2,gap,
                           "Removing dead equations (" FMT_NC " of " FMT_NC " to do)"
                           " (" FMT_NC " replaced)\n",to_do,done+to_do,replaced))
      gap = 1;
  }
  if (done > 100)
    weed_jobs();
}

/**/

void Rewriter_Machine::Job_Manager::rhs_corrections(unsigned flags,int priority)
{
  Node_Count done = 0;
  Node_Reference e;
  int gap = 3;
  Equation_Queue & queue = rm.pd.is_short ? uncorrected : rhs_pending;

  while (queue.use(&e))
  {
    rm.improve_rhs(e);
    update_inner(flags,priority);
    e->detach(rm.nm,e);
    ++done;
    Node_Count to_do = queue.length();
    if (to_do && rm.status(priority,2,gap,
                           "Correcting RHSes (" FMT_NC " of " FMT_NC " to do)\n",
                           to_do,done + to_do))
      gap = 1;
  }
}

/**/

void Rewriter_Machine::Job_Manager::urgent_deductions()
{
  update_inner(0,Priority_Dubious);
}

/**/

void Rewriter_Machine::Job_Manager::check_dubious_reductions(unsigned flags,int priority)
{
  Node_Count done = 0;
  Node_Reference e;
  int gap = 3;

  while (dubious.use(&e))
  {
    rm.improve_dubious(e);
    update_inner(flags,priority);
    e->detach(rm.nm,e);
    ++done;
    Node_Count to_do = dubious.length();
    if (to_do && rm.status(priority,
                           2,gap,"Checking secondaries (" FMT_NC " of " FMT_NC " to do)\n",
                           to_do,done + to_do))
      gap = 1;
  }
}

/**/

void Rewriter_Machine::Job_Manager::overlaps(Overlap_Queue * queue,unsigned flags,int priority)
{
  Node_Count done = 0;
  Node_Count added = 0;
  int gap = 3;
  Simple_Equation se1(rm.alphabet());
  Simple_Equation se2(rm.alphabet());
  Node_Overlap * current;
  bool is_total = queue == &total_overlaps;
  const char * message = queue == &total_overlaps ?
    "Processing total overlaps (" FMT_NC " of " FMT_NC " to do) (" FMT_NC " new equations found)\n" :
    "Processing special overlaps (" FMT_NC " of " FMT_NC " to do) (" FMT_NC " new equations found)\n" ;
  unsigned ae_flags = AE_INSERT_DESIRABLE;
  int going = 0;
  unsigned was_to_do = 0;

  while ((current = queue->use())!=0)
  {
    ++done;
    if (!maf.aborting)
    {
      Node_Reference e1(rm.nm,current->first);
      if (e1->is_valid())
      {
        Node_Reference e2(rm.nm,current->second);
        if (e2->is_valid() &&
            (is_total || rm.overlap_interesting(e1,e2,current->offset)))
        {
          se1.read_equation(rm.nm,e1);
          se2.read_equation(rm.nm,e2);
          if (rm.consider(e1,e2,current->offset,se1,se2,ae_flags) > 0)
            added++;
        }
      }
    }
    current->release(rm.nm);
    update_inner(flags,priority);
    unsigned to_do = queue->length();
    if (to_do && rm.status(priority,2,gap,message,to_do,done+to_do,added))
    {
      gap = 1;
      if (going++ > 5 && rm.stats.special_limit > rm.stats.auto_expand_limit && to_do > was_to_do)
        rm.stats.special_limit--;
      was_to_do = to_do;
    }
  }
}

/**/

void Rewriter_Machine::Job_Manager::conjugate(unsigned flags,int priority)
{
  Node_Count done = 0;
  Node_Reference e;
  int gap = 3;
  while (unconjugated.use(&e))
  {
    Total_Length tl = e->total_length(rm.nm);
    rm.conjugate(e);
    if (e->is_valid())
    {
      cancel_jobs(e);
      schedule_jobs(e,rm.choose_status(e,maf.options.strategy & MSF_CONJUGATE_FIRST ?
                                       NS_Undifferenced_Equation :
                                       NS_Adopted_Equation));
    }
    e->detach(rm.nm,e);

    ++done;
    update_inner(flags,priority);
    Node_Count to_do = unconjugated.length();
    if (to_do && rm.status(priority,2,gap,
                           "Conjugating new equations (" FMT_NC " of " FMT_NC " to do). Length %d\n",
                           to_do,done + to_do,tl))
      gap = 1;
  }
}

/**/

void Rewriter_Machine::Job_Manager::check_differences(Equation_Queue & queue,
                                                      unsigned flags,int priority)
{
  Node_Count done = 0;
  Node_Reference e;
  int gap = 3;

  while (queue.use(&e))
  {
    Node_Status new_status = rm.learn_differences(e);
    if (e->is_valid())
    {
      cancel_jobs(e);
      schedule_jobs(e,new_status);
    }
    e->detach(rm.nm,e);
    ++done;
    update_inner(flags, priority);
    Node_Count to_do = undifferenced.length();
    if (to_do && rm.status(priority,2,gap,
                           "Checking word-differences (" FMT_NC " of " FMT_NC " to do)\n",
                           to_do,done+to_do))
      gap = 1;
  }
}

/**/

void Rewriter_Machine::Job_Manager::divert_differences()
{
  Node_Reference e;

  while (undifferenced.use(&e))
  {
    cancel_jobs(e);
    schedule_jobs(e,NS_Unconjugated_Equation);
    e->detach(rm.nm,e);
  }
}

/**/

void Rewriter_Machine::Job_Manager::check_pool(unsigned flags,int priority)
{
  Word_Length max_height = rm.height();
  Ordinal_Word lhs_word(rm.alphabet(),max_height*2);
  Ordinal_Word rhs_word(rm.alphabet(),max_height*2);
  Element_Count nr_adopted;
  Element_Count nr_improved;
  Element_Count nr_entries = rm.pool ? rm.pool->count() : 0;
  MAF & maf = this->maf;
  Node_List pruned;
  /* AE_NO_UNBALANCE might seem to be a good idea here, but does not seem
     beneficial */
  unsigned inner_flags = rm.stats.want_secondaries ? AE_KEEP_LHS|AE_KEEP_FAILED :
                                                     AE_KEEP_FAILED;
  int gap = 1;

  if (flags & UM_PRUNE && nr_entries > 1000)
  {
    pruning = true;
    rm.prune();
    pruning = false;
    if (removed.length() > 0)
    {
      Node_Reference e;
      Node_Count done = 0;
      inner_flags |= AE_TIGHT;
      Multi_Node_List mnl(rm.nm,rm.height());
      Node_List pruned;
      while (removed.use(&e))
      {
        mnl.add_state(e);
        ++done;
        Node_Count to_do = removed.length();
        rm.status(priority,2,1,"Sorting long equation (" FMT_NC " of " FMT_NC " to do)\n",
                  to_do,done+to_do);
      }
      mnl.collapse(&pruned);
      done = 0;
      while (pruned.use(&e,rm.nm))
      {
        stats.nr_equations--;
        Working_Equation we(rm.nm,e,e->fast_reduced_node(rm.nm),Derivation(BDT_Known,e));
        rm.add_equation(&we,AE_TIGHT|AE_POOL|AE_KEEP_LHS|AE_NO_REPEAT);
        e->detach(rm.nm,e);
        ++done;
        Node_Count to_do = pruned.length();
        rm.status(priority,2,1,
                  "Temporarily moving long equations to pool (" FMT_NC " of " FMT_NC " to do)\n",
                  to_do,done+to_do);
      }
      nr_entries = rm.pool->count();
    }
  }

  unsigned long start;
  unsigned long time_out = 0;
  bool seen_useful = false;
  int pass = 0;
  inner_flags |= AE_NO_REPEAT;
  do
  {
    pass++;
    start = rm.stats.status_count;
    nr_adopted = 0;
    nr_improved = 0;
    Equation_DB * old_pool = rm.pool;
    rm.reset_pool();
    for (Element_ID id = 0; id < nr_entries;id++)
    {
      size_t size = old_pool->get_key_size(id);
      if (size)
      {
        old_pool->get_lhs(&lhs_word,id);
        old_pool->get_rhs(&rhs_word,id);
        /* Our equation will either turn into an ordinary equation
           or go back into the pool, possibly after being further
           reduced */
        old_pool->remove_entry(id);
        stats.nr_equations--;
        for (;;)
        {
          Working_Equation we(rm.nm,lhs_word,rhs_word,Derivation(id));
          int i = rm.add_equation(&we,inner_flags);
          if (i == 1)
          {
            nr_adopted++;
            seen_useful = true;
          }
          else if (i == 2)
          {
            if (!we.failed && we.total_length() < rm.height()*2 &&
                we.lhs_word().length() >= we.rhs_word().length())
              seen_useful = true;
            if (we.changed)
              nr_improved++;
          }
          update_inner(flags,priority);
          if (i != 1 || we.balanced != 1)
            break;
        }
        Node_Count to_do = nr_entries - id -1;
        if (rm.status(priority,2,gap,
                      "Examining pool (" FMT_ID " of " FMT_ID ") ("
                      FMT_ID " adopted)\n",id+1,nr_entries,nr_adopted))
        {
          if (seen_useful)
            time_out = 0;
          else if (!(inner_flags & AE_DISCARDABLE+AE_TIGHT))
            time_out++;
          if (time_out >= 5 && to_do > stats.nr_visible*10)
          {
            stats.nr_equations -= to_do;
            id = nr_entries - 1;
            if (rm.pool)
              stats.nr_equations -= rm.pool->count();
            rm.restart();
          }
          seen_useful = false;
          gap = 1;
        }
      }
    }
    if (old_pool)
      delete old_pool;
    Element_Count new_entries = rm.pool ? rm.pool->count() : 0;
    if (nr_adopted+nr_improved || new_entries != nr_entries)
    {
      nr_entries = new_entries;
      if (maf.options.log_level > 0)
        rm.status(priority,2,rm.stats.status_count==start ? 1 : 0,
                  "Pool size now " FMT_ID ". " FMT_ID " equations adopted,"
                  " " FMT_ID " equations altered\n",
                  nr_entries,nr_adopted,nr_improved);
    }
    if (inner_flags & AE_TIGHT)
    {
      inner_flags &= ~AE_TIGHT;
      nr_adopted = rm.stats.status_count - start + 1;
    }
  }
  while (nr_adopted &&
         (unsigned long) nr_adopted >= (rm.stats.status_count - start+1)*pass);

  if (rm.pool && rm.stats.pool_limit == UNLIMITED)
  {
    /* In this case the pool only contains equations that we can't deal with
       Let's throw it away and start KB again */
    stats.nr_equations -= rm.pool->count();
    rm.restart();
  }
}

/**/

void Rewriter_Machine::Job_Manager::check_partial_reductions(unsigned flags,int priority)
{
  /* This method checks whether a node w which has a reduction at wg is itself
     reducible by considering overlap wgG, and creates any resulting equations
     In the case of recursive orderings it is very likely that the initial
     reduction will be wrong, and that we need to find some more equations.
     We also need to consider the trailing subwords which don't have full
     length states.

     s represents the word w=xxxab
     The reduction occurs at s->transition(g), s->transition(g)->length(rm.nm)
     is the number of symbols in the first (partial) reduction (say abg).

     Therefore s->length(rm.nm)+1-s->transition(g)->length(rm.nm) is the offset up
     to which we might need to consider subwords.
     We may be able to do better by finding the length of the state at
     xxab. If this is longer we can assume that all its trailing subwords
     have already been examined.
  */

  int g_int;
  Node_Reference s;
  int gap = 3;
  Node_Count added = 0;
  Node_Count done = 0;
  if (rm.stats.want_overlaps && partial_reductions.length())
  {
    Ordinal_Word lhs_word(rm.alphabet(),rm.height()+1);
    while (partial_reductions.length())
    {
      partial_reductions.sort();
      Node_List chunk;
      chunk.merge(&partial_reductions);

      while (chunk.use(&s,rm.nm,&g_int))
      {
        Ordinal g = Ordinal(g_int);

        done++;
        Word_Length sl = s->length(rm.nm);

        if (sl < rm.height() && !rm.difficult(g) && !rm.redundant(g) &&
            s->child(rm.nm,s,g).is_null() && !s->parent(rm.nm)->is_final())
        {
          s->read_word(&lhs_word,rm.nm);
          lhs_word.append(g);
          lhs_word.append(maf.inverse(g));
          Word_Length lhs_length = sl+2;

          Word_Length max_offset = s->is_final() ? 1 :
                                   sl + 1 - s->transition(rm.nm,g)->length(rm.nm);
          if (max_offset > 1)
            max_offset = sl - rm.nm.get_state(lhs_word,sl,1)->length(rm.nm);
          int failed = 0;
          Word_Length i;
          for (i = 0; i < max_offset;i++)
          {
            Working_Equation we(rm.nm,Subword(lhs_word,i));
            we.derivation.add_right_free_reduction(lhs_length-i,we.lhs_word().last_letter());
            we.rhs_word().set_length(sl-i);
            int j = rm.add_equation(&we,AE_TIGHT|AE_DISCARDABLE|AE_NO_REPEAT);
            if (j != 0)
            {
              added++;
              update_inner(flags,priority);
              if (j == 1 && we.balanced == 1)
                i--;
            }
            else
            {
              update_inner(flags,priority); // there may be optimisation to be done
              if (we.failed)
              {
                rm.set_difficult(g);
                failed = true;
              }
              else if (maf.options.partial_reductions & 8 &&
                       we.lhs_word().length())
                failed = 2;
              break;
            }
          }
          if (failed && i+1 < max_offset)
          {
            for (i = max_offset; i-- > 0;)
            {
              Working_Equation we(rm.nm,Subword(lhs_word,i));
              we.derivation.add_right_free_reduction(lhs_length-i,we.lhs_word().last_letter());
              we.rhs_word().set_length(sl - i);
              int j = rm.add_equation(&we,AE_TIGHT|AE_DISCARDABLE|AE_NO_REPEAT);
              if (j != 0) /* AE_INSERT here can cause endless growth */
              {
                added++;
                update_inner(flags,priority);
                if (j==1 && we.balanced==1)
                  i++;
              }
              if (j==2)
                break;
              if (we.failed)
              {
                rm.set_difficult(g);
                break;
              }
            }
          }
        }

        Node_Count to_do = partial_reductions.length() + chunk.length();
        if (to_do &&
            rm.status(priority,2,gap,"Checking partial reductions (" FMT_NC " of " FMT_NC " to do)."
                            " " FMT_NC " equations added. Length %d\n",to_do,
                      done+to_do,added,sl))
          gap = 1;
        s->detach(rm.nm,s);
      }
    }
  }
  else if (rm.stats.want_secondaries)
  {
    /* Since stats.want_overlaps is false we have either reached the stage where
       the Rewriter_Machine is confluent or this is a coset system and we have
       proved that the subgroup has finite index */

    while (partial_reductions.use(&s,&g_int))
    {
      Ordinal g = Ordinal(g_int);
      done++;
      Equation_Word lhs_word(rm.nm,s,g);
      Working_Equation we(rm.nm,lhs_word);
      Node_Count to_do = partial_reductions.length();
      if (rm.add_equation(&we,AE_KEEP_LHS+AE_SECONDARY+AE_NO_BALANCE+AE_COPY_LHS+AE_NO_REPEAT))
      {
        added++;
        update_inner(flags,priority);
      }
      if (to_do && rm.status(priority,2,gap,
                             "Checking partial reductions (" FMT_NC " of " FMT_NC " to do)."
                             " " FMT_NC " equations added\n",
                             to_do,done+to_do,added))
        gap = 1;
      s->detach(rm.nm,s);
    }
  }
}

/**/

void Rewriter_Machine::Job_Manager::recheck_partial_reductions(unsigned flags,int priority)
{
  /* This method was originally called quite often, in the hope that it would
     discover more word-differences, and also in the hope it would discover
     new primaries.
     It is now only called for the former purpose, and usually only in the
     case where we have found a confluent rewriting system. Where a group is
     large and finite there are often very many word-differences, and this
     method may find most (or even all) of them, so that the multiplier builds
     first time if we are lucky.
     It is also called if explore_acceptor() ever gets called, but this would
     only happen if the code that builds automatic structure is running out
     of ideas as to how to find any more word-differences.
   */
  if (rm.need_partial_reduction_checks())
  {
    Node_Reference s;
    Node_List equation_list;
    MAF &maf = this->maf;

    while (stats.shortest_new_lhs != WHOLE_WORD)
    {
      Node_Iterator ni(rm.nm);
      Multi_Node_List mnl(rm.nm,rm.height());
      Word_Length max_length = rm.pd.is_coset_finite ? WHOLE_WORD :
                               rm.stats.lhs_limit;
      Word_Length interest = rm.interest_limit()/2+1;
      if (interest > max_length)
        max_length = interest;
      while (ni.scan(&s))
      {
        Word_Length test_length = s->length(rm.nm) + 1;
        if (!s->is_final() && test_length >= stats.shortest_new_lhs &&
            test_length <= max_length)
        {
          Ordinal bad_ordinal = maf.inverse(s->last_letter());
          Ordinal child_start,child_end;
          rm.nm.valid_children(&child_start,&child_end,s->last_letter());
          for (Ordinal g = child_start;g < child_end;g++)
            if (g != bad_ordinal)
            {
              Node_Reference ns = s->transition(rm.nm,g);
              if (ns->is_final() && ns->parent(rm.nm) != s &&
                  test_length + ns->reduced_length(rm.nm) - ns->length(rm.nm) >= stats.shortest_new_lhs)
              {
                s->attach(rm.nm);
                mnl.add_state(s,g);
                break;
              }
            }
        }
      }

      Word_Length try_length = stats.shortest_new_lhs;
      stats.shortest_new_lhs = WHOLE_WORD;
      mnl.collapse(&equation_list);
      Node_Count added = 0;
      Node_Count done = 0;
      bool aborted = false;
      rm.stats.last_difference = rm.stats.status_count;
      int time_out = 20;
      int g_int;
      while (equation_list.use(&s,rm.nm,&g_int))
      {
        ++done;
        if (!s->is_final() && !aborted)
        {
          Ordinal bad_ordinal = maf.inverse(s->last_letter());
          Ordinal child_start,child_end;
          Word_Length test_length = s->irreducible_length() + 1;
          if (stats.shortest_new_lhs < try_length)
            try_length = stats.shortest_new_lhs;
          rm.nm.valid_children(&child_start,&child_end,s->last_letter());
          for (Ordinal g = Ordinal(g_int);g < child_end;g++)
            if (g != bad_ordinal)
            {
              Node_Reference ns = s->transition(rm.nm,g);
              if (ns->is_final() && ns->parent(rm.nm) != s &&
                  test_length + ns->reduced_length(rm.nm) - ns->lhs_length(rm.nm) >= try_length)
              {
                Equation_Word lhs_word(rm.nm,s,g);
                Working_Equation we(rm.nm,lhs_word);
                if (rm.status(priority,2,1,
                              "Partial reduction checks (" FMT_NC " of " FMT_NC " to do)."
                              " " FMT_NC " equations added. Time %d\n",
                              equation_list.length(),
                              done+equation_list.length(),added,time_out))
                  if (rm.is_dm_changing())
                    time_out = 20;
                  else if (--time_out <= 0)
                  {
                    aborted = true;
                    break;
                  }
                if (rm.add_equation(&we,AE_KEEP_LHS+AE_SECONDARY+AE_INSERT+AE_NO_BALANCE+AE_COPY_LHS+AE_NO_REPEAT))
                {
                  added++;
                  update_inner(flags,priority);
                  if (s->is_final())
                    break;
                }
              }
            }
        }
        s->detach(rm.nm,s);
      }
      if (aborted)
      {
        if (stats.shortest_new_lhs > try_length)
          stats.shortest_new_lhs = try_length;
        break;
      }
      if (flags & UM_NO_REPEAT_PARTIAL)
        break;
    }
  }
}

/**/

bool Rewriter_Machine::Job_Manager::work_pending() const
{
  return new_priority > Priority_Pool;
}
