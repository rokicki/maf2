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


// $Log: subpres.cpp $
// Revision 1.15  2010/06/18 05:57:55Z  Alun
// memory leak fixed
// Revision 1.14  2010/06/10 17:38:14Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.13  2010/06/10 13:57:50Z  Alun
// All tabs removed again
// Revision 1.12  2010/06/07 17:15:03Z  Alun
// Method to extract subgroup presentation from H equations of a coset system
// added.
// Revision 1.11  2009/11/03 19:27:58Z  Alun
// Fail gracefully if subgroup presentation needs to many generators
// Revision 1.10  2009/09/13 12:35:15Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Language_Size data type introduced and used where appropriate
// Revision 1.9  2008/12/24 10:38:20Z  Alun
// Changes to improve performance when there are a lot of subgroup generators
// Revision 1.10  2008/11/12 09:47:01Z  Alun
// Suppress "special overlap" processing when calculating a subgroup presentation.
// In some cases this can take far too long. Probably this should be re-instated
// and maf_rm.cpp should be more intelligent
// Revision 1.9  2008/11/03 11:33:56Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.8  2008/10/09 18:37:22Z  Alun
// Unnecessary extra stage of processing removed
// Revision 1.7  2008/09/30 09:17:27Z  Alun
// Final version built using Indexer.
// Revision 1.4  2007/12/20 23:25:43Z  Alun
//
#include "container.h"
#include "mafword.h"
#include "fsa.h"
#include "maf.h"
#include "equation.h"
#include "maf_wdb.h"
#include "hash.h"
#include "maf_rws.h"
#include "maf_ss.h"
#include "mafcoset.h"

struct Word_Pair
{
  Ordinal_Word g_word;
  Ordinal_Word h_word;
  Word_Pair(const Ordinal_Word &g_word_,const Ordinal_Word &h_word_) :
    g_word(g_word_),
    h_word(h_word_)
  {}
};

struct Word_Pair_List
{
  Element_Count count;
  Element_Count allocated;
  Element_Count reserved;
  Word_Pair ** data;

  Word_Pair_List() :
    count(0),
    allocated(0),
    reserved(0),
    data(0)
  {}

  ~Word_Pair_List()
  {
    for (Element_Count i = 0; i < allocated;i++)
      delete data[i];
    if (data)
      delete [] data;
  }

  void reserve(Element_Count reserved_)
  {
    count = 0;
    if (reserved_ > reserved)
    {
      Word_Pair **new_data = new Word_Pair *[reserved = reserved_];
      for (Element_Count i = 0; i < allocated;i++)
        new_data[i] = data[i];
      if (data)
        delete [] data;
      data = new_data;
    }
  }

  void add(const Ordinal_Word & g_word,const Ordinal_Word & h_word)
  {
    if (count >= allocated)
    {
      data[allocated++] = new Word_Pair(g_word,h_word);
      count++;
    }
    else
    {
      data[count]->g_word = g_word;
      data[count]->h_word = h_word;
      count++;
    }
  }
  void swap(Word_Pair_List & other)
  {
    Element_Count temp;
    temp = reserved;
    reserved = other.reserved;
    other.reserved = temp;
    temp = allocated;
    allocated = other.allocated;
    other.allocated = temp;
    Word_Pair **temp_data = data;
    data = other.data;
    other.data = temp_data;
    count = other.count;
    other.count = 0;
  }
};

MAF *MAF::subgroup_presentation(General_Multiplier & coset_gm_,
                                bool use_schreier_generators)
{
  /*
  Compute a presentation for a subgroup as a rewriting system, using the
  coset system general multiplier, using either the generators specified in
  the original substructure file or Schreier generators

  The KBMAG gpsubpres has a complex algorithm for performing an inferior
  version of this calculation (inferior because it alsways uses the Schreier
  generators, not those in the substructure file), involving the
  computation of a potentially large number of large composite coset
  multipliers. In fact the only information it extracts from these multipliers
  are the Schreier labels of the initial states. Each multiplier it computes
  is for a relator, so all the labels must equal the identity: our coset
  equations are of the form u=hv, but for the identity multiplier they are
  u=hu, so h is the identity. So each label gives rise to a new relator in
  either the g-generators or the Schreier generators for the subgroup, and
  these are collected together to give the new presentation.

  This is often unnecessary. In the first place, if the index of the
  subgroup is finite, and nr_relators*index < |Schreier generators|^2
  it is probably quicker simply to multiply each coset representative by
  each relator, especially if the number of generators is high. In fact this
  is probably quicker even when nr_relators*index is somewhat larger.

  Otherwise it is only necessary to calculate what the potential initial states
  of such automata are, which we can do iteratively as follows:

  We start with the initial states of the coset general multiplier. This
  contains all the initial states required for words of length 1.

  Let us suppose we have the initial states for the coset multiplier which
  contains all words of length n or below. The potential initial states
  for the coset multiplier which contains all words of length 2n or below
  is simply the Cartesian product of this set with itself, with new labels
  consisting of the old labels concatenated.

  So we just repeatedly form this new set of initial states. When we have
  done this k times, we have the initial states for the multiplier with all
  words, and a fortiori relators up to 2^k, so we can stop once 2^k exceeds
  the length of the longest relator in the original presentation. In fact
  we can stop one iteration before this, and then pair inverse words, since
  no other labels can occur in the initial states of the multiplier for a
  relator. An advantage of doing things this way is that we can keep the
  number of labels on each initial state to a minimum.

  At each step we remember one G-word and one H-word for each of these
  initial states. When we form the next set of initial states we reduce each
  new G-Word. We know that initial states with equal G-words can be merged.
  So we merge these initial states, and add any new relators between H-words
  as axioms of the new presentation. We only need to keep the best H-word
  out of a set of merged initial states.

  Actually, when we are not using the Schreier generators there is an
  additional complication. It is also necessary to form the multiplier
  for subgroup generators. This inevitably includes initial states labelled
  by the g-word of each subgroup generator. The h-word label on the
  initial state may not be the corresponding h-generator. (This happens if
  the corresponding h-generator does not occur as an initial state of the
  original MIDFA coset multiplier). We need to add appropriate h-axioms to
  equate the label that actually appears with the generator.

  One potential problem with this algorithm is that where there are lots
  of initial states to start with the number of initial states may increase
  very rapidly compared with the relator length. If that happens then we
  then have to do something similar to what KBMAG does since that allows us
  to eliminate some of the initial states at each stage because in fact they
  do not give rise to any accept states in the composite coset multiplier.

  We can improve our presentation further, by creating a new instance of
  MAF, as a home for the new axioms, each time we add a relator we polish
  it against this new instance of MAF. If we wanted to be extra-sophisticated
  we can even tell MAF to perform a KB expansion each time we add an axiom
  (with some suitable limiting condition to stop it getting out of hand). In
  that case we would come up with probably independent axioms (though
  it is possible we have stopped too soon, or that an earlier axiom could
  be deduced from some combination of the later ones).

  This algorithm requires the automatic structure of the group to have been
  calculated because it expects to be able to load the general multiplier as
  a word reduction method, which the KBMAG method does not.
  Also in some cases the proliferation of initial states means that our
  weeding out of axioms takes so long that the KBMAG approach would be quicker,
  because it can weed out all the non-succeeding initial states. On the other
  hand composition of multipliers for some groups can be very difficult.

*/
  Ordinal first_sub_generator = (coset_symbol+1);
  Element_Count nr_schreier_generators = coset_gm_.nr_initial_states()-1;
  Alphabet * new_alphabet = 0;
  FSA_Simple * new_coset_gm = 0;
  if (options.emulate_kbmag ||
      coset_gm_.label_alphabet().letter_count() == coset_symbol)
    use_schreier_generators = true;
  if (use_schreier_generators)
  {
    new_alphabet = Alphabet::create(AT_String,container);
    if (!new_alphabet->set_nr_letters(coset_symbol + nr_schreier_generators+1))
    {
      container.error_output("Unable to compute presentation as number of"
                             " generators needed (" FMT_ID ")\n"
                             "exceeds MAF's internal limit (" FMT_ID ")\n",
                             coset_symbol + nr_schreier_generators+1,
                             new_alphabet->capacity());
      delete new_alphabet;
      return 0;
    }
    new_alphabet->set_similar_letters("_g",coset_symbol + nr_schreier_generators+1);
    // we have to ensure that the label and base alphabets are compatible
    // because label words are sometimes inserted into collections of words
    // from the base alphabet, and if the ordering is incompatible this goes
    // horribly wrong.
    new_alphabet->set_word_ordering_from(coset_gm_.base_alphabet);
    new_coset_gm = FSA_Factory::copy(*coset_gm_.fsa());
    new_coset_gm->set_label_alphabet(*new_alphabet,true);
  }
  General_Multiplier & coset_gm = use_schreier_generators ?
                                  * new General_Multiplier(*new_coset_gm,true) :
                                  coset_gm_;

  Word_Pair_List current;
  Word_Pair_List next;
  next.reserve(coset_gm.nr_initial_states());
  const Alphabet &alphabet = coset_gm.label_alphabet();
  Word_DB g_words(alphabet,1024);
  Ordinal_Word g_word(alphabet);
  Total_Length relator_length = 1;
  Options options = this->options;
  options.no_differences = true;
  options.strategy = ~0u;
  if (alphabet.letter_count() - first_sub_generator > 10)
    options.partial_reductions = 0;
  options.special_overlaps = 0;
  General_Multiplier & group_gm = options.no_composite ?
                                  * (General_Multiplier *) load_group_fsas(GA_GM) :
                                  * (General_Multiplier *) 0;

  // Create a new MAF object for the subgroup's Rewriter_Machine to live in
  MAF & maf_sub1 = *MAF::create(&coset_gm.container,&options,AT_String,PT_Group);
  // and a second MAF object for a clean presentation to live in
  MAF & maf_sub2 = *MAF::create(&coset_gm.container,&options,AT_String,PT_Group);
  Ordinal nr_sub_generators = alphabet.letter_count() - first_sub_generator;

  maf_sub1.set_nr_generators(nr_sub_generators);
  maf_sub2.set_nr_generators(nr_sub_generators);

  Ordinal h;
  Word_Length required_length = max_relator_length/2+1;
  Ordinal nr_missing_sub_generators = nr_sub_generators;
  Word_List sub_generators(alphabet);
  Sorted_Word_List missing_sub_generators(alphabet);
  Ordinal nr_generators = alphabet.letter_count();

  container.progress(1,"Computing subgroup presentation\n");

  // Add the generators, and build a list of their g-words
  for (h = first_sub_generator; h < nr_generators;h++)
  {
    maf_sub1.set_next_generator(alphabet.glyph(h));
    maf_sub2.set_next_generator(alphabet.glyph(h));
    if (!use_schreier_generators)
    {
      g_word = Ordinal_Word(alphabet,group_word(h));
      reduce(&g_word,g_word);
      if (&group_gm)
        group_gm.reduce(&g_word,g_word);
      missing_sub_generators.insert(g_word);
      sub_generators.add(g_word);
      if (required_length < g_word.length())
        required_length = g_word.length();
    }
  }

  unsigned aa_flags = options.eliminate ?
                               AA_ADD_TO_RM|AA_DEDUCE|AA_ELIMINATE|AA_POLISH :
                               AA_ADD_TO_RM|AA_DEDUCE|AA_POLISH;

  if (use_schreier_generators)
    aa_flags |= AA_DEDUCE_INVERSE;
  /* If there are very many sub generators using a Rewriter_Machine to simplify
     the axiom set is not practical */
  if (nr_sub_generators > 100)
    aa_flags = AA_DEDUCE_INVERSE;

  //Add axioms for inverse relations - we can't do this yet in the Schreier case
  if (!use_schreier_generators)
  {
    for (h = first_sub_generator; h < nr_generators;h++)
    {
      Ordinal h1 = h-first_sub_generator;
      if (inverse(h) != INVALID_SYMBOL)
      {
        Ordinal h2 = inverse(h)-first_sub_generator;
        maf_sub1.set_inverse(h1,h2);
        maf_sub2.set_inverse(h1,h2,0);
      }
    }
  }

  maf_sub1.set_flags();

  // variables for storing axioms for subgroup
  String_Buffer sb1,sb2;

  State_Subset_Iterator ssi;
  Word_List label_wl(alphabet);
  Ordinal_Word label_word(alphabet);
  Ordinal_Word h_word(alphabet);
  for (State_ID state = coset_gm.initial_state(ssi,true); state;
       state = coset_gm.initial_state(ssi,false))
  {
    Label_ID label = coset_gm.get_label_nr(state);
    coset_gm.label_word_list(&label_wl,label);
    Element_Count count = label_wl.count();
    bool have_h = use_schreier_generators;
    bool have_g = false;
    if (use_schreier_generators && state > 1)
    {
      h_word.set_length(1);
      h_word.set_code(0,coset_symbol + state-1);
    }
    for (Element_ID word_nr = 0;word_nr < count;word_nr++)
    {
      label_wl.get(&label_word,word_nr);
      if (!label_word.length())
      {
        h_word = g_word = label_word;
        have_g = have_h = true;
        break;
      }
      else if (!have_g && label_word.value(0) < coset_symbol)
      {
        g_word = label_word;
        have_g = true;
      }
      else if (!have_h && label_word.value(0) >= first_sub_generator)
      {
        h_word = label_word;
        have_h = true;
      }
    }
    next.add(g_word,h_word);
    if (nr_missing_sub_generators && missing_sub_generators.remove(g_word))
    {
      bool found = false;
      for (h = first_sub_generator;h < nr_generators;h++)
        if (g_word == Entry_Word(sub_generators,h-first_sub_generator))
        {
          found = true;
          nr_missing_sub_generators--;
          h_word.set_length(1);
          h_word.set_code(0,h);
          h_word.format(&sb1);
          Element_ID i = next.count-1;
          next.data[i]->h_word.format(&sb2);
          maf_sub1.add_axiom(sb1.get(),sb2.get(),aa_flags);
          if (h_word.compare(next.data[i]->h_word) < 0)
            next.data[i]->h_word = h_word;
          // we must not break here - there may be redundant subgenerators
        }
    }
  }

  bool failed = false;
  if (coset_gm.language_size(true) < LS_HUGE ||
      coset_gm.state_count() < 100 ||
      !&group_gm)
  {
    /* In first case it may be quicker to do explicitly multiply
       all the cosets by the relators.
       In the second case it is likely to be quick to create the
       required composites.
       In the final case we can't work out the g words anyway. */
    failed = true;
  }

  while (relator_length < required_length && !failed)
  {
    current.swap(next);
    if (current.count > 512)
    {
      failed = true;
      break;
    }
    next.reserve(current.count*current.count);
    g_words.empty();
    for (Element_Count i0 = 0; i0 < current.count;i0++)
      for (Element_Count i1 = 0; i1 < current.count;i1++)
      {
        container.status(2,1,"Examining initial states ("
                             FMT_ID " of " FMT_ID ")\n",
                         i0*current.count+i1,current.count*current.count);
        g_word = current.data[i0]->g_word + current.data[i1]->g_word;
        h_word = current.data[i0]->h_word + current.data[i1]->h_word;
        reduce(&g_word,g_word);
        group_gm.reduce(&g_word,g_word);
        Element_ID i;
        g_words.insert(g_word,&i);
        if (i == next.count)
        {
          next.add(g_word,h_word);
          if (nr_missing_sub_generators && missing_sub_generators.remove(g_word))
          {
            for (h = first_sub_generator;h < nr_generators;h++)
              if (g_word == Entry_Word(sub_generators,h-first_sub_generator))
              {
                nr_missing_sub_generators--;
                h_word.set_length(1);
                h_word.set_code(0,h);
                h_word.format(&sb1);
                next.data[i]->h_word.format(&sb2);
                maf_sub1.add_axiom(sb1.get(),sb2.get(),aa_flags);
                if (h_word.compare(next.data[i]->h_word) < 0)
                  next.data[i]->h_word = h_word;
                // we must not break here - there may be redundant subgenerators
              }
          }
          continue;
        }
        h_word.format(&sb1);
        next.data[i]->h_word.format(&sb2);
        maf_sub1.add_axiom(sb1.get(),sb2.get(),aa_flags);
        if (h_word.compare(next.data[i]->h_word) < 0)
          next.data[i]->h_word = h_word;
      }
    relator_length *= 2;
  }

  if (failed)
  {
    /* The number of states was getting big, so we are going to build all
       the multipliers and find the initial states explicitly,
       or the group multiplier was missing */
    if (use_schreier_generators)
    {
      /* In this case we want to remove any h_labels that are present and
         replace them with Schreier generator labels */
      for (State_ID state = coset_gm.initial_state(ssi,true); state;
           state = coset_gm.initial_state(ssi,false))
      {
        Label_ID label = coset_gm.get_label_nr(state);
        coset_gm.label_word_list(&label_wl,label);
        Element_Count count = label_wl.count();
        Word_List new_wl(alphabet);
        for (Element_ID word_nr = 0;word_nr < count;word_nr++)
        {
          label_wl.get(&label_word,word_nr);
          if (label_word.value(0) < coset_symbol)
            new_wl.add(label_word);
        }
        if (state > 1)
        {
          label_word.set_length(1);
          label_word.set_code(0,coset_symbol + state-1);
        }
        new_wl.add(label_word);
        new_coset_gm->set_label_word_list(label,new_wl);
      }
    }

    label_wl.empty();
    subgroup_relators(coset_gm,&label_wl,use_schreier_generators);
    Element_Count nr_words = label_wl.count();
    h_word.set_length(0);
    h_word.format(&sb2);
    for (Element_ID word_nr = 0;word_nr < nr_words;word_nr++)
    {
      label_wl.get(&h_word,word_nr);
      h_word.format(&sb1);
      maf_sub1.add_axiom(sb1.get(),"",aa_flags);
      if (h_word.length() > 50 || aborting)
        aa_flags &= ~(AA_DEDUCE|AA_ELIMINATE);
      container.status(2,1,"Checking subgroup relators ("
                           FMT_ID " of " FMT_ID " to do)\n",
                       nr_words - word_nr,nr_words);
    }
  }

  if (use_schreier_generators)
  {
    for (h = 0; h < nr_sub_generators;h++)
    {
      Ordinal H = maf_sub1.inverse(h);
      if (H != INVALID_SYMBOL)
        maf_sub2.set_inverse(h,H,0);
      else
      {
        h_word.set_length(0);
        h_word.append(h);
        maf_sub1.reduce(&h_word,h_word);
        if (h_word.length()==0)
          maf_sub2.set_inverse(h,h,0);
      }
    }
  }
  maf_sub2.set_flags();
  for (const Linked_Packed_Equation * axiom = maf_sub1.first_axiom();axiom;
       axiom = axiom->get_next())
  {
    Simple_Equation se(maf_sub1.alphabet,*axiom);
    se.lhs_word.format(&sb1);
    se.rhs_word.format(&sb2);
    maf_sub2.add_axiom(sb1.get(),sb2.get(),use_schreier_generators ? AA_DEDUCE_INVERSE : 0);
  }
  if (use_schreier_generators)
    delete & coset_gm;
  delete &maf_sub1;
  return &maf_sub2;
}

/**/

MAF * MAF::rs_presentation(const FSA &coset_table) const
{
  /* Calculate a presentation for the subgroup specified by the coset
     table using the Reidemeister-Schreier algorithm */

  Container & container = coset_table.container;
  Ordinal nr_sub_generators = 0;
  State_Count nr_states = coset_table.state_count();
  Ordinal nr_symbols = Ordinal(coset_table.alphabet_size());
  Transition_Count to_do = nr_states*nr_symbols;
  Transition_Count subgen_count = (nr_states-1)*nr_symbols;
  Ordinal *h_ordinals = new Ordinal[to_do];

  for (State_ID si = 1;si < nr_states;si++)
    for (Ordinal g = 0; g < nr_symbols;g++)
      h_ordinals[si*nr_symbols+g] = INVALID_SYMBOL;

  Transition_Realiser tr(coset_table);
  MAF * maf_sub = MAF::create(&container,0,AT_Simple);
  Element_Count max_generators = maf_sub->alphabet.capacity();

  if (coset_table.label_type() == LT_Words &&
      coset_table.label_count() <= coset_table.alphabet_size()+2)
  {
    Coset_Table_Reducer ctr(*this,coset_table);
    for (State_ID si = 2; si < nr_states;si++)
    {
      if (!(char) si)
        container.status(2,1,"Examining transversal (" FMT_ID " of " FMT_ID
                             " to do).\n",
                         nr_states-si,nr_states-2);
      Ordinal g = ctr.generator(coset_table.get_label_nr(si));
      Ordinal ig = inverse(g);
      State_ID parent_si = coset_table.new_state(si,ig,false);
      h_ordinals[parent_si*nr_symbols+g] = PADDING_SYMBOL;
      h_ordinals[si*nr_symbols+ig] = PADDING_SYMBOL;
      subgen_count -= 2;
    }
  }
  else
  {
    State_ID transversal_si = 2;
    for (State_ID si = 1; transversal_si < nr_states;si++)
    {
      const State_ID * trow = tr.realise_row(si);
      if (!(char) si)
        container.status(2,1,"Creating transversal (" FMT_ID " of " FMT_ID
                             " to do).\n",
                             nr_states-si,nr_states-1);
      for (Ordinal g = 0; g < nr_symbols;g++)
        if (trow[g] == transversal_si)
        {
          h_ordinals[si*nr_symbols+g] = PADDING_SYMBOL;
          h_ordinals[trow[g]*nr_symbols+inverse(g)] = PADDING_SYMBOL;
          transversal_si++;
          subgen_count -= 2;
        }
    }
  }

  if (subgen_count > Transition_Count(max_generators))
  {
    container.error_output("Too many subgroup generators required!\n");
    delete maf_sub;
    delete h_ordinals;
    return 0;
  }

  for (State_ID si = 1; si < nr_states;si++)
  {
    const State_ID * trow = tr.realise_row(si);
    if (!(char) si)
      container.status(2,1,"Examining coset table (" FMT_ID " of " FMT_ID
                           " to do). %d generators so far\n",
                       nr_states-si,nr_states,nr_sub_generators);
    for (Ordinal g = 0; g < nr_symbols;g++)
      if (h_ordinals[si*nr_symbols+g] == INVALID_SYMBOL)
      {
        h_ordinals[si*nr_symbols+g] = nr_sub_generators++;
        if (trow[g] != si || inverse(g) != g)
          h_ordinals[trow[g]*nr_symbols+inverse(g)] = nr_sub_generators++;
      }
  }

  maf_sub->set_nr_generators(nr_sub_generators);

  for (State_ID si = 1; si < nr_states;si++)
  {
    if (!(char) si)
      container.status(2,1,"Setting inverses (" FMT_ID " of " FMT_ID
                           " to do).\n",
                           nr_states-si,nr_states);
    const State_ID * trow = tr.realise_row(si);
    for (Ordinal g = 0; g < nr_symbols;g++)
      if (h_ordinals[si*nr_symbols+g] >= 0)
        maf_sub->set_inverse(h_ordinals[si*nr_symbols+g],
                             h_ordinals[trow[g]*nr_symbols+inverse(g)]);
  }
  maf_sub->set_flags();
  bool coset_error = false;
  to_do = (nr_states-1) * axiom_count();
  Transition_Count remaining = to_do;
  for (const Linked_Packed_Equation *axiom = first_axiom();
       axiom;
       axiom = axiom->get_next())
  {
    Simple_Equation se(alphabet,*axiom);
    if (invert(&se.rhs_word,se.rhs_word))
    {
      se.lhs_word += se.rhs_word;
      Word_Length l = se.lhs_word.length();
      Ordinal_Word h_word(maf_sub->alphabet);
      Ordinal_Word rhs_word(maf_sub->alphabet);
      for (State_ID si = 1;si < nr_states;si++)
      {
        container.status(2,1,"Multiplying coset representatives by relators"
                             " (" FMT_TC " of " FMT_TC " to do)\n",
                         remaining--,to_do);
        h_word.set_length(0);
        State_ID nsi = si;
        for (Word_Length i = 0; i <l;i++)
        {
          Ordinal g = se.lhs_word.value(i);
          Ordinal h = h_ordinals[nsi*nr_symbols+g];
          if (h >= 0)
            h_word.append(h);
          nsi = coset_table.new_state(nsi,g);
        }
        if (nsi != si)
        {
          Simple_Equation se2(alphabet,*axiom);
          String_Buffer sb1,sb2;
          se2.lhs_word.format(&sb1);
          se2.lhs_word.format(&sb2);
          container.error_output("Error in coset_table at coset " FMT_ID
                                 " for equation:\n",si);
          se2.print(container,container.get_stderr_stream());
          coset_error = true;
          break;
        }
        if (h_word.length() > 0)
          maf_sub->add_axiom(h_word,rhs_word);
      }
      if (coset_error)
        break;
    }
  }
  if (coset_error)
  {
    delete maf_sub;
    return 0;
  }
  delete [] h_ordinals;
  return maf_sub;
}

/**/

MAF *MAF::subgroup_presentation(const Rewriting_System & rws,
                                bool use_schreier_generators) const
{
  /*
  Compute a presentation for a subgroup as a rewriting system, using the
  H equations in the input maf object. This is only valid if either
  the rewriting system is completely confluent, or we have detected that
  the index of the subgroup is finite.
  */

  Ordinal first_sub_generator = (coset_symbol+1);
  if (options.emulate_kbmag || first_sub_generator == nr_generators)
    use_schreier_generators = true;
  if (use_schreier_generators)
    return 0;

  container.progress(1,"Computing subgroup presentation\n");
  Ordinal_Word lhs_word(alphabet);
  Ordinal_Word rhs_word(alphabet);
  MAF::Options options = this->options;
  options.no_differences = true;
  options.strategy = ~0u;
  if (alphabet.letter_count() - first_sub_generator > 10)
    options.partial_reductions = 0;
  options.special_overlaps = 0;

  // Create a new MAF object for the subgroup's Rewriter_Machine to live in
  MAF & maf_sub1 = *MAF::create(&container,&options,AT_String,PT_Group);
  // and a second MAF object for a clean presentation to live in
  MAF & maf_sub2 = *MAF::create(&container,&options,AT_String,PT_Group);
  Ordinal nr_sub_generators = alphabet.letter_count() - first_sub_generator;

  maf_sub1.set_nr_generators(nr_sub_generators);
  maf_sub2.set_nr_generators(nr_sub_generators);
  Ordinal h;
  Word_List sub_generators(alphabet);
  Ordinal nr_generators = alphabet.letter_count();

  // Add the generators, and build a list of their g-words
  for (h = first_sub_generator; h < nr_generators;h++)
  {
    maf_sub1.set_next_generator(alphabet.glyph(h));
    maf_sub2.set_next_generator(alphabet.glyph(h));
  }

  unsigned aa_flags = options.eliminate ?
                        AA_ADD_TO_RM|AA_DEDUCE|AA_ELIMINATE|AA_POLISH :
                        AA_ADD_TO_RM|AA_DEDUCE|AA_POLISH;
  bool use_2 = true;

  //Add axioms for inverse relations
  for (h = first_sub_generator; h < nr_generators;h++)
  {
    Ordinal h1 = h-first_sub_generator;
    if (inverse(h) != INVALID_SYMBOL)
    {
      Ordinal h2 = inverse(h)-first_sub_generator;
      maf_sub1.set_inverse(h1,h2);
      maf_sub2.set_inverse(h1,h2,0);
    }
    else
      use_2 = false;
  }

  maf_sub1.set_flags();
  if (use_2)
    maf_sub2.set_flags();

  // variables for storing axioms for subgroup
  String_Buffer sb1,sb2;
  FSA_Simple *rws_h = FSA_Factory::restriction(rws,maf_sub1.alphabet);
  Simple_Finite_Set owner(container,rws.equation_count());
  Special_Subset used(owner);
  State_Count nr_states = rws_h->state_count();
  /* If there are very many sub generators or equations using a
     Rewriter_Machine to simplify the axiom set is not practical */
//  if (nr_sub_generators > 2000 || rws_h->equation_count() > 20000)
    aa_flags = AA_DEDUCE_INVERSE;
//  else
//    use_2 = false;

  for (State_ID si = 1; si < nr_states; si++)
  {
    const State_ID *transition = rws_h->state_access(si);
    for (Ordinal h = 0; h < nr_sub_generators;h++)
      if (transition[h] < 0)
      {
        Element_ID eqn_nr = -transition[h];
        if (!used.contains(eqn_nr))
        {
          used.include(eqn_nr);
          rws.read_lhs(&lhs_word,eqn_nr);
          rws.read_rhs(&rhs_word,eqn_nr);
          lhs_word.format(&sb1);
          rhs_word.format(&sb2);
          if (lhs_word.length() + rhs_word.length() > 50)
            aa_flags &= ~(AA_DEDUCE|AA_ELIMINATE);
          if (use_2)
            maf_sub2.add_axiom(sb1.get(),sb2.get(),aa_flags);
          else
            maf_sub1.add_axiom(sb1.get(),sb2.get(),aa_flags);
        }
      }
    container.status(2,1,"Examining subgroup word states ("
                         FMT_ID " of " FMT_ID " to do)\n",
                     nr_states - si,nr_states);
  }
  delete rws_h;

  if (!use_2)
  {
    maf_sub2.set_flags();
    for (const Linked_Packed_Equation * axiom = maf_sub1.first_axiom();axiom;
         axiom = axiom->get_next())
    {
      Simple_Equation se(maf_sub2.alphabet,*axiom);
      maf_sub2.add_axiom(se.lhs_word,se.rhs_word,0);
    }
  }

  delete &maf_sub1;
  return &maf_sub2;
}
