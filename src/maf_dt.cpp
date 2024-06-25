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


// $Log: maf_dt.cpp $
// Revision 1.16  2010/06/10 13:57:36Z  Alun
// All tabs removed again
// Revision 1.15  2010/06/09 06:03:08Z  Alun
// Various changes required by new style Node_Manager. Changes to method used
// to grow word_difference machines to try to reduce memory usage when dealing
// with large finite groups with many word-differences
// Revision 1.14  2009/09/13 18:47:14Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.13  2009/09/01 23:34:20Z  Alun
// Decision about whether to suppress "interesting" equations removed to maf_rm.cpp
// Revision 1.12  2009/06/15 10:15:33Z  Alun
// August 2009 version. Various changes mostly connected with coset systems
// Revision 1.14  2008/11/12 00:36:41Z  Alun
// Must flag a change when a previously existing state becomes initial
// Revision 1.13  2008/11/04 22:02:00Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.11  2008/10/10 07:47:49Z  Alun
// h words on initial differences were broken by last changes

/* See maf_dt.h for introductory comments about this class.

   While we shall perform the complete KB process for a group with a confluent
   RWS we want to perform as little of it is possible for systems that are
   not confluent but are automatic.

   In order to facilitate this we try to maintain a word-difference machine
   that corresponds to the equations in the Rewriter_Machine. Usually this
   will contain at least some spurious words that were inserted from equations
   that were subsequently improved, but this does not matter too much (though
   it would tend to increase the number of states of the other FSAs prior to
   minimisation, so the code that builds them ensures the Difference_Tracker
   is clean before building the word-acceptor, also having previously eliminated
   any spurious word-differences from "equations" with a reducible LHS prefix
   or a reducible RHS).
   As new equations are found we look to see if they can already be proved by
   this word-difference machine. Equations that cannot be proved by this
   machine either have a new word-difference, or have a new transition between
   word-differences, and are considered interesting, (though in some cases the
   equation will later turn out not be fully reduced). Equations are also
   interesting if they are primary and involve a word-difference or transition
   not previously seen in a primary equation, or have a better LHS than any
   such equation seen before. (By "better" I mean shorter in the first
   instance - as otherwise too many equations get labelled as interesting -
   but when we rebuild the Difference_Tracker we insert the equations by
   shortlex ordering of the LHS, so "better" becomes best.)

   From time to time the Difference_Tracker is rebuilt and used to build
   the Difference_Machine instances used to build the other automata.

   The following links exist between states (nodes/equations) of a
   Rewriter_Machine and the states and transitions of its Difference_Tracker:

   i) Each state is keyed on its reduced word, and up to three associated
   pieces of data are stored,
   1 the shortest equation containing the difference
   2 the shortest primary equation containing the difference (if any).
   3 (only used for coset difference machines and where h generators
   are present). In this case it is the h word for the difference.

   ii) Each transition is labelled by a shortest primary equation, or if there
   isn't such an equation the shortest equation.

  In the course of building or modifying the difference tracker we may well be
  able to form new equations:
  In particular:
    i) if s^(x,y) -> ns, then x w(ns) Y == w(s) where w(s) is the reduced word
       associated with state s and ns^(X,Y) -> s
    ii) if os == s and X os y -> ns1, and X s y -> ns2 then w(ns1)==w(ns2)
  Equations of form i) are discovered when new states are added.
  Equations of form ii) are discovered when a word-difference becomes reducible.

  Calculations of word-differences are primarily done using "half differences",
  usually "left half differences". See prepare_differences() in maf_we.cpp for
  an explanation of what these are and why they are used.
*/
#include "container.h"
#include "maf_dt.h"
#include "maf_nm.h"

Difference_Tracker::Difference_Tracker(Node_Manager &nm_,
                                       State_Count hash_size,
                                       bool cached) :
  LTFSA(nm_.maf.container,nm_.maf.group_alphabet(),
        nm_.maf.group_alphabet().product_alphabet_size(),hash_size,
        sizeof(Node_ID),true,cached ? 0x400000 : 0),
  count(0),
  nm(nm_),
  full_alphabet(nm_.maf.alphabet),
  ok(true),
  closed(false),
  interest_wrong(false),
  dt_stats(stats),
  pending_pair(0),
  pending_tail(&pending_pair)
{
  manage(state_equations);
  manage(state_primaries);
  manage(merge_map);
  manage(merge_next);
  manage(merge_prev);
  manage(distance);

  /* Insert failure state */
  find_state(Node_Reference(0,0));
  /* Insert initial state */
  find_state(nm.start());
  distance[1] = 0;
  /* Set up trivial "equations" x=x */
  nr_generators = base_alphabet.letter_count();
  for (Ordinal g = 0;g < nr_generators;g++)
  {
    set_transition(1,base_alphabet.product_id(g,g),1);
    stats.nr_computed_transitions++;
  }
  Transition_ID nr_symbols = base_alphabet.product_alphabet_size();
  reverse_product = new Transition_ID[nr_symbols];
  /* remember the value of each generators inverse. */
  {
    // build the reverse_produce array that will allow us to quickly find
    // inverse transitions.
    Transition_ID product;
    Ordinal g1,g2;
    for (product = g1 = 0; g1 <= nr_generators;g1++)
      for (g2 = 0; g2 <= nr_generators;g2++,product++)
        if (product < nr_symbols)
          reverse_product[product] = base_alphabet.product_id(nm.inverse(g1),nm.inverse(g2));
  }
  set_single_accepting(1);
  bool extended_coset_system = nm.pd.is_coset_system &&
                               nm.pd.presentation_type != PT_Simple_Coset_System;
  if (extended_coset_system)
  {
    set_label_type(LT_List_Of_Words);
    set_label_alphabet(full_alphabet);
    manage(state_hwords);
  }
  else
  {
    set_label_type(LT_Words);
    set_label_alphabet(base_alphabet);
  }
}

/**/

Difference_Tracker::~Difference_Tracker()
{
  detach_all();
  delete [] reverse_product;
}

/**/

void Difference_Tracker::detach_all()
{
  for (State_ID si = 1; si < count;si++)
  {
    Node_Reference s = nm_state(si);
    if (s)
    {
      if (!s->is_final())
        s->node(nm).clear_flags(NF_DIFFERENCES);
      s->detach(nm,s);
    }
    if (merge_map[si] == si)
    {
      /* we clear all the flags in the hope that this will be marginally quicker
         than clearing the flags individually. This code is only ever called
         when we want to clear all the existing information about word-differences
      */
      Node_Reference e(nm,state_equations[si]);
      if (e)
      {
        e->node(nm).clear_flags(EQ_INTERESTING|EQ_HAS_PRIMARY_DIFFERENCES|EQ_HAS_DIFFERENCES);
        e->detach(nm,e);
      }
      e = Node_Reference(nm,state_primaries[si]);
      if (e)
        e->detach(nm,e);
      if (state_hwords.capacity())
      {
        e = Node_Reference(nm,state_hwords[si]);
        if (e)
        {
          e->node(nm).clear_flags(EQ_INTERESTING|EQ_HAS_PRIMARY_DIFFERENCES|EQ_HAS_DIFFERENCES);
          e->detach(nm,e);
        }
      }
      Transition_ID nr_symbols = base_alphabet.product_alphabet_size();
      for (Transition_ID ti = 0; ti < nr_symbols;ti++)
      {
        e = get_equation(si,ti);
        if (e)
        {
          e->node(nm).clear_flags(EQ_INTERESTING|EQ_HAS_PRIMARY_DIFFERENCES|EQ_HAS_DIFFERENCES);
          e->detach(nm,e);
        }
      }
    }
  }
}

/**/

unsigned Difference_Tracker::learn_equation(Working_Equation & we,
                                            Node_Handle e,
                                            unsigned flags)
{
  /* Create the transitions necessary for the specified equation to
     be recognised by the Difference_Tracker.
     For an equation ug=v we create two sets of word-differences: one which
     reads g, and one which does not, (though these only differ when r>=l)
     The former are currently needed for Diff_Reduce and building the word
     acceptor, the latter are needed for the multiplier, and have the
     desirable property of being closed under inversion. It would probably
     be possible to change the code so that only the second set was needed.
     However, it probably would not be a good idea to do away with the former,
     since they are also needed to recognise a bad word-acceptor when
     building the multiplier. */

  unsigned retcode = 0;
  Ordinal multiplier;
  Node_Reference initial_hs;
  Ordinal_Word original_lhs(we.get_lhs());
  Node_Reference initial_gs = we.adjust_for_dm(&initial_hs,&multiplier,true);
  State_ID initial_si;
  if (initial_gs.is_null())
    return 0;
  retcode = learn_initial_difference(&initial_si,initial_hs,initial_gs,e,original_lhs,flags);

  const Word & lhs = we.get_lhs();
  const Word & rhs = we.get_rhs();
  const Word_Length l = lhs.length();
  const Word_Length r = rhs.length();
  const Word_Length m = max(l,r);
  const Ordinal * left_values = lhs.buffer();
  const Ordinal * right_values = rhs.buffer();

  int fwd_dr_distance = 0;
  int fwd_gm_distance = 0;
  int rev_dr_distance = 0;
  int rev_gm_distance = 0;
  State_ID rev_gm_si = 1;
  State_ID rev_dr_si = 1;

  State_ID fwd_dr_si = initial_si;
  State_ID fwd_gm_si = initial_si;
  Word_Length forward = 0;
  Word_Length reverse = m;
  Ordinal lvalue,rvalue;
  Total_Length g_length = l+r;

  if (l > r)
  {
    retcode |= learn_transition(&rev_dr_si,&rev_dr_distance,multiplier,
                     PADDING_SYMBOL,e,original_lhs,-1,flags|PD_GM_DIFFERENCE|PD_DR_DIFFERENCE,g_length);
    rev_gm_si = rev_dr_si;
    rev_gm_distance = rev_dr_distance;
    --reverse;
  }
  else
    retcode |= learn_transition(&rev_gm_si,&rev_gm_distance,multiplier,
                     PADDING_SYMBOL,e,original_lhs,0,flags|PD_GM_DIFFERENCE,g_length);

  while (forward < reverse)
  {
    if (fwd_dr_distance+fwd_gm_distance <= rev_gm_distance+rev_dr_distance)
    {
      lvalue = left_values[forward];
      rvalue = right_values[forward];
      if (forward+1 < l)
      {
        retcode |= learn_transition(&fwd_dr_si,&fwd_dr_distance,
                         lvalue,rvalue,
                         e,original_lhs,forward+1,flags|PD_DR_DIFFERENCE|PD_GM_DIFFERENCE,g_length);
        fwd_gm_si = fwd_dr_si;
        fwd_gm_distance = fwd_dr_distance;
      }
      else if (forward + 1 == l)
      {
        retcode |= learn_transition(&fwd_dr_si,&fwd_dr_distance,
                                    multiplier,rvalue,
                                    e,original_lhs,forward+1,flags|PD_DR_DIFFERENCE,g_length);
        retcode |= learn_transition(&fwd_gm_si,&fwd_gm_distance,
                                    PADDING_SYMBOL,rvalue,
                                    e,original_lhs,forward+1,flags|PD_GM_DIFFERENCE,g_length);
      }
      else
      {
        retcode |= learn_transition(&fwd_dr_si,&fwd_dr_distance,lvalue,rvalue,
                                    e,original_lhs,forward+1,
                                    flags|PD_DR_DIFFERENCE,g_length);
        retcode |= learn_transition(&fwd_gm_si,&fwd_gm_distance,lvalue,rvalue,
                                    e,original_lhs,forward+1,
                                    flags|PD_GM_DIFFERENCE,g_length);
      }
      forward++;
    }
    else
    {
      if (reverse > l)
      {
        --reverse;
        lvalue = left_values[reverse];
        rvalue = right_values[reverse];
        retcode |= learn_transition(&rev_dr_si,&rev_dr_distance,
                                    lvalue,rvalue,e,original_lhs,reverse-m,
                                    flags|PD_DR_DIFFERENCE,g_length);

        retcode |= learn_transition(&rev_gm_si,&rev_gm_distance,
                                    lvalue,rvalue,e,original_lhs,reverse-m,
                                    flags|PD_GM_DIFFERENCE,g_length);
      }
      else if (reverse == l)
      {
        /* In this case we must have l <= r, because otherwise
           we dealt with the last symbol of the LHS before this while
           statement */
        --reverse;
        rvalue = right_values[reverse];
        retcode |= learn_transition(&rev_dr_si,&rev_dr_distance,
                                    multiplier,rvalue,e,original_lhs,reverse-m,
                                    flags|PD_DR_DIFFERENCE,g_length);
        retcode |= learn_transition(&rev_gm_si,&rev_gm_distance,
                                    PADDING_SYMBOL,rvalue,e,original_lhs,
                                    reverse-m,flags|PD_GM_DIFFERENCE,g_length);
        if (rev_gm_si != rev_dr_si)
          merge_states(rev_gm_si,rev_dr_si,Derivation(BDT_Double,e,Node_Reference(0,0),reverse));
      }
      else
      {
        --reverse;
        lvalue = left_values[reverse];
        rvalue = right_values[reverse];
        retcode |= learn_transition(&rev_dr_si,&rev_dr_distance,
                                    lvalue,rvalue,e,original_lhs,reverse-m,
                                    flags|PD_DR_DIFFERENCE|PD_GM_DIFFERENCE,g_length);
        rev_gm_si = rev_dr_si;
        rev_gm_distance = rev_dr_distance;
      }
    }
    rev_gm_si = merge_map[rev_gm_si];
    rev_dr_si = merge_map[rev_dr_si];
    fwd_gm_si = merge_map[fwd_gm_si];
    fwd_dr_si = merge_map[fwd_dr_si];
    if (!rev_gm_si || !rev_dr_si || !fwd_gm_si || !fwd_dr_si)
      return 0;
  }
  if (fwd_dr_si != rev_dr_si)
  {
    merge_states(fwd_dr_si,rev_dr_si,Derivation(BDT_Double,e,Node_Reference(0,0),reverse));
    rev_gm_si = merge_map[rev_gm_si];
    fwd_gm_si = merge_map[fwd_gm_si];
  }
  if (fwd_gm_si != rev_gm_si)
    merge_states(fwd_gm_si,rev_gm_si,Derivation(BDT_Double,e,Node_Reference(0,0),reverse));

  if (!e.is_null() && e->is_valid())
  {
    if (retcode & DTC_EQUATION)
      if (g_length > stats.max_difference_eqn)
        stats.max_difference_eqn = g_length;
    if (retcode & DTC_PRIMARY_EQUATION)
      if (g_length > stats.max_primary_difference_eqn)
        stats.max_primary_difference_eqn = g_length;
    if (retcode & DTC_INTEREST_EQUATION)
      if (g_length > stats.max_interesting_eqn)
        stats.max_interesting_eqn = g_length;
  }
  return retcode;
}

/**/

unsigned Difference_Tracker::learn_initial_difference(State_ID *initial_si,
                                                      Node_Handle h_initial,
                                                      Node_Handle g_initial,
                                                      Node_Handle e,
                                                      const Word & new_lhs,
                                                      unsigned flags)
{
  /* This gets called to set the initial difference
     for a coset equation. h_initial and g_initial are the H and G words
     for the group element */
  State_ID ni = find_state(g_initial,false);
  unsigned retcode = 0;

  if (ni == 1)
  {
    *initial_si = 1;
    return 0;
  }

  bool ok = ni && is_initial(ni);

  if (!ni)
  {
    Equation_Word ew(nm,g_initial);
    ni = create_difference(ew,0,0);
  }

  flags |= PD_DR_DIFFERENCE|PD_GM_DIFFERENCE;
  if (h_initial != g_initial && state_hwords.capacity())
  {
    Node_Reference old_h(nm,state_hwords[ni]);
    if (old_h != h_initial)
    {
      if (!old_h.is_null())
        old_h->detach(nm,old_h);
      state_hwords[ni] = h_initial;
      h_initial->attach(nm);
      ok = false;
    }
  }
  if (!ok)
  {
    // we have to be careful to flag a change to the initial state
    // since it can happen that a word-difference first arises in the
    // ordinary way and afterwards becomes an initial state
    // this happens for example when a generator is eliminated
    // but is then also found to be in the subgroup
    if (flags & PD_CHECK)
      stats.nr_recent_changes++;
    flag_changes(retcode = DTC_STATE);
    set_is_initial(ni,true);
    // initial states must be labelled because the tracker is used by
    // Diff_Reduce and equations are created base on the results.
    label_states(*this,ni);
  }
  *initial_si = ni;
  return retcode | setup_difference(ni,e,new_lhs,flags);
}

/**/

unsigned Difference_Tracker::setup_difference(State_ID nsi,Node_Handle e,
                                              const Word & new_lhs,unsigned flags)
{
  unsigned retcode = 0;
  Node_Reference ns = nm_state(nsi);
  if (!ns->flagged(NF_IS_GM_DIFFERENCE|NF_IS_DR_DIFFERENCE))
  {
    Word_Length length = ns->length(nm);
    if (length > stats.max_difference_length)
    {
      stats.max_difference_length = length;
      nm.set_vital_limit(vital_eqn_limit());
      if (flags & PD_CHECK)
        max_changed = 1;
    }
    if (flags & PD_CHECK)
    {
      if (nm.maf.options.log_flags & LOG_WORD_DIFFERENCES)
      {
        Ordinal_Word ow(full_alphabet);
        Simple_Equation se(nm,e);
        String_Buffer sb;
        ns->read_word(&ow,nm);
        ow.format(&sb);
        container.output(container.get_log_stream(),
                         "New word-difference %s in equation:\n",sb.get().string());
        se.print(container,container.get_log_stream());
      }
      stats.nr_recent_changes++;
    }

    flag_changes(DTC_STATE);
    if (!e.is_null() && e->language(nm) == language_L3)
      stats.nr_tertiary_differences++;
    retcode |= DTC_STATE;
  }

  if (!ns->is_final())
  {
    /* It is possible that when we get here a word-difference
       has become reducible, hence the if () above */
    if (flags & PD_GM_DIFFERENCE)
      ns->node(nm).set_flags(NF_IS_GM_DIFFERENCE);

    if (flags & PD_DR_DIFFERENCE)
      ns->node(nm).set_flags(NF_IS_DR_DIFFERENCE);
  }

  if (e)
  {
    /* Check whether this equation is the best equation associated with
       this difference */
    Node_Reference old_e(nm,state_equations[nsi]);
    if (old_e.is_null() || !old_e->is_valid() || old_e->compare(nm,new_lhs)==1)
    {
      e->attach(nm)->set_flags(EQ_HAS_DIFFERENCES);
      state_equations[nsi] = e;
      if (!old_e.is_null())
        old_e->detach(nm,old_e);
      retcode |= DTC_EQUATION;
    }

    if (e->fast_is_primary())
    {
      /* Check whether this equation is the best primary equation associated with
         this difference */

      if (!state_primaries[nsi])
      {
        flag_changes(DTC_PRIMARY_STATE);
        retcode |= DTC_PRIMARY_STATE;
        stats.nr_primary_differences++;
        if (flags & PD_CHECK && !ns->flagged(NF_IS_PRIMARY_DIFFERENCE))
          stats.nr_recent_primary_changes++;
        ns->node(nm).set_flags(NF_IS_PRIMARY_DIFFERENCE);
      }

      old_e = Node_Reference(nm,state_primaries[nsi]);
      if (old_e.is_null() || !old_e->is_valid() || old_e->compare(nm,new_lhs)==1)
      {
        e->attach(nm)->set_flags(EQ_HAS_PRIMARY_DIFFERENCES);
        state_primaries[nsi] = e;
        if (!old_e.is_null())
          old_e->detach(nm,old_e);
        retcode |= DTC_PRIMARY_EQUATION;
      }
    }
  }
  return retcode;
}

/**/

unsigned Difference_Tracker::learn_transition(State_ID * si,int * distance,
                                              Ordinal lvalue,Ordinal rvalue,
                                              Node_Handle e,
                                              const Word &new_lhs,
                                              int position,unsigned flags,
                                              Total_Length g_length)
{
  State_ID osi,nsi;
  Transition_ID ti = base_alphabet.product_id(lvalue,rvalue);
  Transition_ID reverse_ti = reverse_product[ti];

  if (position <= 0)
  {
    nsi = *si;
    osi = create_transition(nsi,reverse_ti,flags);
    *si = osi;
    nsi = merge_map[nsi];
  }
  else
  {
    osi = *si;
    nsi = create_transition(osi,ti,flags);
    *si = nsi;
    osi = merge_map[osi];
  }
  *distance = this->distance[*si];
  unsigned retcode = 0;
  if (!*si)
    return 0;
  if (!e.is_null())
  {
    Boolean primary = e->fast_is_primary();

    /* check whether this equation should be associated with the transition */
    Node_Reference old_e = get_equation(osi,ti);
    if (old_e.is_null() ||
        (primary ? !old_e->fast_is_primary() || old_e->compare(nm,new_lhs)==1 :
                   !old_e->fast_is_primary() && old_e->compare(nm,new_lhs)==1))
    {
      if (!old_e.is_null())
        old_e->detach(nm,old_e);
      set_equation(osi,ti,e);
      e->node(nm).set_flags(EQ_INTERESTING);
      retcode |= DTC_INTEREST_EQUATION;
      if (primary && (old_e.is_null() || !old_e->fast_is_primary()))
        flag_changes(DTC_PRIMARY_TRANSITION);
      if (old_e.is_null())
      {
        flag_changes(DTC_TRUE_TRANSITION);
        stats.nr_computed_transitions--;
        stats.nr_proved_transitions++;
        retcode |= DTC_TRUE_TRANSITION;
      }
      if (flags & PD_CHECK)
        stats.nr_recent_transitions++;
      if (g_length > stats.max_interesting_eqn)
        stats.max_interesting_eqn = g_length;
    }
  }
  retcode |= setup_difference(nsi,e,new_lhs,flags);
  return retcode;
}

/**/

Node_Reference Difference_Tracker::nm_difference(Equation_Word * ew1,const Equation_Word & ew0,Transition_ID ti)  const
{
  Node_Reference s = ew0.state_word(true);
  Ordinal g1,g2;
  base_alphabet.product_generators(&g1,&g2,ti);
  if (!s.is_null() && !nm.maf.options.no_half_differences)
  {
    Node_Reference ns = ew1->left_half_difference(s,g1,false);
    if (!ns.is_null())
      ns = ew1->left_half_difference(ns,g2,false);
    if (!ns.is_null())
    {
      ew1->read_word(ns,true);
      return ns;
    }
  }
  ew1->set_length(0);
  if (g1 != PADDING_SYMBOL)
    ew1->append(nm.safe_inverse(g1));
  ew1->join(*ew1,ew0);
  if (g2 != PADDING_SYMBOL)
    ew1->append(g2);
  bool failed = false;
  ew1->reduce(0,&failed);
  return ew1->state_word(true);
}

/**/

bool Difference_Tracker::valid_transition(State_ID si,Transition_ID ti,State_ID nsi) const
{
  Equation_Word ew0(nm,nm_state(si));
  Equation_Word ew1(nm,nm_state(nsi));
  Equation_Word ew2(nm);
  nm_difference(&ew2,ew0,ti);
  String reason = 0;
  if (!nm.maf.is_valid_equation(&reason,ew1,ew2))
  {
    MAF_INTERNAL_ERROR(container,
                       ("Incorrect word-difference " FMT_ID " " FMT_TID " " FMT_ID "\n",si,ti,nsi));
    return false;
  }
  return true;
}

/**/

State_ID Difference_Tracker::check_difference_label(State_ID si,Equation_Word & ew0,bool needed)
{
  Node_Reference s = nm_state(si);
  Node_Reference ns = ew0.state_word(true);

  if (ns != s && (s->compare(nm,ew0) == 1 || needed && ew0.length() < limit))
  {
    if (ns.is_null())
      ns = ew0.realise_state();
    if (!ns.is_null())
    {
      State_ID nsi = find_state(ns);
      merge_states(nsi,si,Derivation(BDT_Update,s,-1,ns));
    }
  }
  return merge_map[si];
}

/**/

State_ID Difference_Tracker::create_transition(State_ID si,
                                               Transition_ID ti,
                                               unsigned flags)
{
  State_ID nsi = new_state(si,ti);
  if (!nsi || flags & PD_CHECK)
  {
    Node_Reference s = nm_state(si);
    Equation_Word ew0(nm,s);
    Equation_Word ew1(nm);
    Node_Reference ns = nm_difference(&ew1,ew0,ti);
    if (!ns.is_null() && !nsi)
      nsi = find_state(ns,false);

    if (nsi)
      nsi = check_difference_label(nsi,ew1,true);
    else
      nsi = create_difference(ew1,distance[si]+1,0);

    si = merge_map[si];
    State_ID ex_nsi = new_state(si,ti);
    State_ID reverse_si = new_state(nsi,reverse_product[ti]);
    if (reverse_si && reverse_si != si)
    {
      merge_states(si,reverse_si,Derivation(BDT_Reverse,s,ti));
      if (flags & PD_CHECK)
        stats.nr_recent_changes += 2;
    }
    else if (!reverse_si || !ex_nsi)
    {
      set_transition(si,ti,nsi);
      set_transition(nsi,reverse_product[ti],si);
      flag_changes(DTC_COMPUTED_TRANSITION);
      stats.nr_computed_transitions += 2;
      if (flags & PD_CHECK)
        stats.nr_recent_changes += 2;
    }
    else if (ex_nsi != nsi)
      MAF_INTERNAL_ERROR(container,
                         ("Unexpected coincidence: " FMT_ID " " FMT_ID " in Difference_Tracker::create_transition()\n",nsi,ex_nsi));
  }
  return merge_map[nsi];
}

/**/

State_ID Difference_Tracker::create_difference(Equation_Word &ew0,
                                               Word_Length defining_length,
                                               State_ID si)
{
  Equation_Word ew1(nm);
  Equation_Word ew2(nm);
  Transition_ID nr_symbols = base_alphabet.product_alphabet_size();
  State_ID * transition = new State_ID[nr_symbols];
  Transition_ID ti;
  bool restart = false;
  State_ID nsi = 0;
  Node_Reference s = ew0.state_word(true);

  if (si)
    get_transitions(transition,si);
  else
    for (ti = 0; ti < nr_symbols;ti++)
      transition[ti] = 0;

  /* First of all we create the transition table for the putative new state,
     (or recalculate the transitions for our an existing state) */
  for (ti = 0; ti < nr_symbols;ti++)
  {
    Transition_ID was = transition[ti];
    Node_Reference ns = nm_difference(&ew1,ew0,ti);
    transition[ti] = ns ? find_state(ns,false) : 0;
    restart = false;

    if (transition[ti])
    {
      if (defining_length > distance[transition[ti]]+1)
        defining_length = distance[transition[ti]]+1;
      if (was && transition[ti] != was)
         merge_states(transition[ti],was,Derivation(BDT_Update,s,ti));
    }
    if (!nm.is_confluent())
    {
      if (transition[ti] || nm.pd.inversion_difficult &&
                      (ew1.length() <= ew0.length()+2 || ew1.length() < limit) ||
          defining_length==0)
      {
        bool forced = true;
        if (transition[ti] || nm.pd.inversion_difficult &&
                      (ew1.length() <= ew0.length()+2 || ew1.length() < limit))
          forced = false;
        /* We have formed v == G1*u*g2. So u == g1*v*G2. We see if
           g1vG2 is an existing state, or a better word */
        Node_Reference reverse_s = nm_difference(&ew2,ew1,reverse_product[ti]);

        if (reverse_s != s || s.is_null())
        {
          int cmp = ew0.compare(ew2);
          if (cmp != 0)
          {
            if (reverse_s)
            {
              nsi = find_state(reverse_s,false);
              if (nsi && nsi != si)
                break;
            }
            Working_Equation we(nm,ew0,ew2,Derivation(BDT_Reverse,s,ti));
            unsigned ae_flags = AE_TIGHT|AE_DISCARDABLE;
            if (!defining_length && nm.pd.is_coset_finite)
              ae_flags |= AE_INSERT;
            else
              ae_flags |= AE_VERY_DISCARDABLE;
            int added = we.learn(ae_flags);
            if (cmp > 0 &&
                (ew2.length() <= ew0.length() || added==1 || ew2.length() < limit))
            {
              ew0 = ew2;
              restart = true;
              bool failed = false;
              if (added ==1)
                ew0.reduce(0,&failed);
              s = ew0.state_word(true);
            }
          }
        }
      }

      if (transition[ti])
      {
        nsi = new_state(transition[ti],reverse_product[ti],false);
        if (nsi && nsi != si)
          break;
      }
    }
    if (!transition[ti] && was)
      transition[ti] = was;
    if (restart)
      ti = -1;
  }

  if (nsi && nsi != si)
  {
    nsi = merge_map[nsi];
    nsi = check_difference_label(nsi,ew0,true);
  }
  else
  {
    /* If we get here, then we believe the state is OK
       and we have already set up its transitions, and we know
       that there are no conflicting reverse transitions */
    Node_Reference s;
    if (!si)
    {
      s = ew0.realise_state();
      nsi = find_state(s);
      distance[nsi] = defining_length;
    }
    else
    {
      nsi = si;
      s = nm_state(nsi);
    }

    if (!nm.maf.options.no_half_differences)
      for (Ordinal g = 0;g < nr_generators;g++)
        ew1.left_half_difference(s,g,true);
    for (ti = 0;ti < nr_symbols;ti++)
      if (transition[ti])
      {
        transition[ti] = merge_map[transition[ti]];
        set_transition(transition[ti],reverse_product[ti],nsi);
        flag_changes(DTC_COMPUTED_TRANSITION);
        stats.nr_computed_transitions += 2;
        if (flags & PD_CHECK)
          stats.nr_recent_transitions += 2;
      }
    set_transitions(nsi,transition);
  }

  delete [] transition;
  return nsi;
}

/**/

void Difference_Tracker::merge_states(State_ID si1,State_ID si2,const Derivation &d)
{
  si1 = merge_map[si1];
  si2 = merge_map[si2];
  if (si1 != si2)
  {
    Transition_ID nr_symbols = base_alphabet.product_alphabet_size();
    State_ID * transition = new State_ID[nr_symbols*2];
    merge_pair(si1,si2,d,transition);
    while (pending_pair)
    {
      si1 = pending_pair->si1;
      si2 = pending_pair->si2;
      Pending_Pair * temp = pending_pair;
      pending_pair = pending_pair->next;
      delete temp;
      if (!pending_pair)
        pending_tail = &pending_pair;
      merge_pair(si1,si2,d,transition);
    }
    delete [] transition;
  }
}

/**/

void Difference_Tracker::merge_pair(State_ID si1,State_ID si2,const Derivation &d,State_ID * buffer)
{
  si1 = merge_map[si1];
  si2 = merge_map[si2];
  if (si1 != si2)
  {
    Node_Reference s1 = nm_state(si1);
    Node_Reference s2 = nm_state(si2);
#if 0
    Equation_Word lhs(nm,s1);
    Equation_Word rhs(nm,s2);
    String reason;
    if (!nm.maf.is_valid_equation(&reason,lhs,rhs))
    {
      String_Buffer sb1,sb2;
      container.output(container.get_log_stream(),"Bad merge %s=%s\n",
                       lhs.format(&sb1).string(),rhs.format(&sb2).string());
      MAF_INTERNAL_ERROR(container,(reason));
    }
#endif
    if (s1->compare(nm,s2) < 0)
    {
      State_ID temp = si1;
      si1 = si2;
      si2 = temp;
      s1 = s2;
      s2 = nm_state(si2);
    }
    State_ID si3 = si1;
    while (si3)
    {
      merge_map[si3] = si2;
      si3 = merge_next[si3];
    }
    si3 = si2;
    while (merge_next[si3])
      si3 = merge_next[si3];
    merge_next[si3] = si1;
    merge_prev[si1] = si3;
    if (distance[si2] > distance[si1])
      distance[si2] = distance[si1];

    stats.nr_differences--;
    if (state_primaries[si1] && state_primaries[si2])
    {
      stats.nr_primary_differences--;
      flag_changes(DTC_PRIMARY_STATE);
    }
    flag_changes(DTC_STATE);
    stats.nr_recent_changes++;

    /* remove any equation labels associated with the state that is disappearing*/
    Node_Reference e(nm,state_equations[si1]);
    if (!state_equations[si2])
      state_equations[si2] = e;
    else if (!e.is_null())
      e->detach(nm,e);
    state_equations[si1] = 0;

    e = Node_Reference(nm,state_primaries[si1]);
    if (!e.is_null())
    {
      if (!state_primaries[si2])
      {
        state_primaries[si2] = e;
        if (!s2->is_final())
          s2->node(nm).set_flags(NF_IS_PRIMARY_DIFFERENCE);
      }
      else
        e->detach(nm,e);
    }
    state_primaries[si1] = 0;

    if (state_hwords.capacity())
    {
      Node_Reference node(nm,state_hwords[si1]);
      if (!node.is_null())
      {
        if (!state_hwords[si2])
          state_hwords[si2] = node;
        else
          node->detach(nm,node);
      }
      state_hwords[si1] = 0;
    }
    Transition_ID nr_symbols = base_alphabet.product_alphabet_size();
    bool must_insert = nm.pd.is_coset_system && (is_initial(si1) || is_initial(si2));
    nm.equate_nodes(s1,s2,d,-1,true,must_insert);
    State_ID * trow_si1 = buffer;
    State_ID * trow_si2 = buffer + nr_symbols;
    get_transitions(trow_si1,si1);
    get_transitions(trow_si2,si2);
    for (Transition_ID ti = 0;ti < nr_symbols;ti++)
    {
      State_ID nsi1 = trow_si1[ti];
      State_ID nsi2 = trow_si2[ti];
      State_ID was_nsi2 = nsi2;
      bool gone = false;
      bool e_reused = false;

      Node_Reference old_e(0,0);
      nsi2 = merge_map[nsi2];
      nsi1 = merge_map[nsi1];
      if (nsi1)
      {
        old_e = get_equation(si1,ti);
        gone = true;
        if (nsi2 && nsi1 != nsi2)
          queue_pair(nsi1,nsi2);
        else if (!nsi2)
        {
          nsi2 = nsi1;
          e_reused = !old_e.is_null();
        }
      }
      if (nsi2 != was_nsi2)
      {
        if (e_reused)
        {
          set_equation(si2,ti,old_e);
          if (old_e->fast_is_primary())
            flag_changes(DTC_PRIMARY_TRANSITION|DTC_COMPUTED_TRANSITION|DTC_TRUE_TRANSITION);
          else
            flag_changes(DTC_TRUE_TRANSITION|DTC_COMPUTED_TRANSITION);
          gone = false;
        }
        trow_si2[ti] = nsi2;
      }
      if (nsi2 == si2)
        trow_si2[reverse_product[ti]] = si2;
      else
        set_transition(nsi2,reverse_product[ti],si2);
      /* remove any association between equations and the transition
         currently being considered in the state to be removed */
      if (!old_e.is_null())
      {
        old_e->detach(nm,old_e);
        set_equation(si1,ti,Node_Reference(0,0));
      }
      if (gone)
      {
        if (!old_e.is_null())
          stats.nr_proved_transitions--;
        else
          stats.nr_computed_transitions--;
        stats.nr_recent_transitions++;
      }
    }
    set_transitions(si2,trow_si2);
    if (is_initial(si1) && !is_initial(si2))
    {
      set_is_initial(si2,true);
      set_is_initial(si1,false);
      label_states(*this,si2);
    }
  }
}

/**/

void Difference_Tracker::queue_pair(State_ID si1,State_ID si2)
{
  /* queue pairs of states for later merging.
     Clearly if we already know that s1=s2 and s1=s3 there is no need
     to queue s2=s3. We try to economise the number of pairs for
     later merging by ordering each pair so that s1 > s2 (as this
     is likely to be the correct order), and then decreasing either
     s1 or s2 if possible */

  Pending_Pair *item;
  for (item = pending_pair;item;item = item->next)
  {
    if (si2 > si1)
    {
      State_ID temp = si2;
      si2 = si1;
      si1 = temp;
    }
    if (item->si1 == si1)
    {
      if (item->si2 == si2)
        return;
      si1 = item->si2;
    }
    else if (item->si1 == si2)
      si2 = item->si2;
  }
  if (si2 > si1)
  {
    State_ID temp = si2;
    si2 = si1;
    si1 = temp;
  }

#if 0
    Node_Reference s1 = nm_state(si1);
    Node_Reference s2 = nm_state(si2);
    Equation_Word lhs(nm,s1);
    Equation_Word rhs(nm,s2);
    String reason;
    if (!nm.maf.is_valid_equation(&reason,lhs,rhs))
    {
      String_Buffer sb1,sb2;
      container.output(container.get_log_stream(),"Bad merge %s=%s\n",
                       lhs.format(&sb1).string(),rhs.format(&sb2).string());
      MAF_INTERNAL_ERROR(container,(reason));
    }
#endif
  item = new Pending_Pair(si1,si2);
  *pending_tail = item;
  pending_tail = &item->next;
}

/**/

void Difference_Tracker::scan_all_differences()
{
  bool again = true;
  while (again)
  {
    State_Count limit = count*2;
    again = false;
    Equation_Word ew0(nm);
    for (State_ID si = 1;si < count;si++)
      if (merge_map[si] == si)
      {
        if (si > limit)
        {
          again = true;
          break;
        }
        if (!(char) si)
          container.status(2,1,"Checking difference machine (" FMT_ID " of " FMT_ID " states to do).\n",
                           count-si,count);
        ew0.read_word(nm_state(si),true);
        create_difference(ew0,distance[si],si);
        nm.update_machine();
      }
  }
}

/**/

void Difference_Tracker::remove_differences(Node_Handle e)
{
  /* When an equation is removed, or needs to have its RHS improved this method
     will be called. We remove any labels currently set on this equation */

  // N.B. We deliberately don't call e->reduced_node(nm,e) below.
  // We want what was there when the equation was differenced.
  Working_Equation we(nm,e,e->raw_reduced_node(nm),Derivation(BDT_Known,e));
  Node_Reference hs;
  Ordinal multiplier;
  Node_Reference ns = we.adjust_for_dm(&hs,&multiplier,false);
  State_ID state = find_state(ns,false);
  const Equation_Word & lhs_word = we.lhs_word();
  const Equation_Word & rhs_word = we.rhs_word();
  Word_Length l = lhs_word.length();
  Word_Length r = rhs_word.length();
  Word_Length m = max(l,r);
  const Ordinal * left_values = lhs_word.buffer();
  const Ordinal * right_values = rhs_word.buffer();
  Transition_ID ti;
  bool bad = false;
  State_ID last_common_si = state;
  for (Word_Length i = 0; i < m;i++)
  {
    ti = base_alphabet.product_id(left_values[i],right_values[i]);
    if (e == get_equation(state,ti))
    {
      bad = true;
      e->detach(nm,e);
      set_equation(state,ti,Node_Reference(0,0));
      if (e->fast_is_primary())
        flag_changes(DTC_PRIMARY_TRANSITION|DTC_TRUE_TRANSITION);
      else
        flag_changes(DTC_TRUE_TRANSITION);
      stats.nr_computed_transitions++;
      stats.nr_proved_transitions--;
    }
    state = new_state(state,ti,false);
    if (!state)
    {
      bad = true;
      break;
    }
    if (e == state_equations[state])
    {
      bad = true;
      e->detach(nm,e);
      state_equations[state] = 0;
      flag_changes(DTC_STATE);
    }
    if (e == state_primaries[state])
    {
      bad = true;
      e->detach(nm,e);
      state_primaries[state] = 0;
      stats.nr_primary_differences--;
      flag_changes(DTC_PRIMARY_STATE);
    }
  }

  if (l < r)
  {
    state = last_common_si;
    for (Word_Length i = l-1; i < m;i++)
    {
      ti = base_alphabet.product_id(PADDING_SYMBOL,right_values[i]);
      if (e == get_equation(state,ti))
      {
        bad = true;
        e->detach(nm,e);
        set_equation(state,ti,Node_Reference(0,0));
        if (e->fast_is_primary())
          flag_changes(DTC_PRIMARY_TRANSITION|DTC_TRUE_TRANSITION);
        else
          flag_changes(DTC_TRUE_TRANSITION);
        stats.nr_computed_transitions++;
        stats.nr_proved_transitions--;
      }
      state = new_state(state,ti,false);
      if (!state)
      {
        bad = true;
        break;
      }
      if (e == state_equations[state])
      {
        bad = true;
        e->detach(nm,e);
        state_equations[state] = 0;
        flag_changes(DTC_STATE);
      }
      if (e == state_primaries[state])
      {
        bad = true;
        e->detach(nm,e);
        state_primaries[state] = 0;
        stats.nr_primary_differences--;
        flag_changes(DTC_PRIMARY_STATE);
      }
    }
  }

  if (bad)
  {
    /* we can't tell whether we should remove states or transitions and
       over-long equations may get associated with the states and transitions
       this equation has detached from */
    interest_wrong = true;
    ok = false;
  }
}

/**/

bool Difference_Tracker::relabel_difference(Node_Handle s,Node_Flags flags)
{
  /* This gets called if a word-difference becomes reducible. There are
     two cases to consider: the simple case where we simply need to
     relabel the difference, and the more difficult case where the new word
     is already present as a word-difference.
     On entry flags is the value of s->flags at the time it became reducible.
     These flags should be transferred to ns. s should be left alone since
     by the time we get to look at it it may have been turned into an equation
     with word-differences. */
  bool retcode = false;
  bool keep = false;
  State_ID si = find_state(s,false,false);
  Equation_Word w0(nm,s);
  Equation_Word w1(nm,stats.max_difference_length+2);
  Equation_Word w2(nm,stats.max_difference_length+2);
  Transition_ID nr_symbols = base_alphabet.product_alphabet_size();

  if (si)
  {
    bool failed = false;
    w0.reduce(0,&failed);
    if (merge_map[si] != si)
    {
      if (!failed)
        check_difference_label(merge_map[si],w0,false);
    }
    else
    {
      Node_Reference ns = w0.state_word(true);
      State_ID nsi = 0;

      if (ns)
      {
        nsi = find_state(ns,false,false);
        if (nsi)
        {
          if (merge_map[si] != merge_map[nsi])
          {
            Derivation d(BDT_Update,ns,-1,s);
            merge_states(nsi,si,d);
          }
        }
      }

      if (!nsi)
      {
        if (!(state_equations[si] || state_primaries[si] || merge_next[si]) && nm.pd.inversion_difficult)
        {
          for (Transition_ID ti = 0;ti < nr_symbols && !keep;ti++)
          {
            State_ID si2 = new_state(si,ti);
            if (si2 &&
                (!get_equation(si,ti).is_null() ||
                 !get_equation(si2,reverse_product[ti]).is_null()))
             keep = true;
          }
        }
        else
          keep = true;

        if (keep)
        {
          if (ns.is_null())
            ns = w0.realise_state();
        }
        if (ns.is_null())
          keep = false;

        if (keep)
        {
          ns->node(nm).set_flags(flags);
          set_state_key(si,ns);
          ns->attach(nm);

          if (!nm.maf.options.no_equations)
          {
            /* we need to recalculate the products, since we may see
               different states from before */
            if (!nm.maf.options.no_half_differences)
              for (Ordinal g1 = PADDING_SYMBOL;g1 < nr_generators;g1++)
              {
                Node_Reference hs = w1.left_half_difference(ns,g1,true);
                if (!hs.is_null())
                  hs->node(nm).set_flags(NF_IS_HALF_DIFFERENCE);
              }
            for (Transition_ID ti = 0;ti < nr_symbols;ti++)
            {
              State_ID si2 = new_state(si,ti);
              Node_Reference ns1 = nm_difference(&w1,w0,ti);
              if (si2)
              {
                Node_Reference s2 = nm_state(si2);
                if (ns1.is_null())
                  ns1 = w1.realise_state();
                if (s2 != ns1 && !ns1.is_null() && !s2.is_null())
                {
                  Derivation d(BDT_Update,ns,ti,s);
                  State_ID si1 = find_state(ns1,false);
                  if (si1 && si2 != si1)
                  {
                    merge_states(si2,si1,d);
                    if (merge_map[si] != si)
                      break;
                  }
                  else
                    nm.equate_nodes(s2,ns1,d,-1,true);
                }
              }
              else if (!ns1.is_null() && ns1->flagged(NF_IS_DIFFERENCE))
              {
                State_ID si1 = find_state(ns1);
                State_ID reverse_si = new_state(si1,reverse_product[ti]);
                if (!reverse_si)
                {
                  set_transition(si,ti,si1);
                  set_transition(si1,reverse_product[ti],si);
                  stats.nr_computed_transitions += 2;
                  flag_changes(DTC_COMPUTED_TRANSITION);
                  stats.nr_recent_transitions++;
                }
                else
                {
                  Derivation d(BDT_Update,ns,ti,s);
                  merge_states(si,reverse_si,d);
                  if (merge_map[si] != si)
                    break;
                }
              }
            }
          }
        }
        else
        {
          stats.nr_differences--;
          flag_changes(DTC_STATE);
          for (Transition_ID ti = 0;ti < nr_symbols;ti++)
          {
            State_ID si2 = new_state(si,ti);
            if (si2)
            {
              stats.nr_computed_transitions -= 2;
              stats.nr_recent_transitions++;
              if (si2 != si)
                set_transition(si2,reverse_product[ti],0);
            }
          }
        }
      }
    }

    if (!keep)
    {
      merge_next[merge_prev[si]] = merge_next[si];
      if (merge_next[si])
        merge_prev[merge_next[si]] = merge_prev[si];
      merge_map[si] = 0;
      set_is_initial(si,false);
      remove_state(si);
      if (si < state_count())
        holes.append_one(si);
      else
        count--;
    }
  }
  s->detach(nm,s);
  return retcode;
}

/**/

bool Difference_Tracker::is_interesting_equation(const Working_Equation & we,
                                                 unsigned flags)
{
  /* the code used to decide whether to lie here when the number of word
     differences is very high . Since this is a strategic decision it belongs
     in the maf_rm.cpp wrapper for this method rather than here, except that
     we should lie anyway if we are in a state where we cannot tell, i.e. when
     the tracker has been created but no equations have been put into it yet */
  if (count < 2 && !nm.is_confluent())
    return false;
  bool ignore_primary = (flags & AE_IGNORE_PRIMARY)!=0;
  State_ID si = 1,nsi;
  const Equation_Word & lhs_word = we.lhs_word();
  const Equation_Word & rhs_word = we.rhs_word();
  Word_Length l = lhs_word.fast_length();
  Word_Length r = rhs_word.fast_length();
  const Ordinal * left_values = lhs_word.buffer();
  const Ordinal * right_values = rhs_word.buffer();
  Word_Length i = 0,j = 0;
  Node_Reference e;
  Transition_ID ti;
  bool is_primary = !ignore_primary && lhs_word.get_language() <= language_L1 &&
                    rhs_word.get_language() == language_L0;
  bool check_visible = false;
  bool interesting = false;

  if (nm.pd.is_coset_system)
  {
    Ordinal g = lhs_word.first_letter();
    if (g == nm.pd.coset_symbol)
    {
      Node_Reference hstate;
      Node_Reference initial_state = we.pre_adjust(&hstate);
      if (initial_state.is_null())
        return true;
      for (j = 0; j < r;j++)
        if (right_values[j] == nm.pd.coset_symbol)
          break;
      if (j == r)
        we.fatal_error("Coset symbol missing on RHS of equation\n");

      si = find_state(initial_state,false);
      if (!si || !is_initial(si))
        return true;
      i = 1;
      j++;
    }
    else if (g > nm.pd.coset_symbol)
      return false; // We don't care about equations in the H generators alone
    else
      check_visible = true;
  }

  Node_Reference ns = nm.start();
  l--; /* we read the equation using multiplier style word-differences
          rather than word-acceptor differences as this can change the
          answer we will give (because it can happen that an equation
          is accepted by the acceptor style dm, but not the multiplier one).
          We might need to check both in the non-shortlex case. */
  while (i < l || j < r)
  {
    Ordinal lvalue = i < l ? left_values[i++] : PADDING_SYMBOL;
    if (check_visible && lvalue != PADDING_SYMBOL)
    {
      ns = ns->transition(nm,lvalue);
      if (ns->flagged(NF_INVISIBLE|NF_REDUCIBLE))
        return false; // we only expect to see NF_INVISIBLE, but check
                      // NF_REDUCIBLE to prevent GPFs if we are passed an
                      // equation with a reducible prefix
    }
    Ordinal rvalue = j < r ? right_values[j++] : PADDING_SYMBOL;
    if (si)
    {
      ti = base_alphabet.product_id(lvalue,rvalue);
      e = get_equation(si,ti);
      if (e.is_null() || is_primary && !e->fast_is_primary())
      {
        if (!check_visible)
          return true;
        interesting = true;
      }
      nsi = new_state(si,ti,false);
      if (!nsi)
      {
        if (!check_visible)
          return true;
        interesting = true;
      }
      if (!interest_wrong && !interesting &&
          (!e->fast_is_primary() || is_primary) && e->compare(nm,lhs_word)==1)
        interest_wrong = true;
      si = nsi;
    }
  }
  if (si)
    si = new_state(si,base_alphabet.product_id(left_values[l],PADDING_SYMBOL),false);
  return interesting || !is_accepting(si);
}

/**/

State_ID Difference_Tracker::find_state(Node_Handle s,bool insert,bool mapped)
{
  Node_ID nid(s);
  State_ID si = LTFSA::find_state(&nid,sizeof(Node_ID),false);

  if (!si)
  {
    if (insert)
    {
      if (!holes.pop(&si))
      {
        si = LTFSA::find_state(&nid,sizeof(Node_ID),true);
        count++;
      }
      else
        set_state_key(si,nid);
      if (si)
      {
        s->attach(nm)->set_flags(NF_IS_DIFFERENCE);
        stats.nr_differences++;
      }
      merge_map[si] = si;
      merge_next[si] = 0;
      distance[si] = WHOLE_WORD;
      merge_prev[si] = si;
    }
  }
  else if (mapped)
    si = merge_map[si];
  return si;
}

/**/

void Difference_Tracker::label_states(Difference_Tracker & other,State_ID si)
{
  Ordinal_Word word(full_alphabet,stats.max_difference_length);
  bool is_extended_coset_machine = label_type() == LT_List_Of_Words;
  Word_List wl(full_alphabet);
  State_ID first = si == -1 ? 1 : si;
  State_ID last = si == -1 ? count-1 : si;
  if (si == -1)
    set_nr_labels(count,LA_Direct);

  for (si = first; si <= last;si++)
  {
    Node_Reference s = nm_state(si);
    if (s)
    {
      s->read_word(&word,nm);
      if (is_extended_coset_machine && si != 1 && is_initial(si))
      {
        wl.empty();
        wl.add(word);
        State_ID other_si = other.find_state(s);
        s = Node_Reference(nm,other.state_hwords[other_si]);
        s->read_word(&word,nm);
        wl.add(word);
        set_label_word_list(si,wl);
      }
      else
        set_label_word(si,word);
    }
    if (si != last)
      container.status(2,1,"Labelling difference machine states (state " FMT_ID " of " FMT_ID ")\n",
                       si,count);
  }
}

/**/

bool Difference_Tracker::is_inverse_complete()
{
  /* The difference machine should be closed under inversion since
     for every equation ux=v vX=u is also an equation, and its GM word
     differences are the inverse of the first equations.
     However, despite our always constructing each equation's conjugate, and
     going to a lot of trouble to try to keep the word-differences correct,
     it can happen that the word-differences get out of kilter.
     This method looks for this situation and tries to correct it by telling
     the tracker to learn an appropriate transition again. This should result
     in a call to  equate_nodes() that will fix the word-difference machine when
     it is rebuilt. */

  bool retcode = true;
  bool simple_coset_system = nm.pd.presentation_type == PT_Simple_Coset_System;
  Equation_Word ew(nm);

  for (State_ID si = 1; si < count;si++)
  {
    if (merge_map[si] == si)
    {
      Node_Reference s = nm_state(si);
      if (s->flagged(NF_IS_GM_DIFFERENCE))
      {
        Node_Reference e;
        container.status(2,1,"Checking word-differences are inverse closed."
                             " (" FMT_ID " of " FMT_ID " to do)\n",
                         count-si,count);
        Node_Reference is = s->node(nm).set_inverse(nm,s,Node_Reference(0,0),is_initial(si));
        if (is.is_null())
        {
          ew.read_inverse(s,true,true);
          is = ew.realise_state();
        }
        if ((!is->flagged(NF_IS_GM_DIFFERENCE) ||
             si!=1 && is_initial(si) != is_initial(find_state(is))) &&
            !(e = Node_Reference(nm,state_equations[si])).is_null())
        {
          Working_Equation we(nm,e,e->fast_reduced_node(nm),Derivation(BDT_Known,e));
          Node_Reference hstate;
          Ordinal multiplier;
          Node_Reference start = we.adjust_for_dm(&hstate,&multiplier,false);
          Node_Reference ihstate = simple_coset_system ?
                          hstate : hstate->node(nm).set_inverse(nm,hstate);

          if (start)
          {
            Word_Length l = we.lhs_word().length();

            const Word_Length r = we.get_rhs().length();
            const Word_Length m = l > r ? l-1 : r;
            Node_Reference istart = start->node(nm).set_inverse(nm,start,Node_Reference(0,0),true);

            const Ordinal * left_values = we.get_lhs().buffer();
            const Ordinal * right_values = we.get_rhs().buffer();
            State_ID current_si,current_isi;
            Equation_Word lhs_word(nm,e);
            learn_initial_difference(&current_si,hstate,start,e,lhs_word,PD_BOTH);
            learn_initial_difference(&current_isi,ihstate,istart,Node_Reference(0,0),lhs_word,PD_BOTH);
            for (Word_Length i = 0; i < m;i++)
            {
              Ordinal lvalue = i < l-1 ? left_values[i] : PADDING_SYMBOL;
              State_ID next_si = new_state(current_si,
                                     base_alphabet.product_id(lvalue,
                                                              right_values[i]));
              State_ID next_isi = new_state(current_isi,
                                     base_alphabet.product_id(right_values[i],
                                                              lvalue));
              if (next_si && next_isi)
              {
                Node_Reference next_s = nm_state(next_si);
                Node_Reference next_is = nm_state(next_isi);
                Node_Reference inext_s = next_s->node(nm).set_inverse(nm,next_s);
                if (inext_s.is_null())
                {
                  ew.read_inverse(next_s,true,true);
                  inext_s = ew.realise_state();
                }
                if (next_is != inext_s)
                {
                  Derivation d(BDT_Inversion_Closure,e,Node_Reference(0,0),i);
                  nm.equate_nodes(next_is,inext_s,d,-1,true);
                }
              }
              else
                break;
              current_si = next_si;
              current_isi = next_isi;
            }
          }
          retcode = false;
        }
      }
    }
  }
  return retcode;
}

/**/

bool Difference_Tracker::dm_changed(unsigned gwd_flags,bool recent) const
{
  unsigned mask = 0;
  if (!(gwd_flags & GWD_KNOWN_TRANSITIONS))
  {
    /* we don't care about changes that don't actually affect transitions */
    mask = (DTC_PRIMARY_TRANSITION|DTC_TRUE_TRANSITION);
  }
  else
  {
    /* we do care about changes to what transitions are proved, and don't
       care about transitions we have computed */
    mask = DTC_COMPUTED_TRANSITION;
  }

  if (gwd_flags & GWD_PRIMARY_ONLY)
  {
    /* This option only really makes sense if GWD_KNOWN_TRANSITIONS is also
       specified, and needs to be treated as though it were anyway. We may
       have computed transitions between primary differences or proved
       transitions between them that only occur in secondaries, but we don't
       have flags that could tell us about this */
    mask = DTC_STATE |
           DTC_TRUE_TRANSITION |
           DTC_COMPUTED_TRANSITION;
  }
  else
    mask |= DTC_PRIMARY_STATE; /* In this case we don't care about a known
                                  difference becoming primary or not primary*/
  return ((recent ? recent_changes : changes) & ~mask) !=0 ;
}

/**/

class Filtered_Word_Difference_Machine : public Delegated_FSA
{
  private:
    Difference_Tracker & dt;
    unsigned gwd_flags;
  public:
    Filtered_Word_Difference_Machine(Difference_Tracker & dt_,
                                     unsigned gwd_flags_) :
      Delegated_FSA(&dt_,false),
      dt(dt_),
      gwd_flags(gwd_flags_)
    {
    }
    virtual const State_ID * dense_transition_table() const
    {
      return 0;
    }
    virtual unsigned get_flags() const
    {
      if (gwd_flags & GWD_PRIMARY_ONLY+GWD_KNOWN_TRANSITIONS)
        return 0;
      return dt.get_flags();
    }
    virtual void get_transitions(State_ID * buffer,State_ID state) const
    {
      FSA::get_transitions(buffer,state);
    }
    virtual State_ID new_state(State_ID si,
                               Transition_ID symbol_nr,
                               bool buffer = true) const
    {
      return dt.filtered_new_state(si,symbol_nr,buffer,gwd_flags);
    }
};

/**/

State_ID Difference_Tracker::filtered_new_state(State_ID si,
                                                Transition_ID symbol_nr,
                                                bool buffer,
                                                unsigned gwd_flags) const
{
  unsigned test_flag = gwd_flags & GWD_PRIMARY_ONLY ? NF_IS_PRIMARY_DIFFERENCE : NF_IS_DIFFERENCE;
  Node_Reference s = nm_state(si);
  State_ID nsi = 0;
  if (!s.is_null() && s->flagged(test_flag))
  {
    nsi = new_state(si,symbol_nr,buffer);
    if (nsi)
    {
      if (si == 1 && !(gwd_flags & GWD_PRIMARY_ONLY))
      {
        Ordinal g1,g2;
        base_alphabet.product_generators(&g1,&g2,symbol_nr);
        if (g1 == PADDING_SYMBOL || g1 == g2)
          return nsi;
      }

      Node_Reference ns = nm_state(nsi);
      if (!ns->flagged(test_flag))
        nsi = 0;
      if (gwd_flags & GWD_KNOWN_TRANSITIONS)
      {
        Node_Reference e = get_equation(si,symbol_nr);
        if (e.is_null() || gwd_flags & GWD_PRIMARY_ONLY && !e->fast_is_primary())
          nsi = 0;
      }
    }
  }
  return nsi;
}

/**/

FSA_Simple * Difference_Tracker::grow_wd(unsigned gwd_flags)
{
  /* Grows a word-difference machine from the current Difference_Tracker.
     It is convenient to build this as another Difference_Tracker object,
     though in fact it does not have the proper associations between equations
     and the states and transitions of the FSA, and is therefore returned
     as an ordinary FSA object.
  */

  is_inverse_complete();

  /* Make sure the word-differences needed for the identity multiplier
     are present. */
  Equation_Word w0(nm,stats.max_difference_length+2);
  Transition_ID nr_symbols = base_alphabet.product_alphabet_size();
  State_ID * transition =  new State_ID[nr_symbols];
  get_transitions(transition,1);
  for (Ordinal g1 = 0;g1 < nr_generators;g1++)
  {
    bool failed = false;
    w0.set_length(0);
    w0.append(g1);
    if (!w0.reduce(0,&failed))
    {
      transition[base_alphabet.product_id(g1,g1)] = 1;
      /* The $ x transitions are needed for multiplications of
         form ux===v (i.e. no reduction). These may not be present
         yet */
      transition[base_alphabet.product_id(PADDING_SYMBOL,g1)] =
        find_state(nm.start()->transition(nm,g1));
    }
  }
  set_transitions(1,transition);
  delete [] transition;
  label_states(*this,-1);
  Filtered_Word_Difference_Machine temp(*this,gwd_flags);
  if (!holes.count() && !(gwd_flags & GWD_PRIMARY_ONLY))
    return FSA_Factory::copy(temp);
  return FSA_Factory::trim(temp);
}

/**/

void Difference_Tracker::compute_transitions(unsigned gwd_flags)
{
  /* Look for transitions between states that we did not spot before.
     Although we try to make as many deductions as we can about the word
     difference machine as we go along, it does happen that we miss some
     possible deductions because reductions discovered later on affect
     what we should have deduced from previously known equations.
     This could probably be fixed by insisting that we notify the difference
     tracker whenever a word-difference, half difference, or any word of
     the form X wd y becomes reducible, and repeating the deduction process
     for any word-differences that might be affected. But usually this would
     be a lot of work for nothing. So instead we just call this function
     when we want our Difference_Tracker to be as good as currently possible.
  */
  Equation_Word w0(nm,stats.max_difference_length+2);
  State_ID si;
  Transition_ID nr_symbols = alphabet_size();
  State_ID * transition =  new State_ID[nr_symbols];

  unsigned test_flag = gwd_flags & GWD_PRIMARY_ONLY ?
                       NF_IS_PRIMARY_DIFFERENCE : NF_IS_DIFFERENCE;
  for (si = 1;si < count;si++)
  {
    if (merge_map[si] == si)
    {
      container.status(2,1,"Recalculating difference machine transitions (" FMT_ID " of " FMT_ID " states to do).\n",
                       count-si,count);
      Node_Reference s = nm_state(si);
      if (s->flagged(test_flag))
      {
        get_transitions(transition,si);
        for (Ordinal g1 = PADDING_SYMBOL;g1 < nr_generators;g1++)
        {
          Node_Reference is(0,0);
          if (!nm.maf.options.no_half_differences)
            is = w0.left_half_difference(s,g1,true);
          for (Ordinal g2 = PADDING_SYMBOL;g2 < nr_generators;g2++)
          {
            Transition_ID ti = base_alphabet.product_id(g1,g2);
            if (ti < nr_symbols)
            {
              if (!transition[ti])
              {
                Node_Reference ns = !is.is_null() ? w0.left_half_difference(is,g2,false) :
                                                    w0.difference(s,g1,g2,false);
                transition[ti] = !ns.is_null() && ns->flagged(test_flag) ?
                                 find_state(ns) : 0;
              }
            }
          }
        }
        set_transitions(si,transition);
      }
    }
  }
  delete [] transition;
}

/**/

bool Difference_Tracker::import_difference_machine(const FSA_Simple & dm)
{
  Transition_ID nr_transitions = base_alphabet.product_alphabet_size();
  if (dm.alphabet_size() == nr_transitions &&
      dm.base_alphabet.letter_count() == base_alphabet.letter_count() &&
      count==2)
  {
    State_Count nr_states = dm.state_count();
    Ordinal_Word word(dm.base_alphabet);
    Equation_Word ew(nm);

    for (State_ID si = 1; si < nr_states ; si++)
    {
      dm.label_word(&word,si);
      ew = word;
      if (find_state(ew.realise_state()) != si)
        return false;
    }
    for (State_ID si = 1; si < nr_states; si++)
      set_transitions(si,dm.state_access(si));
    scan_all_differences();
    return true;
  }
  return false;
}
