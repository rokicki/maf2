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


// $Log: mafgeowa.cpp $
// Revision 1.2  2010/06/10 13:57:31Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/17 10:32:34Z  Alun
// New file.
//

#include "container.h"
#include "mafword.h"
#include "keyedfsa.h"
#include "maf_cfp.h"
#include "maf_rm.h"
#include "maf_we.h"

static FSA_Simple * geodesic_pair_recogniser(const FSA & wa,
                                             const FSA & dm)
{
  /*
    This method constructs the fsa of which the states are doubles (d,s2),
    with s2 a state of wa and d of dm.
    The alphabet is the usual product alphabet as used by word-difference
    machines. However padding characters are not allowed at all.
    The alphabet member (g1,g2) maps (d,s2) to (d^(g1,g2),s2^g2)
    if both components are non-zero, and to zero otherwise.
    There is just one accept state - the state in which the words are of
    equal length and recognised as equal as elements. (We need a special dm
    to get this right since ordinary dms will not have all the extra
    differences between words which have a reducible prefix but are still the
    same length as a reduced word)
  */
  const Alphabet & base_alphabet = dm.base_alphabet;
  const Ordinal nr_generators = base_alphabet.letter_count();
  const int nr_transitions = base_alphabet.product_alphabet_size();
  Container & container = wa.container;
  const State_Count nr_differences = dm.state_count();
  State_ID key[2],old_key[2];
  enum Keys {DIFF,WA_RHS};
  key[DIFF] = nr_differences;
  key[WA_RHS] = wa.state_count();
  Pair_Packer key_packer(key);
  State_ID *transition = new State_ID[nr_transitions];
  Keyed_FSA factory(container,base_alphabet,nr_transitions,
                    max(State_Count(wa.state_count()*nr_transitions),
                        SUGGESTED_HASH_SIZE),
                    key_packer.key_size());
  void * packed_key;
  Word_Length state_length = 0;

  container.progress(1,"Building geodesic pair recogniser from difference"
                       " machine with " FMT_ID " states\n",nr_differences);

  /* Insert failure and initial state */
  key[0] = 0;
  key[1] = 0;

  factory.find_state(packed_key = key_packer.pack_key(key));
  key[DIFF] = dm.initial_state();
  key[WA_RHS] = wa.initial_state();
  factory.find_state(key_packer.pack_key(key));

  State_ID state = 0;
  State_ID nr_states = factory.state_count();
  State_ID ceiling = 1;
  memset(transition,0,sizeof(State_ID) * nr_transitions);
  while (factory.get_state_key(packed_key,++state))
  {
    Word_Length length = state >= ceiling ? state_length : state_length-1;
    key_packer.unpack_key(old_key);
    container.status(2,1,"Building pair recogniser state " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Length %d\n",
                     state,nr_states-state,nr_states,length);
    State_ID wa_rhs_state = old_key[WA_RHS];
    State_ID diff_state = old_key[DIFF];
    for (Ordinal g2 = 0; g2 < nr_generators;g2++)
    {
      key[WA_RHS] = wa.new_state(wa_rhs_state,g2);
      for (Ordinal g1 = 0; g1 < nr_generators;g1++)
      {
        Transition_ID product_id = base_alphabet.product_id(g1,g2);
        key[DIFF] = dm.new_state(diff_state,product_id);

        if (key[0] && key[1])
        {
          State_ID new_state = factory.find_state(key_packer.pack_key(key));
          transition[product_id] = new_state;
          if (new_state >= nr_states)
          {
            if (state >= ceiling)
            {
              state_length++;
              ceiling = new_state;
            }
            nr_states++;
          }
        }
        else
          transition[product_id] = 0;
      }
    }
    factory.set_transitions(state,transition);
  }
  /* Set the accept states - we don't bother with labels */
  state = 0;
  factory.clear_accepting(true);
  while (factory.get_state_key(packed_key,++state))
  {
    key_packer.unpack_key(old_key);
    if (dm.is_accepting(old_key[DIFF]))
      factory.set_is_accepting(state,true);
  }
  if (transition)
    delete [] transition;
  factory.tidy();
  factory.remove_keys();
  return FSA_Factory::minimise(factory);
}

/**/

static int check_geodesic_word_acceptor(Sorted_Word_List * words_to_learn,
                                        Rewriter_Machine * rm,
                                        const FSA &difference_machine,
                                        const FSA &word_acceptor)
{
  /* This method performs the same checks that would be accomplished
     by first building and then validating a multiplier which allows
     geodesic words on the left hand side. It is modelled on
     validate_difference_machine()

     The purpose of this function is to be able to find missing geodesic word
     differences.

     The method works by recreating the geodesic word-acceptor, but with
     states that consist of subsets of the power set of dm * wa - where
     the subset for each state corresponds to pairs (d1,w1) with d1 a state
     of dm, and w1 a state of wa (a real word-acceptor - not a geodesic one).
     The initial state consists of the set {(1,1)}, and for a
     state s={(d1,w1),(d2,w2),...} the state s^g1 consists of all the pairs
     (dn^(g1,g2),wn^g2) where g2 ranges over the generators and the padding
     symbols, and for which neither state in the pair is in the failure state.
     In other words for each accepted word u we are forming the set of word
     differences that can be reached with v an accepted word.

     word-acceptor state values:
     1..n-1 normal
     n      padding symbol read one or more times on right.

     If the u word is geodesic then we should reach the pair (1,s) with
     s != n. If the word is not geodesic then we should reach it with s == n.
     If we do not reach it at all then the status of the word is unknown and
     there is a missing geodesic word-difference.

     The amount of information per state is very large for this test,
     especially for finite groups with a lot of word-differences. However it
     is practical, if slower, for larger examples than the KBMAG multiplier
     like method.
     If the accepted language of the geodesic acceptor is not too large it
     would probably be quicker just to enumerate a selection of words from it,
     because the amount of checking saved by equality of states is minimal,
     and the packing and unpacking of the state information is expensive.
  */
  State_Count nr_differences = difference_machine.state_count();
  State_Count nr_wa_states = word_acceptor.state_count();

  Sparse_Function_Packer key_packer(nr_differences,nr_wa_states+1,1);
  void * packed_key = (char *) key_packer.get_buffer();
  const Alphabet & alphabet = difference_machine.base_alphabet;
  const Ordinal nr_generators = alphabet.letter_count();
  Ordinal g1,g2;
  Transition_ID product_id;
  Container & container = difference_machine.container;
  Keyed_FSA factory(container,alphabet,nr_generators,nr_wa_states,0);
  State_ID state = 0;
  State_ID *key = new State_ID[nr_differences];
  State_ID *old_key = new State_ID[nr_differences];
  State_ID dsi,ndsi,nwsi;
  State_ID ceiling = 1;
  Word_Length state_length = 0;
  Ordinal_Word lhs_word(alphabet,1);
  Ordinal_Word rhs_word(alphabet,1);
  State_Count corrections = 0;
  State_Count errors = 0;
  bool aborted = false;
  int running = 0;
  Strong_Diff_Reduce sdr(rm);

  /* Insert and skip failure state */
  factory.find_state(packed_key,0);
  /* Insert initial state */
  memset(key,0,nr_differences*sizeof(State_ID));
  key[1] = 1;
  size_t size = key_packer.pack(key);
  size_t tot_size = size;
  factory.find_state(packed_key,size);
  State_Count count = 2;
  int count_down = 10;

  while (factory.get_state_key(packed_key,++state))
  {
    Word_Length length = state >= ceiling ? state_length : state_length - 1;
    if (container.status(2,1,"Geodesic acceptor checks: state " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Word Length %u.\n"
                           "Errors:" FMT_ID " Corrections:" FMT_ID ". Timeout %d\n",
                     state,count-state,count,length+1,errors,corrections,count_down))
    {
      running++;
      if (corrections)
        count_down--;
      else if (running > count_down)
        count_down = running;
      if (count_down < 0)
      {
        if (aborted)
          break;
        count_down = 20;
        aborted = true;
      }
    }
    key_packer.unpack(old_key);

    factory.defining_word(&lhs_word,state,length);

    for (g1 = 0; g1 < nr_generators;g1++)
    {
      memset(key,0,nr_differences*sizeof(State_ID));
      bool no_transition = false;

      for (dsi = 1; dsi < nr_differences;dsi++)
      {
        if (old_key[dsi])
        {
          ndsi = difference_machine.new_state(dsi,alphabet.product_id(g1,PADDING_SYMBOL));
          if (ndsi)
          {
            key[ndsi] = nr_wa_states;

            if (ndsi == 1)
            {
              no_transition = true;
              break;
            }
          }
          if (old_key[dsi] < nr_wa_states)
          {
            for (product_id = alphabet.product_base(g1),g2 = 0;
                 g2 < nr_generators;g2++,product_id++)
            {
              nwsi = word_acceptor.new_state(old_key[dsi],g2);
              if (nwsi)
              {
                ndsi = difference_machine.new_state(dsi,product_id);
                if (ndsi && !key[ndsi])
                  key[ndsi] = nwsi;
              }
            }
          }
        }
      }

      if (key[1] == 0)
      {
        /* We have found a word we can neither reject nor accept.
           It is therefore certainly reducible, but we don't know
           yet if it is geodesic or not. */
        Word_Length l = length+1;
        lhs_word.set_length(l);
        lhs_word.set_code(length,g1);
        sdr.reduce(&rhs_word,lhs_word,0);
        if (rhs_word.length() == lhs_word.length())
        {
          errors++; // it is geodesic
          /* we are not going to create bad states, as this prolongs the
             checking a lot. gpgeowa will have another go with new words
          */
          words_to_learn->insert(lhs_word);
        }
        else
          no_transition = true;
        Working_Equation we(*rm,lhs_word,rhs_word,Derivation(BDT_Diff_Reduction));
        if (rm->learn_differences(we))
        {
          rm->update_machine();
          corrections++;
          if (rm->is_dm_changing(false) && count_down < 60 && !no_transition)
            count_down += 10;
        }
        lhs_word.set_length(length);
      }

      if (!no_transition && !aborted && key[1])
      {
        size = key_packer.pack(key);
        State_ID nstate = factory.find_state(packed_key,size);
        if (nstate >= count)
        {
          tot_size += size;
          if (state >= ceiling)
          {
            state_length++;
            ceiling = nstate;
          }
          State_Definition definition;
          definition.state = state;
          definition.symbol_nr = g1;
          factory.set_definition(nstate,definition);
          count++;
        }
      }
    }
  }
  delete [] key;
  delete [] old_key;
  if (errors)
  {
    if (!corrections)
      return -2;
    if (aborted)
      return -1;
    return 0;
  }
  return 1;
}

/**/

FSA_Simple *MAF::labelled_product_fsa(const FSA &fsa,bool accept_only) const
{
  /* The states of any product FSA which represents equations between
     words have an associated set of word-differences. In most cases MAF
     creates this set in the first place and uses it to label states
     correctly (a noticeable exception being for multiplier FSAs where only
     initial and accepting states are labelled). This method allows missing
     labels to be reconstructed. Currently it is only needed for geodesic
     word functionality which constructs initially unlabelled FSAs and
     for gpdifflabs

     This version of the code builds a new FSA in case the current states
     cannot be labelled consistently, something that can happen when the
     input fsa is an unlabelled general multiplier. So we have to build
     2 FSAs at once here - the word-difference machine we need to label
     the states, and the FSA with the the correct state set.

     This code won't work for completely unlabelled MIDFAs since we
     have no idea what the label on the other initial states should be.

     This version of the code will blow up if someone is foolish enough to
     call it with a two-variable FSA which is not some kind of word-difference
     machine, or multiplier since the factories may not close at all.
  */
  Transition_ID nr_transitions = fsa.base_alphabet.product_alphabet_size();
  if (fsa.alphabet_size() != nr_transitions)
  {
    container.error_output("Input FSA should be 2-variable\n");
    return 0;
  }
  const Alphabet & alphabet = fsa.base_alphabet;
  State_Count nr_states = fsa.state_count();
  Ordinal_Word lhs_word(alphabet);
  Ordinal_Word label_word(alphabet);

  State_ID key[2],old_key[2];
  key[0] = nr_states;
  key[1] = MAX_STATES; /* it is not clear what the final number of states
                          in the label FSA will be so we can only pack
                          the state number from the starting FSA */
  Pair_Packer key_packer(key);

  Keyed_FSA labels(container,alphabet,
                   alphabet.product_alphabet_size(),nr_states,0,false);
  Keyed_FSA new_fsa(container,alphabet,
                    alphabet.product_alphabet_size(),nr_states,key_packer.key_size());

  /* Insert failure states */
  void * packed_key;
  labels.find_state(0,0);
  key[0] = 0;
  key[1] = 0;
  new_fsa.find_state(packed_key = key_packer.pack_key(key));

  /* Insert initial states */
  size_t size;
  if (fsa.has_multiple_initial_states())
  {
    /* let's try and make gpdifflabs work on a cos.migm machine at least
       since we might like a fully labelled version of it */
    State_Subset_Iterator ssi;
    for (State_ID state = fsa.initial_state(ssi,true);
         state;
         state = fsa.initial_state(ssi,false))
    {
      Label_ID label = fsa.get_label_nr(state);
      if (!label)
      {
        container.error_output("Unable to determine label for unlabelled"
                               " initial states\n");
        return 0;
      }
      /* any kind of MIDFA with labelled initial states puts G-word
         labels first */
      fsa.label_word(&label_word,label);
      reduce(&label_word,label_word);
      Packed_Word pw(label_word,&size);
      key[0] = state;
      key[1] = labels.find_state(pw,size);
      new_fsa.find_state(key_packer.pack_key(key));
    }
  }
  else
  {
    Packed_Word pw(lhs_word,&size);
    key[0] = fsa.initial_state();
    key[1] = labels.find_state(pw,size);
    new_fsa.find_state(key_packer.pack_key(key));
  }
  Label_Count nr_initial_labels = labels.state_count();


  /* now create the rest of the new states */
  State_ID *transition = new State_ID[nr_transitions];
  State_ID ceiling = 1;
  State_ID state = 0;
  Word_Length state_length = 0;
  nr_states = new_fsa.state_count();
  Transition_Realiser tr1(fsa);
  while (new_fsa.get_state_key(packed_key,++state))
  {
    Word_Length length = state >= ceiling ? state_length : state_length - 1;
    key_packer.unpack_key(old_key);

    if (!(char) state)
      container.status(2,1,"Building labelled product FSA " FMT_ID " (" FMT_ID " of " FMT_ID " to do)."
                       " Length %d\n",state,nr_states-state,nr_states,length);
    const State_ID * t1 = tr1.realise_row(old_key[0],0);
    for (Transition_ID ti = 0; ti < nr_transitions;ti++)
    {
      key[0] = t1[ti];
      if (key[0])
      {
        key[1] = labels.new_state(old_key[1],ti,true);
        if (!key[1])
        {
          /* there is no word difference here yet, but we need it, so
             we create it */
          Ordinal g1,g2;
          alphabet.product_generators(&g1,&g2,ti);
          label_word.unpack((const Byte *)labels.get_state_key(old_key[1]));
          if (g1 != PADDING_SYMBOL)
          {
            lhs_word.set_length(1);
            lhs_word.set_code(0,inverse(g1));
            label_word = lhs_word + label_word;
          }

          if (g2 != PADDING_SYMBOL)
            label_word.append(g2);
          reduce(&label_word,label_word);
          Packed_Word pw(label_word,&size);
          key[1] = labels.find_state(pw,size);
          labels.set_transition(old_key[1],ti,key[1]);
        }

        transition[ti] = new_fsa.find_state(key_packer.pack_key(key));
        if (transition[ti] >= nr_states)
        {
          if (state >= ceiling)
          {
            state_length++;
            ceiling = transition[ti];
          }
          nr_states++;
        }
      }
      else
        transition[ti] = 0;
    }
    new_fsa.set_transitions(state,transition);
  }
  delete [] transition;

  /* preserve the label_type and initial labels if the old FSA had some labels
     otherwise we use word labels and set the initial label */
  bool keep_initial_labels = fsa.has_multiple_initial_states();
  if (fsa.label_type() == LT_List_Of_Words && keep_initial_labels)
  {
    new_fsa.set_label_type(LT_List_Of_Words);
    new_fsa.set_label_alphabet(fsa.label_alphabet());
  }
  else
  {
    new_fsa.set_label_type(LT_Words);
    new_fsa.set_label_alphabet(alphabet);
  }

  Label_Count nr_labels = labels.state_count();
  Label_ID label_id;
  new_fsa.set_nr_labels(nr_labels);
  if (keep_initial_labels)
  {
    Word_List label_wl(new_fsa.label_alphabet());
    for (label_id = 1;label_id < nr_initial_labels;label_id++)
    {
      fsa.label_word_list(&label_wl,label_id);
      new_fsa.set_label_word_list(label_id,label_wl);
      new_fsa.set_is_initial(label_id,true); //the initial labels match initial states
    }
  }
  else
  {
    label_word.set_length(0);
    new_fsa.set_label_word(1,label_word);
  }

  /* now examine the remaining states of the label fsa to create any new labels */
  const void * packed_label;
  for (label_id = nr_initial_labels; label_id < nr_labels;label_id++)
    if ((packed_label = labels.get_state_key(label_id))!=0)
    {
      container.status(2,1,"Creating labels:label " FMT_ID " (" FMT_ID " of " FMT_ID " to do)\n",
                     label_id,nr_labels-label_id,nr_labels);
      label_word.unpack((const Byte *)packed_label);
      new_fsa.set_label_word(label_id,label_word);
    }

  /* now set the labels and accept states in the new FSA by examining the
     new states */
  new_fsa.clear_accepting();
  state = 0;
  while (new_fsa.get_state_key(packed_key,++state))
  {
    key_packer.unpack_key(old_key);
    container.status(2,1,"Labelling state:state " FMT_ID " (" FMT_ID " of " FMT_ID " to do)\n",
                     state,nr_states-state,nr_states);
    bool accepted = fsa.is_accepting(old_key[0]);
    if (accepted)
      new_fsa.set_is_accepting(state,true);
    if (!accept_only || accepted)
      new_fsa.set_label_nr(state,old_key[1]);
  }
  new_fsa.change_flags((fsa.get_flags() & GFF_TRIM+GFF_MIDFA) | GFF_ACCESSIBLE | GFF_BFS,0);

  if (accept_only) // not sure about this - copied from old code - probably needed
    new_fsa.sort_labels();
  return FSA_Factory::minimise(new_fsa);
}

/**/

class Geodesic_Learner
{
  public:
    MAF & maf;
    Rewriter_Machine &rm;
    Container & container;
    const Ordinal nr_generators;
    Sorted_Word_List learn;
  private:
    int time_out;
    int errors;
  public:
    Geodesic_Learner(MAF & maf_) :
      maf(maf_),
      learn(maf_.alphabet),
      nr_generators(maf_.generator_count()),
      rm(maf_.rewriter_machine()),
      container(maf_.container),
      time_out(20),
      errors(0)
    {}

    Element_Count count() const
    {
      return learn.count();
    }
    int error_count() const
    {
      return errors;
    }
    void learn_all(const FSA & wa)
    {
      Element_Count count;
      for (Element_ID word_nr = 0; word_nr < (count = learn.count()); word_nr++)
      {
        if (container.status(2,1,"Learning new geodesic pair number " FMT_ID "."
                             " (" FMT_ID " of " FMT_ID " to do),time_out %d\n",word_nr,
                             count-word_nr,count,time_out) && --time_out <= 0)
          break;
        learn_now(*learn.word(word_nr),true,wa,0);
      }
      learn.empty();
      errors = 0;
      time_out = 20;
    }

    void learn_later(const Word & word)
    {
      if (learn.count() < 2000)
        learn.insert(word);
    }

    bool test(const Word & word,State_ID si,const FSA & wa,bool test_parent)
    {
      bool retcode = false;
      bool valid = true;
      Ordinal_Word lhs_word(word);
      Ordinal_Word rhs_word(maf.alphabet);
      Word_Length l = lhs_word.length();

      /* In general when this code is called the word itself is already
         known to be geodesic, and the equation for it has been found.
         However, this is not the case when the code is called for dealing
         with a non prefix-closed acceptor, or when testing dubious words*/
      if (si && !wa.is_accepting(si) || test_parent)
      {
        learn.remove(lhs_word);
        maf.reduce(&rhs_word,lhs_word);
        if (l != rhs_word.length())
          if (si)
          {
            String_Buffer sb;
            MAF_INTERNAL_ERROR(container,
                     ("Unexpected reduction found for alleged geodesic word %s\n",
                      lhs_word.format(&sb).string()));
          }
          else
          {
            valid = false;
            errors++;
          }

        Working_Equation we(rm,lhs_word,rhs_word,Derivation(BDT_Unspecified));
        if (rm.learn_differences(we))
        {
          retcode = valid;
          if (time_out < 60 && valid)
            time_out += 10;
        }
      }

      if (valid)
      {
        Ordinal irvalue = l ? maf.inverse(lhs_word.value(l-1)) : PADDING_SYMBOL;
        for (Ordinal g = 0 ;g < nr_generators;g++)
        {
          if (g != irvalue && (si == 0 || !wa.new_state(si,g)))
          {
            lhs_word.set_length(l+1);
            lhs_word.set_code(l,g);
            maf.reduce(&rhs_word,lhs_word);
            if (rhs_word.length() == l+1 && !wa.read_word(lhs_word))
            {
              errors++;
              learn_later(lhs_word);
            }
            Working_Equation we(rm,lhs_word,rhs_word,Derivation(BDT_Unspecified));
            if (rm.learn_differences(we))
            {
              retcode = true;
              if (time_out < 60)
                time_out += 10;
            }
          }
        }
      }
      return retcode;
    }

    bool learn_now(const Word & word,bool force,const FSA & wa,State_ID si)
    {
      bool retcode = false;
      if (force || !learn.find(word))
        retcode = test(word,si,wa,si != 0);
      return retcode;
    }
};

/**/

class Geodesic_Automata_Factory
{
  public:
    Geodesic_Automata_Factory(MAF & maf,
                               bool want_multiplier,
                               unsigned fsa_format_flags)
    {
      FSA_Simple * geodesic_pairs = 0;
      FSA_Simple * geodesic_wa = 0;
      Container &container = maf.container;

      maf.import_difference_machine(*maf.fsas.diff2);
      Geodesic_Learner gl(maf);

      for (;;)
      {
        FSA * tdiff = gl.rm.grow_wd(CD_STABILISE|CD_KEEP,0);
        geodesic_pairs = geodesic_pair_recogniser(*maf.fsas.wa,*tdiff);
        // until correct geodesic_wa does not accept enough words
        container.progress(1,"Geodesic pair recogniser has " FMT_ID " states\n",
                           geodesic_pairs->state_count());
        geodesic_wa = FSA_Factory::exists(*geodesic_pairs,false,false);
        container.progress(1,"Geodesic word-acceptor has " FMT_ID " states\n",
                           geodesic_wa->state_count());
        bool ok = true;
        if (geodesic_wa->accept_type() != SSF_All)
        {
          container.progress(1,"The geodesic word-acceptor is not prefix-closed,"
                               " so it is incorrect.\n");
          geodesic_wa->create_definitions();
          State_Count nr_states = geodesic_wa->state_count();
          State_ID si;
          Ordinal_Word ow(geodesic_wa->base_alphabet);
          for (si = 1; si < nr_states;si++)
            if (!geodesic_wa->is_accepting(si))
            {
              container.status(2,1,"Checking for rejected prefixes."
                               " (" FMT_ID " of " FMT_ID " to do)\n",
                               nr_states-si,nr_states);
              geodesic_wa->defining_word(&ow,si);
              gl.test(ow,si,*geodesic_wa,true);
            }
          ok = false;
          geodesic_wa->set_accept_all();
        }

        FSA_Simple * outer_geodesic_wa = maf.build_acceptor_from_dm(tdiff,false,true);
        FSA_Simple * dubious_words = FSA_Factory::and_not_first(*outer_geodesic_wa,
                                                                *geodesic_wa);
        delete outer_geodesic_wa;
        bool uncertain = true;
        if (dubious_words->language_size(false)==0)
        {
          container.progress(1,"The inner and outer geodesic word-acceptors,"
                               " agree so both are correct.\n");
          uncertain = false;
        }
        else
        {
          dubious_words->create_definitions(true);
          dubious_words->create_accept_definitions();
          FSA::Principal_Value_Cache cache(*dubious_words);
          State_Count nr_states = dubious_words->state_count();
          State_ID si;
          Ordinal_Word ow(dubious_words->base_alphabet);
          int time_out = 20;
          for (si = 1; si < nr_states/2;si++)
          {
            if (dubious_words->principal_value(&cache,&ow,si))
            {
              if (container.status(2,1,"Checking boundary."
                             " (" FMT_ID " of " FMT_ID " to do).Timeout %d\n",nr_states/2-si,
                               nr_states,time_out) && --time_out <= 0)
                break;
              if (gl.test(ow,0,*geodesic_wa,false) && time_out < 60)
                time_out += 10;
            }
            if (dubious_words->principal_value(&cache,&ow,nr_states-si))
            {
              if (gl.test(ow,0,*geodesic_wa,false) && time_out < 60)
                time_out += 10;
            }
          }
          if (gl.error_count() != 0)
            ok = false;
        }
        delete dubious_words;

        if (uncertain)
        {
          if (ok)
            container.progress(1,"The geodesic word-acceptor is prefix-closed,"
                                 " and will now be checked.\n");
          if (gl.error_count() < 10)
            ok = check_geodesic_word_acceptor(&gl.learn,&gl.rm,*tdiff,
                                              *maf.fsas.wa) &&
                 gl.error_count() == 0;
        }

        delete tdiff;
        if (ok)
          break;
        delete geodesic_pairs;
        gl.learn_all(*geodesic_wa);
        delete geodesic_wa;
      }

      maf.save_fsa(geodesic_wa,".geowa",fsa_format_flags);
      delete geodesic_wa;

      /* current geodesic_pairs accepts pairs in which w2 is reduced.
         we want it to accept all geodesic pairs. We can form this FSA
         by composing the FSA with its transpose */
      FSA_Simple * gp_transpose = FSA_Factory::transpose(*geodesic_pairs);
      FSA_Simple * gp_full = FSA_Factory::composite(*geodesic_pairs,*gp_transpose);
      maf.save_fsa(gp_full,".geopairs",fsa_format_flags);
      FSA_Simple * labelled_full = maf.labelled_product_fsa(*gp_full);
      delete gp_full;
      FSA_Simple * geodiff = FSA_Factory::merge(*labelled_full);
      delete labelled_full;
      maf.save_fsa(geodiff,".geodiff",fsa_format_flags);
      delete geodiff;

      if (want_multiplier)
      {
        /* We also want the near geodesic pair machine which accepts equal
           words in which the longer word is no more than one longer than
           the accepted word. We form this from geopairs*gm*gp_transpose.
           If we label this correctly we have the multiplier for the geodesic
           automatic structure */
        maf.load_fsas(GA_GM);
        FSA_Simple * gp_near = FSA_Factory::composite(*geodesic_pairs,*maf.fsas.gm);
        FSA_Simple * gp_temp = FSA_Factory::composite(*gp_near,*gp_transpose);
        delete gp_near;
        gp_near = maf.labelled_product_fsa(*gp_temp,true);
        delete gp_temp;
        /* we label the accept states to turn this into the geodesic multiplier*/
        maf.save_fsa(gp_near,".near_geopairs",fsa_format_flags);
        gp_temp = maf.labelled_product_fsa(*gp_near);
        delete gp_near;
        FSA_Simple * near_geodiff = FSA_Factory::merge(*gp_temp);
        delete gp_temp;
        maf.save_fsa(near_geodiff,".near_geodiff",fsa_format_flags);
        delete near_geodiff;
      }
      delete gp_transpose;
      delete geodesic_pairs;
    }
};

void MAF::grow_geodesic_automata(bool near_machine,
                                 unsigned fsa_format_flags)
{
  Geodesic_Automata_Factory gaf(*this,near_machine,fsa_format_flags);
}
