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


// $Log: maf_subwa.cpp $
// Revision 1.12  2011/06/11 12:03:24Z  Alun
// Added new code for translating a word-acceptor, which does things properly
// Revision 1.11  2010/06/10 13:57:42Z  Alun
// All tabs removed again
// Revision 1.10  2010/05/17 00:06:11Z  Alun
// June 2010 version
// Revision 1.9  2009/09/13 19:14:55Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.8  2009/09/02 06:30:58Z  Alun
// Did not cope with normal subgroup coset systems
// Revision 1.7  2008/11/03 16:29:56Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.8  2008/11/03 17:29:55Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.7  2008/10/09 17:04:53Z  Alun
// Code previously assumed that the coset word-difference machine was adequate
// for reducing group words as well
// Revision 1.6  2008/09/29 19:13:31Z  Alun
// Switch to using Hash+Collection_Manager as replacement for Indexer.
// Currently this is about 10% slower, but this is a more flexible
// architecture.
// Revision 1.2  2007/10/24 21:15:30Z  Alun
//

#include <stdlib.h>
#include "container.h"
#include "keyedfsa.h"
#include "maf.h"
#include "mafword.h"
#include "maf_dr.h"
#include "maf_wdb.h"
#include "equation.h"
#include "maf_ss.h"
#include "maf_spl.h"

/* Class Subgroup_Word_Acceptor_Factory attempts to build an FSA which will
enumerate precisely those words in a group which are reduced as group words
and are members of the subgroup described in the .sub file being processed.

In order to construct this FSA we first create an FSA which attempts to
track the coset words in the subgroup belong to as they are multiplied by
a G word that is one of the generators. This is a "subgroup member recogniser"
and does not care about whether words are reduced or not, and ideally should,
and often in practice does, never reach the failure state. (For a finite-index
subgroup the coset table correctly recognises any word that is a subgroup
element, but we may not actually need the whole of this. And of course the
coset table does not exist for infinite index subgroups). We then form an
actual word-acceptor that rejects reducible words, by anding this FSA with
the ordinary word-acceptor for the group.

The resulting trial word-acceptor FSA will probably not be correct initially,
and will fail to accept some words which it should accept.

We can find some of these words by forming the composite multiplier for the
subgroup generators and checking the "and" of this with our trial word
acceptor. This will most likely find more words, which we then teach our
acceptor, until eventually, we hope, it is closed. This process won't
necessarily complete. It is perfectly possible for a subgroup of an automatic
group not to be automatic.

To construct our initial membership FSA we start from the list of subgroup
generators, and ask the FSA to learn to accept these words. For each word we
examine that changes the membership FSA we also add all its right multiples
and their inverses to the list of words to be tested.

Note that, although all the words we are dealing with are words in the "g"
generators, we have to have two word reduction mechanisms available: one
for coset reduction, and one for group word reduction. Although the
Diff_Reduce version of word reduction notionally supports reduction using
only its 1 state (i.e. reduction as a group element, not as a coset), it
may not produce the correct answer, since not all the word-differences
are needed. So, since we need the group multiplier anyway, we use that for
reduction. It might be quicker to use RWS reduction where available, but this
could increase memory usage a lot. In practice GM reduction is usually fast
anyway.
*/

class Subgroup_Word_Acceptor_Factory
{
  private:
    FSA_Simple * answer;
    Keyed_FSA * coset_membership;
    Word_DB subgroup_words;
    Element_ID test_sequence;
  public:
    MAF & maf;
    FSA * wa;
    FSA * composite_multiplier;
    const Alphabet & alphabet;
    const Alphabet & full_alphabet;
    const Presentation_Data &pd;
    Sorted_Word_List generators;
    Ordinal nr_sub_generators;
    Container &container;
  public:
    bool add_missing_words(FSA_Simple *trial_acceptor,
                           FSA_Simple * exists,
                           const Ordinal_Word &multiplier_word,
                           Word_Reducer & group_wr)
    {
      /* on entry trial_acceptor is our candidate subgroup word-acceptor
         and exists is the left or right exists for the intersection of
         this with a group multiplier for a subgroup generator.
         We find the words this is not accepting that it should and schedule
         these to be learned.
         The input "exists" FSA is deleted ASAP.
         The return value is true if words that should be accepted are not.
      */

      Ordinal_Word test_word(alphabet);
      bool retcode = false;
      FSA_Simple *missing = FSA_Factory::and_not_first(*trial_acceptor,
                                                       *exists);
      delete exists;
      if (missing->language_size(false) != 0)
      {
        retcode = true;
        FSA::Word_Iterator wi(*missing);
        unsigned added = 0;
        int count_down = 10;
        int running = 0;
        State_Count nr_states = missing->state_count();
        char loops = 0;
        missing->create_accept_definitions();
        Accept_Definition definition;
        Special_Subset seen(*missing);
        bool again = true;

        for (Word_Length max_prefix = 0;count_down >= 0 && !maf.aborting && again;max_prefix++)
        {
          again = false;
          for (State_ID si = wi.first();
               si && count_down >= 0 && !maf.aborting;
               si = wi.next(wi.word.length() < max_prefix))
          {
            if (wi.word.length() == max_prefix && !seen.contains(si))
            {
              again = true;
              test_word = wi.word;
              seen.include(si);
              while (!missing->is_accepting(si))
              {
                if (missing->get_accept_definition(&definition,si))
                {
                  test_word.append(Ordinal(definition.symbol_nr));
                  si = missing->new_state(si,definition.symbol_nr);
                }
                else
                {
                  si = 0;
                  break;
                }
              }

              test_word += multiplier_word;
              group_wr.reduce(&test_word,test_word);

              if (si && subgroup_words.insert(test_word))
                if (++added >= 5000)
                  count_down = 0;
             if (seen.count() == nr_states-1)
               count_down = 0;
            }
            if (!++loops &&
                container.status(2,1,"Correcting membership FSA with"
                                     " unrecognised subgroup elements.\n"
                                     "Corrections=%u/" FMT_ID ". Timeout %d."
                                     " Length %d/%d Visited:"
                                     FMT_ID "/" FMT_ID "\n",
                                 added,subgroup_words.count(),
                                 count_down,max_prefix,test_word.length(),
                                 seen.count(),missing->state_count()))
            {
              running++;
              count_down--;
            }
          }
        }
      }
      delete missing;
      return retcode;
    }

    void add_to_generators(Ordinal_Word *sub_gen,Word_Reducer &group_wr)
    {
      Ordinal_Word word(*sub_gen);
      group_wr.reduce(&word,word);
      *sub_gen = word;
      generators.insert(*sub_gen);
    }

    Subgroup_Word_Acceptor_Factory(MAF & maf_,
                                   Word_Reducer & coset_wr,
                                   const FSA * coset_dm) :
      maf(maf_),
      alphabet(maf_.group_alphabet()),
      full_alphabet(maf_.alphabet),
      generators(maf_.group_alphabet()),
      pd(maf_.properties()),
      container(maf_.container),
      test_sequence(0),
      subgroup_words(maf_.group_alphabet(),1024),
      composite_multiplier(0),
      answer(0)
    {
      coset_membership = new Keyed_FSA(container,alphabet,
                                             alphabet.letter_count(),
                                             1024,0);
      // we'd like to use a coset_gm reducer, but this may not exist
      Word_List wl(alphabet);
      Ordinal_Word word(alphabet);
      Ordinal_Word test_word(alphabet);
      const FSA_Buffer & group_fsas = maf.group_fsas();

      maf.load_group_fsas(GA_WA|GA_GM);
      if (!group_fsas.wa || !group_fsas.gm)
      {
        container.error_output("The automatic structure for the group has not"
                               " been computed, so it is not\npossible to"
                               " compute the subgroup word-acceptor.\n");
        return;
      }

      GM_Reducer group_wr(*group_fsas.gm);

      container.progress(1,"Building subgroup word-acceptor\n");
      /* Initialise the set of words to be tested, and the list of
         subgroup generators for forming the multiplier */

      /*
         Initialise the list of generators for the subgroup.
         For a normal coset system, we shall use the Schreier generators
         given by the initial states of the coset difference machine to do
         this. This technique will work for any kind of coset system, but
         might result in our having to create a multiplier for very many words.
         We shall append the generators specified in the axioms as well for
         when the code is invoked without a coset word-difference machine,
         or a poor quality one, or we have an ordinary coset system.

         Note that the g words we see may not correspond to the
         original h generators. For example, if the subgroup generators
         were given as a,a*b we may well see a,b instead, and
         if there was a third generator a*a we would not see that at all. */
      if (coset_dm && maf.properties().is_normal_coset_system)
      {
        const FSA & dm = *coset_dm;
        State_Subset_Iterator ssi;
        Word_List dwl(dm.label_alphabet());
        Ordinal_Word dword(dm.label_alphabet());

        for (State_ID si = dm.initial_state(ssi,true);
             si != 0;si = dm.initial_state(ssi,false))
        {
          if (dm.label_word_list(&dwl,dm.get_label_nr(si)))
          {
            unsigned count = dwl.count();
            for (unsigned word_nr = 0; word_nr < count;word_nr++)
            {
              if (dwl.get(&dword,word_nr) && dword.length() > 0 && dword.value(0) < pd.coset_symbol)
              {
                word = dword;
                add_to_generators(&word,group_wr);
                maf.invert(&word,word);
                add_to_generators(&word,group_wr);
                break; /* we only want the G word for the label */
              }
            }
          }
        }
      }

      /* Now get the generators from the axioms */
      maf.subgroup_generators(&wl);
      Element_Count count = wl.count();
      for (Element_ID i = 0; i < count;i++)
      {
        wl.get(&word,i);
        add_to_generators(&word,group_wr);
        maf.invert(&word,word);
        add_to_generators(&word,group_wr);
      }

      nr_sub_generators = Ordinal(generators.count());
      wl.empty();
      word.set_length(0);
      subgroup_words.add(word);
      wl.add(word);
      for (Ordinal i = 0; i < nr_sub_generators;i++)
      {
        generators.get(&word,i);
        subgroup_words.add(word);
        wl.add(word);
      }

      // Create the failure and initial state of the coset_membership FSA
      coset_membership->find_state(0,0);
      word.set_length(0);
      size_t size;
      Packed_Word pw(word,&size);
      coset_membership->find_state(pw,size);

      while (accept_words(group_wr,coset_wr) && !maf.aborting)
      {
        coset_membership->set_single_accepting(1);
        answer = FSA_Factory::fsa_and(*coset_membership,*group_fsas.wa);
        container.progress(1,"Subgroup word-acceptor has " FMT_ID " states\n",
                           answer->state_count());

        if (!composite_multiplier)
        {
          container.progress(1,"Constructing multiplier for subgroup generators\n");
          composite_multiplier = group_fsas.gm->composite(maf,wl);
        }
        FSA_Simple * fsa_temp =
          FSA_Factory::product_intersection(*composite_multiplier,
                                            *answer,*answer,
                                            "subgroup multiplier");
       /* fsa_temp accepts (u,v) with label w if
          uw=v and u and w are accepted by our trial subgroup word-acceptor
          Clearly for any accepted u (or v) and subgroup multiplier w
          there should be exactly one (u,v) */

        Multiplier sg_gm(*fsa_temp,true);
        unsigned count = wl.count();
        container.progress(1,"Checking multiplier with %d generators and "
                             FMT_ID " states\n",count,sg_gm.state_count());
        bool ok = true;
        for (unsigned multiplier_nr = 1; multiplier_nr < count; multiplier_nr++)
        {
          Word_List hword(alphabet);
          wl.get(&word,multiplier_nr);
          hword.add(word);
          FSA_Simple *h_multiplier = sg_gm.composite(maf,hword);

          /* we construct the left "exists" for the multiplier FSA.
             This should accept the same language as the word-acceptor.
             Any words it does not accept need to be added */
          FSA_Simple * exists = FSA_Factory::exists(*h_multiplier,false,false);
          if (add_missing_words(answer,exists,word,group_wr))
            ok = false;
          /* now we do the same for the right "exists" FSA */
          exists = FSA_Factory::exists(*h_multiplier,false,true);
          maf.invert(&word,word);
          delete h_multiplier;
          if (add_missing_words(answer,exists,word,group_wr))
            ok = false;
          if (maf.aborting)
          {
            ok = false;
            break;
          }
        }
        if (ok)
          break;
        delete answer;
        answer = 0;
      }
    }

    ~Subgroup_Word_Acceptor_Factory()
    {
      if (coset_membership)
        delete coset_membership;
      if (composite_multiplier)
        delete composite_multiplier;
    }

    FSA_Simple * take()
    {
      FSA_Simple * answer_ = answer;
      answer = 0;
      return answer_;
    }
  private:
    bool accept_words(Word_Reducer &group_wr,Word_Reducer &coset_wr)
    {
      Byte_Buffer key;
      Ordinal_Word test_word(alphabet);
      int timeout = 60;
      Ordinal_Word known_word(alphabet);

      while ((subgroup_words.get(&known_word,test_sequence)) != 0 && !maf.aborting)
      {
        test_sequence++;
        String_Buffer sb;
        State_Count count = subgroup_words.count();
        if (container.status(2,1,"Correcting membership FSA. (" FMT_ID " of "
                                 FMT_ID " words to do).\nMembership count "
                                 FMT_ID ". Word length %d. Timeout %d\n",
                             count-test_sequence,count,
                             coset_membership->state_count(),
                             known_word.length(),timeout) && timeout > 0)
          timeout--;
        int test_result = recognised(known_word,coset_wr);
        if (test_result <= 0)
        {
          if (test_result < 0)
            return false;
          if (timeout > 0)
            for (Ordinal sg = 0; sg < nr_sub_generators; sg++)
            {
              test_word = known_word + *generators.word(sg);
              group_wr.reduce(&test_word,test_word);
              subgroup_words.add(test_word);
              maf.invert(&test_word,test_word);
              group_wr.reduce(&test_word,test_word);
              subgroup_words.add(test_word);
            }
        }
      }
      return !maf.aborting;
    }
    int recognised(const Word & known_subgroup_word,Word_Reducer &coset_wr)
    {
      /* The word passed as a parameter is known to be reduced and a member
         of the subgroup. We check whether our current coset_membership
         accepts the word, and if not try to alter it so that it does.
         The return value is 1 if the word could already be recognised,
         0 if we learned it OK, and -1 if the word appears to be in another
         coset. In this case our coset reduction is wrong and we cannot
         continue. */

      Ordinal_Word test_word(full_alphabet);
      Ordinal_Word coset_word(full_alphabet);
      int answer = 1;
      Keyed_FSA & membership = *coset_membership;
      String_Buffer sb;

      /* Now form the successive coset reductions */
      test_word.set_length(1);
      test_word.set_code(0,pd.coset_symbol);
      test_word += known_subgroup_word;
      const Ordinal * values = test_word.buffer();
      Word_Length l = test_word.length();
      State_ID si = 1;
      for (Word_Length i = 1; i < l ; i++)
      {
        Transition_ID ti = values[i];
        State_ID nsi = membership.new_state(si,ti);
        if (!nsi)
        {
          coset_wr.reduce(&coset_word,Subword(test_word,0,i+1));
          Word_Length prefix_length = coset_word.find_symbol(pd.coset_symbol);
          coset_word = Subword(coset_word,prefix_length+1,WHOLE_WORD);
          size_t size;
          Packed_Word pw(coset_word,&size);
          nsi = membership.find_state(pw,size);
          membership.set_transition(si,ti,nsi);
          answer = 0;
        }
        si = nsi;
      }
      if (si != 1)
      {
        /* if we get here there is an error in the coset difference machine.
           We aren't going to worry about this, because normally this code
           will only be called using automata that have been proved to be
           correct. */
        String_Buffer sb1,sb2;
        known_subgroup_word.format(&sb1);
        coset_word.format(&sb2);
        container.error_output("%s is a member of the subgroup, but belongs"
                               " to coset %s according to the\ndifference"
                               " machine. The coset difference machine is"
                               " incorrect.\n",
                               sb1.get().string(),sb2.get().string());
        answer = -1;
      }
      return answer;
    }
};

/**/

FSA_Simple * MAF::subgroup_word_acceptor(Word_Reducer &coset_wr,
                                         const FSA * coset_dm)
{
  Subgroup_Word_Acceptor_Factory swaf(*this,coset_wr,coset_dm);
  return swaf.take();
}

/**/

FSA_Simple * MAF::translate_acceptor(const MAF & maf_new,
                                     const MAF & maf_start,
                                     const FSA & fsa_start,
                                     const Word_List &wl)
{
  /* On entry maf_new contains the presentation information for a group or
     monoid H using an alphabet with n symbols.
     fsa_start is assumed to be a word-acceptor for another monoid or group G
     accepting words in some other alphabet, and maf_start is the MAF
     object to which the acceptor is relevant. wl is a list of n words in
     the alphabet of maf_start/fsa_start which sets up a translation from the
     alphabet of maf_new to the alphabet of fsa_start and maf_start
     (presumably defining some homomorphism). Typically H is a subgroup of
     G.
     A new word-acceptor is created using the alphabet of maf_new which
     accepts a word if the original acceptor accepts the translation
     after free reduction (with a proviso explained below).
     The example below is in fact precisely the one for which this
     method has been created, since it is useful for certain Kleinian groups.
     For example, suppose the alphabet of maf_new is a,A,b,B, and
     the alphabet of fsa_start is p,q,r and that the latter are involutions.
     wl contains the words qr rq rp pr so that a=qr,A=rq etc.
     Suppose the new fsa has read and accepted the symbol B, so
     that the original acceptor has just read pr. We will allow
     the symbol A to be accepted if, prior to the original FSA
     accepting pr it would have accepted pq, since the total input is
     prrq=pq. However, we will not accept b rp, since the total input is
     then empty after cancellation. In other words we insist that at
     least some of the new input remains at each step. To be definite
     we use the following rule: as each new letter is read, cancellations
     are allowed against the previous input. At least the first letter
     of the previous input and the last letter of the new input must
     remain. We ensure the former by accepting the first letter immediately.
     For this specific example it is easy to see that the translated FSA
     is a word-acceptor for the subgroup, since any accepted word of even
     length in the original alphabet has a unique translation into the
     new alphabet, and it is never necessary to perform any more difficult
     substitution than pq->prrq=BA or qp=qrrp=ab. Given a more complicated
     translation it is not at all clear that the FSA this process builds
     will be a correct word-acceptor, as it might well be that some words
     in the original alphabet might not be easily translatable into the
     new alphabet, or that more "buffering" than is done here might be
     required.
  */
  Container & container = fsa_start.container;
  struct Key
  {
    State_ID si;
    Ordinal last_g;
    Word_Length buffer_start;
  } old_key,key;
  Ordinal nr_symbols = maf_new.generator_count();
  Keyed_FSA factory(container,maf_new.alphabet,nr_symbols,
                    fsa_start.state_count()*nr_symbols,sizeof(Key));
  Ordinal_Word last_word(maf_start.alphabet);
  State_ID * transition = new State_ID[nr_symbols];

  /* Insert failure and initial states */
  key.si = 0;
  key.last_g = PADDING_SYMBOL;
  key.buffer_start = 0;
  factory.find_state(&key);
  key.si = fsa_start.initial_state();
  factory.find_state(&key);

  State_ID si = 0;
  while (factory.get_state_key(&old_key,++si))
  {
    container.status(2,1,"Translating FSA: Processing state " FMT_ID " of " FMT_ID "\n",
                    si,factory.state_count());
    if (old_key.last_g != PADDING_SYMBOL)
      wl.get(&last_word,old_key.last_g);
    else
      last_word.set_length(0);
    for (Ordinal g = 0; g < nr_symbols;g++)
    {
      if (old_key.last_g != PADDING_SYMBOL)
      {
        if (maf_new.inverse(g) == old_key.last_g)
        {
          transition[g] = 0;
          continue;
        }
      }
      const Entry_Word new_word(wl,g);
      Word_Length old_end = last_word.length();
      Word_Length new_start = 0;
      Word_Length l = new_word.length();
      Word_Length i;
      for (i = 0; i < l;i++)
        if (old_end > old_key.buffer_start &&
            new_word.value(i)==maf_start.inverse(last_word.value(old_end-1)))
        {
          old_end--;
          new_start++;
        }
        else
          break;
      if (new_start == l)
      {
        transition[g] = 0;
        continue;
      }
      State_ID wa_si = old_key.si;
      /* Accept all that remains of the previous input now */
      for (i = old_key.buffer_start;i < old_end;i++)
      {
        wa_si = fsa_start.new_state(wa_si,last_word.value(i));
        if (!wa_si)
          break;
      }
      /* Accept the first letter of the new word now */
      if (wa_si)
        wa_si = fsa_start.new_state(wa_si,new_word.value(new_start++));
      if (!wa_si && maf_new.inverse(g) == g)
      {
        /* In this case the inverse of our translated word is equal
           to the word itself. It is possible that this might be
           accepted when the normal word is not */
        Ordinal_Word alt_word(maf_start.alphabet,l);
        maf_start.invert(&alt_word,new_word);
        old_end = last_word.length();
        new_start = 0;
        for (i = 0; i < l;i++)
          if (old_end > old_key.buffer_start &&
              alt_word.value(i)==maf_start.inverse(last_word.value(old_end-1)))
        {
          old_end--;
          new_start++;
        }
        else
          break;
        if (new_start == l)
        {
          transition[g] = 0;
          continue;
        }
        wa_si = old_key.si;
        /* Accept all that remains of the previous input now */
        for (i = old_key.buffer_start;i < old_end;i++)
        {
          wa_si = fsa_start.new_state(wa_si,last_word.value(i));
          if (!wa_si)
            break;
        }
        /* Accept all that remains of the new word now */
        if (wa_si)
        {
          for (;new_start < l;new_start++)
          {
            wa_si = fsa_start.new_state(wa_si,alt_word.value(new_start));
            if (!wa_si)
              break;
          }
        }
      }
      if (!wa_si)
      {
        transition[g] = 0;
        continue;
      }

      key.si = wa_si;
      key.last_g = g;
      key.buffer_start = new_start;
      transition[g] = factory.find_state(&key);
    }
    factory.set_transitions(si,transition);
  }

  /* Set the accept states of the new FSA. It is possible that the new
     acceptor might not be prefix closed */
  si = 0;
  while (factory.get_state_key(&old_key,++si))
  {
    container.status(2,1,"Translating FSA: Setting accept states " FMT_ID " of " FMT_ID "\n",
                    si,factory.state_count());
    State_ID wa_si = old_key.si;
    if (old_key.last_g != PADDING_SYMBOL)
      wl.get(&last_word,old_key.last_g);
    else
      last_word.set_length(0);
    Word_Length l = last_word.length();
    for (Word_Length i = old_key.buffer_start; i < l;i++)
    {
      wa_si = fsa_start.new_state(wa_si,last_word.value(i));
      if (!wa_si)
        break;
    }
    factory.set_is_accepting(si,wa_si && fsa_start.is_accepting(wa_si));
  }
  if (transition)
    delete [] transition;
  factory.remove_keys();
  return FSA_Factory::minimise(factory);
}

/**/

static FSA_Simple * improve_translation(FSA_Simple *fsa,const MAF & maf,
                                 Ordinal offset)
{
  /* This is a potentially general purpose method which replaces any
     freely reducible words in a language with their free reductions.
     If I ever get around to implementing NFA automata, then this
     method could be adapted to implement determinisation. The method
     essentially works like this:
     Separate the states so that each state is reached only with a
     single generator.
     From each state look for all the two-step transitions of the form
     g*g^-1. Add epsilon transitions to the new states.
     From each state reached with generator g replace any transition
     labelled g^-1 with a transition to the failure state.
     determinise the resulting NFA */

  class Translation_Improver
  {
    private:
      const MAF & maf;
      FSA_Simple * candidate;
      Ordinal offset;
      Transition_ID nr_transitions;
    public:
      Translation_Improver(FSA_Simple * candidate_,const MAF & maf_,
                           Ordinal offset_) :
        candidate(candidate_),
        maf(maf_),
        offset(offset_),
        nr_transitions(candidate->alphabet_size())
      {
        improve();
      }
      FSA_Simple * answer() const
      {
        return candidate;
      }
    private:
      void improve()
      {
        Container &container = maf.container;

        if (!language_is_freely_reduced())
        {
          /* Initialise the new FSA */
          State_List key;
          State_List old_key;
          State_ID ceiling = 1;
          Word_Length state_length = 0;
          Keyed_FSA factory(candidate->container,candidate->base_alphabet,nr_transitions,
                            candidate->state_count(),0);

          /* Create the failure and initial states */
          key.empty();
          key.append_one(0);
          key.append_one(0);
          factory.find_state(key.buffer(),key.size());

          key.empty();
          include(key,candidate->initial_state());
          key.append_one(0);
          key.append_one(0);
          factory.find_state(key.buffer(),key.size());

          State_Count count = factory.state_count();

          /* Now create all the new states and the transition table */
          State_ID state = 0;
          State_ID * transition = new State_ID[nr_transitions];
          while (old_key.reserve(factory.get_key_size(++state)/sizeof(State_ID),false),
                 factory.get_state_key(old_key.buffer(),state))
          {
            Word_Length length = state >= ceiling ? state_length : state_length - 1;
            if (!(state & 255))
              container.status(2,1,"Improving translation: state " FMT_ID
                                  " (" FMT_ID " of " FMT_ID " to do)."
                                  " Length %u\n",
                             state,count-state,count,length);

            State_Count nr_keys = factory.get_key_size(state)/sizeof(State_ID);
            Transition_ID last_gen = old_key.buffer()[nr_keys-1];
            for (Transition_ID ti = 0; ti < nr_transitions;ti++)
            {
              if (last_gen==0 ||
                   ti+offset != maf.inverse(last_gen-1+offset))
              {
                key.empty();
                for (State_ID *si = old_key.buffer() ; *si; si++)
                {
                  State_ID nsi = candidate->new_state(*si,ti,false);
                  if (nsi)
                    include(key,nsi);
                }
                if (key.count())
                {
                  key.append_one(0);
                  key.append_one(ti+1);
                  transition[ti] = factory.find_state(key.buffer(),key.size());
                }
                else
                  transition[ti] = 0;
                if (transition[ti] >= count)
                {
                  if (state >= ceiling)
                  {
                    state_length++;
                    ceiling = transition[ti];
                  }
                  count++;
                }
              }
              else
                transition[ti] = 0;
            }
            factory.set_transitions(state,transition);
          }
          /* we don't need this any more */
          delete [] transition;

          /* Set the accept states in the new FSA */
          state = 0;
          factory.clear_accepting(true);
          key.empty();
          while (factory.get_state_key(old_key.buffer(),++state))
          {
            if (!(char) state)
              container.status(2,1,"Setting state info: state " FMT_ID " "
                                 "(" FMT_ID " of " FMT_ID " to do).\n",
                                 state,count-state,count);

            bool accepted = false;
            for (State_ID *si = old_key.buffer() ; *si; si++)
              if (candidate->is_accepting(*si))
              {
                accepted = true;
                break;
              }
            if (accepted)
              factory.set_is_accepting(state,true);
          }
          factory.remove_keys();
          delete candidate;
          candidate = FSA_Factory::minimise(factory);
        }
      }

      bool language_is_freely_reduced() const
      {
        State_ID nsi;
        State_Count nr_states = candidate->state_count();
        for (State_ID si = 1;si < nr_states;si++)
          for (Transition_ID g = 0; g < nr_transitions;g++)
          {
            nsi = candidate->new_state(si,g);
            if (nsi && candidate->new_state(nsi,maf.inverse(g+offset)-offset))
              return false;
          }
        return true;
      }

      void include(State_List &state_list,State_ID si)
      {
        /* Find all states reachable from a given state by a sequence
           of transitions of the form g*g^-1. */
        State_List to_do;
        if (state_list.insert(si))
        {
          to_do.append_one(si);
          while (to_do.pop(&si))
          {
            for (Ordinal g = 0; g < nr_transitions; g++)
            {
              State_ID nsi = candidate->new_state(si,g);
              if (nsi)
                nsi = candidate->new_state(nsi,maf.inverse(g+offset)-offset);
              if (nsi && state_list.insert(nsi))
                to_do.append_one(nsi);
            }
          }
        }
      }
  } improver(fsa,maf,offset);
  return improver.answer();
}



/**/

FSA_Simple * MAF::translate_acceptor()
{
  /* On entry the MAF object should be a coset system with named subgroup
     generators with inverses. The subgroup should have index 1, and the
     multiplier should have been computed. In addition a word acceptor for
     the group should have been computed.
     We compute words for the old generators in the new generators.
     Then we compute a word acceptor which accepts a word if and only if
     it is a sequence of these words for old generators, and when the word
     is translated back into the original alphabet using the obvious mapping
     the word is accepted by the original acceptor.
     Then we improve the acceptor by replacing any words which can be
     freely reduced by their free reductions. */

  if (presentation_type != PT_Coset_System_With_Inverses)
  {
    container.error_output("Translation is only implemented for coset systems"
                           " with named subgenerators\nwith inverses.\n");
    return 0;
  }

  const FSA * coset_wa = load_fsas(GA_WA);

  if (!coset_wa)
  {
    container.error_output("The coset word acceptor has not been computed\n");
    return 0;
  }

  if (coset_wa->language_size() != 1)
  {
    container.error_output("The subgroup does not have index 1\n");
    return 0;
  }

  const FSA * wa = load_group_fsas(GA_WA);
  if (!wa)
  {
    container.error_output("The group word acceptor has not been computed\n");
    return 0;
  }

  const FSA *coset_gm = load_fsas(GA_GM);
  if (!coset_gm)
  {
    container.error_output("The coset general multiplier has not been computed\n");
    return 0;
  }

  if (coset_gm->label_alphabet().letter_count() < nr_generators)
  {
    container.error_output("The coset general multiplier initial states are"
                           " not labelled with words in the\nsubgroup"
                           " generators.\n");
    return 0;
  }

  if (!load_reduction_method(GAT_General_Multiplier))
  {
    container.error_output("Unexpectedly unable to perform coset word reduction\n");
    return 0;
  }

  /* We are ready to begin! */
  Ordinal_Word ow(coset_gm->label_alphabet());
  Word_List h_words(coset_gm->label_alphabet());
  Alphabet &h_alphabet = * Alphabet::create(AT_String,container);
  Ordinal nr_g_generators = coset_symbol;
  Ordinal nr_sub_generators = nr_generators - nr_g_generators - 1;
  Ordinal first_sub_generator = nr_g_generators + 1;
  Word_Length max_length = 0;
  /* First compute the words in the new generators which map to the
     old generators */
  for (Ordinal g = 0; g < nr_g_generators;g++)
  {
    ow.set_length(0);
    ow.append(nr_g_generators);
    ow.append(g);
    reduce(&ow,ow,WR_H_LABEL);
    Ordinal l = ow.length()-1;
    if (l > max_length)
      max_length = l;
    if (ow.value(l) != nr_g_generators)
    {
      container.error_output("Main generator %d is not in the subgroup!\n",g);
      delete &h_alphabet;
      return 0;
    }
    else
      h_words.add(Subword(ow,0,l));
  }

  h_alphabet.set_nr_letters(nr_sub_generators);
  for (Ordinal h = 0; h < nr_sub_generators;h++)
    h_alphabet.set_next_letter(ow.alphabet().glyph(h+first_sub_generator));

  Transition_ID nr_transitions = nr_sub_generators;
  State_ID * transition = new State_ID[nr_transitions];
  State_ID state;

  /* Initialise the new FSA */
  State_ID pair[2],old_pair[2];
  State_Pair_List key;
  Byte_Buffer packed_key;
  const void * old_packed_key;
  State_ID ceiling = 1;
  Word_Length state_length = 0;
  size_t key_size;
  Keyed_FSA factory(container,h_alphabet,nr_transitions,
                    wa->state_count(),0);
  bool all_dense = key.all_dense(wa->state_count(),
                                 max_length*nr_g_generators);

  /* Create the failure and initial states */
  key.empty();
  packed_key = key.packed_data(&key_size);
  factory.find_state(packed_key,key_size);

  pair[0] = wa->initial_state();
  pair[1] = 0;
  key.insert(pair,all_dense);
  packed_key = key.packed_data(&key_size);
  factory.find_state(packed_key,key_size);
  key.empty();

  State_Count count = factory.state_count();

  /* Now create all the new states and the transition table */
  state = 0;
  State_Pair_List old_key;
  while ((old_packed_key = factory.get_state_key(++state))!=0)
  {
    Word_Length length = state >= ceiling ? state_length : state_length - 1;
    if (!(state & 255))
      container.status(2,1,"Translating word-acceptor: state " FMT_ID
                            " (" FMT_ID " of " FMT_ID " to do). Length %u\n",
                       state,count-state,count,length);

    old_key.unpack((const Byte *) old_packed_key);
    State_Pair_List::Iterator old_key_pairs(old_key);

    for (Ordinal h = 0; h < nr_sub_generators;h++)
    {
      key.empty();
      if (old_key_pairs.first(old_pair))
      {
        do
        {
          for (Ordinal g = 0; g < nr_g_generators;g++)
          {
            Word_Length so_far = 0;
            if (old_pair[1] != 0)
            {
              if (old_pair[1] % nr_g_generators != g)
                 continue;
              so_far = old_pair[1] / nr_g_generators;
            }
            if (h_words.word(g).value(so_far++) == h + first_sub_generator)
            {
              if (so_far == h_words.word(g).length())
              {
                pair[0] = wa->new_state(old_pair[0],g);
                pair[1] = 0;
              }
              else
              {
                pair[0] = old_pair[0];
                pair[1] = so_far*nr_g_generators + g;
              }
              key.insert(pair,all_dense);
            }
          }
        }
        while (old_key_pairs.next(old_pair));
      }
      packed_key = key.packed_data(&key_size);
      transition[h] = factory.find_state(packed_key,key_size);
      if (transition[h] >= count)
      {
        if (state >= ceiling)
        {
          state_length++;
          ceiling = transition[h];
        }
        count++;
      }
    }
    factory.set_transitions(state,transition);
  }
  /* we don't need this any more */
  delete [] transition;

  /* Set the accept states in the new FSA */
  state = 0;
  factory.clear_accepting(true);
  key.empty();

  while ((old_packed_key = factory.get_state_key(++state))!=0)
  {
    if (!(char) state)
      container.status(2,1,"Setting state info: state " FMT_ID " "
                           "(" FMT_ID " of " FMT_ID " to do).\n",
                           state,count-state,count);
    bool accepted = false;
    State_Pair_List old_key((const Byte *) old_packed_key);
    State_Pair_List::Iterator old_key_pairs(old_key);
    if (old_key_pairs.first(old_pair))
    {
      do
      {
        bool accepting_pair = wa->is_accepting(old_pair[0]) && old_pair[1]==0;
        if (accepting_pair)
          accepted = true;
      }
      while (!accepted && old_key_pairs.next(old_pair));
    }
    if (accepted)
      factory.set_is_accepting(state,true);
  }
  factory.remove_keys();
  FSA_Simple * xlat_wa = FSA_Factory::minimise(factory);
  xlat_wa = improve_translation(xlat_wa,*this,first_sub_generator);
  return xlat_wa;
}

