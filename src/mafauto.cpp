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


// $Log: mafauto.cpp $
// Revision 1.23  2011/05/20 10:16:50Z  Alun
// detabbed
// Revision 1.22  2010/07/01 18:27:18Z  Alun
// Behaved badly with finite index subgroups when detect_finite_index was not on
// Revision 1.21  2010/06/18 00:32:20Z  Alun
// provisional automata were no longer handled correctly
// Revision 1.20  2010/06/10 13:57:28Z  Alun
// All tabs removed again
// Revision 1.19  2010/05/17 10:29:37Z  Alun
// Many methods formerly in this module moved elsewhere
// Now constructs coset table if possible, and in this case
// subgroup word-acceptor is computed without using general multiplier to
// validate it.
// Revision 1.18  2009/11/11 00:16:03Z  Alun
// Copes with failure to correct multiplier because corrections were made in
// prior call to validate_dm
// Revision 1.17  2009/10/13 23:29:09Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSAs KBMAG would not (if any)
// Revision 1.16  2009/09/16 07:42:14Z  Alun
// Improved use of weak word-acceptor - now will do process_mistakes() using a
// later strong word-acceptor
// Revision 1.15  2009/09/14 09:57:59Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Language_Size data type introduced and used where appropriate
// Revision 1.14  2009/08/23 19:20:29Z  Alun
// Initial states of the word-difference machine are now inserted into a
// database. I can't remember now why I did this, but it is probably to improve
// performance when there are a very large number of initial states.
// Revision 1.17  2008/11/12 02:00:41Z  Alun
// The validation checks on the multiplier have to check the initial state.
// It can go wrong for coset multipliers when generators have been eliminated.
// The labels for eliminated generators were not always correct either.
// Revision 1.16  2008/11/05 02:26:49Z  Alun
// Less likely to keep using dm1 when acceptor is stable
// Revision 1.15  2008/11/02 21:17:41Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.14  2008/10/13 10:17:45Z  Alun
// Various corrections to comments, unnecessary call to explore_acceptor()
// removed
// Revision 1.13  2008/10/09 17:05:41Z  Alun
// printf() argument correctness improved
// various optimisations for geodesic orders
// Revision 1.12  2008/09/29 20:39:31Z  Alun
// Switch to using Hash+Collection_Manager as replacement for Indexer.
// Currently this is about 10% slower, but this is a more flexible
// architecture.
// Revision 1.6  2007/12/20 23:25:42Z  Alun
//

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include "awdefs.h"
#include "fsa.h"
#include "maf_rm.h"
#include "mafnode.h"
#include "keyedfsa.h"
#include "mafauto.h"
#include "container.h"
#include "maf_dr.h"
#include "maf_rws.h"
#include "maf_cfp.h" // Key packers
#include "maf_el.h"
#include "maf_spl.h"
#include "maf_we.h"
#include "maf_wdb.h"
#include "maf_ss.h"

/* This module contains code for building and checking word_acceptor and
multiplier FSAs for automatic groups and coset systems. The algorithms used are
based on those described in the paper "The Warwick Automatic Groups software"
by Derek F Holt. This implementation is very loosely based on source code from
the KBMAG package.

All the FSAs are built using the Keyed_FSA class. The FSAs we build can have
very many states prior to mininisation, and in the case of acceptor FSAs the
key that identifies a state can also be very big, so we use various
"key packers" to reduce the size of the keys using data compression.
*/

FSA_Simple * MAF::tidy_difference_machine(FSA_Simple *old_diff) const
{
  /* This code used to build a copy of a word-difference machine, mostly
     because Difference_Tracker did not build an FSA_Simple FSA. Now it
     does, so this routine now modifies the input FSA rather than building
     a new one. For large finite groups this can save a lot of memory.
     However, a new machine will be built if the label type needs to be
     changed for KBMAG compatibility.

     On entry dm is a perfectly good word-difference machine
     already, (usually it will have been built using a Difference_Tracker).

     All this code does is to sort its states so that they appear
     in label word order (but with all initial states first).
     This is done to make the output files for the FSA more readable.
     It also makes it slightly easier to compare difference machines,
     particularly the diff2 and diff2c machines, which will have the same
     states and only differ in their transitions.
     (To compare the primary diff1c and a diff2 machine it would be
     desirable to order the states so that all the primary differences came
     before the others.)
  */

  const Transition_ID nr_symbols = old_diff->alphabet_size();
  State_ID * transition = new State_ID[nr_symbols];
  const State_Count nr_differences = old_diff->state_count();
  State_ID *sort = new State_ID[nr_differences];
  Container & container = old_diff->container;
  const Alphabet & alphabet = old_diff->base_alphabet;
  sort[0] = 0;
  bool is_midfa = old_diff->has_multiple_initial_states();
  Ordinal_Word label(alphabet);
  Ordinal_Word other_label(alphabet);
  Word_List real_label(alphabet);
  Label_Type label_type = options.emulate_kbmag ? LT_Words : old_diff->label_type();
  bool use_word_labels = label_type == LT_Words;
  FSA_Simple * dm;
  bool copying = false;
  if (options.emulate_kbmag &&
      (label_type != old_diff->label_type() ||
       alphabet != old_diff->label_alphabet()))
  {
    dm = new FSA_Simple(container,alphabet,nr_differences,nr_symbols);
    dm->set_label_type(label_type);
    dm->set_label_alphabet(alphabet);
    dm->set_nr_labels(nr_differences,LA_Direct);
    copying = true;
  }
  else
    dm = old_diff;

  for (State_ID si = 1; si < nr_differences;si++)
  {
    if (!(char) si)
      container.status(2,1,"Sorting word-differences. (" FMT_ID " of " FMT_ID ")\n",si,nr_differences);
    if (copying)
    {
      old_diff->get_transitions(transition,si);
      dm->set_transitions(si,transition);
    }
    old_diff->label_word(&label,si);
    if (copying)
    {
      if (use_word_labels)
        dm->set_label_word(si,label);
      else
      {
        old_diff->label_word_list(&real_label,si);
        dm->set_label_word_list(si,real_label);
      }
    }
    State_ID low = 1;
    State_ID high = si;
    if (is_midfa)
    {
      while (low < high)
      {
        State_ID mid = low + (high-low)/2;
        old_diff->label_word(&other_label,sort[mid]);
        if (old_diff->is_initial(si) < old_diff->is_initial(sort[mid]) ||
            old_diff->is_initial(si) == old_diff->is_initial(sort[mid]) &&
            label.compare(other_label) > 0)
          low = mid+1;
        else
          high = mid;
      }
    }
    else
    {
      while (low < high)
      {
        State_ID mid = low + (high-low)/2;
        old_diff->label_word(&other_label,sort[mid]);
        if (label.compare(other_label) > 0)
          low = mid+1;
        else
          high = mid;
      }
    }
    memmove(sort+low+1,sort+low,(si-low)*sizeof(State_ID));
    sort[low] = si;
    if (copying && is_midfa)
      dm->set_is_initial(si,old_diff->is_initial(si));
  }
  delete [] transition;
  if (copying)
    delete old_diff;
  dm->set_single_accepting(1);
  dm->sort_states(sort);
  dm->change_flags(GFF_TRIM|GFF_ACCESSIBLE);
  delete [] sort;
  return dm;
}

/**/

FSA_Simple * MAF::polish_difference_machine(FSA *dm,bool delete_old)
{
  FSA_Simple * wa = build_acceptor_from_dm(dm,false,false,false,2,0);
  FSA_Simple * L2_acceptor = FSA_Factory::fsa_not(*wa,true);
  L2_acceptor->set_accept_all();
  bool geodesic_reducer = dm->base_alphabet.order_is_geodesic() ||
                          dm->base_alphabet.order_is_effectively_shortlex();
  FSA_Simple * improver = FSA_Factory::product_intersection(*dm,*L2_acceptor,*wa,
                                                            "improver",geodesic_reducer);
  if (delete_old)
    delete dm;
  delete wa;
  delete L2_acceptor;
  FSA_Simple * better_diff = FSA_Factory::merge(*improver);
  delete improver;
  return tidy_difference_machine(better_diff);
}

/**/

static inline bool better_compare_state(char new_shortlex_state,
                                        char old_shortlex_state)
{
  /*  This is a utility function for build_shortlex_wa_from_dm()
      See below for why we need it.
      Here is the explanation of its rather mysterious looking test.

   we have coded up the truth table here which looked like this:

   os 0 1 2 3 (NB here 1 = lhs_better,2=rhs_better,3=lhs=rhs)
 ns
  0   0000
  1   1000
  2   1101
  3   1100
  i.e.  0000100011011100
     but we need to turn this round since bits go right to left, so we get
       0011101100010000 == 0x3b10
  */

  return (0x3b10 >> (new_shortlex_state*4+old_shortlex_state)) & 1;
}

/**/

static FSA_Simple * build_shortlex_wa_from_dm(MAF & maf,
                                              const FSA *fsa_wd,
                                              bool create_equations,
                                              bool geodesic,
                                              bool force_group_acceptor,
                                              Word_Length max_context,
                                              State_Count max_states)
{
  /* This method is called to implement the construction of the word-acceptor
     of a shortlex automatic group, or the coset word-acceptor for a shortlex
     automatic coset system. This is an important special case, since it is
     one of the only orderings which has a finite greater than automaton.
     The method also works for "right shortlex" ordering, for which only
     a very minor change is required, and works in the "geodesic" case for
     any of the orderings that respect word length.

     The general principle of this method is described more fully in
     MAF::build_acceptor_from_dm() below.

     The states of the fsa that we are calculating consist of subsets of the
     Cartesian product of the states of fsa_wd and fsa_gt. The latter has 4
     states, so the elements of these subsets are technically pairs of the
     form (s,i), where s is a state of the fsa_wd, and 1<=i<=4.
     Here i==1 means lhs==rhs, i==2 means lhs > rhs with equal lengths, i==3
     means rhs>lhs with equal lengths, and i==4 means lhs is longer than rhs.
     When building an ordinary word-acceptor i==1 never occurs except in
     the pair (1,1), which is always present. If (s,2) and (s,3) both occur
     together, then we can omit (s,3), since any path from (s,3) to failure
     will also be a path from (s,2) to failure. Similarly (s,4) never occurs
     with either (s,2) or (s,3), and in fact can be regarded as (s,2) since
     we never read a padding symbol on the left.(KBMAG won't extend states
     (s,4) except with padding symbol but MAF is happy to do so for reasons
     explained in Diff_Reduce::reduce()). In addition we do not include
     state (1,3), since in that case we have a worse word.

     The coset acceptor is almost the same as an ordinary acceptor, except
     that we do now have to worry about (n,1) states. This is because we
     are building up an equation for the form u==hv rather than u==v, so
     in general we won't stay in the initial state when we read the same
     letter on both sides, since in general Ghg != h. So we do have to
     think about the possibility of (s,1) and (s,n) for n!=2. Analogous to
     our arguments above we can see that (s,1) is better than (s,3)
     but worse than (s,2). So we only ever need to remember at most one
     state of fsa_gt for each state of fsa_wd.

     As we build up the subset that represents a state of the word-acceptor,
     we form a key that represents this information.
     For d a state number of fsa_wd,key[d] indicates which if any pair (d,g)
     of fsa_wd*fsa_gt is present in the subset. We do not use the values
     1,2,3,4 but 3,2,1,2 as this makes it easier for the state to be packed,
     and we don't need to keep 2 and 4 separate. In the ordinary case this
     numbering makes it very easy to test whether or not to record a new
     pair (d,ng) when we already  have (d,og) (with og possibly 0 meaning not
     present). In the coset case the better_compare() function decides this.

     If geodesic is true, we don't want to reject words in which the reduced
     word is the same length. In this case all we need to do is to start
     fsa_gt from state 3, so only reductions that shorten the word will be
     recognised. This results in the construction of an "outer" geodesic
     word-acceptor: all geodesic words are accepted, but some non geodesic
     words will usually be accepted as well, because we will not have
     computed enough word-differences.
  */
  const char SEEN_LHS_BETTER = 1;
  const char SEEN_RHS_BETTER = 2;
  const char SEEN_EQUAL = 3;
  State_Count nr_differences = fsa_wd->state_count();
  Container &container = fsa_wd->container;
  const Alphabet & alphabet = fsa_wd->base_alphabet;
  Word_Length state_length = 0;
  Rewriter_Machine * rm = create_equations ? &maf.rewriter_machine() : 0;
  Strong_Diff_Reduce sdr(rm);
  bool want_coset_acceptor = fsa_wd->has_multiple_initial_states() &&
                             !force_group_acceptor;
  bool need_equal = want_coset_acceptor && !geodesic;
  Compared_Subset_Packer key_packer(nr_differences,2,need_equal);
  void * packed_key = key_packer.get_buffer();
  char *key = new char[nr_differences];
  State_Pair_List old_key;
  Ordinal nr_generators = alphabet.letter_count();
  Keyed_FSA factory(container,alphabet,nr_generators,
                    nr_differences*nr_generators*2,0);
  State_ID identity = fsa_wd->accepting_state();
  State_ID * transition = new State_ID[nr_generators];
  Ordinal_Word lhs_word(alphabet,1);
  Ordinal_Word rhs_word(alphabet,1);
  Transition_Realiser tr(*fsa_wd);
  bool is_right_shortlex = alphabet.order_type() == WO_Right_Shortlex && !geodesic;
  const char * status_message =  create_equations ?
    "Building word-acceptor and equations " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Length %u\n":
    "Building word-acceptor state " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Length %u\n";

  if (want_coset_acceptor)
  {
    /* We can't limit the state set for coset acceptors yet */
    max_context = 0;
    max_states = 0;
  }
  bool unlimited = max_states == 0 && max_context == 0;
  bool need_definitions = create_equations || !unlimited;

  /* Insert and skip failure state */
  factory.find_state(packed_key,0);
  /* Insert initial state */
  memset(key,0,nr_differences);
  if (want_coset_acceptor)
  {
    State_Subset_Iterator ssi;
    for (State_ID state = fsa_wd->initial_state(ssi,true);
         state;
         state = fsa_wd->initial_state(ssi,false))
      key[state] = geodesic ? SEEN_LHS_BETTER : SEEN_EQUAL;
    container.progress(1,"Building coset word-acceptor from word-difference"
                         " machine with " FMT_ID " states\n",nr_differences);
  }
  else
    container.progress(1,"Building word-acceptor from word-difference"
                       " machine with " FMT_ID " states\n",nr_differences);
  size_t size = key_packer.pack(key);
  factory.find_state(packed_key,size);
  State_Count count = 2;
  State_ID ceiling = 1;
  State_ID wa_state = 0;
  State_ID suffix_wa_state = 0;
  Ordinal irvalue = 0; // Declared and initialised here to shut up stupid compilers
  while (factory.get_state_key(packed_key,++wa_state))
  {
    Word_Length length = wa_state >= ceiling ? state_length : state_length - 1;
    if (!(wa_state & 255))
      container.status(2,1,status_message,wa_state,count-wa_state,count,length);
    /* Ensure the correct state for equal words so far is present */
    key_packer.unpack(&old_key);
    old_key.append(1,geodesic ? SEEN_LHS_BETTER : SEEN_EQUAL);

    if (need_definitions)
    {
      factory.defining_word(&lhs_word,wa_state,length);
      irvalue = length ? maf.inverse(lhs_word.value(length-1)) : PADDING_SYMBOL;
      if (!want_coset_acceptor && length)
        suffix_wa_state = factory.read_word(Subword(lhs_word,1,length));
    }

    for (Ordinal g1 = 0;g1 < nr_generators;g1++)
    {
      /* Calculate action of generators on this state. To get the image, we
         have to apply (g1,g2) to each element in the subset of fsa_wd*fsa_gt
         corresponding to the state, for each generator g2 and for g2=Idword.

         Since we ensured above that the equal words so far state is present we
         don't need to do anything special to cope with reductions that only
         reduce a subword.
      */
      memset(key,0,nr_differences);
      bool no_transition = false;
      /* We will set no_transition to be true if we find that the
         transition leads to failure. */
      State_Pair_List::Iterator spi(old_key);
      State_ID old_state[2] = {0,0};// to satisfy compiler;
      bool there;
      for (there = spi.first(old_state);there;there = spi.next(old_state))
      {
        State_ID state = old_state[0];
        const State_ID * t1 = tr.realise_row(state) + alphabet.product_base(g1);
        State_ID new_state;
        char gt_state = (char) old_state[1];
        Ordinal g2;
        if (gt_state == SEEN_EQUAL)
        {
          /* In this case the LHS and RHS are equal so far. In the group
             case state must be identity, but in coset case it could be
             some other state */
          for (g2 = 0; g2 < nr_generators;g2++)
          {
            new_state = t1[g2];
            if (!new_state)
              continue;
            gt_state = g2 < g1 ? SEEN_RHS_BETTER : SEEN_LHS_BETTER;
            if (new_state == identity)
            {
              if (gt_state == SEEN_RHS_BETTER)
              {
                no_transition = true; /* In this case the generator is
                                         reducible or coset-reducible */
                break;
              }
              continue; /* otherwise the new word is either equal or worse
                           and does not need to be included*/
            }
            if (g1 == g2)
              gt_state = SEEN_EQUAL;
            if (need_equal ?
                better_compare_state(gt_state,key[new_state]) :
                gt_state > key[new_state])
              key[new_state] = gt_state;
          }
        }
        else
        {
          for (g2 = 0; g2 < nr_generators;g2++)
          {
            new_state = t1[g2];
            if (!new_state)
              continue;
            gt_state = (char) old_state[1];

            if (is_right_shortlex && g1 != g2)
              gt_state = g2 < g1 ? SEEN_RHS_BETTER : SEEN_LHS_BETTER;
            if (new_state == identity)
            {
              if (gt_state == SEEN_RHS_BETTER)
              {
                no_transition = true;
                break;
              }
              continue;
            }
            /* the shortlex state is the same as before, i.e. old_key[state]*/
            if (need_equal ?
                better_compare_state(gt_state,key[new_state]) :
                gt_state > key[new_state])
              key[new_state] = gt_state;
          }
        }
        if (no_transition)
          break;
        /* now deal with pad on the right */
        new_state = t1[g2];
        if (new_state == identity)
        {
          no_transition = true;
          break;
        }
        else if (new_state)
          key[new_state] = SEEN_RHS_BETTER;
      }

      if (no_transition)
      {
        transition[g1] = 0;
        if (create_equations && g1 != irvalue)
        {
          Word_Length l = length+1;
          lhs_word.set_length(l);
          lhs_word.set_code(length,g1);
          sdr.reduce(&rhs_word,lhs_word);
          rm->add_correction(lhs_word,rhs_word);
          lhs_word.set_length(length);
        }
      }
      else
      {
        size = key_packer.pack(key);

        if (unlimited ||
            (max_states == 0 || count < max_states) &&
            (max_context == 0 || length < max_context))
          transition[g1] = factory.find_state(packed_key,size);
        else
        {
          transition[g1] = factory.find_state(packed_key,size,false);
          if (!transition[g1])
            transition[g1] = factory.new_state(suffix_wa_state,g1);
        }

        if (transition[g1] >= count)
        {
          if (wa_state >= ceiling)
          {
            state_length++;
            ceiling = transition[g1];
          }
          if (need_definitions)
          {
            State_Definition definition;
            definition.state = wa_state;
            definition.symbol_nr = g1;
            factory.set_definition(transition[g1],definition);
          }
          count++;
        }
      }
    }
    factory.set_transitions(wa_state,transition);
  }
  factory.remove_keys();
  factory.change_flags(GFF_ACCESSIBLE|GFF_TRIM,0);
  delete [] transition;
  delete [] key;
  container.progress(1,"Word-acceptor with " FMT_ID " states prior to"
                       " minimisation constructed.\n",factory.state_count());
  return FSA_Factory::minimise(factory);
}

/**/

FSA_Simple * MAF::build_acceptor_from_dm(const FSA *fsa_wd,
                                         bool create_equations,
                                         bool geodesic,
                                         bool force_group_acceptor,
                                         Word_Length max_context,
                                         State_Count max_states)
{
  /* This method is intended to be called only in the construction of the
     word-acceptor of an automatic group, or the coset word-acceptor
     for an automatic coset system. In principle this code will work for
     any word ordering.

     It works on the output of a word-difference machine for the group
     "anded" with the (usually infinite) greater than automaton sa_gt.
     If sa_gt is actually finite, then the code in build_shortlex_wa_from_dm()
     is usually used instead. Unless otherwise remarked the comment here applies
     to that method as well.

     The returned automaton accepts a word only if it is irreducible, on the
     basis that if fsa_wd() accepts u,v as synonyms where v precedes u in in
     the ordering, then u is reducible. In the case of the coset acceptor,
     the acceptor accepts exactly one word in each coset - the word that
     comes first in the ordering.

     The states of the fsa that we are calculating consist of subsets of the
     Cartesian product of the states of fsa_wd and sa_gt. sa_gt may have an
     infinite number of states, but we hope that we will only see finitely
     many of them during the construction of the word-acceptor. If not then
     the construction will fail.

     If the ordering respects length we will encounter a reduction when
     trying to find the transition to the next state at each word which has
     an irreducible prefix.

     If the ordering does not respect length, then when we are looking for
     the transitions from a new state we have to see whether we can reduce
     the new words by reading additional symbols on the right. We simply
     check whether Diff_Reduce::reduce() can reduce the defining word of the
     potential new state: we only have to check the word for reducibility,
     and can pass a flag to indicate that it does not have a reducible
     prefix, so this is not particularly inefficient (at least it is
     not much more inefficient than repeating the rather complicated
     code for this case would be).

     The coset acceptor is the same as an ordinary acceptor, except
     that fsa_wd has multiple initial states.

     As we build up the subset that represents a state of the word-acceptor,
     we form a key that represents this information. Unfortunately we cannot
     assume that we will only have to remember one state of sa_gt for each
     difference. Even though it is certainly the case (even for coset systems)
     that one of the v paths is better than the other, we may not be able
     to tell which, and may need to keep both. In fact this is unlikely to
     occur in MAF, because our policy of allowing interior padding on the
     right means we are much less likely to have to do this.

     If geodesic is true, we don't want to reject words in which the reduced
     word is the same length. This only makes sense for orderings in which
     for all words x,y, x is shorter than y => x < y and x is longer than y
     => x > y. We may as well use the shortlex code in this case, and
     simply start the greater than automaton in its x > y state.
  */
  const Alphabet & alphabet = fsa_wd->base_alphabet;
  if (alphabet.order_is_effectively_shortlex() ||
      alphabet.order_type() == WO_Right_Shortlex ||
      geodesic && alphabet.order_is_geodesic())
    return build_shortlex_wa_from_dm(*this,fsa_wd,create_equations,geodesic,
                                     force_group_acceptor,max_context,
                                     max_states);

  if (!alphabet.order_supports_automation())
    return 0;

  /* Possibly, a useful special case here would be to see if one or more
     generators has been found reducible. In this case it might
     turn out that the alphabet was shortlex on the remaining symbols.
     We could build the appropriate word-acceptor from the restriction
     of the alphabet, and a dm built from the equations involving
     only the remaining symbols.
     However we would have to work out what to about non-reducible
     generators with a reducible inverse. */

  /* Using the "complete" difference machine is problematical when
     building a non shortlex acceptor, and we want to kill off as
     many transitions as possible.

     1) If a generator x is reducible then we assume that we will never find a
     reduction with x appearing on the right hand side. This is not really
     justified since we might have an RWS along the lines of Z=Y*z*Y Y=y^n, and
     for sufficiently large n we would not be able to prove Z reducible without
     going via Y, since we would not have been able to realise the required
     equation.
     However, in the non-shortlex case,in which elimination of generators is
     common, doing this greatly increases our chances of being able to build a
     word-acceptor at all, and we know that such transitions are not needed in
     the multiplier (whereas transitions with a reducible generator on the
     left do of course occur).

     2) If we read a padding symbol on the left, then we know that from
     then on this is the only permitted symbol on the left. Therefore there
     should not be any ($,g) transitions to states from which ($,g) transitions
     alone cannot get to 1. In fact there should a unique path to 1 - the
     reduced word for the inverse of the word-difference we are currently at.
     Similarly, if we take a (g1,x) transition where g1 is reducible, then we
     know that a reduction is possible, and that therefore we should have
     reached a state of the difference machine from which ($,g) transitions can
     get to 1. For the word-difference machine for a multiplier this is not the
     case, since it is also OK if we can get to a state from which (x,$) gets
     to 1.

     For a reducer 1 is a valid $ target, but does not itself have any $
     transitions since 1 is effectively a final state if it is returned to.

     So we first modify our input difference machine so that it satisfies these
     conditions.
  */

  FSA_Simple * fsa_temp = FSA_Factory::copy(*fsa_wd);
  Ordinal nr_generators = alphabet.letter_count();

  /* Ensure 1^(g,g) = 1 for all generators.
     We need to do this in case we are being called with a diff1c type
     word-difference machine, or a KBMAG one */
  {
    State_ID * transitions = fsa_temp->state_lock(1);
    for (Ordinal g1 = 0;g1 < nr_generators;g1++)
      transitions[alphabet.product_id(g1,g1)] = 1;
    fsa_temp->state_unlock(1);
  }

  FSA_Simple * fsa_temp2 = FSA_Factory::copy(*fsa_temp);
  State_Count nr_differences = fsa_temp2->state_count();
  Ordinal_Word lhs_word(alphabet,1);
  Rewriter_Machine * rm = &rewriter_machine();

  for (State_ID si = 1;si < nr_differences;si++)
  {
    State_ID * transitions = fsa_temp2->state_lock(si);
    /* Kill all transitions involving a reducible generator on the RHS */
    for (Ordinal g2 = 0;g2 < nr_generators;g2++)
      if (rm->redundant(g2))
        for (Ordinal g1 = 0;g1 <= nr_generators;g1++)
          transitions[alphabet.product_id(g1,g2)] = 0;

    fsa_temp2->state_unlock(si);
  }

  {
    Diff_Reduce dr(fsa_temp2);
    /* Now we convert fsa_temp into the FSA which just contains the
       $,g transitions that should be in the difference machine*/
    for (State_ID si = 1;si < nr_differences;si++)
    {
      container.status(2,1,"Removing bad transitions from difference machine (" FMT_ID " of " FMT_ID ")\n",
                       si,nr_differences);
      State_ID * transitions = fsa_temp->state_lock(si);
      /* kill all the non $,g transitions */
      for (Ordinal g1 = 0;g1 < nr_generators;g1++)
        for (Ordinal g2 = 0;g2 <= nr_generators;g2++)
          transitions[alphabet.product_id(g1,g2)] = 0;
      bool dollar_found = false;
      for (Ordinal g2 = 0;g2 < nr_generators;g2++)
      {
        Transition_ID ti = alphabet.product_id(nr_generators,g2);
        if (rm->redundant(g2) || si == 1)
          transitions[ti] = 0;
        else if (transitions[ti])
          dollar_found = true;
      }
      if (dollar_found)
      {
        fsa_temp->label_word(&lhs_word,si);
        invert(&lhs_word,lhs_word);
        rm->reduce(&lhs_word,lhs_word);
        for (Ordinal g2 = 0;g2 < nr_generators;g2++)
        {
          Transition_ID ti = alphabet.product_id(nr_generators,g2);
          if (g2 != lhs_word.value(0))
            transitions[ti] = 0;
        }
      }
      fsa_temp->state_unlock(si);
    }
  }
  fsa_temp->set_initial_all();
  fsa_temp->change_flags(GFF_MIDFA,GFF_TRIM);
  fsa_temp->create_accept_definitions();
  Special_Subset dollar_states(*fsa_temp2); // we are going to delete fsa_temp
  Accept_Definition ad;
  dollar_states.assign_membership(1,true);
  for (State_ID si = 2;si < nr_differences;si++)
    dollar_states.assign_membership(si,fsa_temp->get_accept_definition(&ad,si));
  int killed = 0;
  for (State_ID si = 1;si < nr_differences;si++)
  {
    State_ID * transitions = fsa_temp2->state_lock(si);
    /* Kill transitions to non dollar states from which only
       $ transitions are allowed */
    for (Ordinal g1 = 0;g1 < nr_generators;g1++)
      if (rm->redundant(g1))
        for (Ordinal g2 = 0; g2 < nr_generators;g2++)
        {
          Transition_ID ti = alphabet.product_id(g1,g2);
          if (transitions[ti] && !dollar_states.contains(transitions[ti]))
          {
            transitions[ti] = 0;
            killed++;
          }
        }
    if (dollar_states.contains(si))
    {
      for (Ordinal g2 = 0; g2 < nr_generators;g2++)
      {
        Transition_ID ti = alphabet.product_id(PADDING_SYMBOL,g2);
        if (transitions[ti] && !fsa_temp->new_state(si,ti))
        {
          transitions[ti] = 0;
          killed++;
        }
      }
    }
    else
      for (Ordinal g2 = 0; g2 < nr_generators;g2++)
      {
        Transition_ID ti = alphabet.product_id(PADDING_SYMBOL,g2);
        if (transitions[ti])
        {
          transitions[ti] = 0;
          killed++;
        }
      }
    fsa_temp2->state_unlock(si);
  }
  delete fsa_temp;
  fsa_temp = fsa_temp2;
  fsa_temp->change_flags(0,GFF_TRIM);
  fsa_temp2 = FSA_Factory::minimise(*fsa_temp);
  delete fsa_temp;
  fsa_temp = fsa_temp2;
  fsa_wd = fsa_temp;
  fsa_temp2 = 0;
  nr_differences = fsa_wd->state_count();
  Strong_Diff_Reduce sdr(rm);
  bool want_coset_acceptor = fsa_wd->has_multiple_initial_states() &&
                             !force_group_acceptor;

  State_Pair_List key;
  Keyed_FSA factory(container,alphabet,nr_generators,
                    nr_differences*nr_generators*2,0);
  State_ID identity = fsa_wd->accepting_state();
  State_ID * transition = new State_ID[nr_generators];
  Ordinal_Word rhs_word(alphabet,1);
  Transition_Realiser tr(*fsa_wd);
  Hash gt_states(4,alphabet.gt_key_size());
  const void * old_gt_state;
  const void * gt_state;
  const void * check_gt_state;
  size_t gt_state_size;
  State_ID gt_si;
  Byte_Buffer packed_key;
  const void * old_packed_key;
  size_t key_size;
  State_ID pair[2],old_pair[2],check_pair[2];
  // note can_lengthen will be different from the is_short value in maf
  // if we are a coset system.
  bool can_lengthen = !alphabet.order_is_geodesic();
  Diff_Reduce dr(fsa_wd);

  /* Insert and skip failure state */
  gt_state = alphabet.gt_failure_state(&gt_state_size);
  gt_states.find_entry(gt_state,gt_state_size);
  factory.find_state(0,0);
  /* Insert initial state */
  gt_state = alphabet.gt_initial_state(&gt_state_size);
  gt_si = gt_states.find_entry(gt_state,gt_state_size);

  key.empty();
  if (want_coset_acceptor)
  {
    State_Subset_Iterator ssi;
    for (State_ID dm_si = fsa_wd->initial_state(ssi,true);
         dm_si;
         dm_si = fsa_wd->initial_state(ssi,false))
      key.insert(dm_si,gt_si,false);
    container.progress(1,"Building non-shortlex coset word-acceptor\n"
                         "Word-difference machine has " FMT_ID " states\n",
                       nr_differences);
  }
  else
  {
    container.progress(1,"Building non-shortlex word-acceptor\n"
                         "Word-difference machine has " FMT_ID " states\n",
                       nr_differences);
    key.insert(fsa_wd->initial_state(),gt_si,false);
  }
  packed_key = key.packed_data(&key_size);
  factory.find_state(packed_key,key_size);
  State_Count count = 2;
  State_ID ceiling = 1;
  Word_Length state_length = 0;
  State_ID wa_state = 0;
  State_ID suffix_wa_state = 0;
  Ordinal irvalue = PADDING_SYMBOL; // Declared and initialised here to shut up stupid compilers
  if (want_coset_acceptor)
  {
    /* We can't limit the state set for coset acceptors yet */
    max_context = 0;
    max_states = 0;
  }
  bool unlimited = max_context == 0 && max_states == 0;
  bool need_definitions = create_equations || can_lengthen || !unlimited;
  const char * status_message =  create_equations ?
    "Building word-acceptor & equations " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Length %u\n":
    "Building word-acceptor state " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Length %u\n";

  while ((old_packed_key = factory.get_state_key(++wa_state))!=0)
  {
    Word_Length length = wa_state >= ceiling ? state_length : state_length - 1;
    if (!(wa_state & 16))
      container.status(2,1,status_message,
                       wa_state,count-wa_state,count,length);

    State_Pair_List old_key((const Byte *) old_packed_key);
    State_Pair_List::Iterator old_key_pairs(old_key);
    State_Pair_List::Iterator new_key_pairs(key);
    if (need_definitions)
    {
      factory.defining_word(&lhs_word,wa_state,length);
      irvalue = length ? inverse(lhs_word.value(length-1)) : PADDING_SYMBOL;
      if (!want_coset_acceptor && length)
        suffix_wa_state = factory.read_word(Subword(lhs_word,1,length));
    }

    for (Ordinal g1 = 0;g1 < nr_generators;g1++)
    {
      /* Calculate action of generators on this state. To get the image, we
         have to apply (g1,g2) to each element in the subset of fsa_wd*fsa_gt
         corresponding to the state, for each generator g2 and for g2=Idword.

         Since MAF ensures that s^(g,g) = s for all generators g when s is the
         1 state of a difference machine, we don't need to do anything
         special to cope with reductions that only reduce a subword.
       */

      key.empty();
      bool no_transition = rm->redundant(g1) || can_lengthen && g1 == irvalue;
      /* We will set no_transition to be true if we find that the
         transition leads to failure. */

      if (!no_transition && old_key_pairs.first(old_pair))
      {
        do
        {
          Transition_ID ti = alphabet.product_base(g1);
          const State_ID * t1 = tr.realise_row(old_pair[0]) + ti;
          old_gt_state = gt_states.get_key(old_pair[1]);
          for (Ordinal g2 = 0; g2 <= nr_generators;g2++,ti++)
          {
            pair[0] = t1[g2];
            if (!pair[0])
              continue;
            gt_state = alphabet.gt_new_state(&gt_state_size,old_gt_state,ti);
            if (!gt_state)
              continue;  /* This won't currently happen, but would if I
                            decide to disallow interior padding on the
                            right */
            bool wanted = true;
            /* before including the new difference, see if the same
               difference is already present with a better vword */
            if (new_key_pairs.first(check_pair))
            {
              bool advance;
              do
              {
                advance = true;
                if (check_pair[0] == pair[0])
                {
                  check_gt_state = gt_states.get_key(check_pair[1]);
                  int cmp = alphabet.gt_compare(gt_state,check_gt_state);
                  if (cmp == 0 || cmp == 2)
                    wanted = false;
                  if (cmp == 1)
                  {
                    key.remove(check_pair);
                    advance = false;
                  }
                }
              }
              while ((advance ? new_key_pairs.next(check_pair) :
                                new_key_pairs.current(check_pair)) &&
                     check_pair[0] <= pair[0]);
            }
            if (wanted)
            {
              pair[1] = gt_states.find_entry(gt_state,gt_state_size);
              if (pair[0] == identity)
              {
                if (alphabet.gt_is_accepting(gt_state))
                {
                  no_transition = true; /* In this case the generator is
                                           reducible or coset-reducible */
                  break;
                }
                if (pair[1] != 1)
                  continue; /* otherwise the new word is worse
                               and does not need to be included */
              }
              key.insert(pair,false);
            }
          }
          if (no_transition)
            break;
        }
        while (old_key_pairs.next(old_pair));
      }

      if (!no_transition && can_lengthen)
      {
        lhs_word.set_length(length);
        lhs_word.append(g1);
        if (suffix_wa_state && !factory.new_state(suffix_wa_state,g1))
          no_transition = true;
        if (!no_transition && dr.reduce(&lhs_word,lhs_word,
                              WR_CHECK_ONLY|WR_ASSUME_L2))
          no_transition = true;
      }

      if (no_transition)
      {
        transition[g1] = 0;
        if (create_equations && g1 != irvalue)
        {
          Word_Length l = length+1;
          lhs_word.set_length(l);
          lhs_word.set_code(length,g1);
          sdr.reduce(&rhs_word,lhs_word);
          rm->add_correction(lhs_word,rhs_word);
          lhs_word.set_length(length);
        }
      }
      else
      {
        packed_key = key.packed_data(&key_size);
        if (unlimited ||
            (max_states == 0 || count < max_states) &&
            (max_context == 0 || length < max_context))
          transition[g1] = factory.find_state(packed_key,key_size);
        else
        {
          transition[g1] = factory.find_state(packed_key,key_size,false);
          if (!transition[g1])
            transition[g1] = factory.new_state(suffix_wa_state,g1);
        }

        if (transition[g1] >= count)
        {
          if (wa_state >= ceiling)
          {
            state_length++;
            ceiling = transition[g1];
          }
          if (need_definitions)
          {
            State_Definition definition;
            definition.state = wa_state;
            definition.symbol_nr = g1;
            factory.set_definition(transition[g1],definition);
          }
          count++;
        }
      }
    }
    factory.set_transitions(wa_state,transition);
  }
  factory.remove_keys();
  factory.change_flags(GFF_ACCESSIBLE|GFF_TRIM,0);
  delete [] transition;
  delete fsa_temp;
  container.progress(1,"Word-acceptor with " FMT_ID " states prior to"
                       " minimisation constructed.\n",factory.state_count());
  return FSA_Factory::minimise(factory);
}

/**/

void Group_Automata::create_defining_equations(Rewriter_Machine * rm,FSA *wa)
{
  /* Our difference machine will usually be incomplete when we try to
     build the automata. We can call this function in an attempt to
     create some of the missing word-differences. Normally we will be
     able either to use validate_difference_machine() or build and
     check the multiplier. But this method may find some missing word
     differences in the case where the acceptor is too big, and afterwards
     the acceptor may be different. */

  State_Count nr_states = wa->state_count();
  Container &container = rm->container;
  Ordinal nr_generators = rm->generator_count();
  Ordinal_Word lhs_word(rm->alphabet(),1);
  Ordinal_Word rhs_word(rm->alphabet(),1);
  Strong_Diff_Reduce sdr(rm);

  container.progress(1,"Using word-acceptor to create equations\n");

  wa->create_definitions(true);
  for (State_ID state = 1; state < nr_states;state++)
  {
    wa->defining_word(&lhs_word,state);
    Word_Length length = lhs_word.length();
    container.status(2,1,"Updating tree with defining equations (" FMT_ID " of " FMT_ID "). Depth %u\n",
                     state,nr_states,length);
    Ordinal irvalue = length ? rm->inverse(lhs_word.value(length-1)) : PADDING_SYMBOL;
    for (Ordinal g1 = 0;g1 < nr_generators;g1++)
    {
      if (!wa->new_state(state,g1))
      {
        if (length==1 || g1 != irvalue)
        {
          Word_Length l = length+1;
          lhs_word.set_length(l);
          lhs_word.set_code(length,g1);
          sdr.reduce(&rhs_word,lhs_word);
          rm->add_correction(lhs_word,rhs_word);
          lhs_word.set_length(length);
        }
      }
    }
  }
}

/**/

Difference_Machine_Index::Difference_Machine_Index(const FSA & difference_machine) :
  alphabet(difference_machine.base_alphabet),
  initial_dm_labels(* new Word_DB(alphabet,
                                  difference_machine.nr_initial_states()))
{
  Array_Of<State_ID> & xlat = * new Array_Of<State_ID>;

  initial_dm_labels.manage(xlat,true);
  State_Subset_Iterator ssi;
  Word_List wl(difference_machine.label_alphabet());
  Ordinal_Word ow(difference_machine.label_alphabet());
  /* Build an index of the initial states of the word-difference machine */
  {
    for (State_ID dm_si = difference_machine.initial_state(ssi,true);
         dm_si;
         dm_si = difference_machine.initial_state(ssi,false))
    {
      Label_ID label = difference_machine.get_label_nr(dm_si);
      difference_machine.label_word_list(&wl,label);
      Element_Count nr_words = wl.count();
      Ordinal nr_generators = alphabet.letter_count();

      for (Element_ID word_nr = 0;word_nr < nr_words;word_nr++)
      {
        wl.get(&ow,word_nr);
        if (!ow.length() || ow.value(0) < nr_generators)
        {
          Element_ID id;
          initial_dm_labels.insert(ow,&id);
          xlat[id] = dm_si;
        }
      }
    }
  }
  dm_si_from_dm_label = xlat.buffer();
}

Difference_Machine_Index::~Difference_Machine_Index()
{
  delete &initial_dm_labels;
}

bool Difference_Machine_Index::find(const Word & word,State_ID * si) const
{
  Element_ID word_nr;
  if (initial_dm_labels.find(word,&word_nr))
  {
    *si = dm_si_from_dm_label[word_nr];
    return true;
  }
  *si = 0;
  return false;
}

/**/

int Group_Automata::validate_difference_machine(const FSA *dm,
                                                const FSA *wa,
                                                Rewriter_Machine *rm)
{
  /* This method performs the same checks that would be accomplished
     by first building and then validating the multiplier. The return value
     is 1 if we get to the end without any problems, in which case we
     should be able to build the multiplier. Other return values are as
     follows:
       -2: we got all the way to the end, found errors, but not any we could
           correct
       -1: we gave up after correcting some errors
       0:  we got all the way to the end after correcting errors.

     The purpose of this function is to be able to find missing word
     differences in the case where the word-acceptor is probably incorrect
     and has too many states for it to be easy to build the multiplier.

     The method works by recreating the word-acceptor, but with states
     that consist of subsets of the power set of dm * wa - where
     the subset for each state corresponds to pairs (d1,w1) with d1 a state
     of the word-difference machine, and w1 a state of the word-acceptor
     (with an extra state for padded words)). The initial state consists
     of the set {(1,1)}, and for a state s={(d1,w1),(d2,w2),...}
     the state s^g1 consists of all the pairs (dn^(g1,g2),wn^g2) where g2
     ranges over the generators and the padding symbol, and for which both
     states in the pair are not the failure state. In other words for each
     accepted word u we are forming the set of word-differences that can be
     reached with v also an accepted word.

     We can detect three kinds of error:

     1) For any one state there should never be two pairs with the same
     value for the difference machine state, because if so then the word
     acceptor is accepting two different words for the same group element
     since we have Uv1==Uv2==d so that v1==v2==ud.

     2) If the u word is accepted we should never see the pair (1,n) for non
     zero n other than for v==u. Actually this is just a special case of 1,
     but it should be noted that when we build and check the multiplier we
     will detect errors of type 2 and 3, but not errors of type 1 (though by
     the time there are no errors of type 2 and 3 there will be no errors
     of type 1 either)

     In both these cases where the same word-difference is seen twice
     we need to reconstruct the attainable differences and form an
     equation between the two v words.

     3) If the u word is not accepted we should reach the pair (1,n) with
     u != v. If not then we can reduce u and find a new word-difference.

     word-acceptor state values:
     1..n-1 normal
     n      padding symbol read one or more times on right

     The method uses two (possibly initially identical) word-difference
     machines. The first word-difference machine, explicitly passes as dm,
     may be a "weak" word-difference machine, only containing states and
     transitions we have actually seen.

     The second word-difference machine is the current difference tracker,
     containing all transitions that are possible based on our knowledge
     of group multiplication. This is updated as we make corrections.

  */
  const FSA & difference_machine = *dm;
  State_Count nr_differences = difference_machine.state_count();
  const FSA & word_acceptor = *wa;
  State_Count nr_wa_states = word_acceptor.state_count();

  Sparse_Function_Packer key_packer(nr_differences,nr_wa_states+1,1);
  /* I am naughtily using my knowledge of the implementation of key packing
     below. (Sparse_Function_Packer used to be local to this function so
     it isn't that naughty really). key[1] will never be zero, so there will
     always be a zero gap indicator at the start of the packed key. So we
     don't store this.
     The ,1) on the end of the key_packer constructor is unconnected to the
     +1 below. */
  void * packed_key = (char *) key_packer.get_buffer()+1;
  const Alphabet & alphabet = rm->alphabet();
  const Ordinal nr_generators = difference_machine.base_alphabet.letter_count();
  Ordinal g1,g2;
  Transition_ID product_id;
  Container & container = rm->container;
  Keyed_FSA factory(container,difference_machine.base_alphabet,nr_generators,nr_wa_states,0);
  State_ID state = 0;
  State_ID *key = new State_ID[nr_differences];
  State_ID *old_key = new State_ID[nr_differences];
  State_ID dsi,ndsi,nwsi;
  State_ID ceiling = 1;
  Word_Length state_length = 0;
  Ordinal_Word lhs_word(alphabet,1);
  Ordinal_Word alt_lhs_word(alphabet,1);
  Ordinal_Word rhs_word(alphabet,1);
  State_Count corrections = 0;
  State_Count errors = 0;
  State_Count bad_states = 0;
  bool aborted = false;
  int running = 0;
  Strong_Diff_Reduce sdr(rm);

  /* Insert and skip failure state */
  factory.find_state(packed_key,0);
  /* Insert initial state */
  memset(key,0,nr_differences*sizeof(State_ID));
  key[1] = 1;
  size_t size = key_packer.pack(key)-1;
  size_t tot_size = size;
  factory.find_state(packed_key,size);
  State_Count count = 2;
  int count_down = 60;
  Diff_Equate de(dm,&word_acceptor);

  while (factory.get_state_key(packed_key,++state))
  {
    Word_Length length = state >= ceiling ? state_length : state_length - 1;
    if (container.status(2,1,"Checking reductions state " FMT_ID " ("
                             FMT_ID " of " FMT_ID " to do). Word Length %u.\n"
                             "Corrections=" FMT_ID ". Partial reductions=" FMT_ID
                             ". Bad states=" FMT_ID ". Timeout %d\n",
                         state,count-state,count,length+1,
                         corrections,errors,bad_states,count_down))
    {
      running++;
      if (corrections)
        count_down--;
      else if (running > count_down && !(errors || bad_states))
        count_down = running;
      if (count_down < 0)
      {
        if (aborted)
          break;
        count_down = 60;
        aborted = true;
      }
    }
    key_packer.unpack(old_key);

    factory.defining_word(&lhs_word,state,length);

    for (g1 = 0; g1 < nr_generators;g1++)
    {
      bool should_reject = word_acceptor.new_state(old_key[1],g1)==0;
      memset(key,0,nr_differences*sizeof(State_ID));
      bool no_transition = false;
      bool bad_state = false;

      for (dsi = 1; dsi < nr_differences;dsi++)
      {
        if (old_key[dsi])
        {
          ndsi = difference_machine.new_state(dsi,alphabet.product_id(g1,PADDING_SYMBOL));
          if (ndsi)
          {
            nwsi = nr_wa_states;
            if (!key[ndsi])
              key[ndsi] = nwsi;
            else
              bad_state = true;

            if (ndsi == 1)
            {
              no_transition = true;
              if (should_reject)
                break;
              bad_state = true;
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
                if (ndsi)
                {
                  if (ndsi == 1 && (dsi != 1 || g1 != g2))
                  {
                    if (should_reject)
                    {
                      no_transition = true;
                      break;
                    }
                    bad_state = true;
                  }
                  if (key[ndsi])
                    bad_state = true; /* Here we have already accepted two
                                         equal words that are close to the u
                                         word. We should be able to find
                                         an equation to reject one of the
                                         v words */
                  else
                    key[ndsi] = nwsi;
                }
              }
            }
            if (no_transition)
              break;
          }
        }
      }

      if (should_reject && !no_transition)
      {
        Word_Length l = length+1;
        lhs_word.set_length(l);
        lhs_word.set_code(length,g1);
        errors++;
        sdr.reduce(&rhs_word,lhs_word,0);
        if (rm->add_correction(lhs_word,rhs_word))
        {
          corrections++;
          if (rm->is_dm_changing(false) && count_down < 60)
            count_down += 10;
        }
        lhs_word.set_length(length);
      }

      if (bad_state)
      {
        Word_Length l = length+1;
        lhs_word.set_length(l);
        lhs_word.set_code(length,g1);
        bad_states++;
        if (de.equate(&alt_lhs_word,&rhs_word,lhs_word))
        {
          if (rm->add_correction(alt_lhs_word,rhs_word))
          {
            corrections++;
            if (rm->is_dm_changing(false) && count_down < 60)
              count_down += 10;
          }
        }
        lhs_word.set_length(length);
      }

      if (!should_reject && !aborted)
      {
        size = key_packer.pack(key)-1;
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
  if (errors || bad_states)
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

enum GM_Keys {GMK_DIFF,GMK_WA_LHS,GMK_WA_RHS};

static void set_gm_labels_and_accept_states(Keyed_FSA & factory,
                                            const FSA & dm,
                                            Triple_Packer &key_packer)
{
  /* This function sets up the labels and accept states of a
     general multiplier, and is used by both build_gm() and
     build_finite_gm()
  */
  const Alphabet & base_alphabet = dm.base_alphabet;
  const Alphabet & label_alphabet = dm.label_alphabet();
  Ordinal nr_generators = base_alphabet.letter_count();
  Container & container = dm.container;
  State_ID key[3];
  void * packed_key = key_packer.get_buffer();
  Label_ID *labels = new Label_ID[nr_generators+dm.nr_initial_states()+1];
  Label_Count nr_labels = 1; // unlabelled states have label 0
  State_ID gm_state;
  State_Subset_Iterator ssi;
  labels[0] = 0;

  /* find the labels for the initial states */
  for (gm_state = factory.initial_state(ssi,true); gm_state;
       gm_state = factory.initial_state(ssi,false))
  {
    factory.get_state_key(packed_key,gm_state);
    key_packer.unpack_key(key);
    labels[nr_labels++] = dm.get_label_nr(key[GMK_DIFF]);
  }

  /* Find the actual labels associated with each generator in case any
     are reducible */
  Transition_ID product_id = base_alphabet.product_base(PADDING_SYMBOL);
  State_ID diff_state = dm.initial_state();
  Element_List accepting_labels;
  Label_ID * multiplier_labels = new Label_ID[nr_generators+1];
  Ordinal_Word ow(base_alphabet);
  Diff_Reduce dr(&dm);
  for (Ordinal g = 0; g < nr_generators;g++,product_id++)
  {
    /* depending on what kind of dm our multiplier is being built with the
       $,g transition may or may not give us the correct answer for reducible
       generators.
       The required transition will be missing if a "known transition" machine
       is being used. */
    State_ID new_state = dm.new_state(diff_state,product_id);
    if (!new_state)
    {
      /* In this case the generator is reducible, but we don't know what to*/
      ow.set_length(0);
      ow.append(g);
      dr.reduce(&ow,ow);
      new_state = diff_state;
      Word_Length l = ow.length();
      for (Word_Length i = 0; i < l;i++)
      {
        Transition_ID ti = base_alphabet.product_id(PADDING_SYMBOL,ow.value(i));
        new_state = dm.new_state(new_state,ti);
      }
    }
    if (new_state)
    {
      Label_ID diff_label = dm.get_label_nr(new_state);
      Label_Count i;
      for (i = 0; i < nr_labels;i++)
        if (labels[i] == diff_label)
          break;
      if (i == nr_labels)
        labels[nr_labels++] = diff_label;
      accepting_labels.insert(diff_label);
      multiplier_labels[g] = i;
    }
    else
      multiplier_labels[g] = 0;
  }
  /* Record the label for the "$" multiplier */
  accepting_labels.insert(dm.get_label_nr(diff_state));
  multiplier_labels[nr_generators] = 1;

  /* Set up translation from dm labels for multipliers to our labels */
  Element_List xlat_diff_label;
  Element_List::Iterator eli(accepting_labels);
  for (Label_ID diff_label = eli.first();diff_label;diff_label = eli.next())
    for (Ordinal g = 0;g <= nr_generators;g++)
      if (labels[multiplier_labels[g]] == diff_label)
      {
        xlat_diff_label.append_one(multiplier_labels[g]);
        break;
      }
  const Element_ID * xlat = xlat_diff_label.buffer();

  /* Create all the labels */
  factory.set_label_type(LT_List_Of_Words);
  factory.set_label_alphabet(label_alphabet);
  factory.set_nr_labels(nr_labels,LA_Mapped);
  Word_List wl(label_alphabet,0);
  Ordinal_Word s(label_alphabet);
  for (Label_Count i = 1; i < nr_labels;i++)
  {
    if (dm.label_word(&s,labels[i],0))
      wl.add(s);
    for (Ordinal g = 0;g < nr_generators;g++)
      if (multiplier_labels[g] == i)
      {
        s.set_length(1);
        s.set_code(0,g);
        wl.add(s,true);
      }
    if (dm.label_word(&s,labels[i],1))
      wl.add(s,true);
    factory.set_label_word_list(i,wl);
    wl.empty();
  }
  delete [] labels;

  /* Set the label numbers for the initial states */
  for (gm_state = factory.initial_state(ssi,true); gm_state;
       gm_state = factory.initial_state(ssi,false))
    factory.set_label_nr(gm_state,gm_state);

  /* Set the accepting states and their labels */
  gm_state = 0;
  factory.clear_accepting(true);
  State_Count count = factory.state_count();
  while (factory.get_state_key(packed_key,++gm_state))
  {
    if (!(char) gm_state)
      container.status(2,1,"Setting multiplier accept states (" FMT_ID " of " FMT_ID ")\n",
                        gm_state,count);
    key_packer.unpack_key(key);
    Label_ID label = dm.get_label_nr(key[GMK_DIFF]);
    Element_Count label_pos;
    if (accepting_labels.find(label,&label_pos))
    {
      factory.set_label_nr(gm_state,xlat[label_pos]);
      factory.set_is_accepting(gm_state,true);
    }
  }
  delete [] multiplier_labels;
}

/**/

static void initialise_gm(Keyed_FSA & factory,
                          const FSA & dm,
                          Triple_Packer &key_packer,
                          Rewriter_Machine * rm,
                          Difference_Machine_Index **dmi_ = 0)
{
  /* Create the failure and initial states for a general multiplier.
     This is quite complicated for coset multipliers as we try to
     pair the generators nicely */

  State_ID key[3];
  Ordinal_Word ow1(dm.base_alphabet,1);
  Ordinal_Word ow2(dm.base_alphabet,1);
  Ordinal_Word label_word(dm.label_alphabet(),1);
  Difference_Machine_Index &dmi = * new Difference_Machine_Index(dm);
  Equation_Word ew(*rm);

  /* Insert failure state */
  key[0] = 0;
  key[1] = 0;
  key[2] = 0;

  factory.find_state(key_packer.pack_key(key));
  /* Insert initial state(s) */
  State_ID gm_state;
  key[GMK_WA_LHS] = 1;
  key[GMK_WA_RHS] = 1;
  State_Subset_Iterator ssi;
  State_Subset_Iterator ssi1;
  for (State_ID si = dm.initial_state(ssi,true); si ; si = dm.initial_state(ssi,false))
  {
    key[GMK_DIFF] = si;
    gm_state = factory.find_state(key_packer.pack_key(key));
    rm->container_status(2,1,"Initialising multiplier (" FMT_ID " of " FMT_ID ").\n",
                           si,dm.state_count());

    if (!factory.is_initial(gm_state))
    {
      factory.set_is_initial(gm_state,true);
      if (rm)
      {
        /* We are going to try to build the multiplier with the inverse
           pairs of initial states adjacent, as that will make the
           subgroup presentation better */
        Label_ID label = dm.get_label_nr(si);
        dm.label_word(&label_word,label);
        ew = label_word;
        Node_Reference s = ew.realise_state();
        if (ew.read_inverse(s,false) && dmi.find(ew,&key[GMK_DIFF]))
        {
          State_ID igm_state = factory.find_state(key_packer.pack_key(key));
          factory.set_is_initial(igm_state,true);
        }
      }
    }
  }
  if (dmi_)
    *dmi_ = &dmi;
  else
    delete &dmi;
}

/**/

int Group_Automata::build_gm(FSA_Simple **answer,
                             Rewriter_Machine * rm,
                             const FSA * word_acceptor,
                             const FSA * difference_machine,
                             bool correct_only,State_Count *nr_factory_states)
{
  /*
    This method constructs the fsa of which the states are triples (s1,s2,d),
    with s1 and s2 states of word_acceptor and d of difference_machine.
    More precisely, if word_acceptor has n states 1..n-1 and fail state, then
    s1 and s2 may also be n, meaning that the end of string symbol has been
    read on lhs or rhs. The accept states are the states in which d has
    length 1 or 0 - in other words a generator or IdWord. The accept states
    are labelled with a word list containing all words of length 0 or 1
    which are equal to the final difference. The initial states
    are the states (1,1,d) with d an initial state of difference_machine.
    The accept states therefore represent the equations ux = hv, with h
    an initial state,and x a multiplier, and u,v accepted. We only label
    the accept states and the initial states. For a group multiplier there is
    a single initial state (1,1,1). For a coset system multiplier there are
    multiple initial states unless the subgroup is trivial.

    The alphabet is the usual product alphabet as used by word-difference
    machines.

    The alphabet member (g1,g2) maps (s1,s2,d) to (s1^g1,s2^g2,d^(g1,g2))
    if all three components are nonzero, and to zero otherwise.
    There are several categories of accept-states - one for each distinct
    group element of word-length 0 or 1, and these are specified by
    the labels of the states, which is the reduced word for that group element.

    If during the construction, a nontrivial equation between two words is
    discovered as a result of encountering the identity word-difference,
    then the word-acceptor must be accepting both of these words, and must
    therefore be wrong. In this case the method aborts without returning an
    fsa. It can be called again with parameter correct_only true to allow
    corrections to be made. At first sight this may seem inefficient, but the
    amount of data for the multiplier can be huge, so it is more efficient to
    keep the cases completely separate since this significantly reduces
    the amount of storage required, and in any case the multiplier will almost
    always build without any equations being discovered.

    I did toy with the idea of removing correction mode, since MAF always used
    to use the strongest available set of word-differences when building a
    word-acceptor, in which case the multiplier will always build successfully.
    However, there are some examples, notably cox5335c, where the acceptor
    built with only known transitions builds far more quickly and has far
    fewer states. So now MAF tries to work out whether to build an acceptor
    from a strong or a weak set of word-differences.

    build_gm() returns the following values:
       if correct_only is false:
       0 if a word_acceptor error is found, 1 if not
       if correct_only is true:
       0 if no word_acceptor corrections are found,1 if equations are found
       and build completed, 2 if equations are found but build did not complete
  */
  const FSA & wa = *word_acceptor;
  const FSA & dm = *difference_machine;
  const State_ID end_of_string = wa.state_count();
  const Alphabet & base_alphabet = dm.base_alphabet;
  const Alphabet & label_alphabet = dm.label_alphabet();
  const Ordinal nr_generators = base_alphabet.letter_count();
  const int nr_transitions = base_alphabet.product_alphabet_size();
  Container & container = wa.container;
  const State_Count nr_differences = dm.state_count();
  State_ID key[3],old_key[3];
  key[GMK_WA_LHS] = key[GMK_WA_RHS] = end_of_string+1;
  key[GMK_DIFF] = nr_differences;
  Triple_Packer key_packer(key);
  State_ID *transition = correct_only ? 0 : new State_ID[nr_transitions];
  State_Count expected;

  if (nr_differences < SUGGESTED_HASH_SIZE*8/end_of_string)
  {
    expected = end_of_string*nr_differences;
    if (expected < SUGGESTED_HASH_SIZE)
      expected = SUGGESTED_HASH_SIZE;
  }
  else
    expected = SUGGESTED_HASH_SIZE*8;

  Keyed_FSA factory(container,base_alphabet,nr_transitions,
                    expected,key_packer.key_size());
  State_Definition definition = {0,0};
  State_Count nr_successes = 0;
  State_Count nr_failures = 0;
  Ordinal_Word lhs_word(base_alphabet,1);
  Ordinal_Word rhs_word(base_alphabet,1);
  Ordinal_Word label_word(label_alphabet,1);
  Word_Length state_length = 0;
  // the next line is OK, because if the dm does not have multiple
  // initial states then the subgroup must be trivial, and the coset
  // multiplier is inevitably identical to the ordinary multipier
  const bool want_coset_multiplier = dm.has_multiple_initial_states();
  // In the geodesic case we only ever need to read one pad character on
  // either side. Such a state is always final (accepting or not)
  // so we can fix the wa_state values for both parts of the word.
  Special_Subset paddable_dm_states(dm);
  const bool geodesic = dm.base_alphabet.order_is_geodesic() ||
                        dm.base_alphabet.order_is_effectively_shortlex();

  if (geodesic)
  {
    Word_List wl(label_alphabet);
    for (State_ID dm_si = 1;dm_si < nr_differences;dm_si++)
    {
      Label_ID label = dm.get_label_nr(dm_si);
      dm.label_word_list(&wl,label);
      Element_Count count = wl.count();
      for (Element_ID word_nr = 0;word_nr < count;word_nr++)
      {
        wl.get(&label_word,word_nr);
        if (!label_word.length() ||
            label_word.value(0) < nr_generators && label_word.length()==1)
        {
          paddable_dm_states.include(dm_si);
          break;
        }
      }
    }
  }

  *answer = 0;
  if (want_coset_multiplier)
    container.progress(1,"Building coset multiplier from difference machine"
                       " with " FMT_ID " states\n",nr_differences);
  else
    container.progress(1,"Building multiplier from difference machine"
                       " with " FMT_ID " states\n",nr_differences);

  initialise_gm(factory,dm,key_packer,rm);
  void * packed_key = key_packer.get_buffer();
  State_ID gm_state = 0;
  State_ID nr_states = factory.state_count();
  State_ID ceiling = 1;
  Transition_Realiser tr_wa(wa);
  Transition_Realiser tr_dm(dm);

  while (factory.get_state_key(packed_key,++gm_state))
  {
    Word_Length length = gm_state >= ceiling ? state_length : state_length-1;
    key_packer.unpack_key(old_key);
    if (!(char) gm_state)
      rm->container_status(2,1,"Building multiplier state " FMT_ID " (" FMT_ID " of " FMT_ID " to do)."
                               " Length %d\n",
                           gm_state,nr_states-gm_state,nr_states,length);
    State_ID wa_lhs_state = old_key[GMK_WA_LHS];
    State_ID wa_rhs_state = old_key[GMK_WA_RHS];
    State_ID diff_state = old_key[GMK_DIFF];
    Transition_ID product_id = 0;
    bool dead_state = true;
    const State_ID * tr_wa_left = wa_lhs_state != end_of_string ? tr_wa.realise_row(wa_lhs_state,0) : 0;
    const State_ID * tr_wa_right = wa_rhs_state != end_of_string ? tr_wa.realise_row(wa_rhs_state,1) : 0;
    const State_ID * tr_dm_pair = tr_dm.realise_row(diff_state);
    for (Ordinal g1 = 0; g1 <= nr_generators;g1++)
    {
      if (g1 == nr_generators)
        key[GMK_WA_LHS] = end_of_string;
      else if (wa_lhs_state != end_of_string)
        key[GMK_WA_LHS] = tr_wa_left[g1];
      else
        key[GMK_WA_LHS] = 0;
      for (Ordinal g2 = 0; g2 <= nr_generators;g2++,product_id++)
      {
        if (product_id == nr_transitions)
          continue;

        if (key[GMK_WA_LHS])
        {
          key[GMK_DIFF] = tr_dm_pair[product_id];

          if (g2 == nr_generators)
            key[GMK_WA_RHS] = end_of_string;
          else if (wa_rhs_state != end_of_string)
            key[GMK_WA_RHS] = tr_wa_right[g2];
          else
            key[GMK_WA_RHS] = 0;
        }


        if (key[0] && key[1] && key[2])
        {
          if (geodesic && (key[GMK_WA_LHS] == end_of_string ||
                           key[GMK_WA_RHS] == end_of_string))
          {
            key[GMK_WA_LHS] = key[GMK_WA_RHS] = end_of_string;
            if (!paddable_dm_states.contains(key[GMK_DIFF]))
            {
              if (!correct_only)
                transition[product_id] = 0;
              continue;
            }
          }
          dead_state = false;
          if (key[GMK_DIFF]==1 && g1!=g2)
          {
            /* We have found an equation between two distinct words
               accepted by word_acceptor, and so it must be wrong. */
            if (!correct_only)
            {
              delete [] transition;
              return false;
            }
            else
            {
              Word_Length i = 1;
              Word_Length l = length+1;
              Word_Length r = l;
              lhs_word.allocate(l);
              rhs_word.allocate(r);
              Ordinal * lvalues = lhs_word.buffer();
              Ordinal * rvalues = rhs_word.buffer();

              if (g1 == nr_generators)
                l--;
              else
                lvalues[length] = g1;
              if (g2 == nr_generators)
                r--;
              else
                rvalues[length] = g2;
              State_ID state = gm_state;

              for (i = 1; i <= length;i++)
              {
                factory.get_definition(&definition,state);
                Ordinal g = definition.symbol_nr / (nr_generators+1);
                if (g == nr_generators)
                  l--;
                else
                  lvalues[length-i] = g;
                g = definition.symbol_nr % (nr_generators+1);
                if (g == nr_generators)
                  r--;
                else
                  rvalues[length-i] = g;
                state = definition.state;
              }
              lhs_word.set_length(l);
              rhs_word.set_length(r);
              if (want_coset_multiplier)
              {
                /* we need to put in the h word from the initial state
                   of the difference machine and the coset_symbol */
                Ordinal_Word label(label_alphabet);
                State_ID ikey[3];
                factory.get_state_key(packed_key,state);
                key_packer.unpack_key(ikey);
                dm.label_word(&label,dm.get_label_nr(ikey[GMK_DIFF]),1);
                label.append(rm->pd.coset_symbol);
                rhs_word = label + rhs_word;
                label.set_length(0);
                label.append(rm->pd.coset_symbol);
                lhs_word = label + lhs_word;
              }
              if (rm->add_correction(lhs_word,rhs_word))
                nr_successes++;
              else if (rm->add_correction(rhs_word,lhs_word))
                nr_successes++;
              else
                nr_failures++;
              if (nr_successes * 100 < nr_failures && nr_failures > 50000)
                return nr_successes ? 2 : 0;
            }
          }
          else
          {
            State_ID new_gm_state = factory.find_state(key_packer.pack_key(key));
            if (correct_only)
            {
              /* If the number of states in the multiplier is getting too big
                 then the word-acceptor is probably hopelessly wrong as well,
                 so we give up. How big is too big? Well the initial multiplier
                 for the rubik antislice group has about two million states, so
                 multipliers definitely can get very big.
              */
              if (nr_successes && gm_state > 2048*2048)
                return nr_successes ? 2 : 0;
              if (new_gm_state >= nr_states)
              {
                definition.state = gm_state;
                definition.symbol_nr = product_id;
                factory.set_definition(new_gm_state,definition);
              }
            }
            else
              transition[product_id] = new_gm_state;
            if (new_gm_state >= nr_states)
            {
              if (gm_state >= ceiling)
              {
                state_length++;
                ceiling = new_gm_state;
              }
              nr_states++;
            }
          }
        }
        else if (!correct_only)
          transition[product_id] = 0;
      }
    }
    if (!correct_only && !dead_state)
      factory.set_transitions(gm_state,transition);
  }
  tr_wa.unrealise();
  tr_dm.unrealise();

  if (!correct_only)
  {
    set_gm_labels_and_accept_states(factory,dm,key_packer);
    if (transition)
      delete [] transition;
    if (nr_factory_states)
      *nr_factory_states = nr_states;
    factory.tidy();
    factory.remove_keys();
    *answer = FSA_Factory::minimise(factory);
    container.progress(1,"Multiplier with " FMT_ID " states constructed.\n",
                       (*answer)->state_count());
    return true;
  }
  if (nr_successes > 0)
    container.progress(1,FMT_ID " word-acceptor correcting equations were added\n",nr_successes);
  else
    container.progress(1,"Could not correct word-acceptor\n");
  return nr_successes > 0;
}

/**/

FSA_Simple * Group_Automata::build_finite_gm(Rewriter_Machine * rm,
                                             const FSA * word_acceptor,
                                             const FSA * difference_machine)
{
  /*
    This method constructs the General_Multiplier for a finite group or
    coset system that is known to be confluent, by enumerating the
    accepted words and using the Rewriter_Machine to reduce the necessary
    multiples. This may well be quicker than building the multiplier in
    the normal way if the group is not too large and has a lot of word
    differences.
  */
  const FSA & wa = *word_acceptor;
  const FSA & dm = *difference_machine;
  const State_ID end_of_string = wa.state_count();
  const Alphabet & base_alphabet = dm.base_alphabet;
  const Alphabet & full_alphabet = rm->alphabet();
  const Ordinal nr_generators = base_alphabet.letter_count();
  const int nr_transitions = base_alphabet.product_alphabet_size();
  Container & container = wa.container;
  const State_Count nr_differences = dm.state_count();
  State_ID key[3];
  key[GMK_WA_LHS] = key[GMK_WA_RHS] = end_of_string+1;
  key[GMK_DIFF] = nr_differences;
  Triple_Packer key_packer(key);
  Keyed_FSA factory(container,base_alphabet,nr_transitions,
                    max(State_Count(end_of_string*nr_transitions),
                        SUGGESTED_HASH_SIZE),
                    key_packer.key_size());
  Ordinal_Word lhs_word(full_alphabet,1);
  Ordinal_Word rhs_word(full_alphabet,1);
  Ordinal_Word g_word(full_alphabet,1);
  Ordinal_Word irhs_word(full_alphabet,1);
  const bool want_coset_multiplier = dm.has_multiple_initial_states();
  bool failed = false;
  const Presentation_Data & pd = rm->pd;
  bool extended_coset_multiplier = want_coset_multiplier &&
                                   pd.presentation_type != PT_Simple_Coset_System;
  Difference_Machine_Index *dmi;

  if (want_coset_multiplier)
    container.progress(1,"Building coset multiplier from difference machine"
                       " with " FMT_ID " states\n",nr_differences);
  else
    container.progress(1,"Building multiplier from difference machine"
                       " with " FMT_ID " states\n",nr_differences);

  initialise_gm(factory,dm,key_packer,rm,&dmi);

  /* Iterator through all the words, and multiply them by the generators */
  FSA::Word_Iterator wi(wa);
  if (want_coset_multiplier)
  {
    lhs_word.set_length(1);
    lhs_word.set_code(0,pd.coset_symbol);
  }

  Word_List wl(dm.label_alphabet(),0);
  const Language_Size lang_size = wa.language_size(true);
  Element_ID word_nr = 0;
  Special_Subset_Iterator ssi;
  for (State_ID wa_lhs_state = wi.first();wa_lhs_state;
       wa_lhs_state = wi.next(true))
  {
    if (!(char) ++word_nr)
      container.status(2,1,"Building multiplier. Processing word (" FMT_ID " of " FMT_LS ")."
                           " " FMT_ID " states so far\n",word_nr,lang_size,
                       factory.state_count());
    for (Ordinal g = PADDING_SYMBOL; g < nr_generators;g++)
    {
      if (want_coset_multiplier)
      {
        lhs_word.set_length(1);
        lhs_word.set_code(0,pd.coset_symbol);
        lhs_word += wi.word;
      }
      else
        lhs_word = wi.word;
      Word_Length l = lhs_word.length();
      if (g != PADDING_SYMBOL)
      {
        lhs_word.append(g);
        l++;
      }
      rm->reduce(&rhs_word,lhs_word);
      Word_Length r = rhs_word.length();
      Word_Length ls = 0;
      Word_Length rs = 0;

      key[GMK_DIFF] = 1;
      if (want_coset_multiplier)
      {
        ls = rs = 1;
        while (rhs_word.value(rs) >= pd.coset_symbol)
          rs++;
        g_word = Subword(lhs_word,1,l);
        r -= rs;
        l--;

        if (rs > 1 || !extended_coset_multiplier)
        {
          /* We have a non-empty h-word on the RHS, or possibly should have
             We need to find its G-word
             Since ug=hv the gword for h can be found by reducing ugV
             and this is probably easier than converting h and reducing that
             (and works even when we don't remember h words)
          */
          rm->invert(&irhs_word,Subword(rhs_word,rs));
          g_word += irhs_word;
          rm->reduce(&g_word,g_word);
          dmi->find(g_word,&key[GMK_DIFF]);
        }
      }
      bool ok = false;
      if (key[GMK_DIFF]) /* if not then an initial state is missing, and we are
                        deliberately going to build a wrong multiplier */
      {
        key[GMK_WA_LHS] = 1;
        key[GMK_WA_RHS] = 1;
        State_ID gm_state = factory.find_state(key_packer.pack_key(key));
        if (g != PADDING_SYMBOL)
          l--;
        Word_Length i;
        Word_Length m = l >= r ? l : r;
        const Ordinal * lvalues = lhs_word.buffer()+ls;
        const Ordinal * rvalues = rhs_word.buffer()+rs;
        // now create the states for this equation
        for (i = 0; i < m;i++)
        {
          Ordinal g1 = i < l ? lvalues[i] : PADDING_SYMBOL;
          Ordinal g2 = i < r ? rvalues[i] : PADDING_SYMBOL;
          key[GMK_WA_LHS] = i < l ? wa.new_state(key[GMK_WA_LHS],g1,false) : end_of_string;
          key[GMK_WA_RHS] = i < r ? wa.new_state(key[GMK_WA_RHS],g2,false) : end_of_string;
          Transition_ID ti = base_alphabet.product_id(g1,g2);
          key[GMK_DIFF] = dm.new_state(key[GMK_DIFF],ti);
          if (key[GMK_DIFF])
          {
            State_ID new_gm_state = factory.find_state(key_packer.pack_key(key));
            if (!failed)
              factory.set_transition(gm_state,ti,new_gm_state);
            gm_state = new_gm_state;
          }
          else
            break;
        }
        ok = i==m;
      }

      if (!ok)
      {
        rm->add_correction(lhs_word,rhs_word);
        failed = true;
      }
    }
  }

  delete dmi;
  if (!failed)
  {
    set_gm_labels_and_accept_states(factory,dm,key_packer);
    factory.tidy();
    factory.remove_keys();
    // coset multipliers are not trim because we don't know if all the initial
    // states are needed
    factory.change_flags(want_coset_multiplier ? 0 : GFF_TRIM,GFF_BFS);
    FSA_Simple *answer = FSA_Factory::minimise(factory);
    answer->sort_bfs();
    container.progress(1,"Multiplier with " FMT_ID " states constructed.\n",
                       answer->state_count());
    return answer;
  }
  return 0;
}

/**/

int Group_Automata::gm_valid(const General_Multiplier & gm,
                             Rewriter_Machine * rm,unsigned gwd_flags)
{
  /* If u is an accepted word then for each generator g, then
     either ug is also accepted, or there is a state of the multiplier
     which has accepted u and in which Uv=g. So for each accepted u
     we need to find all the states which accept both u & v, and check
     that there is one that solves Uv=g.

     We are not actually going to construct a new fsa - we just go through the
     motions of constructing its states. */

  class Validator
  {
    private:
      const Ordinal nr_generators;
      const General_Multiplier & gm;
      const FSA & multiplier;
      Container & container;
      const Alphabet & base_alphabet;
      Rewriter_Machine *rm;
      bool * includes;
      State_List key;
      Transition_Realiser tr;
      State_ID bad_state;
      State_ID ceiling;
      State_ID state;
      Label_Count found;
      Ordinal_Word lhs_word;
      Ordinal_Word rhs_word;
      Keyed_FSA factory;
      State_Definition definition;
      unsigned gwd_flags;
      State_Count nr_successes;
      State_Count nr_failures;
      int count_down;
      int running;
      Strong_Diff_Reduce sdr;
      Word_Length state_length;
      bool aborted;
      bool can_lengthen;
      bool corrected;
      bool is_coset_multiplier;
      bool ok;

    public:
      Validator(const General_Multiplier & gm_,
                Rewriter_Machine * rm_,unsigned gwd_flags_) :
        gm(gm_),
        multiplier(*gm_.fsa()),
        container(gm_.container),
        base_alphabet(gm_.base_alphabet),
        nr_generators(gm_.base_alphabet.letter_count()),
        factory(container,base_alphabet,nr_generators,gm.state_count(),0),
        ok(true),
        aborted(false),
        nr_successes(0),
        nr_failures(0),
        lhs_word(gm_.base_alphabet,1),
        rhs_word(gm_.base_alphabet,1),
        ceiling(1),
        bad_state(0),
        state_length(0),
        running(0),
        count_down(30),
        rm(rm_),
        sdr(rm_),
        tr(*gm_.fsa()),
        gwd_flags(gwd_flags_)
      {
        Ordinal g1,g2;
        State_Count key_size = 2; // initially there are at most two states in the key
        State_Count count = 2; // we create one initial state
        includes = new bool[nr_generators+1];
        bool is_shortlex = base_alphabet.order_is_effectively_shortlex();
        can_lengthen = !is_shortlex && !base_alphabet.order_is_geodesic();
        is_coset_multiplier = multiplier.has_multiple_initial_states();
        State_List old_key;

        /* Each state in the new fsa will be represented as a subset of the set of
           states of multiplier. The initial state is one-element set containing
           the initial states of multiplier.
           The subsets will be stored as variable-length keys in the Hash,
           with states in increasing order.
        */

        // insert failure and initial states
        key.append_one(0);
        factory.find_state(key.buffer(),sizeof(State_ID));
        key.empty();
        State_Subset_Iterator ssi;
        for (State_ID si = multiplier.initial_state(ssi,true);
             si;
             si = multiplier.initial_state(ssi,false))
          key.append_one(si);
        key.append_one(0);
        factory.find_state(key.buffer(),sizeof(State_ID)*key.count());
        old_key.reserve(key.count(),false);
        check_state(1,0,IdWord); // we need to check the initial state!

        state = 0;
        while (factory.get_state_key(old_key.buffer(),++state))
        {
          Word_Length length = state >= ceiling ? state_length : state_length-1;
          if (rm->container_status(2,1,"Multiplier checks ("
                                       FMT_ID " of " FMT_ID " to do)."
                                       " Length %u. Corrected " FMT_ID
                                       ". Timeout %d\n",
                                   count-state,count,
                                   length,nr_successes,count_down))
          {
            running++;
            if (nr_failures || nr_successes)
              count_down--;
            else if (running > count_down)
              count_down = running;
            if (count_down < 0)
            {
              if (aborted)
                break;
              count_down = 60;
              aborted = true;
            }
          }
          /* Calculate action of generator g1 on state to get the image.
             We must apply (g1,g2) to each element mstate in the subset corresponding
             to state for each generator g2 of the base-alphabet (including
             the padding symbol). */
          for (g1 = 0; g1 < nr_generators;g1++)
          {
            key.empty();
            for (State_ID * s = old_key.buffer();*s;s++)
            {
              State_ID mstate = *s;
              const State_ID * trow = tr.realise_row(mstate) +
                                      base_alphabet.product_base(g1);
              for (g2 = 0;g2 <= nr_generators;g2++)
              {
                State_ID nmstate = trow[g2];
                if (nmstate!=0)
                  key.insert(nmstate);
              }
            }
            key.append_one(0);
            Element_Count key_count = key.count();
            State_ID nstate = factory.find_state(key.buffer(),key_count*sizeof(State_ID),!aborted);
            if (nstate == -1 || nstate >= count || bad_state && nstate >= bad_state)
            {
              check_state(nstate,length,g1);

              if (nstate >= count)
              {
                if (found == nr_generators+1 ||
                    includes[nr_generators] &&
                    (corrected && state_length < rm->height() ||
                     rm->pd.is_coset_finite))
                {
                  definition.state = state;
                  definition.symbol_nr = g1;
                  factory.set_definition(nstate,definition);
                  count++;
                  if (state >= ceiling)
                  {
                    state_length++;
                    ceiling = nstate;
                  }
                  if (key_count > key_size)
                    key_size = key_count;
                }
                else
                  factory.remove_state(nstate);
              }
            }
          }
          old_key.reserve(key_size,false);
        }
      }
      ~Validator()
      {
        delete [] includes;
      }
      void check_state(State_ID nstate,Word_Length length,Ordinal g1)
      {
        /* We have a new state. We must check it is valid - i.e.
           that it contains an accept-state of each multiplier.
        */

        found = 0;
        corrected = false;
        memset(includes,0,nr_generators+1);
        for (State_ID * t = key.buffer();*t;t++)
        {
          if (multiplier.is_accepting(*t))
          {
            Label_ID ln = multiplier.get_label_nr(*t);
            for (Ordinal g = 0; g <= nr_generators;g++)
              if (gm.label_for_generator(ln,g))
                if (!includes[g])
                {
                  includes[g] = true;
                  found++;
                }
          }
        }

        /* If ug=v is a reduction but v has the same length as ug
           (or possibly longer in non-shortlex case)
           then we won't have found the g accept state for u yet.
           We need to read one more letter (or possibly more in non-shortlex
           case).
        */
        for (;;)
        {
          State_List::Iterator sli(key);
          State_List new_key;
          for (State_ID mstate = sli.first(); mstate && found < nr_generators+1;
               mstate = sli.next())
          {
            const State_ID * trow = tr.realise_row(mstate) +
                                    base_alphabet.product_base(PADDING_SYMBOL);
            for (Ordinal g2 = 0; g2 < nr_generators;g2++)
            {
              State_ID nmstate = trow[g2];
              if (can_lengthen && nmstate)
                new_key.append_one(nmstate);
              if (nmstate && multiplier.is_accepting(nmstate))
              {
                Label_ID ln = multiplier.get_label_nr(nmstate);
                for (Ordinal g = 0; g <= nr_generators;g++)
                  if (gm.label_for_generator(ln,g))
                    if (!includes[g])
                    {
                      includes[g] = true;
                      found++;
                   }
              }
            }
          }
          if (!can_lengthen || !new_key.count())
            break;
          key.take(new_key);
        }

        if (found < nr_generators+1)
        {
          if (!bad_state)
            bad_state = nstate;
          for (Ordinal g = 0; g <= nr_generators;g++)
            if (!includes[g])
            {
              /* The state is invalid for generator g.
               * We reconstruct the offending word w, using the state-definitions,
              */
              ok = false;
              State_ID error_state = state;
              Word_Length l = length;
              if (g1 != IdWord)
                l++;
              if (is_coset_multiplier)
                l++;
              Word_Length i = 1;
              if (g != nr_generators)
              {
                l++;
                i++;
              }
              lhs_word.set_length(l);
              if (g != nr_generators)
                lhs_word.set_code(l-1,g);
              if (g1 != IdWord)
              {
                lhs_word.set_code(l-i,g1);
                while (factory.get_definition(&definition,error_state))
                {
                  i++;
                  lhs_word.set_code(l-i,Ordinal(definition.symbol_nr));
                  error_state = definition.state;
                }
              }
              if (is_coset_multiplier)
                lhs_word.set_code(0,rm->pd.coset_symbol);
              if (!rm->reduce(&rhs_word,lhs_word,WR_PREFIX_ONLY))
              {
                sdr.reduce(&rhs_word,lhs_word,0);
                if (rm->add_correction(lhs_word,rhs_word))
                {
                  nr_successes++;
                  corrected = true;
                  if (rm->is_dm_changing(false) && count_down < 60)
                    count_down = count_down < 55 ? count_down+5 : 60;
                  else if (rm->is_dm_changing(false,gwd_flags) && count_down < 60 && running < 60)
                    count_down += 2;
                }
                else
                  nr_failures++;
              }
            }
        }
      }
      int result()
      {
        if (ok)
          container.progress(1,"Multiplier passes checks\n");
        else
        {
          container.progress(1,"Multiplier fails checks."
                               " " FMT_ID " new equations were found\n",nr_successes);
          if (aborted)
            container.progress(1,"Checking was aborted at state " FMT_ID " of " FMT_ID ""
                                 " after " FMT_ID " failed corrections\n",
                           state,factory.state_count(),nr_failures);
         }
         return ok ? 1 : (nr_successes > 0 ? 0 : -1);
      }
  } validator(gm,rm,gwd_flags);

  return validator.result();
}

/**/

unsigned General_Multiplier::reduce(Word * answer,
                                    const Word & start_word,
                                    unsigned flags) const
{
  /* This function uses a multiplier to reduce a word.
     It should always be able to reduce a word at least as well as
     the Rewriter_Machine. It works by finding the point (if any)
     at which the multiplier rejects the leading subword of our current
     best guess at the reduction. We then find the label of the generator
     of the rvalue, and then look for the unique word which the multiplier
     accepts with the appropriate difference.
  */

  Word_Length start_length = start_word.length();
  Word_Length valid_length = 0;
  Word_Length length = start_length;
  Word_List label_wl(label_alphabet());
  unsigned retcode = 0;
  Ordinal_Word test(base_alphabet,start_length);
  State_ID idword_state = initial_state();
  Label_ID initial_label = get_label_nr(idword_state);
  Ordinal rvalue = PADDING_SYMBOL; // Initialised to shut up stupid compilers
  Ordinal_Word prefix(answer->alphabet());
  State_ID h_state = 0;
  Ordinal coset_symbol = base_alphabet.letter_count();
  bool want_prefix = (flags & WR_SCHREIER_LABEL+WR_H_LABEL)!=0;
  bool can_detect_trivial = labels_consistent();

  if (answer != &start_word)
  {
    answer->set_length(length);
    word_copy(*answer,start_word,length);
  }
  Ordinal * values = answer->buffer();

  if (!length)
    return 0;
  if (flags & WR_PREFIX_ONLY)
    rvalue = values[--length];

  /* Remove any h word and coset symbol from front of word.
     We do this so that users don't have to worry about whether
     or not they can put _H in front of a word to indicate coset
     reduction is required */
  {
    Word_Length i;
    for (i = 0; i < length;i++)
      if (values[i] < coset_symbol)
        break;
    if (i > 0)
    {
      if (values[i-1] != coset_symbol)
        MAF_INTERNAL_ERROR(container,("Bad word passed to General_Multiplier::reduce()\n"));
      prefix = Subword(*answer,0,i-1);

      length -= i;
      *answer = Subword(*answer,i);
      want_prefix = true;
    }
  }
  for (;;)
  {
    State_ID state = idword_state;
    for (valid_length = 0;valid_length < length;valid_length++)
    {
      if ((unsigned) values[valid_length] >= (unsigned) base_alphabet.letter_count())
        MAF_INTERNAL_ERROR(container,("Bad word passed to General_Multiplier::reduce()\n"));
      Transition_ID t = base_alphabet.product_id(values[valid_length],
                                                 values[valid_length]);
      state = fsa__->new_state(state,t);
      if (!state)
      {
        if (flags & WR_CHECK_ONLY)
          return 1;
        break;
      }
    }

    if (valid_length == length)
      break;
    retcode++;
    if (can_detect_trivial &&
        label_for_generator(initial_label,values[valid_length]))
    {
      /* In this case the generator is trivial, so we don't need to
         do anything complicated */
      memcpy(values+valid_length,values+valid_length+1,
             (length-valid_length-1)*sizeof(Ordinal));
      length--;
      continue;
    }

    /* otherwise we want to find the unique word v such that Uv has the
       same label as the offending generator */
    Ordinal desired_multiplier = Ordinal(multiplier_nr[values[valid_length]]);
    multiply(&test,Subword(*answer,0,valid_length),desired_multiplier,&h_state);
    if (h_state != 1)
      if (flags & WR_SCHREIER_LABEL)
        prefix.append(h_state-1 + coset_symbol);
      else if (flags & WR_H_LABEL)
      {
        label_word_list(&label_wl,get_label_nr(h_state));
        for (Element_ID i = 0; i < label_wl.count();i++)
        {
          const Entry_Word ew(label_wl,i);
          if (ew.value(0) > coset_symbol)
          {
            prefix = prefix + ew;
            break;
          }
        }
      }
    if (valid_length+1 < length)
      *answer = test + Subword(*answer,valid_length+1,length);
    else
      *answer = test;
    values = answer->buffer();
    length = answer->length();
    if (flags & WR_ONCE)
      break;
  }
  answer->set_length(length + (flags & WR_PREFIX_ONLY ? 1 : 0));
  if (flags & WR_PREFIX_ONLY)
    answer->set_code(length,rvalue);
  if (want_prefix)
  {
    prefix.append(coset_symbol);
    *answer = prefix + *answer;
  }
  return retcode;
}

/**/

/* class Vital_Builder is used to implement the functionality of
   Group_Automata::build_vital(), which was originally one very
   large function. In order to make the code more manageable it
   has been split into the various methods of this class.
*/
class Vital_Builder
{
  private:
    bool provisional;
    bool is_finite;
    bool cheated;
    bool wa_correct;
    bool wa_stable;
    bool use_dm1;
    bool dm1_tried;
    bool weak_acceptor_tried;
    bool huge_L1;
    bool all_equations_tried;
    bool create_all_equations;
    bool force_finite;
    bool defining_equations_done;
    bool impossible;
    bool is_weak_acceptor;
    bool primary_mistakes_tried;
    bool is_confluent;
    bool resume_kb;
    bool try_multiplier;
    bool try_improver;
    bool tried_multiplier;
    bool tried_validate_dm;
    State_Count last_acceptor_size;
    State_Count acceptor_size;
    State_Count expected_multiplier_size;
    State_Count last_multiplier_size;
    State_Count expected_recogniser_size;
    unsigned long explore_time;
    unsigned long weak_wa_build_time;
    unsigned long strong_wa_build_time;
    unsigned long wa_build_time;
    unsigned long gm_build_time;
    unsigned multiplier_attempts;
    int action;
    Language_Size language_size;
    Rewriter_Machine * rm;
    Rewriter_Machine::Status &stats;
    Container & container;
    MAF & maf;
    const Presentation_Data &pd;
    FSA_Simple *dm1;
    FSA_Simple *dm2;
    FSA_Simple *last_word_acceptor;
    FSA_Simple *last_weak_word_acceptor;
    FSA_Simple *word_acceptor;
    General_Multiplier *multiplier;
    Rewriting_System * rws;
    Group_Automata &ga;
  public:
    Vital_Builder(Group_Automata & ga_,Rewriter_Machine * rm_,
                  bool is_confluent_,int action_) :
      ga(ga_),
      container(rm_->container),
      maf(rm_->maf),
      rm(rm_),
      pd(rm_->pd),
      stats(rm_->status()),
      is_confluent(is_confluent_),
      action(action_),
      provisional(action_ == 2 || action_ == -1 || rm_->maf.options.is_kbprog),
      dm1(ga_.dm1),
      dm2(ga_.dm2),
      word_acceptor(ga_.word_acceptor),
      multiplier(ga_.multiplier),
      rws(ga_.rws),
      last_word_acceptor(0),
      last_weak_word_acceptor(0)
    {
      is_weak_acceptor = try_multiplier = is_finite = force_finite = false;
      primary_mistakes_tried = wa_stable = wa_correct = false;
      use_dm1 = dm1_tried = create_all_equations = false;
      huge_L1 = defining_equations_done = all_equations_tried = false;
      cheated = tried_multiplier = impossible = false;
      resume_kb = false; // gets set true in deal_with_acceptor() if we decide
                         // a confluent rewriting system exists.
      explore_time = 60;
      last_acceptor_size = 0;
      last_multiplier_size = 0;
      expected_multiplier_size = 0;
      expected_recogniser_size = 0;
      strong_wa_build_time = weak_wa_build_time = wa_build_time = 0;
      gm_build_time = 0;
      multiplier_attempts = 0;
      language_size = 0;

      if (stats.complete)
        rm->update_machine(UM_RECHECK_PARTIAL);

      if (!multiplier)
      {
        bool save = stats.coset_complete;
        rm->update_machine(UM_CHECK_POOL+UM_PRUNE);
        stats.coset_complete = save;
      }
    }
    ~Vital_Builder()
    {
      if (last_word_acceptor)
        delete last_word_acceptor;
      if (last_weak_word_acceptor)
        delete last_weak_word_acceptor;
      ga.dm1 = dm1;
      ga.dm2 = dm2;
      ga.word_acceptor = word_acceptor;
      ga.multiplier = multiplier;
      ga.rws = rws;
    }
    bool process()
    {
      while (!multiplier)
      {
        for (;;) /* this inner loop repeats until we are happy with the word-acceptor */
        {
          delete_bad_automata(); /* we do this twice because we want to save
                                    memory by not keeping useless automata */
          if (!provisional)
            rm->check_differences(wa_correct || wa_stable ?
                                  CD_STABILISE : CD_STABILISE|CD_EXPLORE);
          delete_bad_automata();
          if (!dm2)
          {
            dm2 = rm->grow_wd(action==2 ? 0 : CD_STABILISE,0);
            if (dm2)
              dm2 = maf.tidy_difference_machine(dm2);
          }

          if (maf.aborting || provisional || maf.options.is_kbprog)
          {
            kbprog_emulation(action);
            return true;
          }

          if (word_acceptor == 0)
          {
            cheated = false;
            build_word_acceptor();
            if (!word_acceptor)
              return false;
          }

          deal_with_acceptor();
          tried_multiplier = false;
          if (resume_kb)
            return false;

          if (!dm2) // we have built the word-acceptor from a confluent
            break;  // rws, but we can't build anything else.

          tried_validate_dm = false;
          if (multiplier_is_possible())
            break;
          if (!find_more_differences())
            break;
        }

        if (maf.aborting)
          return true;

        if (!dm2 || resume_kb ||
            stats.complete && !pd.is_coset_finite &&
            expected_multiplier_size > 100000000)
        {
          // in the first case we have built the word-acceptor for a monoid
          // and there is no multiplier.
          // In the second we are giving up for now and will try kb completion
          // In the last case we have a confluent RWS for a large finite group
          // (or a coset system) and don't bother with the multiplier.
          break;
        }
        if (!build_multiplier())
        {
          if (impossible)
            return true; /* we cannot do anything else - the object is not automatic */
        }
        else if (multiplier_valid() != 0)
          break;
      }
      bool result = !resume_kb;
      if (result && multiplier && !stats.complete && maf.options.validate)
        result = axiom_check();
      if (!result)
      {
      }
      return result;
    }

    void delete_bad_automata()
    {
      if (dm2 && !rm->dm_valid(0))
      {
        delete dm2;
        dm2 = 0;
      }
      if (dm1 && !rm->dm_valid(GWD_KNOWN_TRANSITIONS))
      {
        delete dm1;
        dm1 = 0;
      }

      if (word_acceptor && !wa_correct && !dm2 && !maf.aborting)
      {
        if (!wa_stable || wa_build_time < gm_build_time)
        {
          if (!is_weak_acceptor)
          {
            if (last_word_acceptor)
              delete last_word_acceptor;
            last_word_acceptor = word_acceptor;
          }
          else
            delete word_acceptor;
          word_acceptor = 0;
          cheated = false;
        }
        else
          cheated = true;
      }
    }

    void kbprog_emulation(int action)
    {
      if (action != 2 || maf.aborting)
      {
        dm1 = rm->grow_wd(CD_STABILISE|CD_EXPLORE,GWD_KNOWN_TRANSITIONS|GWD_PRIMARY_ONLY);
        if (dm1)
          dm1 = maf.tidy_difference_machine(dm1);
        if (dm1 && !stats.complete)
          container.progress(1,"Provisional complete difference machine has"
                               " " FMT_ID " states\nProvisional primary difference"
                               " machine has " FMT_ID " states\n",
                             dm2->state_count(),dm1->state_count());
      }
      if (!rws)
      {
        unsigned flags = RWSC_MINIMAL;
        if (stats.complete || dm2 == 0)
          flags |= RWSC_NEED_FSA;
        if (dm2!=0 && !stats.complete)
          flags |= RWSC_CRITICAL_ONLY;
        rws = new Rewriting_System(rm,flags);
      }
    }

    void build_word_acceptor()
    {
      is_weak_acceptor = false;
      if (stats.complete)
      {
        wa_correct = true;
        rws = new Rewriting_System(rm);
        if (!pd.is_coset_system)
        {
          container.progress(1,"Building word-acceptor from confluent RWS\n");
          FSA_Simple * temp = FSA_Factory::copy(*rws->fsa());
          temp->remove_rewrites();
          word_acceptor = FSA_Factory::minimise(*temp);
          delete temp;
        }
        else
        {
          container.progress(1,"Building word-acceptor from confluent RWS\n");
          FSA_Simple * temp = FSA_Factory::copy(*rws->fsa());
          temp->remove_rewrites();
          State_ID si = temp->initial_state();
          si = temp->new_state(si,pd.coset_symbol,false);
          temp->set_single_initial(si);
          word_acceptor = FSA_Factory::restriction(*temp,maf.group_alphabet());
          delete temp;
        }
      }
      else if (pd.is_coset_system && (stats.coset_complete || pd.is_coset_finite))
      {
        wa_correct = true;
        if (rws)
          delete rws;
        rws = new Rewriting_System(rm,RWSC_MINIMAL|RWSC_NEED_FSA);
        container.progress(1,"Building word-acceptor from RWS\n");
        FSA_Simple * temp = FSA_Factory::copy(*rws->fsa());
        temp->remove_rewrites();
        State_ID si = temp->initial_state();
        si = temp->new_state(si,pd.coset_symbol,false);
        temp->set_single_initial(si);
        word_acceptor = FSA_Factory::restriction(*temp,maf.group_alphabet());
        delete temp;
      }
      else
      {
        bool again = false;
        bool force_strong = maf.options.no_weak_acceptor;

        do
        {
          if (again)
          {
            delete_bad_automata();
            if (!dm2)
            {
              dm2 = rm->grow_wd(CD_STABILISE,0);
              if (dm2)
                dm2 = maf.tidy_difference_machine(dm2);
            }
            again = false;
          }
          unsigned long expected_strong_wa_build_time = (strong_wa_build_time+wa_build_time)/2;
          unsigned long expected_weak_wa_build_time = (weak_wa_build_time+wa_build_time)/2;
          if (expected_strong_wa_build_time > 15 &&
              expected_strong_wa_build_time > expected_weak_wa_build_time &&
              !create_all_equations && !force_strong)
          {
            /* It is taking quite a long time to build the word-acceptor
               We will try building the acceptor from a weak difference machine
               instead */
            if (!dm1)
            {
              dm1 = rm->grow_wd(CD_STABILISE,GWD_KNOWN_TRANSITIONS);
              if (dm1)
                dm1 = maf.tidy_difference_machine(dm1);
            }
            time_t now = time(0);
            container.progress(1,"Building word-acceptor from weak difference machine\n");
            word_acceptor = maf.build_acceptor_from_dm(dm1,false);
            is_weak_acceptor = true;
            rm->set_timeout(wa_build_time = time(0) - now);
            weak_wa_build_time = wa_build_time;
            if (last_word_acceptor) /* if there is no last_word_acceptor
                                       we are just going to use it */
            {
              /* If it took a long time to build the weak word-acceptor
                 or it is much bigger than the last strong word-acceptor
                 it is probably no good. */
              if ((weak_wa_build_time < strong_wa_build_time ||
                   !tried_multiplier) &&
                  word_acceptor->state_count()/2 <=
                  last_word_acceptor->state_count()+500)
              {
                /* The acceptor is promising, so we check
                   whether it is at least as good as the last strong acceptor */
                FSA_Simple * mistakes =
                  FSA_Factory::and_not_first_trim(*word_acceptor,
                                                  *last_word_acceptor);
                if (mistakes->language_size(false) != 0)
                {
                  /* The new word-acceptor accepts words the old one rejected.
                     We will correct the mistakes since we have gone to the
                     trouble of computing them, but will increase wa_build_time
                     so that unless it is much quicker to build the weak acceptor
                     we will use a strong acceptor next time.*/
                  weak_wa_build_time += 20;
                  process_mistakes(mistakes); /* mistakes get deleted */
                  delete last_word_acceptor;
                  last_word_acceptor = 0;
                  delete word_acceptor;
                  word_acceptor = 0;
                  again = true;
                }
                else
                {
                  delete mistakes;
                  /* we will drop out and actually use our weak acceptor
                     and will most likely build it this way in future */
                }
              }
              else
              {
                /* The acceptor we just built is probably
                   no good. So we build a strong one instead.
                   We'll keep this weak word-acceptor handy in case we
                   can't build a multiplier yet */
                last_weak_word_acceptor = word_acceptor;
                word_acceptor = 0;
                again = true;
                weak_wa_build_time += 20;
                force_strong = true;
              }
            }
          }
          else
          {
            if (last_word_acceptor)
            {
              /* as we are going to build a new strong word-acceptor
                 we can throw any saved one we may have away now */
              delete last_word_acceptor;
              last_word_acceptor = 0;
            }
            time_t now = time(0);
            word_acceptor = maf.build_acceptor_from_dm(dm2,create_all_equations);
            rm->set_timeout(wa_build_time = time(0) - now);
            if (!create_all_equations)
              strong_wa_build_time = wa_build_time;
            is_weak_acceptor = false;
          }
          create_all_equations = false;
        }
        while (again);
      }
    }

    void deal_with_acceptor()
    {
      if (last_word_acceptor)
      {
        delete last_word_acceptor;
        last_word_acceptor = 0;
      }
      acceptor_size = word_acceptor->state_count();
      wa_stable = acceptor_size == last_acceptor_size;
      if (!wa_stable)
      {
        multiplier_attempts = 0;
        if (last_acceptor_size && last_acceptor_size*2 < acceptor_size ||
            last_acceptor_size > 2 * acceptor_size ||
            last_acceptor_size >= acceptor_size + 10000 ||
            acceptor_size >= last_acceptor_size + 10000)
        {
          /* If the acceptor is very different in size from before
             ignore any actions we have taken before when deciding what
             to try next. */
          last_multiplier_size = 0;
          dm1_tried = false;
          primary_mistakes_tried = false;
          all_equations_tried = false;
          defining_equations_done = false;
          huge_L1 = false;
          try_multiplier = false;
        }
      }
      last_acceptor_size = acceptor_size;
      language_size = word_acceptor->language_size(true);
      is_finite = language_size != LS_INFINITE;
      /* Here we have just checked the size of language L0.
         From the point of view of confluence what we should be checking is
         the size of language L1. But we aren't building the L1 acceptor just
         now and it might be rather a waste of time to do so.

         In a coset system we have just built a coset acceptor, and
         the size of its language may be finite when the g language is
         infinite. There is not too much hope of getting confluence then.
         Actually the coset part of the Rewriter_Machine probably is already
         confluent. In principle we could build the group word-acceptor to
         find out about finite/confluence issues.
      */
      if (!is_confluent)
      {
        if (is_finite)
        {
          ga.set_finite(maf,!pd.is_coset_system,true);
          if (pd.is_coset_system)
          {
            container.progress(1,"The index of the subgroup is finite ("
                                  FMT_LS ")\n",language_size);
            resume_kb = !stats.coset_complete;
            if (resume_kb)
            {
              if (!maf.options.detect_finite_index)
                maf.options.detect_finite_index = 1;
            }
          }
          else
          {
            container.progress(1,"Language is finite. Knuth Bendix will be"
                                 " continued until confluence\n");
            maf.options.assume_confluent = true;
            resume_kb = true;
          }
        }
        else
          container.progress(1,"Accepted language is infinite. Word-acceptor "
                             "has " FMT_ID " states\n",acceptor_size);
      }
      else
      {
        ga.set_finite(maf,is_finite && !pd.is_coset_system,is_finite);
        if (is_finite)
        {
          if (language_size != LS_HUGE)
            container.progress(1,"Accepted language contains " FMT_LS " words.",
                               language_size);
          else
            container.progress(1,
                               "Accepted language is finite but very large.");
        }
        else
          container.progress(1,"Accepted language is infinite.");
        container.progress(1," Word-acceptor has " FMT_ID " states\n",
                           acceptor_size);
      }
    }

    int multiplier_is_possible()
    {
      /* returns:
          1 if yes,
         -1 if we should give up because it will be too big,
          0 if it is too difficult now as the word-acceptor is big.
      */

      State_Count nr_dm_states = dm2->state_count();
      /* We estimate the number of unminimised multiplier states as
         acceptor_size*min(acceptor_size,nr_dm_states).
         In fact it is often close to acceptor_size*nr_dm_states.
         The theoretical maximum is
         (acceptor_size-1)*(acceptor_size-1)*(nr_dm_states-1)+1, but
         nowhere near this is usually reached. If acceptor_size<nr_dm_states
         then acceptor_size*nr_dm_states is likely to be close.
         If acceptor_size > nr_dm_states then the number of multiplier
         states is likely to be much less than our guess.
         Typically the unminimised multiplier will be much bigger when built
         from a dm2 machine.
      */

      if (acceptor_size < MAX_STATE_COUNT/nr_dm_states)
        expected_multiplier_size = acceptor_size*nr_dm_states;
      else
        expected_multiplier_size = MAX_STATE_COUNT;
      if (last_multiplier_size != 0)
        if (wa_stable)
          expected_multiplier_size = last_multiplier_size;
        else
          expected_multiplier_size = expected_multiplier_size/2 + last_multiplier_size/2;

      if (stats.complete && expected_multiplier_size > 100000000)
      {
        if (!pd.is_coset_finite)
          container.progress(1,"Multiplier would be too large, so is not being"
                               " constructed\n");
        return -1;
      }

      if (dm1_tried)
        use_dm1 = expected_multiplier_size > 10000000;
      else if (wa_correct)
        use_dm1 = expected_multiplier_size > 2000000 && last_multiplier_size > 2000000;
      else
        use_dm1 = expected_multiplier_size > 5000000;

      if (wa_correct || wa_stable || try_multiplier || maf.options.force_multiplier ||
          expected_multiplier_size < (primary_mistakes_tried ? 10000000 : 5000000))
        return 1; // the acceptor is small enough to try building the
                  // multiplier at once or it is believed ot be correct
      return 0; // we need to try something else
    }

    bool find_more_differences()
    {
      /* look for missing word-differences when we think building the
         multiplier will be a bad idea. The return value is 0 if
         if we should try to build the multiplier after all.

         I have tried several approaches to finding out more differences
         when not enough word-differences are known to build a usable
         word-acceptor.

         1) Build the L1 acceptor, then the primary recogniser and look for
            L1 words we don't recognise. This is now very good at finding
            differences but sometimes the L1 acceptor is very big, so one
            might just as well build a multiplier (though that is less good
            at finding the missing differences).

         2) Build a "weak" or "outer" word-acceptor using only primary or
            known transitions. This also sometimes works well on the first
            iteration, but usually doesn't manage to find many differences
            later on. A problem with this mode is that the weak word-acceptor
            can be extremely hard to build.

         3) Go through the motions of validating multiplication directly on
            the word-acceptor and dm. This works well, but can be slow and
            is memory hungry. Generally method 1) works better.

         4) Examine the states of the word-acceptor or L1 acceptor and
            simply create all the equations for the defining words of the
            states or transitions. If we do this, equations will also be
            created for the unminimised version of our next word-acceptor.

         5) Just enumerate words using-the word-acceptor and try reducing
            them.
      */

      /* Usually only one of the next four methods will actually do anything*/

      if (rm->dm_valid(0) && /* We may have just created a whole bunch of equations */
          !last_weak_word_acceptor) /* if we have a weak_word-acceptor we should do try_weak_acceptor() instead*/
        try_primary_mistakes();
      if (rm->dm_valid(0))
        try_weak_acceptor();
      if (rm->dm_valid(0))
      {
        time_t now = time(0);
        tried_validate_dm = true;
        try_validate_dm();
        if (time(0) - now > 300)
          try_multiplier = true;
      }
      return !rm->dm_valid(0);
    }

    void try_primary_mistakes()
    {
      if (!wa_stable && !pd.is_coset_system && !huge_L1)
      {
        FSA_Simple *L1_acceptor = ga.build_L1_acceptor(rm,word_acceptor,word_acceptor,false);
        State_Count nr_L1_states = L1_acceptor->state_count();
        State_Count nr_diff_states = dm2->state_count();
        if (nr_L1_states < MAX_STATE_COUNT/nr_diff_states)
          expected_recogniser_size = nr_L1_states*nr_diff_states;
        else
          expected_recogniser_size = MAX_STATE_COUNT;
        if (expected_recogniser_size > expected_multiplier_size)
          expected_recogniser_size = expected_multiplier_size;
        if (expected_recogniser_size > 15000000)
        {
          if (!defining_equations_done)
          {
            ga.create_defining_equations(rm,word_acceptor);
            defining_equations_done = true;
            if (expected_recogniser_size > 15000000)
            {
              if (!all_equations_tried)
                create_all_equations = true;
              all_equations_tried = true;
            }
            if (!rm->dm_valid(0))
            {
              process_mistakes(L1_acceptor);
              return;
            }
          }
          huge_L1 = expected_recogniser_size > 15000000;
        }
        FSA_Simple * mistakes;
        if (!huge_L1 || !primary_mistakes_tried)
        {
          FSA_Simple * diff_pr;
          if (!huge_L1)
            diff_pr = maf.polish_difference_machine(dm2,false);
          else
          {
            diff_pr = rm->grow_wd(CD_STABILISE,GWD_KNOWN_TRANSITIONS);
            diff_pr = maf.tidy_difference_machine(diff_pr);
          }
          FSA_Simple *pr = ga.build_primary_recogniser(L1_acceptor,
                                                       word_acceptor,
                                                       diff_pr);
          delete diff_pr;
          FSA_Simple * exists = FSA_Factory::exists(*pr,false);
          delete pr;
          mistakes = FSA_Factory::and_not_first_trim(*L1_acceptor,*exists);
          delete L1_acceptor;
          delete exists;
        }
        else
          mistakes = L1_acceptor;
        process_mistakes(mistakes);
        primary_mistakes_tried = true;
      }
    }

    void process_mistakes(FSA_Simple * mistakes)
    {
      Container & container = rm->container;
      Word_DB words_done(mistakes->base_alphabet,1024);
      Strong_Diff_Reduce sdr(rm);
      int count_down = 10;
      int running = 0;
      char loops = 0;
      State_Count corrections = 0;
      Ordinal_Word lhs_word(mistakes->base_alphabet);
      Ordinal_Word rhs_word(mistakes->base_alphabet);
      FSA::Word_Iterator wi(*mistakes);
      mistakes->create_accept_definitions();
      Accept_Definition definition;
      Special_Subset seen(*mistakes);

      if (mistakes->state_count() > 2)
      {
        for (Word_Length max_prefix = 0;count_down >= 0;max_prefix++)
        {
          for (State_ID si = wi.first();
               si && count_down >= 0;
               si = wi.next(wi.word.length() < max_prefix))
          {
            if (wi.word.length() == max_prefix && !seen.contains(si))
            {
              lhs_word = wi.word;
              seen.include(si);
              while (!mistakes->is_accepting(si))
              {
                if (mistakes->get_accept_definition(&definition,si))
                {
                  lhs_word.append(Ordinal(definition.symbol_nr));
                  si = mistakes->new_state(si,definition.symbol_nr);
                }
                else
                {
                  si = 0;
                  break;
                 }
              }
              if (si && words_done.insert(lhs_word))
              {
                sdr.reduce(&rhs_word,lhs_word);
                if (rm->add_correction(lhs_word,rhs_word))
                {
                  corrections++;
                  if (rm->is_dm_changing(false,0) && count_down < 60)
                    count_down = count_down < 50 ? count_down + 10 : 60;
                  else if (rm->is_dm_changing(false,GWD_KNOWN_TRANSITIONS) && count_down < 60 && running < 60)
                    count_down = count_down < 58 ? count_down + 2 : 60;
                }
              }
            }
            if (!++loops &&
                container.status(2,1,"Searching FSA for primary differences.\n"
                                 "Corrections=" FMT_ID "/" FMT_ID ". Timeout %d."
                                 " Length %d/%d Visited:" FMT_ID "/" FMT_ID "\n",
                                 corrections,words_done.count(),
                                 count_down,max_prefix,lhs_word.length(),
                                 seen.count(),mistakes->state_count()))
            {
              running++;
              count_down--;
            }
          }
        }
      }
      delete mistakes;
    }

    void try_weak_acceptor()
    {
      if (!wa_stable && !weak_acceptor_tried && !is_weak_acceptor || last_weak_word_acceptor)
      {
        FSA_Simple *outer_word_acceptor = last_weak_word_acceptor;
        last_weak_word_acceptor = 0;
        if (!outer_word_acceptor)
        {
          if (!dm1)
          {
            dm1 = rm->grow_wd(CD_STABILISE,GWD_KNOWN_TRANSITIONS);
            if (dm1)
              dm1 = maf.tidy_difference_machine(dm1);
          }
          time_t now = time(0);
          outer_word_acceptor = maf.build_acceptor_from_dm(dm1,false);
          rm->set_timeout(wa_build_time = time(0) - now);
          weak_wa_build_time = wa_build_time;
        }
        FSA_Simple * mistakes =
          FSA_Factory::and_not_first_trim(*outer_word_acceptor,*word_acceptor);
        delete outer_word_acceptor;
        process_mistakes(mistakes);
        weak_acceptor_tried = true;
      }
    }

    int try_validate_dm()
    {
      /* validate_difference_machine() is equivalent to checking the
         multiplier: if it returns 1 then a multiplier built from that
         dm will pass the checks.
         So this function returns 1 if it turns out we have a valid dm and
         acceptor and 0 otherwise.
      */
      if (!use_dm1)
      {
        int dm_ok = ga.validate_difference_machine(dm2,word_acceptor,rm);
        if (dm_ok == 1)
        {
          /* we would not have got here unless the multiplier was going to be
             big. Since dm2 appears to be OK let us try building the multiplier
             with dm1 since we expect that to be a lot easier */
          if (!dm1_tried)
            use_dm1 = true;
          return 1;
        }
        if (dm_ok == -2)
          resume_kb = true;
        else if (dm_ok == -1)
          rm->explore_acceptor(word_acceptor,dm2,is_finite,explore_time);
        return 0;
      }

      if (!dm1)
      {
        dm1 = rm->grow_wd(CD_STABILISE,GWD_KNOWN_TRANSITIONS);
        if (dm1)
          dm1 = maf.tidy_difference_machine(dm1);
      }

      if (ga.validate_difference_machine(dm1,word_acceptor,rm) == 1)
        return 1;
      return 0;
    }

    bool build_multiplier()
    {
      tried_multiplier = true;
      last_multiplier_size = 0;
      time_t now = time(0);
      FSA_Simple * gm_fsa = 0;
      bool use_finite = force_finite;

      if (last_weak_word_acceptor)
      {
        /* We may have a left over weak word-acceptor. We don't need it
           any more */
        delete last_weak_word_acceptor;
        last_weak_word_acceptor = 0;
      }

      if (!use_finite && pd.is_coset_finite)
        use_finite = language_size <
                     (Language_Size) expected_multiplier_size/(maf.generator_count()+1);
      if (use_finite)
      {
        gm_fsa = ga.build_finite_gm(rm,word_acceptor,dm2);
        if (!gm_fsa)
        {
          if (++multiplier_attempts > maf.options.max_multiplier_attempts)
          {
            maf.aborting = true;
            impossible = true;
          }
          return false;
        }
        if (pd.is_coset_system && !stats.coset_complete)
        {
          wa_correct = false;
          stats.coset_complete = true;
        }
        force_finite = true;
      }
      else
      {
        if (use_dm1)
        {
          if (!dm1)
          {
            dm1 = rm->grow_wd(CD_STABILISE,GWD_KNOWN_TRANSITIONS);
            if (dm1)
              dm1 = maf.tidy_difference_machine(dm1);
          }
          dm1_tried = true;
        }

        ga.build_gm(&gm_fsa,rm,word_acceptor,use_dm1 ? dm1 : dm2,false,
                    &last_multiplier_size);
      }

      gm_build_time = time(0) - now;
      rm->set_timeout(gm_build_time);

      if (!gm_fsa)
      {
        if (!cheated && !use_finite)
        {
          /* we won't usually get here, since we generally build a word
             acceptor from a strong dm, but we can get here if we used a
             weak acceptor, or kept our old acceptor when we should not have */
          container.progress(1,"Failed to build multiplier - word-acceptor is "
                               "incorrect.\nAttempting to make corrections\n");
          ga.build_gm(&gm_fsa,rm,word_acceptor,dm2,true);
        }
        if (!use_finite)
        {
          delete word_acceptor;
          word_acceptor = 0;
        }
        return false;
      }
      multiplier = new General_Multiplier(*gm_fsa,true);
      return true;
    }

    int multiplier_valid()
    {
      if (force_finite)
        return 1; // if we have built a gm with build_finite_gm() it is surely correct
      time_t now = time(0);
      int multiplier_state = Group_Automata::gm_valid(*multiplier,rm,use_dm1 ?
                                                    GWD_KNOWN_TRANSITIONS : 0);
      now = time(0) - now;

      if (multiplier_state < 0)
      {
        if (!tried_validate_dm) /* if we did validate the dm we may have made
                                   all the corrections we could already */
        {
          delete multiplier;
          multiplier = 0;
          resume_kb = true; // we did not manage to correct it - should not be
          return -1;        // possible unless the multiplier is only wrong for
                            // words too long to form equations from
        }
        multiplier_state = 0;
      }

      if (multiplier_state == 0)
      {
        delete multiplier;
        multiplier = 0;
        if (++multiplier_attempts > maf.options.max_multiplier_attempts)
        {
          impossible = true;
          maf.aborting = true;
          return -1;
        }
        return 0;
      }
      return 1;
    }

    bool axiom_check()
    {
      if (pd.is_coset_system)
      {
        FSA_Simple * temp = FSA_Factory::determinise_multiplier(*multiplier);
        General_Multiplier dgm(*temp,true); /* dgm now owns temp and will
                                               delete it*/
        return maf.check_axioms(dgm,CA_Hybrid,maf.options.validate_inverses);
      }
      return maf.check_axioms(*multiplier,CA_Hybrid,maf.options.validate_inverses);
    }

};

bool Group_Automata::build_vital(Rewriter_Machine * rm,bool is_confluent,
                                 int action)
{
  /* We used to set maf.is_confluent here, but that is not correct.
     The rewriting system in MAF is only confluent if it was to start
     off with as we have not changed it. We also used to look in rm.status()
     to see if we have got to confluence, but we know this from the
     second parameter to build_vital() */

  Vital_Builder vb(*this,rm,is_confluent,action);
  return vb.process();
}

/**/

bool Group_Automata::load_vital(MAF & maf)
{
  maf.load_fsas(GA_GM);
  maf.load_fsas(GA_WA);
  maf.load_fsas(GA_DIFF2);
  word_acceptor = maf.fsas.wa;
  dm2 = maf.fsas.diff2;
  multiplier = maf.fsas.gm;
  owner = false;
  return word_acceptor && dm2 && multiplier;
}

/**/

void Group_Automata::erase()
{
  if (owner)
  {
    if (word_acceptor)
    {
      delete word_acceptor;
      word_acceptor = 0;
    }
    if (dm1)
    {
      delete dm1;
      dm1 = 0;
    }
    if (dm2)
    {
      delete dm2;
      dm2 = 0;
    }
    if (multiplier)
    {
      delete multiplier;
      multiplier = 0;
    }
    if (rws)
    {
      delete rws;
      rws = 0;
    }
  }
}

/**/

void Group_Automata::grow_automata(Rewriter_Machine * rm,
                                   FSA_Buffer * buffer,
                                   unsigned write_flags,
                                   unsigned retain_flags,
                                   unsigned exclude_flags)
{
  String_Buffer sb;
  MAF & maf = rm->maf;
  Container & container = maf.container;
  const Presentation_Data & pd = maf.properties();
  Rewriter_Machine::Status &stats = rm->status();
  FSA_Simple * L1_acceptor = 0;
  FSA_Simple * primary_recogniser = 0;
  FSA_Simple * diff1c = 0;
  FSA_Simple * diff2c = 0;
  FSA_Simple * equation_recogniser = 0;

  if (retain_flags == 0 && write_flags == 0)
    write_flags = GA_ALL; // This is a kludge for kbprog and kbprogcos

  if (!buffer)
    retain_flags = 0;

  unsigned all_flags;
  if (maf.is_coset_system)
  {
    if (stats.want_differences)
      all_flags = GA_RWS | GA_MAXRWS| GA_WA | GA_DIFF2 | GA_GM | GA_MAXKB |
                    GA_DIFF2C | GA_SUBPRES | GA_SUBWA | GA_DGM |
                    GA_L1_ACCEPTOR | GA_MINKB | GA_DIFF1C | GA_RR;
    else
      all_flags = GA_SUBPRES|GA_RWS|GA_MAXRWS|GA_WA|GA_L1_ACCEPTOR;
   if (pd.is_coset_finite)
     all_flags |= GA_COSETS;
  }
  else if (!stats.want_differences)
    all_flags = GA_RWS|GA_MAXRWS|GA_WA|GA_L1_ACCEPTOR;
  else
    all_flags = GA_RWS | GA_MAXRWS| GA_WA | GA_DIFF2 | GA_GM | GA_MAXKB |
                GA_DIFF2C | GA_L1_ACCEPTOR | GA_MINKB | GA_DIFF1C | GA_RR;

  all_flags &= ~exclude_flags;
  if (write_flags == GA_ALL)
    write_flags = all_flags;
  if (retain_flags == GA_ALL)
    retain_flags = all_flags;

  bool provisional = ((write_flags & GA_PDIFF2)!=0 || maf.aborting);
  if (write_flags & GA_PDIFF2 && !dm2)
    write_flags &= ~(GA_DIFF2|GA_PDIFF2);

  unsigned needed_flags = write_flags | retain_flags;
  if (needed_flags & GA_DIFF2 && maf.options.emulate_kbmag)
    needed_flags |= GA_DIFF1C;


  if (word_acceptor)
  {
    if (maf.aborting && !stats.complete)
      maf.save_fsa(word_acceptor,".pwa"); /* In this case the word-acceptor is
                                             probably correct, because it was
                                             stable, but we could not build a
                                             multiplier */
    else if (write_flags & GA_WA)
      maf.save_fsa(word_acceptor,GAT_WA);
    if (retain_flags & GA_WA && owner)
      buffer->wa = FSA_Factory::copy(*word_acceptor);
  }

  if (dm2)
  {
    if (write_flags & GA_DIFF2)
    {
      if (provisional && !maf.options.emulate_kbmag)
        maf.save_fsa(dm2,GAT_Provisional_DM2);
      else
      {
        maf.save_fsa(dm2,GAT_Full_Difference_Machine);
        maf.delete_fsa(GAT_Provisional_DM2);
      }
    }
    if (retain_flags & GA_DIFF2)
      buffer->diff2 = FSA_Factory::copy(*dm2);
  }
  else if (needed_flags & GA_DIFF2)
  {
    if (maf.is_group)
    {
      if (!maf.is_shortlex)
      {
        container.error_output("Words are not ordered in shortlex sequence.\n");
        container.error_output("Difference machines are only created for"
                               " shortlex groups unless\n-forcedifferences"
                               " is specified\n");
      }
      else if (!stats.want_differences)
        container.error_output("The -nowd option was specified, so word-differences were not calculated\n");
    }
    else if (maf.is_coset_system)
    {
      if (!stats.want_differences)
        container.error_output("The -nowd option was specified, so word-differences were not calculated\n");
    }
    else
      container.error_output("The presentation was not a group or coset system"
                             " presentation.\n");
  }

  if (maf.aborting && dm1 && !stats.complete)
  {
    if (write_flags & GA_DIFF2)
    {
      String suffix;
      if (maf.options.emulate_kbmag)
      {
        String suffix = maf.is_coset_system ? ".midiff1" : ".diff1";
        maf.save_fsa(dm1,suffix);
      }
      else
        maf.save_fsa(dm1,GAT_Provisional_DM1);
    }
  }

  if (multiplier)
  {
    if (write_flags & GA_GM)
      maf.save_fsa(multiplier,GAT_General_Multiplier);
    if (retain_flags & GA_GM)
      buffer->gm = new General_Multiplier(*multiplier);
    if (maf.options.write_success)
    {
      String filename = sb.make_filename("", maf.filename,".success");
      Output_Stream * success_file = container.open_text_output_file(filename,false);
      container.output(success_file,"0;\n");
      container.close_output_file(success_file);
    }
    // We create a new rws containing the set of equations that proved the
    // correct word-difference machine.
    if (!rws && (needed_flags & GA_RWS))
      rws = new Rewriting_System(rm,RWSC_MINIMAL|RWSC_CRITICAL_ONLY);
  }
  else if (needed_flags & GA_GM)
  {
    if (!dm2)
      container.error_output("Multipliers cannot be calculated without a word"
                             "difference machine.\n");
    else if (!maf.aborting)
      container.error_output("Either the multiplier is too big to build or the"
                             " number of word-differences is not finite\n");
  }

  if (needed_flags & GA_DGM)
  {
    if (multiplier)
    {
      FSA_Simple * answer = FSA_Factory::determinise_multiplier(*multiplier);
      if (write_flags & GA_DGM)
        maf.save_fsa(answer,GAT_Deterministic_General_Multiplier);
      if (retain_flags & GA_DGM)
        buffer->dgm = new General_Multiplier(*answer,true);
      else
        delete answer;
    }
    else
      container.error_output("The MIDFA general multiplier does not exist,"
                               " so the deterministic general multiplier"
                               " cannot be computed\n");
  }

  if (rws)
  {
    if (write_flags & GA_RWS)
    {
      if (maf.name != 0)
        rws->set_name(maf.name);
      bool want_rws = stats.complete ||
                      maf.is_coset_system && stats.coset_complete ||
                      provisional && dm1==0 && dm2==0;
      if (want_rws && provisional && dm2)
        provisional = false;
      String filename = sb.make_filename("", maf.filename,provisional &&
                                         !maf.options.emulate_kbmag ?
                                         ".preduce" : ".reduce");
      maf.delete_fsa(GAT_Provisional_RWS);
      Output_Stream * rws_stream = want_rws ?
                                 container.open_text_output_file(filename) : 0;
      if (needed_flags & GA_RWS && !rws_stream)
        container.error_output("The rewriting system is not confluent"
                               " so no .reduce file was created.\n");
      String suffix = ".kbprog";
      if ((!rws_stream || provisional) && !maf.options.emulate_kbmag)
        suffix = provisional ? ".pkbprog" : ".akbprog";
      filename = sb.make_filename("", maf.filename,suffix);
      Output_Stream * kb_stream = container.open_text_output_file(filename);
      rws->print(kb_stream,rws_stream);
      if (rws_stream)
        container.close_output_file(rws_stream);
      container.close_output_file(kb_stream);
    }
    if (retain_flags & GA_RWS)
      buffer->min_rws = new Rewriting_System(*rws);
  }

  bool need_pr = needed_flags & GA_MINKB ||
                 needed_flags & GA_DIFF1C && !(stats.complete && dm2);

  FSA_Simple * reduction_recogniser = 0;
  if (needed_flags & GA_DIFF2C+GA_RR && multiplier && dm2)
    reduction_recogniser = reducer(maf,*multiplier,*dm2,false);
  if (reduction_recogniser)
  {
    if (write_flags & GA_RR)
      maf.save_fsa(reduction_recogniser,GAT_Reduction_Recogniser);
    if (needed_flags & GA_DIFF2C)
    {
      diff2c = FSA_Factory::merge(*reduction_recogniser);
      diff2c = maf.tidy_difference_machine(diff2c);
    }
  }

  if (diff2c)
  {
    container.progress(1,"The correct difference machine has " FMT_ID " states.\n",
                       diff2c->state_count());

    if (write_flags & GA_DIFF2C)
      maf.save_fsa(diff2c,GAT_Correct_Difference_Machine);
    if (retain_flags & GA_DIFF2C)
      buffer->diff2c = diff2c;
  }
  else if (needed_flags & GA_DIFF2C)
    container.error_output("The correct difference machine could not be created\n");

  if (needed_flags & GA_MAXKB)
  {
    if (reduction_recogniser)
      equation_recogniser = build_equation_recogniser_from_rr(*reduction_recogniser);
    else if (multiplier && dm2)
      equation_recogniser = reducer(maf,*multiplier,*dm2,true);
  }

  if (reduction_recogniser)
  {
    if (retain_flags & GA_RR)
      buffer->rr = reduction_recogniser;
    else
      delete reduction_recogniser;
  }

  if (equation_recogniser)
  {
    Language_Size er_count = equation_recogniser->language_size(true);
    if (er_count == LS_INFINITE)
    {
      if (stats.complete)
        container.progress(1,"There are infinitely many L2 equations: "
                             "the fast RWS is not confluent\n");
    }
    else if (er_count == LS_HUGE)
    {
      if (stats.complete)
        container.progress(1,"There are very many L2 equations: "
                             "the fast rewriting system is too big to calculate.\n");
    }
    else
      container.progress(1,"There are " FMT_LS " equations in the fast rewriting system.\n",er_count);
    container.progress(1,"The equation recogniser has " FMT_ID " states.\n",
                       equation_recogniser->state_count());

    if (write_flags & GA_MAXKB)
      maf.save_fsa(equation_recogniser,GAT_Equation_Recogniser);
    if (retain_flags & GA_MAXKB)
      buffer->maxkb = equation_recogniser;

  }
  else if (needed_flags & GA_MAXKB)
    container.error_output("The complete equation recogniser could not be created\n");

  if ((needed_flags & GA_MINRED || need_pr) && word_acceptor)
    if (!maf.is_coset_system)
    {
      if (stats.complete || !maf.aborting)
        L1_acceptor = build_L1_acceptor(rm,word_acceptor,word_acceptor,false);
    }
    else if (stats.complete || stats.coset_complete || dm2)
    {
      FSA_Simple * gwa;
      if (!stats.complete && !stats.coset_complete)
        gwa = maf.build_acceptor_from_dm(dm2,false,false,true);
      else
      {
        container.progress(1,"Building word-acceptor from confluent RWS\n");
        FSA_Simple * temp = FSA_Factory::copy(*rws->fsa());
        temp->remove_rewrites();
        gwa = FSA_Factory::restriction(*temp,maf.group_alphabet());
        delete temp;
      }
      L1_acceptor = build_L1_acceptor(rm,word_acceptor,gwa,false);
      delete gwa;
    }
  Language_Size L1_count = 0;
  if (L1_acceptor)
  {
    L1_count = L1_acceptor->language_size(true);
    if (L1_count == LS_INFINITE)
      container.progress(1,"The L1 language is infinite, so the minimal "
                           "rewriting system is not confluent\n");
    else if (L1_count == LS_HUGE)
      container.progress(1,"The L1 language is huge so the minimal rewriting"
                           " system cannot be computed\n");
    else
      container.progress(1,"The L1 language contains " FMT_LS " words\n",L1_count);
    container.progress(1,"The L1 acceptor has " FMT_ID " states.\n",
                         L1_acceptor->state_count());
    if (write_flags & GA_L1_ACCEPTOR)
      maf.save_fsa(L1_acceptor,GAT_L1_Acceptor);
    if (retain_flags & GA_L1_ACCEPTOR)
      buffer->minred = L1_acceptor;
  }
  else if (needed_flags & GA_L1_ACCEPTOR)
    container.error_output("The L1 acceptor could not be created\n");

  if (need_pr && L1_acceptor && word_acceptor &&
      (equation_recogniser || dm2 && multiplier || stats.complete && (dm1 || dm2)))
  {
    if (equation_recogniser)
      primary_recogniser = build_primary_recogniser(L1_acceptor,word_acceptor,
                                                    equation_recogniser);
    else
      primary_recogniser = build_primary_recogniser(L1_acceptor,word_acceptor,
                                                    stats.complete && dm1 ?
                                                    dm1 : (diff2c ? diff2c : dm2));
  }

  if (equation_recogniser && !(retain_flags & GA_MAXKB))
    delete equation_recogniser;

  if (primary_recogniser)
  {
    /* In here I used to check that the size of the primary recogniser's language was the same as the
       size of the L1 language. I commented this out but did not give a reason. I think that there
       is a problem when we are dealing with coset systems */
    container.progress(1,"Primary equation recogniser has " FMT_ID " states.\n",
                       primary_recogniser->state_count());

    if (write_flags & GA_MINKB)
      maf.save_fsa(primary_recogniser,GAT_Primary_Recogniser);
    if (retain_flags & GA_MINKB)
      buffer->minkb = primary_recogniser;
  }
  else if (needed_flags & GA_MINKB)
  {
    container.error_output("The primary recogniser could not be created\n");
    if (!dm2)
      container.error_output("The required difference machine does not exist\n");
    if (!L1_acceptor || !word_acceptor)
      container.error_output("The required word-acceptors do not exist\n");
    if (dm2 && L1_acceptor && word_acceptor && !multiplier)
      container.error_output("This FSA is probably too large to calculate\n");
  }

  if (diff2c && !(retain_flags & GA_DIFF2C))
    delete diff2c;

  if (L1_acceptor && !(retain_flags & GA_L1_ACCEPTOR))
    delete L1_acceptor;

  if (needed_flags & GA_DIFF1C)
  {
    if (primary_recogniser || stats.complete && dm2)
    {
      if (primary_recogniser)
      {
        diff1c = FSA_Factory::merge(*primary_recogniser);
        diff1c = maf.tidy_difference_machine(diff1c);
      }
      else if (dm1 && stats.complete && maf.aborting)
      {
        diff1c = dm1;
        dm1 = 0;
      }
      else
      {
        diff1c = rm->grow_wd(CD_STABILISE,GWD_KNOWN_TRANSITIONS|GWD_PRIMARY_ONLY);
        if (diff1c)
         diff1c = maf.tidy_difference_machine(diff1c);
      }
    }
  }

  if (diff1c)
  {
    container.progress(1,"Correct primary difference machine has " FMT_ID " states.\n",diff1c->state_count());

    if (write_flags & GA_DIFF1C)
    {
      maf.save_fsa(diff1c,GAT_Primary_Difference_Machine);
      maf.delete_fsa(GAT_Provisional_DM1);
    }
    if (maf.options.emulate_kbmag)
      maf.save_fsa(diff1c,maf.is_coset_system ? ".midiff1" : ".diff1");
    if (retain_flags & GA_DIFF1C)
      buffer->diff1c = diff1c;
    else
      delete diff1c;
  }
  else if (needed_flags & GA_DIFF1C)
    container.error_output("The correct primary difference machine could not be created\n");

  if (primary_recogniser && !(retain_flags & GA_MINKB))
    delete primary_recogniser;
  if (needed_flags & GA_SUBPRES && maf.is_coset_system && !maf.aborting)
  {
    MAF * maf_sub = 0;
    if (multiplier)
      maf_sub = maf.subgroup_presentation(*multiplier,
                                          maf.options.use_schreier_generators);
    if ((stats.coset_complete || pd.is_coset_finite) &&
        !maf.options.use_schreier_generators &&
        pd.presentation_type == PT_Coset_System_With_Inverses &&
        rws->fsa()!=0 && !maf_sub)
      maf_sub = maf.subgroup_presentation(*rws,false);

    if (maf_sub)
    {
      if (!maf.options.emulate_kbmag)
      {
        String filename = sb.make_filename("",maf.subgroup_filename !=0 ?
                                           maf.subgroup_filename.string() :
                                           maf.filename.string(),
                                           ".rws");
        Output_Stream * os = container.open_text_output_file(filename);
        maf_sub->print(os);
        container.close_output_file(os);
      }
      String filename = sb.make_filename("",maf.subgroup_filename !=0 ?
                                         maf.subgroup_filename.string() :
                                         maf.filename.string(),
                                         ".pres");
      Output_Stream * os = container.open_text_output_file(filename);
      maf_sub->output_gap_presentation(os,true);
      container.close_output_file(os);
      delete maf_sub;
    }
  }

  if (stats.complete && !maf.aborting && needed_flags & GA_MAXRWS)
  {
    Rewriting_System * fast_rws = new Rewriting_System(rm,RWSC_NEED_FSA);
    if (write_flags & GA_MAXRWS)
    {
      if (maf.name != 0)
        fast_rws->set_name(maf.name);
      String filename = sb.make_filename("", maf.filename,".fastreduce");
      Output_Stream * rws_stream = stats.complete ?
                                container.open_text_output_file(filename) : 0;
      filename = sb.make_filename("", maf.filename,".fastkbprog");
      Output_Stream * kb_stream = container.open_text_output_file(filename);
      fast_rws->print(kb_stream,rws_stream);
      container.close_output_file(rws_stream);
      container.close_output_file(kb_stream);
    }
    if (retain_flags & GA_RWS)
      buffer->fast_rws = fast_rws;
    else
      delete fast_rws;
  }

  FSA_Simple *cosets = 0;
  const FSA_Buffer & group_fsas = maf.group_fsas();
  if (needed_flags & GA_SUBWA)
    if (!maf.load_group_fsas(GA_WA))
    {
      container.error_output("Unable to create subgroup word-acceptor until"
                             " group word-acceptor is computed.\n");
      needed_flags &= ~GA_SUBWA;
    }

  if (needed_flags & GA_COSETS+GA_SUBWA && maf.is_coset_system &&
      pd.is_coset_finite && !maf.aborting && word_acceptor)
  {
    RWS_Reducer coset_wr(*rws);
    cosets = maf.coset_table(coset_wr,*word_acceptor);
    if (cosets)
    {
      if (write_flags & GA_COSETS)
        maf.save_fsa(cosets,GAT_Coset_Table);
      if (retain_flags & GA_COSETS)
        buffer->cosets = cosets;
    }
  }

  FSA_Simple * subwa = 0;
  if (needed_flags & GA_SUBWA && maf.is_coset_system && !maf.aborting)
  {
    if (cosets && word_acceptor)
      subwa = FSA_Factory::fsa_and(*group_fsas.wa,*cosets);
    else if (dm2)
    {
      Diff_Reduce coset_wr(dm2);
      subwa = maf.subgroup_word_acceptor(coset_wr,dm2);
    }
  }

  if (subwa)
  {
    Language_Size subwa_count = subwa->language_size(true);
    if (subwa_count == LS_INFINITE)
      container.progress(1,"The subgroup is infinite\n");
    else if (subwa_count == LS_HUGE)
      container.progress(1,"The subgroup is very large but finite\n");
    else
      container.progress(1,"The subgroup contains " FMT_LS " elements\n",subwa_count);
    container.progress(1,"The subgroup word-acceptor has " FMT_ID " states.\n",
                       subwa->state_count());

    if (write_flags & GA_SUBWA)
      maf.save_fsa(subwa,GAT_Subgroup_Word_Acceptor);

    if (retain_flags & GA_MAXKB)
      buffer->subwa = subwa;
    else
      delete subwa;
  }

  if (cosets && !(retain_flags & GA_COSETS))
    delete cosets;

  if (maf.options.is_kbprog && needed_flags & GA_DIFF2)
  {
    /* we generate this for compatibility with KBMAG. MAF does not use this
       file at all, and won't need it in its GAP interface */
    String filename = sb.make_filename("", maf.filename,".kbprog.ec");
    Output_Stream * success_file = container.open_text_output_file(filename,false);
    container.output(success_file,"_ExitCode := %d;\n",maf.aborting ? 2 : 0);
    container.close_output_file(success_file);
  }

}

bool Group_Automata::transfer(FSA_Buffer * buffer)
{
  if (buffer && owner)
  {
    if (dm2)
    {
      if (buffer->diff2)
        delete buffer->diff2;
      buffer->diff2 = dm2;
    }
    if (word_acceptor)
    {
      if (buffer->wa)
        delete buffer->wa;
      buffer->wa = word_acceptor;
    }
    if (multiplier)
    {
      if (buffer->gm)
        delete buffer->gm;
      buffer->gm = multiplier;
    }
    if (rws)
    {
      if (buffer->min_rws)
        delete buffer->min_rws;
      buffer->min_rws = rws;
    }
    if (dm1)
    {
      delete dm1;
      dm1 = 0;
    }
    owner = false;
    return true;
  }
  return false;
}
