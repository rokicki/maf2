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


// $Log: maf_em.cpp $
// Revision 1.9  2010/06/10 13:57:37Z  Alun
// All tabs removed again
// Revision 1.8  2010/06/08 05:33:22Z  Alun
// Various changes to management of secondary equations to deal with
// problems with correcting word-difference machine.
// Changes required by new style Node_Manager interface
// Revision 1.7  2009/11/10 09:21:00Z  Alun
// normalised_limit member renamed to visible_limit
// Changes to improve behaviour of large finite index coset systems with generator names
// Revision 1.6  2009/09/16 07:38:28Z  Alun
// Additional source code changes needed to get clean compilation on latest GNU
// Revision 1.5  2009/09/12 18:47:49Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2008/12/26 21:35:18Z  Alun
// Changes to management of coset equations
// Revision 1.5  2008/11/12 02:09:50Z  Alun_Williams
// Remember the AE_CORRECTION flag for a queued LHS - this speeds up building
// the multiplier for certain coset systems
// Revision 1.4  2008/10/23 17:46:22Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2008/10/01 01:27:12Z  Alun
// Diagnostics removed
// Revision 1.2  2008/09/30 09:25:12Z  Alun
// Final version built using Indexer.
//

/*
Equation_Manager is used to manage insertion of equations into the tree.
For rationale see maf_em.h

Insertion of an equation is atomic, as far as users of this class are
concerned. However, when an equation is inserted it may need to be
processed several times to extract all its consequences.

The general plan of campaign is to have one "current" equation which
is what we are working on now. Any other equations are pushed onto a
queue.
*/

#include "awcc.h"
#include "maf_rm.h"
#include "maf_em.h"
#include "maf_we.h"
#include "mafnode.h"
#include "maf_nm.h"
#include "container.h"

Managed_Equation::Managed_Equation(Node_Manager &nm,
                                   const Word & lhs,const Word & rhs,
                                   const Derivation & d,unsigned flags_) :
  owner(true),
  next(0),
  we(new Working_Equation(nm,lhs,rhs,d)),
  flags(flags_)
{
}

/**/

Managed_Equation::~Managed_Equation()
{
  if (owner && we)
    delete we;
}

/**/

Managed_Equation & Managed_Equation::operator=(Managed_Equation & other)
{
  if (owner)
    delete we;
  we = other.we;
  owner = other.owner;
  other.owner = false;
  flags = other.flags;
  next = other.next;
  return *this;
}

/**/

Equation_Manager::Equation_Manager(Rewriter_Machine & rm_) :
  rm(rm_),
  maf(rm_.maf),
  nm(rm_),
  pd(rm_.pd),
  tail(&head),
  container(rm_.maf.container),
  head(0),
  current(*this),
  stats(rm_.status()),
  some_discarded(false),
  ae_flags_or(0)
{
  if (!maf.options.unbalance || pd.is_coset_system)
    ae_flags_or |= AE_NO_UNBALANCE;
}

/**/

Equation_Manager::~Equation_Manager()
{
  while (head)
  {
    Managed_Equation * item = head;
    head = head->next;
    delete item;
  }
}

/**/

bool Equation_Manager::use()
{
  if (head)
  {
    current = *head;
    delete head;
    head = current.next;
    if (!head)
      tail = &head;
    return true;
  }
  return false;
}

/**/

int Equation_Manager::learn(Working_Equation * we,unsigned flags,bool * discarded)
{
  int retcode = 0;
  Managed_Equation me(we,flags);
  current = me;
  some_discarded = false;
  int local_retcode = process();
  if (local_retcode == 1 || retcode == 0)
    retcode = local_retcode;
  while (use())
  {
    local_retcode = process();
    if (local_retcode == 1 || retcode == 0)
      retcode = local_retcode;
  }
  if (some_discarded)
    *discarded = true;
  return retcode;
}

/**/

enum Accepted_Reason
{
  AR_Rejected,
  AR_Axiom,
  AR_Small,
  AR_Short_Side,
  AR_Forced,
  AR_Better_RHS_For_Existing_LHS,
  AR_Beneficial,
  AR_Highly_Desirable,
  AR_Desirable,
  AR_Interesting,
  AR_Collapse,
  AR_Provoke_Collapse,
  AR_Small_G,
  AR_Small_H,
  AR_Special
};

enum EM_Action
{
  EMA_Unknown = -1,
  EMA_Do_Nothing,
  EMA_General_Insert,
  /* An insert is weak if inserting it does not actually change the
     reduction for the LHS, and strong if it does.
     An insert is General if we have decided we want it without working
     out whether it is weak or strong. */
  EMA_Weak_Insert,
  EMA_Strong_Insert,
  EMA_Reduce_Again,
  EMA_Save_LHS_And_Reduce_Again
};

int Equation_Manager::process()
{
  /* On entry we contains an equation that has been deduced.
     We normalise the equation (i.e make sure the prefix of the lhs and the
     rhs are both currently irreducible according to Rewriter_Machine: a
     Diff_Reduce instance based on the Difference_Tracker might have a
     different opinion).

     We then examine the equation and decide whether to insert it into the
     tree, pool it or throw it away. In the case where ths LHS was not
     fully reduced we may also need to start again with a fully reduced LHS.

     Then, if necessary, we insert the equation into the tree or pool.
     We may also be able to deduce some more equations, which we add to
     the pending lists.

     The return value is 0 if the equation is trivial, or already known,
     or it is too big and we are allowed to omit it.
     1 if a new equation is created in the tree,
     or 2 if an equation is added or modified in the pool

     This function is not recursive, because it puts any further new equations
     that are discovered in a queue. It may iterate a few times under
     various circumstances.
  */

  int retcode = 0;

  for (;;)
  {
    if (!(flags & AE_COPY_LHS) && !stats.want_secondaries && maf.options.secondary < 2)
      flags &= ~AE_KEEP_LHS;
    /* We are usually only going to create one equation in this method.
       However when improving an RHS or creating a secondary equation
       we may find an equation between L0 words. When that happens we
       may need to construct more than one equation. We also conjugate
       certain pool equations.
    */

    was_total = we->total_length();
    if (!we->prepare_for_insert(flags))
      break;
    Equation_Properties ep;
    we->read_properties(&ep);
    Equation_Word & lhs = we->lhs_word();
    Equation_Word & rhs = we->rhs_word();

    if (maf.options.swap_bad &&
        ep.l < ep.r+1 && !(flags & AE_NO_UNBALANCE+AE_NO_SWAP) && !we->failed)
    {
      Equation_Word &spare = we->spare_word();
      spare = rhs;
      Word_Length reversible = 0;
      Ordinal v;
      State old_rhs = spare.state_word(false);
      while (old_rhs && !old_rhs->is_final() && reversible < ep.l &&
             (v = maf.inverse(lhs.value(ep.l-reversible-1)))!=INVALID_SYMBOL)
      {
        spare.append(v);
        if (spare.get_language() != language_L0)
        {
          spare.set_length(ep.r+reversible);
          break;
        }
        else
        {
          reversible++;
          old_rhs = spare.state_word(false);
        }
      }
      if (reversible && spare.compare(lhs) > 0)
      {
        Managed_Equation * me1 = new Managed_Equation(nm,spare,
                                                      Subword(lhs,0,ep.l-reversible),
                                                      Derivation(BDT_Unspecified),
                                                      AE_NO_UNBALANCE|AE_DISCARDABLE);
        queue(me1);
        Managed_Equation * me2 = new Managed_Equation(we,flags|AE_NO_SWAP,owner);
        queue(me2);
        owner = false;
        return 0;
      }
    }

    String reason = 0;
    if (!maf.is_valid_equation(&reason,lhs,rhs))
    {
      we->print(container,container.get_log_stream());
      MAF_INTERNAL_ERROR(container,("%s",reason.string()));
      break;
    }

    flags |= ae_flags_or;
    /* changed is set true if the the equation was modified by
       prepare_for_insert. In that case we must clear the AE_COPY_LHS flag,
       if set, because we cannot be sure the RHS is the current reduction of
       the LHS */
    if (we->changed)
      flags &= ~AE_COPY_LHS;

    action_needed = EMA_Unknown;

    if (flags & AE_SAVE_AXIOM)
      maf.add_polished_axiom(nm,lhs,rhs);
    int wanted = is_insertable(ep);

    if (maf.options.max_stored_length[0])
      if (ep.e.is_null() &&
          (ep.l > maf.options.max_stored_length[0] ||
           ep.r > maf.options.max_stored_length[1]))
      {
        rm.status().some_ignored = true;
        break;
      }

    if (!we->failed)
    {
      /* Even if AE_DISCARDABLE is set, keep the equation if it is small enough */

      if (flags & AE_DISCARDABLE && ep.total <=
          (maf.options.strategy & MSF_AGGRESSIVE_DISCARD ?
           stats.pool_limit : stats.discard_limit))
        flags &= ~AE_DISCARDABLE;

      if (!wanted && lhs.get_language() != language_L0 &&
          (!(flags & AE_TIGHT+AE_POOL) || flags & AE_DISCARDABLE))
      {
        /* Before throwing a way a secondary equation, find the corresponding
           primary and think about that */
        flags &= ~(AE_KEEP_LHS|AE_SECONDARY|AE_COPY_LHS);
        continue;
      }
    }

    if (wanted)
    {
      moderate(&ep);
      choose_action(ep);
      if (action_needed >= EMA_Reduce_Again)
      {
        unsigned new_flags = AE_KEEP_LHS|AE_COPY_LHS|AE_DISCARDABLE;
        if (action_needed == EMA_Save_LHS_And_Reduce_Again)
        {
          if (flags & AE_MUST_SIMPLIFY)
          {
            /* This deals with the case where a repair cannot be honoured
               and e.p==0 */
            new_flags |= flags & (AE_MUST_SIMPLIFY|AE_INSERT);
            flags &= ~AE_MUST_SIMPLIFY;
          }
          queue(lhs,lhs,Derivation(BDT_Trivial),new_flags | (flags & AE_CORRECTION));
        }
        flags &= ~(AE_KEEP_LHS|AE_COPY_LHS|AE_SECONDARY|AE_INSERT);
        continue;
      }

      if (action_needed != EMA_Do_Nothing)
      {
        if (action_needed == EMA_Strong_Insert)
          flags |= AE_MUST_SIMPLIFY|AE_STRONG_INSERT;
        rm.add_to_tree(we,flags,was_total);
        return 1;
      }
      break;
    }
    else if (!(flags & AE_DISCARDABLE) &&
             (flags & AE_TIGHT || we->failed || !ep.old_e->is_final()))
    {
      Element_ID id;
      if (rm.add_to_pool(&id,we,flags))
      {
        retcode = 2;
        /* If ep.l <= ep.r we might get a equation that we would like to
           put in the tree when we form the right conjugate.
           Presumably the test that this is not a coset system is
           because coset equations will tend still to be bad, but on the
           face of things it would be better just to exclude coset equations,
           and only then if the g_level flag is set. */

        if (ep.l <= ep.r && !we->failed && !pd.is_coset_system &&
            !head && ep.r < MAX_WORD && !(flags & AE_POOL))
        {
          Ordinal ivalue = maf.inverse(lhs.last_letter());
          if (ivalue != INVALID_SYMBOL)
          {
            Equation_Word &spare = we->spare_word();
            spare = rhs;
            spare.append(ivalue);
            Managed_Equation * me1;
            if (Subword(lhs,0,ep.l-1).compare(spare) < 0)
              me1 = new Managed_Equation(nm,spare,Subword(lhs,0,ep.l-1),
                                         Derivation(id,BDT_Pool_Conjugate),
                                         AE_KEEP_LHS|AE_DISCARDABLE|AE_RIGHT_CONJUGATE);
            else
              me1 = new Managed_Equation(nm,Subword(lhs,0,ep.l-1),spare,
                                         Derivation(id,BDT_Pool_Conjugate),
                                         AE_DISCARDABLE);
            queue(me1);
          }
        }
      }
      break;
    }
    else
    {
      some_discarded = true;
      break; /*Equation is being discarded because long and AE_DISCARDABLE set*/
    }
  }
  return retcode;
}

/**/

int Equation_Manager::is_insertable(const Equation_Properties &ep)
{
  /* See if we want to put this equation in the tree */
  if (we->failed)
    return AR_Rejected;
  if (ep.eq_flags & EQ_AXIOM)
    return AR_Axiom;
  if (flags & AE_POOL)
    return AR_Rejected;
  if (ep.total <= stats.visible_limit)
    return AR_Small;
  if (ep.r <= maf.options.no_pool_below)
    return AR_Short_Side;
  if (ep.l < maf.options.no_pool_below)
  {
    flags |= AE_NO_UNBALANCE;
    return AR_Short_Side;
  }
  if (flags & AE_INSERT)
    return AR_Forced;

  Word_Length height = nm.g_height();
  bool desirable = ep.r <= ep.l && ep.r < height && ep.l <= height;

  if (!ep.e.is_null() && (desirable || ep.e->is_final() &&
                          ep.r <= ep.e->reduced_length(nm)))
    return AR_Better_RHS_For_Existing_LHS;

  if (!(flags & AE_CORRECTION) && desirable)
  {
    if (flags & AE_HIGHLY_DESIRABLE)
      return AR_Highly_Desirable;
    if (!ep.prefix.is_null())
    {
      if (ep.r < ep.l &&
          (!ep.old_e->is_final() && ep.total < stats.discard_limit || !pd.is_short))
        return AR_Beneficial;
      if (flags & AE_INSERT_DESIRABLE &&
          was_total < ep.total && was_total < stats.discard_limit)
        return AR_Desirable;
    }
  }

  if (!(flags & AE_TIGHT))
  {
    action_needed = rm.is_interesting_equation(*we,flags) ? EMA_General_Insert : EMA_Do_Nothing;
    if (action_needed)
      return AR_Interesting;
  }

  Equation_Word & lhs = we->lhs_word();
  const Equation_Word & rhs = we->rhs_word();
  if (maf.options.collapse && !(flags & AE_CORRECTION) && !pd.is_short && !ep.old_e->is_final())
  {
    /* we don't think we want the equation. But if r<=l we had better
       take it if it kills something in the tree already. Since r<=l always
       in is_short case we avoid calling this rather expensive method in
       that case
    */

    if (ep.r < ep.l && nm.in_tree_as_subword(lhs))
      return AR_Collapse;

    if (maf.options.collapse==2 &&
        ep.r >= ep.l && ep.r <= stats.discard_limit && ep.r <= height &&
        Subword(lhs,0,ep.l-1).compare(rhs)>0 && nm.in_tree_as_subword(lhs))
      return AR_Provoke_Collapse;
  }

  if (pd.is_coset_system)
  {
    if (ep.lvalue == pd.coset_symbol)
    {
      if (stats.no_coset_pool)
        return AR_Special;
      if (maf.options.ignore_h_length)
      {
        Total_Length h_length = 0;
        const Ordinal * rvalues = rhs.buffer();
        for (h_length = 0; rvalues[h_length] >= pd.coset_symbol;h_length++)
          ;
        h_length++;
        if (ep.total - h_length < stats.visible_limit)
          return AR_Small_G;
      }
    }

    if (ep.lvalue > pd.coset_symbol && maf.options.no_h)
      return AR_Small_H;
  }
#if 0
  if (ep.e && !ep.e->is_final())
    for (Equation_Node * other_lhs = ep.e->first_lhs();other_lhs;other_lhs = other_lhs->next_lhs())
      if (ep.e->last_letter() == other_lhs->last_letter() ||
          other_lhs->length() + ep.r <= stats.visible_limit)
        return AR_Special;
#endif
  return AR_Rejected;
}

/**/

void Equation_Manager::moderate(Equation_Properties *ep)
{
  if (!(flags & AE_NO_UNBALANCE) && !ep->old_e->is_final())
  {
    /* If l is much less than r we try to temporarily alleviate the
       pain this will cause us by denormalising the equation. This
       will be especially helpful if were planning to insert something
       like A=a^99, as will go for something more reasonable like
       A^33=a^67 instead.
       In fact this is a mixed blessing. If we are unlucky enough to
       have discovered the equation above before a^100=IdWord, unbalancing
       it will delay even further our discovery of the "nice" conjugate
       equation, and we may even end up discovering a^1000=IdWord first
       instead (and then A=a^999). We may get a lot of
       "performing a difficult reduction" events.

        On the whole unbalancing does improve stability, and does improve
        performance.
    */

    Equation_Word & lhs = we->lhs_word();
    Equation_Word & rhs = we->rhs_word();

    while (ep->l + stats.visible_limit < ep->r || ep->l * 2 < ep->r)
    {
      Ordinal inverse_value = maf.inverse(rhs.value(ep->r-1));
      if (inverse_value == INVALID_SYMBOL)
        break;
      Node_Reference ns = ep->old_e->transition(nm,inverse_value);
      if (!ns->is_final())
      {
        lhs.append(inverse_value);
        we->derivation.add_unbalance(inverse_value);
        we->balanced = 2;
        ep->l++;
        rhs.set_length(ep->r -= 1);
        ep->old_e = ns;
      }
      else
        break;
    }
    we->read_properties(ep);
  }
}

/**/

void Equation_Manager::choose_action(const Equation_Properties &ep)
{
  if (ep.old_e->is_final())
  {
    /* In this case we must have passed the AE_KEEP_LHS flag.
       We already have a reduction for the LHS and need to
       decide what to do with the new one */

    State old_rhs = ep.old_e->raw_reduced_node(nm);

    if (flags & AE_COPY_LHS || nm.is_confluent())
    {
      /* In this case the "new reduction" is actually just the
         reduction we already knew for the LHS, so we don't need
         to do any checks. (Or it is asserted to be - we don't
         check this) */
    }
    else
    {
      /* The old and new reductions may be different. Even if the old
         reduction is already known to be reducible and the new one is
         not the old reduction might actually lead to a better reduction.
         Even if the new one is "better" it might be longer, so have
         a bad effect in practice. So things can be difficult...

        The simplest idea, and the one I used to use was to just
        create an equation between the two RHSes and then find out
        what the reduction of the LHS was afterwards. However, this
        approach is no good because the old RHS might be too long
        for an equation to be inserted safely.
      */

      int rhs_change = we->rhs_changed();
      if (rhs_change)
      {
        /* We do have two different reductions for the LHS.
           We have to handle this differently depending on whether or
           not the equation is secondary or primary, and whether or not
           the new RHS is better or worse than before.
        */
        if (ep.e.is_null())
        {
          /* The equation is certainly secondary, so we can take advantage
             of the dubious_RHS queue.

             If the new word is better we will create the desired equation
             straight away. The equation between the RHSes will be found
             when the secondary equation gets to the front of the dubious
             RHS queue. If the new reduction is worse we try to create the
             equation between RHSes, and ask for the pending LHS to be
             added later, because it may be needed for word-differences,
             or we may need it to replace an equation whose RHS should be
             improved.
          */
          if (rhs_change == 1)
            action_needed = flags & AE_INSERT+AE_CORRECTION ?
                            EMA_Save_LHS_And_Reduce_Again : EMA_Reduce_Again;
          else
            action_needed = EMA_Strong_Insert;
        }
        else
        {
          if (rhs_change == 1)
            action_needed = EMA_Reduce_Again;
          else
          {
            action_needed = EMA_Strong_Insert;
            /* we are going to create the equation at once, and use
              equate_nodes() to create the equation between the old rhs
              and the new one*/
          }
        }
      }
      else if (flags & AE_WANT_WEAK_INSERT && ep.e.is_null())
      {
        action_needed = EMA_General_Insert;
        return;
      }
    }

    if (!ep.e.is_null() && action_needed < EMA_General_Insert)
      if (old_rhs->is_final())
        action_needed = EMA_General_Insert;
      else
      {
        action_needed = EMA_Do_Nothing;
        return;
      }
  }
  else
    action_needed = EMA_Strong_Insert; // the equation is certainly a new primary

  if (action_needed < EMA_General_Insert)
  {
    /* At this point we must have reached a situation where the LHS is already
       known to be reducible, and inserting the equation won't change the
       reduction of the LHS. The equation does not exist yet, because if it
       it did we would either have returned or set action_needed to something
       else. */
    if (action_needed == EMA_Unknown)
      action_needed = rm.is_interesting_equation(*we,flags) ? EMA_Weak_Insert : EMA_Do_Nothing;
  }

  if (action_needed == EMA_Do_Nothing && !(flags & AE_RIGHT_CONJUGATE))
  {
    /* It appears the equation is not necessary, but its conjugate might be*/
    Ordinal ivalue = maf.inverse(ep.rvalue);
    if (ivalue != INVALID_SYMBOL)
    {
      Node_Reference old_rhs = we->rhs_word().state_word(false);
      Node_Reference old_conjugate;
      if (!old_rhs.is_null())
        old_conjugate = old_rhs->transition(nm,ivalue);
      if (!old_rhs.is_null() && !old_conjugate->is_final())
      {
        /* In this case, even though we already know how to reduce ax=b
           we do not know how to reduce bX, so we are going to construct
           the equation anyway.
           Of course in this case old_rhs*X may not itself be reducible,
           but if it is not then bX is currently thought irreducible.
           We may also be fooled into inserting by an unbalanced equation here */
        action_needed = EMA_General_Insert;
      }
      if (action_needed==EMA_Do_Nothing &&
          (maf.options.assume_confluent || ep.total > stats.visible_limit))
      {
        if (old_rhs->length(nm) == ep.r &&
            ivalue != INVALID_SYMBOL && !ep.prefix.is_null() &&
            old_conjugate->reduced_node(nm,old_conjugate) == ep.prefix)
          action_needed = EMA_Do_Nothing; /* The conjugate is already present */
        else
        {
          /* We need to check the conjugate */
          bool failed = false;
          Equation_Word & spare = we->spare_word();
          spare = we->rhs_word();
          spare.append(ivalue);
          spare.reduce(0,&failed);
          if (spare != Subword(we->lhs_word(),0,ep.l-1))
            action_needed = EMA_General_Insert;
        }
      }
    }
  }

  if (action_needed != EMA_Do_Nothing &&
      action_needed <= EMA_Strong_Insert &&
      ep.l+ep.r > stats.pool_limit &&
      ep.r > ep.l && ep.r >= nm.height() && !(flags & AE_CORRECTION))
  {
    /* Here I am preventing creation of long secondary equations. The
       equation is getting discarded. This does not matter, because
       in this situation it is more or less certain that we were improving
       the RHS of a secondary and the equation between the rhs and the
       reduction of the LHS by the primary has already been created.
       Experiments seem to indicate that doing this definitely improves
       performance most of the time, even when a worse equation
       already exists at e!
    */
    if (!ep.trailing_subword.is_null() && ep.trailing_subword->is_final())
      action_needed = EMA_Do_Nothing;
  }
}
