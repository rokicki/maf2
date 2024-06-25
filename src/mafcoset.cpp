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


// $Log: mafcoset.cpp $
// Revision 1.5  2016/09/04 20:18:59Z  Alun_Williams
// Fixed Coset_Table_Reducer to work with words containing coset symbol
// isnormal utility was broken if a coset table had been created
// Revision 1.4  2010/10/13 12:37:51Z  Alun
// Typo in comment corrected
// Revision 1.3  2010/06/10 17:20:11Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.2  2010/06/10 13:57:30Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/18 20:52:14Z  Alun
// New file.
//

#include "container.h"
#include "mafcoset.h"
#include "fsa.h"
#include "maf.h"
#include "mafword.h"
#include "maf_wdb.h"
#include "maf_ss.h"

static bool is_coset_table(const FSA & coset_table)
{
  return  !(coset_table.nr_accepting_states() != 1 ||
            coset_table.nr_initial_states() != 1 ||
            !coset_table.is_accepting(coset_table.initial_state()) ||
            coset_table.is_product_fsa() ||
            coset_table.get_flags() & GFF_RWS+GFF_NFA ||
            coset_table.label_type() != LT_Words ||
            coset_table.label_count() > coset_table.alphabet_size()+2);
}

/**/

FSA_Simple * MAF::build_acceptor_from_coset_table(const FSA & coset_table)
{
  /* Build a word acceptor from a coset table. This is used by gptcenum */
  if (!is_coset_table(coset_table))
    MAF_INTERNAL_ERROR(container,
                       ("Bad FSA passed to "
                        "MAF::build_acceptor_from_coset_table().\n"));

  FSA_Simple * factory = FSA_Factory::copy(coset_table);
  const Alphabet &alphabet = factory->base_alphabet;
  Transition_ID nr_symbols = alphabet.letter_count();
  Element_Count nr_cosets = factory->state_count()-1;
  Label_Count nr_labels = factory->label_count();
  Label_ID * label_ids = new Label_ID[nr_symbols];
  Ordinal_Word ow(alphabet);
  for (Label_ID label = 1; label < nr_labels; label++)
  {
    coset_table.label_word(&ow,label);
    if (ow.length() == 1)
     label_ids[ow.value(0)] = label;
  }
  Container & container = alphabet.container;
  for (State_ID si = 1; si <= nr_cosets; si++)
  {
    if (! (char) si)
      container.status(2,1,"Building word acceptor from coset table. (State "
                            FMT_ID " of " FMT_ID ")\n",si,nr_cosets);
    for (Ordinal g = 0; g < nr_symbols; g++)
    {
      State_ID new_si = factory->new_state(si,g);
      Label_ID label = factory->get_label_nr(new_si);
      if (label_ids[g] != label)
        factory->set_transition(si,g,0);
    }
  }
  factory->set_label_type(LT_Unlabelled);
  factory->set_accept_all();
  FSA_Simple * answer = FSA_Factory::minimise(*factory);
  if (!(is_shortlex || is_coset_system && g_level))
    answer->sort_bfs();
  delete [] label_ids;
  delete factory;
  return answer;
}

/**/

void MAF::label_coset_table(FSA_Common * coset_table) const
{
  /* Label an unlabelled standardised coset table so that each state is
     labelled by the last letter of the representative of the corresponding
     coset.
     This is easy if the word-ordering is shortlex, but quite hard otherwise.
     This method is needed by both Coset_Enumerator and
     Low_Index_Subgroup_Finder
  */

  class Coset_Table_Labeller
  {
    private:
      struct Work_Item
      {
        State_ID si;
        Work_Item * next;
        Work_Item(State_ID si_) :
          si(si_),
          next(0)
        {}
      };

      FSA_Common *coset_table;
      Work_Item * head;
      Work_Item **tail;
      const Ordinal nr_generators;
      const State_Count nr_states;
      const Alphabet & alphabet;
      const MAF & maf;
    public:
      Coset_Table_Labeller(FSA_Common * coset_table_,const MAF & maf_) :
        coset_table(coset_table_),
        head(0),
        tail(&head),
        nr_generators(coset_table_->base_alphabet.letter_count()),
        nr_states(coset_table_->state_count()),
        alphabet(coset_table_->base_alphabet),
        maf(maf_)
      {
        /* First of all assume every generator appears in some label,
           and create the labels.
           At the end we will check this, and relabel if need be. */

        coset_table->set_label_type(LT_Words);
        coset_table->set_nr_labels(nr_generators+2);
        Ordinal_Word ow(alphabet),new_word(alphabet),old_word(alphabet);
        coset_table->set_label_word(1,ow);
        for (Ordinal g = 0; g < nr_generators;g++)
        {
          ow.set_length(0);
          ow.append(g);
          coset_table->set_label_word(g+2,ow);
        }
        const Presentation_Data & pd = maf.properties();
        coset_table->set_label_nr(1,1);

        if (pd.is_shortlex || (pd.is_coset_system && pd.g_level))
        {
          /* In this case the defining generator must stem from the
             inverse of the lowest neighbouring state, since otherwise
             the automaton would not be BFS */
          State_Count to_do = nr_states-2;
          for (State_ID si = 2; si < nr_states;si++)
          {
            State_ID best_si = coset_table->new_state(si,0);
            Ordinal best_g = 0;
            for (Ordinal g = 1; g < nr_generators;g++)
            {
              State_ID new_si = coset_table->new_state(si,g);
              if (new_si < best_si)
              {
                best_si = new_si;
                best_g = g;
              }
            }
            best_g = maf.inverse(best_g);
            coset_table->set_label_nr(si,best_g+2);
            if (!(char) --to_do)
              maf.container.status(2,1,"Creating labels (" FMT_ID " of " FMT_ID
                                     " to do)\n",to_do,nr_states-1);
          }
        }
        else
        {
          /* In this case we have to work much harder. We assume that
             the BFS word will be best. However for each new word we visit
             we check whether it gives a better word for all the neighbouring
             states. If so, we have to redo that state. We remember which
             states are pending, because there is no need to requeue a state
             that is already queued. */

          Work_Item * item, *new_item,* next;
          * tail = new_item = new Work_Item(1);
          tail = &new_item->next;
          State_Count to_do = 1;
          State_Count done = 0;
          Special_Subset queued(*coset_table);
          queued.include(1);

          for (item = head; item; item = next)
          {
            queued.exclude(item->si);
            Ordinal this_g = coset_table->get_label_nr(item->si) - 2;
            normal_form(&ow,item->si);
            if (this_g != -1)
              this_g = maf.inverse(this_g);
            for (Ordinal g = 0; g < nr_generators; g++)
              if (g != this_g)
              {
                State_ID new_si = coset_table->new_state(item->si,g);
                Label_ID label = coset_table->get_label_nr(new_si);
                bool better = false;
                new_word = ow;
                new_word.append(g);

                if (label)
                {
                  if (label == g + 2)
                    better = true;
                  else
                  {
                    normal_form(&old_word,new_si);
                    better = new_word.compare(old_word) < 0;
                  }
                }
                else
                 better = true;

                if (better)
                {
                  coset_table->set_label_nr(new_si,g+2);
                  if (!queued.contains(new_si))
                  {
                    queued.include(new_si);
                    *tail = new_item = new Work_Item(new_si);
                    tail = &new_item->next;
                    to_do++;
                  }
                }
              }
            to_do--;
            if (!(char) ++done)
              maf.container.status(2,1,"Creating labels (" FMT_ID " of " FMT_ID
                                     " to do)\n",to_do,done+to_do);
            next = item->next;
            delete item;
          }
        }

        Label_ID new_label_nr = 2;
        Label_ID * new_labels = new Label_ID[nr_generators];
        for (Ordinal g = 0; g < nr_generators; g++)
        {
          new_labels[g] = 0;
          for (State_ID si = 1; si < nr_states; si++)
            if (coset_table->get_label_nr(si) == g + 2)
            {
              new_labels[g] = new_label_nr++;
              break;
            }
        }

        if (new_label_nr != nr_generators+2)
        {
          Label_ID * old_labels = new Label_ID[nr_states];
          for (State_ID si = 1; si < nr_states; si++)
            old_labels[si] = coset_table->get_label_nr(si);
          coset_table->set_label_type(LT_Words);
          coset_table->set_nr_labels(new_label_nr);
          ow.set_length(0);
          coset_table->set_label_word(1,ow);
          coset_table->set_label_nr(1,1);
          for (Ordinal g = 0; g < nr_generators;g++)
          {
            if (new_labels[g] != 0)
            {
              ow.set_length(0);
              ow.append(g);
              coset_table->set_label_word(new_labels[g],ow);
            }
          }

          for (State_ID si = 2; si < nr_states; si++)
            coset_table->set_label_nr(si,new_labels[old_labels[si] - 2]);
          delete [] old_labels;
        }
        delete [] new_labels;
      }

    private:
      void normal_form(Ordinal_Word * ow,State_ID coset_si) const
      {
        ow->set_length(0);
        while (coset_si != 1)
        {
          Ordinal g = coset_table->get_label_nr(coset_si)-2;
          g = maf.inverse(g);
          ow->append(g);
          coset_si = coset_table->new_state(coset_si,g,false);
        }
        maf.invert(ow,*ow);
      }
  } labeller(coset_table,*this);
}
/**/

void MAF::subgroup_generators_from_coset_table(Word_Collection * wc,
                                          const FSA &coset_table) const
{
  /* This method constructs a list of G-words for the Schreier generators
     of the subgroup. It is likely to be highly redundant, but redundant
     generators can be eliminated by  generating a subgroup presentation
     and then using that to generate a new substructure file
  */
  State_Count nr_cosets = coset_table.state_count()-1;
  Coset_Table_Reducer ctr(*this,coset_table);
  Ordinal_Word lhs(coset_table.base_alphabet);
  Ordinal_Word rhs(coset_table.base_alphabet);
  Ordinal nr_generators = Ordinal(coset_table.alphabet_size());

  wc->empty();
  for (State_ID si = 1; si <= nr_cosets; si++)
    for (Ordinal g = 0 ; g < nr_generators; g++)
      if (inverse(g) > g || inverse(g)==g &&
          coset_table.new_state(si,g) >= si)
      {
        ctr.read_word(&lhs,si);
        lhs.append(g);
        ctr.read_word(&rhs,coset_table.new_state(si,g,false));
        invert(&rhs,rhs);
        lhs += rhs;
        free_reduce(&lhs,lhs);
        if (lhs.length() != 0)
          wc->add(lhs);
      }
}

/**/

FSA_Simple * MAF::coset_table(Word_Reducer & coset_wr,
                              const FSA &coset_wa) const
{
  /* Create a standardised coset table for the finite index subgroup for
     which a coset word reducer is contained in coset_wr. The coset table is
     returned as an FSA with a single accept state, whose states are labelled
     with the final letter of the reduced word for the coset according to
     the word ordering that created the word_reducer.
     The reduced words for the cosets will always be a Schreier transversal
     but won't be the shortlex one unless that was the word ordering.

     Unusually, we can create this FSA directly without the need
     for using the typical factory/minimise pattern. So we use a Word_DB to
     index the words properly.
  */

  const Alphabet &alphabet = coset_wa.base_alphabet;
  Transition_ID nr_symbols = alphabet.letter_count();
  Language_Size nr_cosets = coset_wa.language_size(true);

  Container & container = alphabet.container;
  if (nr_cosets >= Language_Size(MAX_STATES))
  {
    if (nr_cosets == LS_INFINITE)
      container.error_output("Attempt to create infinite coset table!\n");
    else
      container.error_output("Number of cosets is too large for a coset table\n");
    return 0;
  }
  State_Count nr_states = nr_cosets+1;
  Word_DB coset_words(alphabet,nr_cosets+1);
  Ordinal_Word word(alphabet);
  Ordinal_Word coset_word(this->alphabet);
  Ordinal_Word new_word(alphabet);
  word.append(INVALID_SYMBOL);
  coset_words.insert(word);
  State_ID * transitions = new State_ID[nr_symbols];
  bool broken = false;

  word.set_length(0);
  coset_words.insert(word);
  FSA_Simple * answer = new FSA_Simple(container,alphabet,nr_states,nr_symbols,TSF_Dense);

  String_Buffer sb;
  for (State_ID si = 1;si < nr_states;si++)
  {
    if (!(char) si)
      container.status(2,1,"Creating coset tables (row " FMT_ID " of " FMT_LS ")\n",
                           si,nr_cosets);

    coset_words.get(&word,si);
    for (Ordinal g = 0;g < nr_symbols;g++)
    {
      if (is_coset_system)
      {
        coset_word.set_length(0);
        coset_word.append(coset_symbol);
        coset_word += word;
      }
      else
        coset_word = word;
      coset_word.append(g);
      coset_wr.reduce(&coset_word,coset_word);
      if (is_coset_system)
      {
        Word_Length l = coset_word.length();
        const Ordinal * buffer = coset_word.buffer();
        Word_Length i;
        for (i = 0; i < l;i++)
          if (buffer[i] < coset_symbol)
            break;
        new_word = Subword(coset_word,i);
      }
      else
        new_word = coset_word;
      if ((transitions[g] = coset_words.enter(new_word)) >= nr_states)
      {
        transitions[g] = 0;
        broken = true;
      }
    }
    if (!broken)
      answer->set_transitions(si,transitions);
    else
      break;
  }
  delete [] transitions;
  if (broken)
  {
    container.error_output("word reducer object and word acceptor passed to"
                           " MAF::coset_table are inconsistent!\n");
    delete answer;
    return 0;
  }

  Label_ID *labels = new Label_ID[nr_symbols+1];
  Label_ID next_label = 1;
  for (Ordinal g = 0; g <= nr_symbols;g++)
    labels[g] = 0;
  for (State_ID si = 1;si <= nr_states;si++)
  {
    if (!(char) si)
      container.status(2,1,"Labelling cosets (row "
                           FMT_ID " of " FMT_ID ")\n",si,nr_states);
    coset_words.get(&word,si);
    Word_Length l = word.length();
    Ordinal g = l ? word.value(l-1) : nr_symbols;
    bool new_label = labels[g]==0;
    if (new_label)
      labels[g] = next_label++;
    answer->set_label_nr(si,labels[g],new_label);
    if (new_label)
      if (g != nr_symbols)
        answer->set_label_word(labels[g],Subword(word,l-1,l));
      else
        answer->set_label_word(labels[g],word);
    coset_words.remove_entry(si);
  }
  delete [] labels;
  answer->change_flags(GFF_TRIM|GFF_MINIMISED|GFF_BFS|GFF_ACCESSIBLE|GFF_DENSE,0);
  answer->set_single_accepting(1);
  return answer;
}

/**/

Coset_Table_Reducer::Coset_Table_Reducer(const MAF & maf_,const FSA & coset_table_) :
  maf(maf_),
  coset_table(coset_table_)
{
  if (!is_coset_table(coset_table))
    MAF_INTERNAL_ERROR(coset_table.container,
                       ("Bad FSA passed to "
                        "Coset_Table_Reducer::Coset_Table_Reducer().\n"));

  Label_Count nr_labels = coset_table.label_count();
  Ordinal_Word label_word(coset_table.label_alphabet());
  generators = new Ordinal[coset_table.label_count()];
  for (Label_ID label = 1; label < nr_labels;label++)
  {
    coset_table.label_word(&label_word,label);
    Ordinal l = label_word.length();
    if (l != (label==1 ? 0 : 1))
      MAF_INTERNAL_ERROR(coset_table.container,
                       ("FSA passed to"
                       " Coset_Table_Reducer::Coset_Table_Reducer() does not have"
                       " labels\nof the expected form.\n"));
    if (l == 1)
      generators[label] = label_word.value(0);
    else
      generators[label] = IdWord;
  }
}

Coset_Table_Reducer::~Coset_Table_Reducer()
{
  delete [] generators;
}

/**/

unsigned Coset_Table_Reducer::reduce(Word * word,const Word & start_word,
                                     unsigned flags,const FSA *)
{
  const Ordinal * values = start_word.buffer();
  Word_Length l = start_word.length();
  Ordinal last_g = IdWord;
  bool want_coset_symbol = false;
  if (flags & WR_PREFIX_ONLY && l)
    last_g = values[--l];

  Word_Length i = 0;
  State_ID si = 1;
  const Presentation_Data & pd = maf.properties();

  if (l && pd.is_coset_system && values[0] == pd.coset_symbol)
  {
    want_coset_symbol = true;
    values++;
    l--;
  }

  if (flags & WR_CHECK_ONLY)
  {
    for (i = 0; i < l;i++)
    {
      si = coset_table.new_state(si,values[i],false);
      Label_ID label = coset_table.get_label_nr(si);
      if (generators[label] != values[i])
        return 1;
    }
    return 0;
  }
  else
    for (i = 0; i < l;i++)
      si = coset_table.new_state(si,values[i],false);

  Ordinal_Word save(start_word);
  read_word(word,si,want_coset_symbol);
  if (last_g != IdWord)
    word->append(last_g);
  return *word != save ? 1 : 0;
}

/**/

void Coset_Table_Reducer::read_word(Word * word,
                                    State_ID si,
                                    bool want_coset_symbol) const
{
  word->set_length(0);
  while (si != 1)
  {
    Label_ID label = coset_table.get_label_nr(si);
    si = coset_table.new_state(si,maf.inverse(generators[label]),false);
    word->append(generators[label]);
  }
  if (want_coset_symbol)
    word->append(maf.properties().coset_symbol);
  Word_Length l = word->length();
  Ordinal * new_values = word->buffer();
  for (Word_Length i = 0; i < l - 1 -i;i++)
  {
    Ordinal temp = new_values[i];
    new_values[i] = new_values[l-1-i];
    new_values[l-1-i] = temp;
  }
}
