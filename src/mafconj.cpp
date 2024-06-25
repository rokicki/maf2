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


// $Log: mafconj.cpp $
// Revision 1.2  2010/06/10 13:57:29Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/16 23:28:46Z  Alun
// New file.
// Revision 1.1  2009/11/02 14:53:02Z  Alun
// New file.
/* This program creates an FSA which acts as a function that maps each
   element of a group to the minimal representative of the conjugacy class
*/
#include <stdlib.h>
#include "maf.h"
#include "maf_rws.h"
#include "container.h"
#include "maf_wdb.h"
#include "maf_el.h"
#include "keyedfsa.h"
#include "maf_cfp.h"

/**/

FSA_Simple * MAF::build_class_table(const Rewriting_System & rws) const
{
  /* Create an FSA that maps each group element of the the finite group
     whose confluent rewriting system is contained in rws to the minimal
     representative of the conjugacy class of the element.
     The FSA has the same states and transitions as the standardised coset
     table for the group, but the states are labelled with the
     appropriate representative from the conjugacy class rather than
     by the last letter of the normal form for the element, and the
     accept states are the states that are class representatives.

     We can create this FSA directly without using the typical factory/minimise
     pattern. So we use a Word_DB to index the words properly.
  */

  const Alphabet &fsa_alphabet = group_alphabet();
  const Alphabet &full_alphabet = alphabet;
  Transition_ID nr_symbols = fsa_alphabet.letter_count();
  State_ID start_state = rws.initial_state();
  if (is_coset_system)
    start_state = rws.new_state(start_state,coset_symbol);
  Language_Size nr_elements = rws.language_size(true,start_state);

  if (nr_elements >= Language_Size(MAX_STATES))
  {
    if (nr_elements == LS_INFINITE)
      container.error_output("Attempt to create conjugacy class table for infinite group!\n");
    else
      container.error_output("Size of group is too large for a conjugacy class table\n");
    return 0;
  }
  State_Count nr_states = nr_elements+1;
  Word_DB element_words(fsa_alphabet,nr_elements+1);
  Word_DB conjugate_words(fsa_alphabet,nr_elements+1);
  Element_List class_sizes;

  Ordinal_Word word(full_alphabet);
  Ordinal_Word new_word(full_alphabet);
  word.append(INVALID_SYMBOL);
  element_words.insert(word);
  State_ID * transitions = new State_ID[nr_symbols];

  word.set_length(0);
  element_words.insert(word);
  FSA_Simple * answer = new FSA_Simple(container,fsa_alphabet,nr_states,
                                       nr_symbols,TSF_Dense);
  answer->set_label_type(LT_Words);
  answer->set_nr_labels(1);
  class_sizes.append_one(0);

  nr_states--;
  String_Buffer sb;
  for (State_ID si = 1;si <= nr_states;si++)
  {
    if (!(char) si)
      container.status(2,1,"Creating conjugacy class table transitions (row "
                           FMT_ID " of " FMT_ID ")\n",
                       si,nr_states);

    element_words.get(&word,si);
    for (Ordinal g = 0;g < nr_symbols;g++)
    {
      if (is_coset_system)
      {
        new_word.set_length(0);
        new_word.append(coset_symbol);
        new_word += word;
      }
      else
        new_word = word;
      new_word.append(g);
      reduce(&new_word,new_word);
      if (is_coset_system)
        new_word = Subword(new_word,new_word.find_symbol(coset_symbol)+1);
      transitions[g] = element_words.enter(new_word);
    }
    answer->set_transitions(si,transitions);
  }
  delete [] transitions;

  Label_Count nr_labels = 1;
  State_Count to_do = nr_states;
  answer->clear_accepting(true);
  for (State_ID si = 1;si <= nr_states && to_do;si++)
  {
    if (answer->get_label_nr(si)==0)
    {
      element_words.get(&word,si);
      element_words.remove_entry(si);
      Label_ID label_nr = nr_labels++;
      answer->set_is_accepting(si,true);
      answer->set_label_nr(si,label_nr,true);
      answer->set_label_word(label_nr,word);
      Element_Count class_size = 1;
      if (--to_do)
      {
        conjugate_words.empty();
        conjugate_words.insert(word);
        Element_ID word_nr = 0;
        while (conjugate_words.get(&word,word_nr++) && to_do)
        {
          State_ID nsi;
          if (element_words.find(word,&nsi))
          {
            container.status(2,1,"Creating conjugacy class table ("
                             FMT_ID " of " FMT_ID " to do)." FMT_ID
                             " classes so far.\n",--to_do,nr_states-1,label_nr);
            element_words.remove_entry(nsi);
            answer->set_label_nr(nsi,label_nr);
          }
          for (Ordinal g = 0; g < nr_symbols;g++)
          {
            new_word.set_length(0);
            if (is_coset_system)
              new_word.append(coset_symbol);
            new_word.append(inverse(g));
            new_word += word;
            new_word.append(g);
            reduce(&new_word,new_word);
            if (is_coset_system)
              new_word = Subword(new_word,new_word.find_symbol(coset_symbol)+1);
            conjugate_words.insert(new_word);
          }
        }
        class_size = word_nr-1;
      }
      class_sizes.append_one(class_size);
    }
  }
  answer->change_flags(GFF_TRIM|GFF_MINIMISED|GFF_BFS,0);

  Output_Stream * os = container.open_text_output_file(String::make_filename(&sb,"",filename.string(),".cc_statistics"));
  container.output(os,"_RWS_Conjugacy_Stats := rec\n"
                      "(\n"
                      "  class_representatives :=\n"
                      "  [\n");
  answer->label_word(&word,1);
  word.format(&sb);
  Element_ID i = 1;
  container.output(os,"    [" FMT_ID ",%s]",i,sb.get().string());
  for (i = 2; i < nr_labels;i++)
  {
    answer->label_word(&word,i);
    word.format(&sb);
    container.output(os,",\n    [" FMT_ID ",%s]",i,sb.get().string());
  }
  container.output(os,"\n  ],\n"
                      "  class_sizes :=\n"
                      "  [\n");
  const Element_ID * buffer = class_sizes.buffer();
  i = 1;
  container.output(os,"    [" FMT_ID "," FMT_ID "]",i,buffer[1]);
  for (i = 2; i < nr_labels;i++)
    container.output(os,",\n    [" FMT_ID "," FMT_ID "]",i,buffer[i]);
  container.output(os,"\n  ],\n"
                      "  class_orders :=\n"
                      "  [\n");


  answer->label_word(&word,1);
  i = 1;
  container.output(os,"    [" FMT_ID ",%lu]",i,order(&word,&rws));
  for (i = 2; i < nr_labels;i++)
  {
    answer->label_word(&word,i);
    container.output(os,",\n    [" FMT_ID ",%lu]",i,order(&word,&rws));
  }
  container.output(os,"\n  ]\n"
                      ");\n");
  container.close_output_file(os);
  return answer;
}

/**/

class Conjugator_Factory
{
  public:
    const MAF & maf;
    const Rewriting_System & rws;
    const FSA_Simple &cclass;
    const Alphabet &alphabet;
    const Ordinal nr_generators;
    const Transition_ID nr_transitions;
    const Element_Count nr_classes;
    const Element_Count nr_elements;
    const State_ID identity_si;
    Container & container;
  private:
    Ordinal * last_g;
    FSA_Simple * answer;

  public:
    Conjugator_Factory(const MAF & maf_,
                       const FSA_Simple & cclass_,
                       const Rewriting_System & rws_) :
      maf(maf_),
      rws(rws_),
      alphabet(rws_.base_alphabet),
      nr_generators(rws_.base_alphabet.letter_count()),
      nr_transitions(rws_.base_alphabet.product_alphabet_size()),
      nr_elements(cclass_.state_count() - 1),
      nr_classes(cclass.label_count()-1),
      identity_si(cclass_.initial_state()),
      container(rws_.base_alphabet.container),
      cclass(cclass_),
      answer(0),
      last_g(new Ordinal[nr_elements+1])
    {
      /* Create a product FSA that accepts a word pair (u,v) when u and v are
         accepted words and Vuv is the conjugacy class representative of u.

         First we create a mapping thatrecords the last generator in the
         accepted word for each coset so that we can use cclass like a
         regular coset table. It is probably quicker to do this than to
         attempt to load an already created coset table.

         Then for each class representative c, and each accepted word v we
         compute the accepted word u for vcV. For each u we record the least
         v.

         We then use this table to create a Keyed_FSA with states keyed on
         u and v to create the states of the FSA we require. States are only
         created when they are needed, following the type of plan used for
         building a finite multiplier.
      */

      create_defining_column();
      answer = create(conjugators());
    }

    FSA_Simple * take() const
    {
      return answer;
    }

  private:

    void create_defining_column()
    {
      /* convert cclass to being a coset table */

      /* Next set the label for each accepted word*/
      FSA::Word_Iterator wi(rws);
      State_ID identity_si = cclass.initial_state();
      last_g[identity_si] = PADDING_SYMBOL;
      for (State_ID wa_si = wi.first();(wa_si = wi.next(true))!=0;)
      {
        Element_ID coset_si = cclass.read_word(identity_si,wi.word);
        last_g[coset_si] = wi.word.value(wi.word.length()-1);
      }
    }

    Element_ID * conjugators() const
    {
      /* create the mapping that maps each element to the least
         element (in reduction ordering) that conjugates an element
         to its conjugacy class representative */

      Element_ID * conjugating_element = new Element_ID[nr_elements+1];
      memset(conjugating_element,0,sizeof(Element_ID)*(nr_elements+1));
      Ordinal_Word rep_word(alphabet);
      Ordinal_Word word(alphabet);
      FSA::Word_Iterator wi(rws);

      for (Element_ID rep = 1; rep <= nr_classes;rep++)
      {
        cclass.label_word(&rep_word,rep);
        /* check whether this class contains only a central element*/
        bool singleton = true;
        State_ID coset_rep_si = cclass.read_word(identity_si,rep_word);
        for (Ordinal g = 0; g < nr_generators;g++)
        {
          State_ID coset_si = cclass.new_state(identity_si,maf.inverse(g),false);
          coset_si = cclass.read_word(coset_si,rep_word);
          coset_si = cclass.new_state(identity_si,g,false);
          if (coset_si != coset_rep_si)
          {
            singleton = false;
            break;
          }
        }

        if (singleton)
          conjugating_element[coset_rep_si] = identity_si;
        else
        {
          Element_Count loops = 0;
          for (State_ID wa_si = wi.first();wa_si;wa_si = wi.next(true))
          {
            Element_ID conjugator = cclass.read_word(identity_si,wi.word);
            Element_ID coset_si = cclass.read_word(conjugator,rep_word);
            maf.invert(&word,wi.word);
            coset_si = cclass.read_word(coset_si,word);
            if (!conjugating_element[coset_si])
              conjugating_element[coset_si] = conjugator;
            else
            {
              normal_form(&word,conjugating_element[coset_si]);
              if (wi.word.compare(word) < 0)
                conjugating_element[coset_si] = conjugator;
            }
            if (!(char) ++loops)
              container.status(2,1,"Computing least conjugating elements"
                               " (class/element " FMT_ID "/" FMT_ID " of " FMT_ID "/" FMT_ID ")\n",
                           rep,nr_classes,loops,nr_elements);
          }
        }
      }
      return conjugating_element;
    }

    void normal_form(Ordinal_Word * ow,State_ID coset_si) const
    {
      ow->set_length(0);
      while (coset_si != identity_si)
      {
        Ordinal g = last_g[coset_si];
        g = maf.inverse(g);
        ow->append(g);
        coset_si = cclass.new_state(coset_si,g,false);
      }
      maf.invert(ow,*ow);
    }

    FSA_Simple *create(Element_ID * conjugating_element)
    {
      State_ID key[2];
      key[0] = key[1] = nr_elements+1;
      Pair_Packer key_packer(key);
      void * packed_key;
      Keyed_FSA real_factory(container,alphabet,nr_transitions,
                        max(nr_elements,Element_Count(1024*1024+7)),
                        key_packer.key_size());
      Keyed_FSA &factory = real_factory;
      Ordinal_Word uword(alphabet);
      Ordinal_Word vword(alphabet);
      Ordinal_Word rep_word(alphabet);

      /* Create the labels */
      factory.set_label_type(LT_Words);
      factory.set_nr_labels(nr_classes+1);
      for (Label_ID label = 1; label <= nr_classes;label++)
      {
        cclass.label_word(&rep_word,label);
        factory.set_label_word(label,rep_word);
      }

      /* create the failure and initial states */
      key[0] = 0;
      key[1] = 0;
      factory.find_state(packed_key = key_packer.pack_key(key));
      key[0] = identity_si;
      key[1] = identity_si;
      factory.find_state(key_packer.pack_key(key));

      /* create all the states and transitions */
      for (Element_ID el = 1;el <= nr_elements;el++)
      {
        if (!(char) el)
          container.status(2,1,"Creating conjugating element transitions"
                               " (row " FMT_ID " of " FMT_ID ")\n",
                           el,nr_elements);

        normal_form(&uword,el);
        normal_form(&vword,conjugating_element[el]);
        Word_Length l = uword.length();
        Word_Length r = vword.length();
        const Ordinal * lvalues = uword.buffer();
        const Ordinal * rvalues = vword.buffer();
        Word_Length m = l < r ? r : l;
        State_ID si = 1;
        for (Word_Length i = 0; i < m ; i++)
        {
          Ordinal g0 = i < l ? lvalues[i] : PADDING_SYMBOL;
          Ordinal g1 = i < r ?  rvalues[i] : PADDING_SYMBOL;
          Transition_ID ti = alphabet.product_id(g0,g1);
          State_ID nsi = factory.new_state(si,ti);
          if (!nsi)
          {
            State_ID old_key[2];
            factory.get_state_key(packed_key,si);
            key_packer.unpack_key(old_key);

            key[0] = g0 == PADDING_SYMBOL ? old_key[0] :
                                           cclass.new_state(old_key[0],g0,false);
            key[1] = g1 == PADDING_SYMBOL ? old_key[1] :
                                           cclass.new_state(old_key[1],g1,false);
            nsi = factory.find_state(key_packer.pack_key(key));
            factory.set_transition(si,ti,nsi);
          }
          si = nsi;
        }
      }

      factory.clear_accepting(true);
      for (Element_ID el = 1;el <= nr_elements;el++)
      {
        key[0] = el;
        key[1] = conjugating_element[el];
        State_ID si = factory.find_state(key_packer.pack_key(key));
        factory.set_is_accepting(si,true);
        factory.set_label_nr(si,cclass.get_label_nr(key[0]));
      }
      factory.tidy();
      factory.remove_keys();
      factory.change_flags(GFF_TRIM,GFF_BFS);
      delete [] conjugating_element;
      delete [] last_g;
      answer = FSA_Factory::minimise(factory);
      answer->sort_bfs();
      return answer;
    }
};

FSA_Simple * MAF::build_conjugator(const FSA_Simple & cclass,
                                   const Rewriting_System & rws) const
{
  Conjugator_Factory cf(*this,cclass,rws);
  return cf.take();
}


