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


// $Log: mafminkb.cpp $
// Revision 1.1  2010/05/17 10:26:12Z  Alun
// New file.
//

#include "container.h"
#include "mafword.h"
#include "keyedfsa.h"
#include "maf_cfp.h"
#include "maf_wdb.h"
#include "maf_el.h"
#include "maf_rm.h"
#include "maf_dr.h"
#include "mafauto.h"

FSA_Simple * Group_Automata::reducer(const MAF & maf,
                                     const General_Multiplier &multiplier,
                                     const FSA & difference_machine,
                                     bool full_length)
{
  /*
    This method constructs the fsa which accepts all word pairs of the
    for (u,v) in which every prefix of u is reduced and v is reduced and
    u=v in the group. The states are labelled with the word-difference
    arrived at at the state. The method of construction here is applicable
    to all word-orderings.

    In order to calculate this FSA we must ensure that we do actually
    have all the word-differences required to calculate it. At worst this
    means that all reduced words for elements that can be expressed as g*wd
    need to be added to the word-difference machine. We can only be missing
    word-differences if the length of words can increase during reduction.
    Even then we hope we are not actually missing word-differences, since
    the tracker always creates word-differences for both the reducer and
    the multiplier. However, it is definitely the case that word-differences
    needed for reductions that lengthen words can be missing. For the reducer
    for a group we can calculate all these word-differences using the
    multiplier. But for a coset system we cannot use the multiplier since
    the labels on the words should be reduced in the group, not reduced
    as coset representatives. We can use a Diff_Reduce instance to do this
    since that only does coset reduction if the word contains a coset symbol.
    However, this can be very slow. So, initially we create the FSA with
    some unlabelled states, and then label it after minimisation, which we
    hope has got rid of most of the troublesome states.

    We can find the extra states and transitions by seeing what accept
    states are accesible from each state of the multiplier via ($,g)
    transitions.
  */

  const State_ID at_reduction = multiplier.state_count();
  const Alphabet & alphabet = multiplier.base_alphabet;
  const Ordinal nr_generators = alphabet.letter_count();
  const int nr_transitions = alphabet.product_alphabet_size();
  Container & container = multiplier.container;
  State_ID key[3],old_key[3];
  key[0] = at_reduction+1;
  key[1] = difference_machine.state_count();
  key[2] = nr_generators+1;
  Triple_Packer key_packer(key);
  // note than this will be different from the is_short value in maf
  // if we are a coset system.
  const bool can_lengthen = !alphabet.order_is_effectively_shortlex() &&
                            !alphabet.order_is_geodesic();
  const bool is_coset_system = multiplier.has_multiple_initial_states();
  Keyed_FSA &factory = *new Keyed_FSA(container,alphabet,nr_transitions,
                                      at_reduction,
                                      key_packer.key_size());
  void * packed_key;
  const String name = full_length ? "equation recogniser" : "reduction recogniser";

  container.progress(1,"Building %s\n",name.string());
  Difference_Machine_Index &dmi = * new Difference_Machine_Index(difference_machine);
  Word_List wl(difference_machine.label_alphabet());
  Ordinal_Word ow(difference_machine.label_alphabet());

  /* Insert failure and initial states. This is much harder than usual
     because we need to match the multiplier and dm initial states */
  key[0] = 0;
  key[1] = 0;
  key[2] = 0;
  factory.find_state(packed_key = key_packer.pack_key(key));

  State_ID si;
  State_Subset_Iterator ssi;
  key[2] = nr_generators;
  key[0] = 0;
  for (key[0] = multiplier.initial_state(ssi,true);
       key[0];
       key[0] = multiplier.initial_state(ssi,false))
  {
    Label_ID label = multiplier.get_label_nr(key[0]);
    multiplier.label_word_list(&wl,label);
    Element_Count nr_words = wl.count();
    bool found = false;
    for (Element_ID word_nr = 0;word_nr < nr_words;word_nr++)
    {
      wl.get(&ow,word_nr);
      if (!ow.length() || ow.value(0) < nr_generators)
      {
        if (dmi.find(ow,&key[1]))
        {
          si = factory.find_state(key_packer.pack_key(key));
          factory.set_is_initial(si,true);
          found = true;
          break;
        }
      }
    }
    if (!found)
    {
      container.error_output("The multiplier and the word-difference machine"
                             " are differently labelled!\nIt is not possible"
                             " to construct the %s or the correct"
                             " word-difference machines.\n",name.string());
      delete &dmi;
      delete &factory;
      return 0;
    }
  }

  delete &dmi;
  State_ID *transition = new State_ID[nr_transitions];
  bool * seen = new bool[nr_generators];
  State_ID nr_states = factory.state_count();
  si = 0;
  while (factory.get_state_key(packed_key,++si))
  {
    key_packer.unpack_key(old_key);
    if (!(char) si)
      container.status(2,1,"Building %s states. (" FMT_ID " of " FMT_ID " to do)\n",
                       name.string(),nr_states-si,nr_states);
    State_ID gm_state = old_key[0];
    State_ID diff_state = old_key[1];
    Transition_ID product_id = 0;
    for (Ordinal g1 = 0; g1 < nr_generators;g1++)
    {
      seen[g1] = false;
      for (Ordinal g2 = 0; g2 <= nr_generators;g2++,product_id++)
      {
        if (gm_state==at_reduction)
          key[0] = key[1] = 0;
        else if (diff_state==1 && g1==g2 && full_length)
          key[0] = key[1] = 0;
        else
        {
          key[0] = multiplier.new_state(gm_state,product_id);
          key[1] = difference_machine.new_state(diff_state,product_id);
          key[2] = old_key[2];
        }

        if (key[0] && key[1])
        {
          transition[product_id] = factory.find_state(key_packer.pack_key(key));
          seen[g1] = true;
          if (transition[product_id] >= nr_states)
            nr_states++;
        }
        else
          transition[product_id] = 0;
      }
    }
    if (gm_state != at_reduction)
    {
      /* Now look for non zero transitions $,g in the multiplier.
         If we see one then we know that by taking $,g transitions
         from now on we will reach an accept state for one or more multipliers
         g1, (with g1 possibly equal to IdWord, though we are not interested
         in that). In this case we have g=dw for some word w which is a
         possible suffix of an accepted word, and for d the word-difference
         corresponding to the multiplier state. On the other hand if we
         have any (g1,g) transitions so far then we know that ug1 is accepted
         and cannot be reduced.

         So we add (g1,g2) transitions for each pair where (g1,gn) is
         zero, but ($,g2) is non-zero, and we have not
         already read the multiplier. If we have already read the multiplier
         then we take ($,g2) transitions as normal until we get to the accept
         state.
      */

      for (Ordinal g2 = 0; g2 < nr_generators;g2++,product_id++)
      {
        key[0] = multiplier.new_state(gm_state,product_id);
        if (key[0])
          key[1] = difference_machine.new_state(diff_state,product_id);

        if (key[0] && key[1])
        {
          if (old_key[2] != nr_generators)
          {
            key[2] = old_key[2];
            /* in this case we already chose the multiplier.
               and we may have reached the accept state for it now */
            if (multiplier.is_accepting(key[0]))
            {
              Label_ID label = multiplier.get_label_nr(key[0]);
              if (label != 0)
              {
                if (multiplier.label_for_generator(label,Ordinal(old_key[2])))
                {
                  key[0] = at_reduction;
                  key[1] = 1;
                  key[2] = nr_generators;
                }
                /* if not, then the new state is probably a dud, and will get
                   trimmed */
              }
            }
            transition[product_id] = factory.find_state(key_packer.pack_key(key));
            if (transition[product_id] >= nr_states)
              nr_states++;
          }
          else
          {
            /* In this case we try each potential multiplier and add new
               states */
            State_ID inner_key[3];
            for (Ordinal g1 = 0; g1 < nr_generators;g1++)
              if (!seen[g1])
              {
                Transition_ID alt_id = alphabet.product_id(g1,g2);
                inner_key[0] = key[0];
                inner_key[1] = key[1];
                inner_key[2] = g1;
                if (multiplier.is_accepting(inner_key[0]))
                {
                  Label_ID label = multiplier.get_label_nr(inner_key[0]);
                  if (label != 0)
                  {
                    if (multiplier.label_for_generator(label,g1))
                    {
                      if (diff_state != 1 || g1 != g2)
                      {
                        inner_key[0] = at_reduction;
                        inner_key[1] = 1;
                        inner_key[2] = nr_generators;
                      }
                      else
                        inner_key[0] = inner_key[1] = inner_key[2] = 0;
                    }
                    else if (!can_lengthen)
                      inner_key[0] = inner_key[1] = inner_key[2] = 0;
                    /* if not, then the new state is probably a dud, and will get
                       trimmed */
                  }
                }
                else if (!can_lengthen || diff_state==1 && g1==g2 && full_length)
                  inner_key[0] = inner_key[1] = inner_key[2] = 0;
                transition[alt_id] = factory.find_state(key_packer.pack_key(inner_key));
                if (transition[alt_id] >= nr_states)
                  nr_states++;
              }

            /* We don't want to take a ($,g) transition here */
            transition[product_id] = 0;
          }
        }
        else
          transition[product_id] = 0;
      }

      /* Now check if this state is an accept state.
         If it is, and we did not already read the multiplier
         then ux=v is an equation and we need to set the
         g1,$ transitions. If we did read the multiplier we should
         not be at an accept state for our chosen multiplier */
      if (old_key[2] == nr_generators)
      {
        Label_ID label = multiplier.get_label_nr(gm_state);
        if (label != 0)
        {
          for (Ordinal g1 = 0;g1 < nr_generators;g1++)
          {
            if (multiplier.label_for_generator(label,g1))
            {
              key[0] = at_reduction;
              key[1] = 1;
              key[2] = nr_generators;
              product_id = alphabet.product_id(g1,PADDING_SYMBOL);
              transition[product_id] = factory.find_state(key_packer.pack_key(key));
              if (transition[product_id] >= nr_states)
                nr_states++;
            }
          }
        }
      }
    }
    else
    {
      factory.set_single_accepting(si);
      for (Ordinal g2 = 0; g2 < nr_generators;g2++,product_id++)
        transition[product_id] = 0;
    }

    factory.set_transitions(si,transition);
  }

  delete [] transition;
  delete [] seen;

  factory.set_label_type(difference_machine.label_type());
  factory.set_label_alphabet(difference_machine.label_alphabet());
  Word_List_DB labels(difference_machine.state_count());
  factory.set_nr_labels(1,LA_Mapped);

  si = 0;
  bool labels_ok = true;
  Ordinal_Word label_ow(difference_machine.label_alphabet());
  Ordinal_Word new_label_ow(difference_machine.label_alphabet());
  while (factory.get_state_key(packed_key,++si))
  {
    if (!(char) si)
      container.status(2,1,"Labelling %s states. (" FMT_ID " of " FMT_ID " to do)\n",
                       name.string(),nr_states-si,nr_states);
    key_packer.unpack_key(old_key);
    bool can_label = true;
    Ordinal g = Ordinal(old_key[2]);
    if (g == nr_generators)
    {
      Label_ID label = difference_machine.get_label_nr(old_key[1]);
      difference_machine.label_word_list(&wl,label);
    }
    else
    {
      State_ID dm_si = difference_machine.new_state(old_key[1],
                                                    alphabet.product_id(g,PADDING_SYMBOL));
      if (dm_si != 0)
      {
        Label_ID label = difference_machine.get_label_nr(dm_si);
        difference_machine.label_word_list(&wl,label);
      }
      else
      {
        new_label_ow.set_length(0);
        new_label_ow.append(maf.inverse(g));
        Label_ID label = difference_machine.get_label_nr(old_key[1]);
        difference_machine.label_word(&label_ow,label);
        if (label_ow.length() && g == label_ow.value(0))
          new_label_ow = Subword(label_ow,1);
        else
        {
          if (is_coset_system)
            can_label = labels_ok = false;
          else
          {
            new_label_ow += label_ow;
            multiplier.reduce(&new_label_ow,new_label_ow);
          }
        }
        wl.empty();
        wl.add(new_label_ow);
      }
    }
    if (can_label)
    {
      Element_ID id;
      bool new_label = labels.insert(&id,wl);
      factory.set_label_nr(si,id,true);
      if (new_label)
        factory.set_label_word_list(id,wl);
    }
  }
  factory.remove_keys();
  if (can_lengthen)
  {
    /* we have put in speculative transitions which do not all reach accept
       states, so the FSA is no longer trim */
    factory.change_flags(0,GFF_TRIM);
  }
  else
  {
    /* We don't need a trim here because we started from a trim FSA and haven't
       done anything to stop it being trim. Essentially all we did is move the
       accept state for each path through the multiplier from the point just
       before where the equation is to the point just after.*/
  }
  FSA_Simple * fsa = FSA_Factory::minimise(factory);
  delete &factory;

  fsa->sort_bfs(); // we did not quite build this FSA in bfs order
  if (!labels_ok)
  {
    State_Count nr_states = fsa->state_count();
    labels_ok = true;

    for (si = 1; si < nr_states;si++)
      if (!fsa->get_label_nr(si))
      {
        labels_ok = false;
        break;
      }
    if (!labels_ok)
    {
      Diff_Reduce dr(&difference_machine);
      fsa->create_definitions();
      for (si = 1; si < nr_states;si++)
        if (!fsa->get_label_nr(si))
        {
          container.status(2,1,"Recalculating labels (" FMT_ID " of " FMT_ID " to do)\n",nr_states-si,nr_states);
          State_Definition definition;
          fsa->get_definition(&definition,si);
          Label_ID label = fsa->get_label_nr(definition.state);
          Ordinal g1,g2;
          alphabet.product_generators(&g1,&g2,definition.symbol_nr);
          fsa->label_word(&label_ow,label);
          if (g1 != PADDING_SYMBOL)
            new_label_ow.append(maf.inverse(g1));
          new_label_ow += label_ow;
          if (g2 != PADDING_SYMBOL)
            new_label_ow.append(g2);
          dr.reduce(&new_label_ow,new_label_ow);
          wl.empty();
          wl.add(new_label_ow);
          Element_ID id;
          bool new_label = labels.insert(&id,wl);
          fsa->set_label_nr(si,id,true);
          if (new_label)
            fsa->set_label_word_list(id,wl);
        }
    }
  }
  fsa->sort_labels();
  return fsa;
}

/**/

FSA_Simple * Group_Automata::build_equation_recogniser_from_rr(FSA_Simple & rr)
{
  /* Build an equation recogniser from a reduction recogniser.
     The rr is modified, but then restored to its original state. */

  const Alphabet & alphabet = rr.base_alphabet;
  const Ordinal nr_generators = alphabet.letter_count();
  const int nr_transitions = alphabet.product_alphabet_size();
  State_ID * transitions = new State_ID[nr_transitions];
  State_ID * save_transitions = new State_ID[nr_transitions];
  rr.get_transitions(save_transitions,1);
  rr.get_transitions(transitions,1);
  for (Ordinal g = 0; g < nr_generators;g++)
    transitions[alphabet.product_id(g,g)] = 0;
  unsigned save_flags = rr.get_flags();
  rr.set_transitions(1,transitions);
  rr.change_flags(0,GFF_ACCESSIBLE|GFF_TRIM|GFF_MINIMISED);
  FSA_Simple * er = FSA_Factory::minimise(rr);
  er->sort_bfs();
  er->sort_labels();
  rr.set_transitions(1,save_transitions);
  rr.change_flags(save_flags,0);
  delete [] transitions;
  delete [] save_transitions;
  return er;
}

/**/

FSA_Simple * Group_Automata::build_primary_recogniser(const FSA * L1_acceptor,
                                                      const FSA * wa,
                                                      FSA_Simple * dm)
{
  /*
    This method constructs the fsa which accepts a word pair (u,v) if and only
    if u is minimally reducible and v is its reduction.
    The code that used to be here is now a general purpose method in FSA_Factory,
    since the method of construction is quite general.
  */
  dm->container.progress(1,"Building primary equation recogniser\n");
  bool geodesic_reducer = dm->base_alphabet.order_is_geodesic() ||
                          dm->base_alphabet.order_is_effectively_shortlex();
  State_ID * save_transition = new State_ID[dm->alphabet_size()];
  State_ID * transition = new State_ID[dm->alphabet_size()];
  const State_ID initial = dm->initial_state();
  const Alphabet & alphabet = dm->base_alphabet;
  dm->get_transitions(save_transition,initial);
  dm->get_transitions(transition,initial);
  Ordinal nr_generators = Ordinal(wa->alphabet_size());
  for (Ordinal g = 0;g < nr_generators;g++)
    transition[alphabet.product_id(g,g)] = 0;
  dm->set_transitions(initial,transition);
  delete [] transition;
  FSA_Simple * answer = FSA_Factory::product_intersection(*dm,*L1_acceptor,*wa,
                                           "primary recogniser",geodesic_reducer);

  dm->set_transitions(initial,save_transition);
  delete [] save_transition;
  return answer;
}

/**/

FSA_Simple *Group_Automata::build_L1_acceptor(Rewriter_Machine * rm,
                                              const FSA * wa,
                                              const FSA * gwa,
                                              bool test_only)
{
  /*
    This method constructs the fsa in which the states are pairs (s1,s2),
    with s1 and s2 states of the word_acceptor for the whole input and s2
    of its suffix. In the case where we are building a group L1_acceptor
    wa and gwa should be the same, but in the case of a coset L1_acceptor
    wa is the coset acceptor, and gwa the group word-acceptor.

    More precisely, if either word_acceptor has n states 1..n-1 and
    fail state, then s1 and s2 may also be n, for s1 n means that the last
    character is not accepted for the whole word. For s2 n is a replacement
    initial state used to ignore the first character.

    There is one accept state - the state in which s1 has reached state n
    and s2 had not reached failure, which we represent by (n,1)
  */
  if (!wa)
    return 0;
  const FSA & word_acceptor = *wa;
  const FSA & group_word_acceptor = *gwa;
  const Alphabet & alphabet = word_acceptor.base_alphabet;
  const State_ID reducible_state = word_acceptor.state_count();
  const State_ID suffix_state = group_word_acceptor.state_count();
  const Ordinal nr_generators = Ordinal(word_acceptor.alphabet_size());
  Container & container = word_acceptor.container;
  Ordinal_Word lhs_word(rm->alphabet(),1);
  Ordinal_Word rhs_word(rm->alphabet(),1);
  Strong_Diff_Reduce sdr(rm);
  State_ID key[2],old_key[2];

  key[0] = reducible_state+1;
  key[1] = suffix_state+1;
  Pair_Packer key_packer(key);
  State_ID *transition = new State_ID[nr_generators];
  Keyed_FSA factory(container,alphabet,nr_generators,
                    max(State_Count(reducible_state+suffix_state),
                        SUGGESTED_HASH_SIZE),
                    key_packer.key_size());
  Word_Length state_length = 0;
  State_ID ceiling = 1;
  void * packed_key;

  if (test_only)
    container.progress(1,"Using word-acceptor to look for primary equations\n");
  else
    container.progress(1,"Building L1 acceptor\n");

  /* Insert failure and initial states */
  key[0] = 0;
  key[1] = 0;
  factory.find_state(packed_key = key_packer.pack_key(key));

  key[0] = 1;
  key[1] = suffix_state;
  factory.find_state(key_packer.pack_key(key));

  State_ID state = 0;
  State_Count nr_states = 2;
  Transition_Realiser tr1(word_acceptor);
  Transition_Realiser &tr2 = gwa == wa ? tr1 : * new Transition_Realiser(group_word_acceptor);
  while (factory.get_state_key(packed_key,++state))
  {
    Word_Length length = state >= ceiling ? state_length : state_length - 1;
    key_packer.unpack_key(old_key);

    if (test_only && state >= 2)
      factory.defining_word(&lhs_word,state,length);

    if (!(char) state)
      container.status(2,1,"Building L1 acceptor state " FMT_ID " (" FMT_ID " of " FMT_ID " to do)."
                       " Depth %d\n",state,nr_states-state,nr_states,length);
    State_ID wa_lhs_state = old_key[0];
    State_ID wa_rhs_state = old_key[1];
    const State_ID * t1 = 0;
    const State_ID * t2 = 0;
    if (wa_lhs_state != reducible_state)
      t1 = tr1.realise_row(wa_lhs_state,0);
    if (wa_rhs_state != suffix_state)
      t2 = tr2.realise_row(wa_rhs_state,1);
    for (Ordinal g1 = 0; g1 < nr_generators;g1++)
    {
      if (wa_lhs_state == reducible_state)
        key[0] = 0;
      else
      {
        key[0] = t1[g1];
        if (!key[0])
          key[0] = reducible_state;
      }

      if (wa_rhs_state == suffix_state)
        key[1] = 1;
      else
        key[1] = t2[g1];

      if (key[0] && key[1])
      {
        if (key[0] == reducible_state)
        {
          key[1] = 1;
          if (test_only)
          {
            Word_Length l = length+1;
            lhs_word.set_length(l);
            lhs_word.set_code(length,g1);
            sdr.reduce(&rhs_word,lhs_word);
            rm->add_correction(lhs_word,rhs_word,true);
            lhs_word.set_length(length);
          }
        }

        transition[g1] = factory.find_state(key_packer.pack_key(key));
        if (transition[g1] >= nr_states)
        {
          if (state >= ceiling)
          {
            state_length++;
            ceiling = transition[g1];
          }
          if (test_only)
          {
            State_Definition definition;
            definition.state = state;
            definition.symbol_nr = g1;
            factory.set_definition(transition[g1],definition);
          }
          nr_states++;
        }
      }
      else
        transition[g1] = 0;
    }
    if (!test_only)
      factory.set_transitions(state,transition);
  }
  if (transition)
    delete [] transition;
  tr1.unrealise();
  if (&tr2 != &tr1)
    delete &tr2;
  if (!test_only)
  {
    /* Set the unique accept state */
    key[0] = reducible_state;
    key[1] = 1;
    factory.set_single_accepting(factory.find_state(key_packer.pack_key(key)));
    factory.remove_keys();
    return FSA_Factory::minimise(factory);
  }
  return 0;
}
