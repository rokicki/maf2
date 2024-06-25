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


// $Log: fsa.cpp $
// Revision 1.24  2011/06/14 00:07:53Z  Alun
// Bad bug fixed in FSA_Factory::minimise() which caused minimisation to
// go wrong and change FSA if lots of labelled initial states disappear
// Revision 1.23  2011/06/11 12:02:38Z  Alun
// detabbed!
// Revision 1.22  2011/06/02 12:48:18Z  Alun
// composite() changed to allow building of labelled composite multipliers
// Revision 1.21  2010/06/10 13:56:56Z  Alun
// All tabs removed again
// Revision 1.20  2010/05/17 07:50:38Z  Alun
// determinise_multiplier(), overlap_language(), and (but commented-out)
// two_way_scanner() methods added to FSA_Factory
// management of initial and accepting states changed to reduce memory churn
// Revision 1.19  2009/11/11 00:15:05Z  Alun
// New minimisation algorithm
// Revision 1.18  2009/10/13 23:15:21Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSAs KBMAG would not (if any)
// Revision 1.17  2009/09/15 21:39:35Z  Alun
// Additional source code changes needed to get clean compilation on latest GNU
// Revision 1.16  2009/09/14 09:57:31Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Language_Size data type introduced and used where appropriate
// Revision 1.15  2009/07/29 04:05:11Z  Alun
// compare() added + several new factory methods, and enhanced methods for
// looking for words that can be repeated
// Revision 1.15  2008/11/03 10:27:41Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.14  2008/10/09 19:50:12Z  Alun
// Extra format options for print()
// Improved key packing for exists(),reverse(),star(),concat()
// Various new "standard" automata added
// Revision 1.13  2008/09/29 20:38:21Z  Alun
// Switch to using Hash+Collection_Manager as replacement for Indexer.
// Currently this is about 10% slower, but this is a more flexible
// architecture.
// Revision 1.6  2007/12/20 23:25:42Z  Alun
//

/* This module contains most of the functions needed for manipulating
   generic FSAs.
   See fsa.h for general comments
*/
#include <string.h>
#include <limits.h>
#include "awcc.h"
#include "awdefs.h"
#include "mafword.h"
#include "fsa.h"
#include "keyedfsa.h"
#include "maf.h"
#include "alphabet.h"
#include "container.h"
#include "maf_cfp.h"
#include "maf_el.h"
#include "maf_spl.h"
#include "maf_wdb.h"
#include "maf_ss.h"

/**/

String const FSA::flag_names[] =
{
 "DFA",
 "NFA",
 "MIDFA",
 "minimized",
 "accessible",
 "trim",
 "sparse",
 "dense",
 "BFS",
 "RWS",
 0
};

String const FSA::state_types[] =
{
  "simple",
  "identifiers",
  "strings",
  "words",
  "list of words",
  "list of integers",
  "labelled",
  "labeled",
  "product",
  0
};

FSA::FSA(Container & container_,const Alphabet &alphabet) :
  Special_Subset_Owner(container_),
  base_alphabet(alphabet),
  name(0)
{
  base_alphabet.attach();
}

FSA::~FSA()
{
  base_alphabet.detach();
  if (name)
    delete [] name;
}

/**/

State_Count FSA::accessible_states(bool * accessible,State_ID start_state) const
{
  State_Count nr_states = state_count();
  Transition_ID nr_transitions = alphabet_size();
  State_ID final_count = 0;
  State_List sl;

  sl.reserve(nr_states,false);
  memset(accessible,0,nr_states);
  if (!is_valid_state(start_state))
    start_state = 0;
  accessible[start_state] = true;
  sl.append_one(start_state);
  final_count++;
  State_List::Iterator sli(sl);
  for (State_ID si = sli.first();si;si = sli.next())
  {
    for (Transition_ID ti = 0; ti < nr_transitions;ti++)
    {
      State_ID nsi = new_state(si,ti);
      if (!is_valid_state(nsi))
        nsi = 0; /* This deals with RWS FSAs */
      if (!accessible[nsi])
      {
        final_count++;
        accessible[nsi] = true;
        if (nsi)
          sl.append_one(nsi);
      }
    }
  }

  return final_count;
}

/**/

Word_Length FSA::defining_length(State_ID state) const
{
  /* Returns the length of the shortest input word that reaches the
     specified state from the initial state. The return value is
     INVALID_LENGTH if the state is inaccessible, or it is discovered that
     the FSA is not BFS sorted before the defining transition is
     encountered. */

  State_Count nr_states = state_count();
  if (state >= nr_states)
    return INVALID_LENGTH;
  State_Definition definition;
  Word_Length answer = 0;
  State_ID prefix_state = state;
  State_ID initial = initial_state();
  while (!is_initial(prefix_state) && get_definition(&definition,prefix_state))
  {
    answer++;
    if (definition.state >= prefix_state)
      return INVALID_LENGTH; /* We risk getting trapped in a loop of inaccessible
                            states here, so the FSA is certainly not BFS and
                            we are allowed to return WHOLE_WORD */
    prefix_state = definition.state;
  }
  if (is_initial(prefix_state))
    return answer;

  const Transition_ID nr_symbols = alphabet_size();
  if (initial != 1)
    return INVALID_LENGTH;
  Word_Length state_length = 0;
  State_ID ceiling = 1;
  State_ID si;
  State_ID count = 2;

  for (si = 1;si < count ;si++)
  {
    Word_Length length = si >= ceiling ? state_length : state_length - 1;
    for (Transition_ID ti = 0;ti < nr_symbols;ti++)
    {
      State_ID nsi = new_state(si,ti);
      if (nsi >= count)
      {
        if (nsi == state)
          return length+1;
        if (nsi > count)
          return INVALID_LENGTH; /* not BFS */
        if (si >= ceiling)
        {
          state_length++;
          ceiling = nsi;
        }
        count++;
      }
    }
  }
  return INVALID_LENGTH; // we should never get here
}

/**/

bool FSA::defining_word(Ordinal_Word * lhs_word,State_ID state,
                        Word_Length length,Ordinal_Word * rhs_word,
                        State_ID * initial) const
{
  State_ID prefix_state = state;
  State_Definition definition;

  if (alphabet_size() == base_alphabet.letter_count())
  {
    if (rhs_word)
      rhs_word->set_length(0);

    if (length == WHOLE_WORD)
      length = defining_length(state);

    lhs_word->set_length(length);
    Ordinal *values = lhs_word->buffer();
    Word_Length i = 1;
    while (get_definition(&definition,prefix_state))
    {
      values[length-i++] = Ordinal(definition.symbol_nr);
      prefix_state = definition.state;
    }
    if (!is_initial(prefix_state))
    {
      lhs_word->set_length(0);
      return false;
    }
    if (initial)
      *initial = prefix_state;
    return true;
  }

  Word_Length l = 0;
  Word_Length r = 0;
  Ordinal nr_generators = Ordinal(base_alphabet.letter_count());
  if (length == WHOLE_WORD)
  {
    length = 0;
    while (get_definition(&definition,prefix_state))
    {
      length++;
      Ordinal g = definition.symbol_nr / (nr_generators+1);
      if (g != nr_generators)
        l++;
      g = definition.symbol_nr % (nr_generators+1);
      if (g != nr_generators)
        r++;
      prefix_state = definition.state;
    }
    if (!is_initial(prefix_state))
    {
      l = r = length = 0;
      prefix_state = 0;
    }
    else
      prefix_state = state;
  }
  else
    l = r = length;

  Word_Length i = 0,j = 0,k = 0;
  lhs_word->allocate(l);
  Ordinal * lvalues = lhs_word->buffer();
  Ordinal * rvalues = 0;
  if (rhs_word)
  {
    rhs_word->allocate(r);
    rvalues = rhs_word->buffer();
  }

  for (i = 0; i < length;i++)
  {
    get_definition(&definition,prefix_state);
    Ordinal g = definition.symbol_nr / (nr_generators+1);
    if (g != nr_generators)
      lvalues[l-++j] = g;
    if (rvalues)
    {
      g = definition.symbol_nr % (nr_generators+1);
      if (g != nr_generators)
        rvalues[r-++k] = g;
    }
    prefix_state = definition.state;
  }

  if (j < l)
    *lhs_word = Subword(*lhs_word,l-j);
  if (rhs_word && k < r)
    *rhs_word = Subword(*rhs_word,r-k);
  if (initial)
    *initial = prefix_state;
  return prefix_state != 0;
}

/**/

bool FSA::accepting_path(Ordinal_Word * lhs_word,State_ID state,
                         Ordinal_Word * rhs_word,State_ID * accept) const
{
  State_ID ns = state;
  Accept_Definition definition;
  bool length_set = false;

  if (get_accept_definition(&definition,ns))
  {
    length_set = true;
    if (definition.distance > MAX_WORD)
      return false;
    lhs_word->set_length(Word_Length(definition.distance));
    Ordinal nr_generators = base_alphabet.letter_count();
    if (alphabet_size() == nr_generators)
    {
      if (rhs_word)
        rhs_word->set_length(0);

      Ordinal *values = lhs_word->buffer();
      Word_Length i = 0;
      while (definition.distance)
      {
        values[i++] = Ordinal(definition.symbol_nr);
        ns = new_state(ns,definition.symbol_nr,false);
        get_accept_definition(&definition,ns);
      }
    }
    else
    {
      Ordinal *lvalues = lhs_word->buffer();
      Ordinal * rvalues = 0;
      if (rhs_word)
      {
        rhs_word->allocate(Word_Length(definition.distance));
        rvalues = rhs_word->buffer();
      }

      Word_Length l = 0;
      Word_Length r = 0;
      while (definition.distance)
      {
        Ordinal g = definition.symbol_nr / (nr_generators+1);
        if (g != nr_generators)
          lvalues[l++] = g;
        if (rvalues)
        {
          g = definition.symbol_nr % (nr_generators+1);
          if (g != nr_generators)
            rvalues[r++] = g;
        }
        ns = new_state(ns,definition.symbol_nr,false);
        get_accept_definition(&definition,ns);
      }
      lhs_word->set_length(l);
      if (rvalues)
        rhs_word->set_length(r);
    }
  }

  if (!length_set)
  {
    lhs_word->set_length(0);
    if (rhs_word)
      rhs_word->set_length(0);
  }

  if (is_accepting(ns))
  {
    if (accept)
      *accept = ns;
    return true;
  }
  return false;
}

/**/

FSA::Principal_Value_Cache::Principal_Value_Cache(const FSA & fsa)
{
  defining_prefix = new Packed_Word[fsa.state_count()];
  accepting_suffix = new Packed_Word[fsa.state_count()];
}

FSA::Principal_Value_Cache::~Principal_Value_Cache()
{
  delete [] defining_prefix;
  delete [] accepting_suffix;
}

bool FSA::principal_value(Principal_Value_Cache * cache,
                          Ordinal_Word * lhs_word,State_ID state,Transition_ID ti,
                          Ordinal_Word * rhs_word,State_ID *initial_state,State_ID * accept) const
{
  State_ID ns = ti == -1 ? state : new_state(state,ti);
  if (!ns)
    return false;

  Ordinal_Word lhs_prefix(base_alphabet);
  Ordinal_Word rhs_prefix(base_alphabet);
  Ordinal_Word lhs_suffix(base_alphabet);
  Ordinal_Word rhs_suffix(base_alphabet);

  lhs_word->set_length(0);
  if (rhs_word)
    rhs_word->set_length(0);
  bool have_suffix = false;
  bool have_prefix = false;
  bool can_cache = false;
  Ordinal nr_generators = base_alphabet.letter_count();

  if (cache && alphabet_size() == nr_generators)
  {
    can_cache = true;
    if (cache->defining_prefix[state])
    {
      lhs_prefix.unpack(cache->defining_prefix[state]);
      have_prefix = true;
    }
    if (cache->accepting_suffix[ns])
    {
      lhs_suffix.unpack(cache->accepting_suffix[ns]);
      have_suffix = true;
    }
  }
  if (!have_suffix)
  {
    if (!accepting_path(&lhs_suffix,ns,&rhs_suffix,accept))
      return false;
    if (can_cache)
      cache->accepting_suffix[ns] = lhs_suffix;
  }
  if (!have_prefix)
  {
    if (!defining_word(&lhs_prefix,state,WHOLE_WORD,&rhs_prefix,initial_state))
      return false;
    if (can_cache)
      cache->defining_prefix[state] = lhs_prefix;
  }
  *lhs_word = lhs_prefix;
  if (ti != -1)
    if (alphabet_size() == nr_generators)
    {
      lhs_word->append(Ordinal(ti));
      rhs_word = 0;
    }
    else
    {
      Ordinal g = ti / (nr_generators+1);
      if (g != nr_generators)
        lhs_word->append(g);
    }
  *lhs_word += lhs_suffix;
  if (rhs_word)
  {
    *rhs_word = rhs_prefix;
    if (ti != -1)
    {
      Ordinal g = ti % (nr_generators+1);
      if (g != nr_generators)
        rhs_word->append(g);
    }
    *rhs_word += rhs_suffix;
  }
  return true;
}

/**/

bool FSA::is_product_fsa() const
{
  return this && alphabet_size() == base_alphabet.product_alphabet_size();
}

/**/

bool FSA::label_word(Ordinal_Word * answer,Label_ID label,Element_ID word_nr) const
{
  if (!labels_are_words())
  {
    answer->set_length(0);
    return false;
  }
  const Byte * data = (const Byte *) get_label_data(label);
  if (label_type() == LT_Words)
  {
    if (word_nr != 0)
    {
      answer->set_length(0);
      return false;
    }
    if (answer->alphabet() == label_alphabet())
      answer->extra_unpack(data);
    else
    {
      Ordinal_Word ow(label_alphabet());
      ow.extra_unpack(data);
      *answer = ow;
    }
    return data != 0;
  }
  if (label_type() == LT_List_Of_Words)
  {
    Word_List wl(label_alphabet());
    wl.unpack(data);
    if (word_nr < wl.count())
    {
      *answer = Entry_Word(wl,word_nr);
      return true;
    }
    answer->set_length(0);
  }
  return false;
}

/**/

bool FSA::label_word_list(Word_List * answer,Label_ID label) const
{
  const Byte * data = (const Byte *) get_label_data(label);
  if (label_type() == LT_List_Of_Words)
  {
    answer->unpack(data);
    return data != 0;
  }
  else
  {
    answer->empty();
    if (label_type() == LT_Words)
    {
      Ordinal_Word ow(label_alphabet());
      ow.extra_unpack(data);
      answer->add(ow);
      return true;
    }
  }
  return false;
}

/**/

Language_Size FSA::language_size(Word_Length *max_word_length,bool exact,State_ID start_state) const
{
  /* Count the size of the accepted language. The FSA should be trim
     because otherwise there may be cycles of failing states which will
     make the language seem infinite even though it is not.
     Return this size, or -1 if (apparently) infinite, or -2 if the number is
     finite but too big for the return type.
     If exact is false, the method does not work out the exact size
     of a finite language, but returns 0 if there are no accept states or
     2 if there are any accept states (even if they are inaccessible).
  */
  if (max_word_length)
    *max_word_length = 0;

  if (accept_type() == SSF_Empty)
    return 0;

  if (has_multiple_initial_states())
  {
    /* We can't count the language of a MIDFA this way. */
    if (start_state == -1)
    {
      FSA_Simple * fsa_temp = FSA_Factory::determinise(*this);
      Language_Size answer = fsa_temp->language_size(max_word_length,exact,start_state);
      delete fsa_temp;
      return answer;
    }
  }

  const Transition_ID nr_symbols = alphabet_size();
  const State_Count nr_states = state_count();
  State_ID i;
  Transition_ID j;
  if (start_state == -1)
    start_state = initial_state();
  if (nr_states == 1) /* If there is only one state it is the failure state */
    return 0;
  if (start_state == 0) /* If we start in the failure state we can't leave it */
    return 0;
  bool check_accessible = start_state != initial_state();
  bool * accessible = 0;
  State_Count nr_accessible = nr_states;
  if (check_accessible)
  {
    accessible = new bool[nr_states];
    nr_accessible = accessible_states(accessible,start_state);
    if (!accessible[0])
    {
      /* If we can't get into the failure state the language must be infinite */
      delete accessible;
      *max_word_length = WHOLE_WORD;
      return LS_INFINITE;
    }
  }

  /* We first count the number of transitions into each state */
  State_Count * in_degree = new State_Count[nr_states];
  for (i = 0; i < nr_states;i++)
    in_degree[i] = 0;
  for (i = 1;i < nr_states;i++)
    if (!check_accessible || accessible[i])
    {
      for (j = 0; j < nr_symbols;j++)
      {
        State_ID ns = new_state(i,j);
        if (is_valid_state(ns))
          in_degree[ns]++;
      }
    }
  if (accessible)
    delete [] accessible;
  /* Now we try to order the states so that if state s <= state t, then there
     is no path from state t to state s. If this is not possible, then the
     accepted language must be infinite.
   */

  State_ID * ordered_state_list = 0;
  bool is_infinite = in_degree[start_state] > 0;

  if (!is_infinite)
  {
    ordered_state_list = new State_ID[nr_states];
    ordered_state_list[1] = start_state;

    State_Count count = 2;
    i = 0;
    while (++i < count)
    {
      State_ID state = ordered_state_list[i];
      for (j = 0;j < nr_symbols;j++)
      {
        State_ID ns = new_state(state,j);
        if (is_valid_state(ns))
        {
          in_degree[ns]--;
          if (in_degree[ns]==0)
            ordered_state_list[count++] = ns;
         }
      }
    }
    if (count != nr_accessible)
      is_infinite = true;
  }

  delete [] in_degree;
  Language_Size answer = 2;

  if (!is_infinite && exact)
  {
    /* We have built the list, so the accepted language is finite. Now we work
       backwards through the list, calculating the number of accepted words
       starting at that state.
     */
    Language_Size * word_count = new Language_Size[nr_states];
    for (i = nr_accessible-1;i >= 1;i--)
    {
      State_ID state = ordered_state_list[i];
      word_count[state] = 0;
      for (j = 0;j < nr_symbols;j++)
      {
        State_ID ns = new_state(state,j);
        if (ns > 0)
          if (word_count[state] > LS_HUGE - word_count[ns])
            word_count[state] = LS_HUGE;
          else
            word_count[state] += word_count[ns];
      }
      if (is_accepting(state) && word_count[state] < LS_HUGE)
        word_count[state]++;
    }
    answer = word_count[start_state];
    delete [] word_count;
  }

  if (max_word_length)
  {
    if (is_infinite)
      *max_word_length = WHOLE_WORD;
    else
    {
      Word_Length * word_length = new Word_Length[nr_states];
      for (i = nr_accessible-1;i >= 1;i--)
      {
        State_ID state = ordered_state_list[i];
        word_length[state] = 0;
        for (j = 0;j < nr_symbols;j++)
        {
          State_ID ns = new_state(state,j);
          if (ns > 0)
            if (word_length[ns] >= MAX_WORD)
              word_length[state] = WHOLE_WORD-1;
            else if (word_length[state] <= word_length[ns])
              word_length[state] = word_length[ns]+1;
        }
      }
      *max_word_length = word_length[start_state];
      delete [] word_length;
    }
  }
  if (ordered_state_list)
    delete [] ordered_state_list;
  return is_infinite ? LS_INFINITE : answer;
}

/**/

State_Count FSA::nr_accessible_states(Word_Length max_input_length) const
{
  /* This method counts the number of different non failure states of the
     FSA that can be reached from the initial state with an input string no
     longer than the specified number of characters.
     This method only works if the FSA is BFS sorted and has initial state
     1.
  */
  const Transition_ID nr_symbols = alphabet_size();
  if (initial_state() != 1)
    return 0;
  Word_Length state_length = 0;
  State_ID ceiling = 1;
  State_ID si;
  State_ID count = 2;

  for (si = 1;si < count ;si++)
  {
    Word_Length length = si >= ceiling ? state_length : state_length - 1;
    if (length > max_input_length)
      return si - 1;
    for (Transition_ID ti = 0;ti < nr_symbols;ti++)
    {
      State_ID nsi = new_state(si,ti);
      if (nsi >= count)
      {
        if (nsi > count)
          return 0; /* not BFS */
        if (si >= ceiling)
        {
          state_length++;
          ceiling = nsi;
        }
        count++;
      }
    }
  }
  return si-1;
}

/**/

static void print_labels(Output_Stream * stream,const FSA & fsa,String indent)
{
  Label_ID nr_labels = fsa.label_count();
  Container & container = fsa.container;

  String_Buffer sb;
  for (Label_ID li = 1; li < nr_labels;li++)
    container.output(stream,li+1 < nr_labels ? "%s[" FMT_ID ",%s],\n" :
                                              "%s[" FMT_ID ",%s]\n",
                     indent.string(),li,fsa.label_text(&sb,li).string());
}

/**/

void FSA::print(Output_Stream * stream,unsigned tpf) const
{
  /* This function outputs FSAs in the same format as KBMAG
     (but with different whitespace).*/

  const Transition_ID nr_symbols = alphabet_size();
  const State_ID nr_states = state_count();
  const Label_ID nr_labels = label_count();
  bool is_product = is_product_fsa();
  bool all_accepted = false;
  bool all_initial = false;

  if (tpf & FF_ANNOTATE)
  {
    if (initial_type() != SSF_All)
      ((FSA *) this)->create_definitions(true);
    else
      all_initial = true;
    if (accept_type() != SSF_All)
      ((FSA *) this)->create_accept_definitions();
    else
      all_accepted = true;
    if (all_accepted && all_initial)
      tpf &= ~FF_ANNOTATE;
  }

  container.output(stream,"%s := rec\n"
                          "(\n"
                          "  isFSA := true,\n",
                          name ? name : "fsa");
  base_alphabet.print(container,stream,is_product ? APF_GAP_Product : APF_GAP_Normal);
  container.output(stream,"  states := rec\n"
                          "  (\n");
  if (nr_labels > 1)
  {
    bool simple_labels = nr_labels == nr_states;
    if (simple_labels)
    {
      for (State_ID s = 1;s < nr_states;s++)
        if (get_label_nr(s) != s)
        {
          simple_labels = false;
          break;
        }
    }

    if (simple_labels)
    {
      container.output(stream,"    type := \"%s\",\n"
                              "    size := " FMT_ID ",\n"
                              "    alphabet := [",
                              state_types[label_type()].string(),
                              nr_states-1);
      label_alphabet().print(container,stream,APF_Bare);
      container.output(stream,"],\n"
                              "    format := \"sparse\",\n"
                              "    names := \n"
                              "    [\n");
      print_labels(stream,*this,"      ");
      container.output(stream,"    ]\n"
                              "  ),\n");
    }
    else
    {
      container.output(stream,"    type := \"labelled\",\n"
                              "    size := " FMT_ID ",\n"
                              "    labels := rec\n"
                              "    (\n"
                              "      type := \"%s\",\n"
                              "      size := " FMT_ID ",\n"
                              "      alphabet := [",nr_states-1,
                       state_types[label_type()].string(),nr_labels-1);

      label_alphabet().print(container,stream,APF_Bare);
      container.output(stream,"],\n"
                              "      format := \"sparse\",\n"
                              "      names := \n"
                              "      [\n");
      print_labels(stream,*this,"        ");
      container.output(stream,"      ]\n"
                              "    ),\n"
                              "    format := \"sparse\",\n"
                              "    setToLabels :=\n"
                              "    [\n");
      bool started = false;
      for (State_ID s = initial_state();s < nr_states;s++)
      {
        Label_ID l = get_label_nr(s);
        if (l != 0)
        {
          container.output(stream,started ? ",\n      [" FMT_ID "," FMT_ID "]" :
                                            "      [" FMT_ID "," FMT_ID "]" ,s,l);
          started = true;
        }
      }
      if (started)
        container.output(stream,"\n");
      container.output(stream,"    ]\n"
                              "  ),\n");
    }
  }
  else
    container.output(stream,"    type := \"simple\",\n"
                            "    size := " FMT_ID "\n"
                            "  ),\n",nr_states-1);
  if (has_multiple_initial_states())
    container.output(stream,"  flags := [\"MIDFA\"");
  else
    container.output(stream,"  flags := [\"DFA\"");

  /* Output other flag values */
  {
    unsigned flags = get_flags() & ~(GFF_DFA|GFF_MIDFA|GFF_NFA);
    int i;
    for (i = 0; flag_names[i] ;i++)
      if (flags & (1 << i))
        container.output(stream,",\"%s\"",flag_names[i].string());
    container.output(stream,"],\n");
  }

  container.output(stream,"  initial := [");
  print_initial(stream);
  container.output(stream,"],\n");
  container.output(stream,"  accepting := [");
  print_accepting(stream);
  container.output(stream,"],\n");

  if (!(tpf & FF_DENSE+FF_SPARSE) && is_product)
    tpf |= FF_SPARSE;
  if (tpf & FF_SPARSE)
    container.output(stream,"  table := rec\n"
                            "  (\n"
                            "    format := \"sparse\",\n"
                            "    transitions :=\n"
                            "    [\n");
  else
    container.output(stream,"  table := rec\n"
                            "  (\n"
                            "    format := \"dense deterministic\",\n"
                            "    transitions :=\n"
                            "    [\n");
  String_Buffer transition_annotation;
  for (State_ID si = 1; si < nr_states ;si++)
  {
    if (tpf & FF_ANNOTATE)
    {
      container.output(stream,"    #" FMT_ID ,si);

      Ordinal_Word lhs_word(base_alphabet);
      Ordinal_Word rhs_word(base_alphabet);
      String_Buffer sb1,sb2;
      if (!all_initial)
      {
        State_ID initial_si;
        if (is_initial(si))
          container.output(stream," initial state");
        else if (defining_word(&lhs_word,si,WHOLE_WORD,&rhs_word,&initial_si))
        {
          if (is_product)
            container.output(stream," reached by: " FMT_ID "^(%s,%s)",initial_si,
                           lhs_word.format(&sb1).string(),rhs_word.format(&sb2).string());
          else
            container.output(stream," reached by: " FMT_ID "^%s",initial_si,
                            lhs_word.format(&sb1).string());
        }
      }

      if (!all_accepted)
      {
        State_ID accept_si;
        if (!all_initial)
          container.output(stream,", ");
        if (is_accepting(si))
          container.output(stream,"accept state");
        else if (accepting_path(&lhs_word,si,&rhs_word,&accept_si))
        {
          if (is_product)
            container.output(stream,"^(%s,%s)=" FMT_ID " is accepted",
                             lhs_word.format(&sb1).string(),
                             rhs_word.format(&sb2).string(),
                             accept_si);
           else
            container.output(stream,"^%s=" FMT_ID " is accepted",
                             lhs_word.format(&sb1).string(),accept_si);
        }
      }
      container.output(stream,"\n");
    }
    else if (tpf & FF_COMMENT && (si % 5 == 0 ||
                             is_product && tpf & FF_ANNOTATE_TRANSITIONS))
      container.output(stream,"    #" FMT_ID "\n",si);

    container.output(stream,"      [");
    if (tpf & FF_SPARSE)
    {
      bool done = false;
      bool line_started = false;
      Ordinal last_g1 = -1,g1,g2;
      for (Transition_ID ti = 0; ti < nr_symbols;ti++)
      {
        State_ID nsi = new_state(si,ti);
        if (nsi)
        {
          if (done)
            container.output(stream,",");
          if (is_product && tpf & FF_ANNOTATE_TRANSITIONS)
          {
            base_alphabet.product_generators(&g1,&g2,ti);
            if (g1 != last_g1)
            {
              if (line_started)
                container.output(stream," #%s\n       ",
                                 transition_annotation.get().string());
              last_g1 = g1;
              transition_annotation.empty();
              line_started = false;
            }
            if (line_started)
              transition_annotation.append(",");
            String_Buffer temp;
            temp.format("(%s,%s)",g1 == PADDING_SYMBOL ? "_" : base_alphabet.glyph(g1).string(),
                                  g2 == PADDING_SYMBOL ? "_" : base_alphabet.glyph(g2).string());
            transition_annotation.append(temp.get());
          }
          container.output(stream,"[" FMT_TID "," FMT_ID "]",ti+1,nsi);
          line_started = done = true;
        }
      }
      if (is_product && tpf & FF_ANNOTATE_TRANSITIONS && line_started)
        if (line_started)
          container.output(stream," #%s\n      ",transition_annotation.get().string());
    }
    else
    {
      for (Transition_ID ti = 0; ti < nr_symbols;ti++)
      {
        if (ti != 0)
          container.output(stream,",");
        container.output(stream,"" FMT_ID ,new_state(si,ti));
      }
    }

    if (si+1 < nr_states)
      container.output(stream,"],\n");
    else
      container.output(stream,"]\n");
  }
  container.output(stream,"    ]\n  )\n);\n");
}

/**/

bool FSA::product_accepted(const Word & lhs_word,const Word & rhs_word) const
{
  if (!is_product_fsa())
    return false;

  State_ID state = initial_state();
  Word_Length l = lhs_word.length();
  Word_Length r = rhs_word.length();
  const Ordinal * lvalues = lhs_word.buffer();
  const Ordinal * rvalues = rhs_word.buffer();
  Word_Length i,j;
  Word_Length common = r < l ? r : l;

  for (i = 0; i < common;i++)
  {
    state = new_state(state,base_alphabet.product_id(lvalues[i],rvalues[i]),false);
    if (!state)
      return false;
  }
  j = common;
  while (i < l || j < r)
  {
    Ordinal lvalue = i < l ? lvalues[i++] : PADDING_SYMBOL;
    Ordinal rvalue = j < r ? rvalues[j++] : PADDING_SYMBOL;
    state = new_state(state,base_alphabet.product_id(lvalue,rvalue),false);
    if (!state)
      return false;
  }
  return is_accepting(state);
}

/**/

State_ID FSA::read_product(const Word & word1,const Word & word2) const
{
  if (!is_product_fsa())
    return 0;
  State_ID state = initial_state();
  Word_Length w1_length = word1.length();
  Word_Length w2_length = word2.length();
  const Ordinal * lvalues = word1.buffer();
  const Ordinal * rvalues = word2.buffer();
  Word_Length m = max(w1_length,w2_length);

  for (Word_Length i = 0;i < m;i++)
  {
    Ordinal g1 = i < w1_length ? lvalues[i] : PADDING_SYMBOL;
    Ordinal g2 = i < w2_length ? rvalues[i] : PADDING_SYMBOL;
    state = new_state(state,base_alphabet.product_id(g1,g2),false);
    if (!state)
      break;
  }
  return state;
}

/**/

State_ID FSA::read_word(State_ID state,const Word & word) const
{
  if (is_product_fsa())
    return 0;
  Word_Length word_length = word.length();
  const Ordinal * values = word.buffer();
  for (Word_Length i = 0;i < word_length;i++)
  {
    state = new_state(state,values[i],false);
    if (!is_valid_state(state))
      break;
  }
  return state;
}

/**/

void FSA::save(String filename,unsigned tpf)
{
  Output_Stream * os = container.open_text_output_file(filename);
  print(os,tpf);
  container.close_output_file(os);
}

/**/

bool FSA::accepts(const Transition_ID * word,size_t word_length) const
{
  State_ID state = initial_state();
  while (word_length--)
  {
    state = new_state(state,*word++,false);
    if (!is_valid_state(state))
      return false;
  }
  return is_accepting(state);
}

/**/

bool FSA::accepts(const Ordinal * word,size_t word_length) const
{
  State_ID state = initial_state();
  while (word_length--)
  {
    state = new_state(state,*word++,false);
    if (!is_valid_state(state))
      return false;
  }
  return is_accepting(state);
}

/**/

bool FSA::accepts(const Word & word) const
{
  return accepts(word.buffer(),word.length());
}

/**/

State_ID FSA::repetend(const Word & word,bool allow_repeat) const
{
  State_ID nr_states = state_count();
  State_ID si;
  for (si = 1; si < nr_states;si++)
    if (read_word(si,word)==si)
      return si;
  if (allow_repeat)
  {
    for (si = 1; si < nr_states;si++)
    {
      State_ID nsi = si;
      Element_List el;
      while (nsi)
      {
        if (!el.insert(nsi))
          return nsi;
        nsi = read_word(nsi,word);
      }
    }
  }
  return 0;
}

/**/

bool FSA::is_repetend(State_ID si,const Word & word) const
{
  Element_List el;
  while (is_valid_state(si))
  {
    if (!el.insert(si))
      return true;
    si = read_word(si,word);
  }
  return false;
}

/**/

FSA::Word_Iterator::Word_Iterator(const FSA & fsa_) :
  state(new State_ID[MAX_WORD+1]),
  fsa(fsa_),
  word(*(current = new Ordinal_Word(fsa_.base_alphabet))),
  depth(0),
  nr_symbols(Ordinal(fsa_.alphabet_size()))
{
  state[0] = fsa.initial_state();
}

FSA::Word_Iterator::~Word_Iterator()
{
  delete [] state;
  delete current;
}

State_ID FSA::Word_Iterator::first(const Ordinal_Word * start_word)
{
  state[0] = fsa.initial_state(ssi,true);
  if (start_word)
    *current = *start_word;
  else
    current->set_length(0);
  Word_Length length = current->length();
  State_ID nsi = state[0];
  Word_Length i;
  for (i = 0; i < length;i++)
  {
    nsi = state[i+1] = fsa.new_state(state[i],current->value(i));
    if (!nsi)
      break;
  }
  depth = i;
  if (nsi)
    return nsi;
  return next(true);
}

/**/

State_ID FSA::Word_Iterator::next(bool inside)
{
  if (!inside)
  {
    if (!depth)
      return 0;
    depth--;
  }

  for (;;)
  {
    Ordinal v = current->value(depth)+1;
    if (v < nr_symbols && depth < MAX_WORD)
    {
      current->set_length(depth+1);
      current->set_code(depth,v);
      State_ID nsi = state[depth+1] = fsa.new_state(state[depth],v);
      if (nsi > 0)
      {
        depth++;
        return nsi;
      }
    }
    else
    {
      if (!depth)
      {
        state[0] = fsa.initial_state(ssi,false);
        current->set_length(0);
        if (!state[0])
          return 0;
        return state[0];
      }
      else
        depth--;
    }
  }
}

FSA::Product_Iterator::Product_Iterator(const FSA & fsa_) :
  state(new State_ID[MAX_WORD+1]),
  fsa(fsa_),
  left_word(*(current_left = new Ordinal_Word(fsa_.base_alphabet))),
  right_word(*(current_right = new Ordinal_Word(fsa_.base_alphabet))),
  depth(0),
  nr_symbols(fsa_.alphabet_size()),
  transitions(new Transition_ID[MAX_WORD+1]),
  left_generators(new Ordinal[nr_symbols]),
  right_generators(new Ordinal[nr_symbols])
{
  state[0] = fsa.initial_state();
  Ordinal g1,g2;
  Ordinal nr_generators = fsa.base_alphabet.letter_count();
  for (g1 = PADDING_SYMBOL; g1 < nr_generators;g1++)
    for (g2 = PADDING_SYMBOL; g2 < nr_generators;g2++)
    {
      Transition_ID ti = fsa.base_alphabet.product_id(g1,g2);
      if (ti < nr_symbols)
      {
        left_generators[ti] = g1;
        right_generators[ti] = g2;
      }
    }
}

/**/

FSA::Product_Iterator::~Product_Iterator()
{
  delete [] state;
  delete [] transitions;
  delete [] left_generators;
  delete [] right_generators;
  delete current_left;
  delete current_right;
}

State_ID FSA::Product_Iterator::first(const Ordinal_Word * start_left,
                                   const Ordinal_Word * start_right)
{
  state[0] = fsa.initial_state(ssi,true);
  if (start_left)
    *current_left = *start_left;
  else
    current_left->set_length(0);
  if (start_right)
    *current_right = *start_right;
  else
    current_right->set_length(0);

  State_ID nsi = state[0];
  Word_Length w1_length = current_left->length();
  Word_Length w2_length = current_right->length();
  const Ordinal * lvalues = current_left->buffer();
  const Ordinal * rvalues = current_right->buffer();
  Word_Length m = max(w1_length,w2_length);

  for (Word_Length i = 0;i < m;i++)
  {
    Ordinal g1 = i < w1_length ? lvalues[i] : PADDING_SYMBOL;
    Ordinal g2 = i < w2_length ? rvalues[i] : PADDING_SYMBOL;
    transitions[i] = fsa.base_alphabet.product_id(g1,g2);
    nsi = fsa.new_state(nsi,transitions[i],false);
    if (!nsi)
      break;
  }
  depth = m;
  transitions[depth] = -1;
  if (nsi)
    return nsi;
  return next(true);
}

/**/

State_ID FSA::Product_Iterator::next(bool inside)
{
  if (!inside)
  {
    if (!depth)
      return 0;
    depth--;
    Ordinal g1 = left_generators[transitions[depth]];
    Ordinal g2 = right_generators[transitions[depth]];
    if (g1 != PADDING_SYMBOL)
      current_left->set_length(current_left->length()-1);
    if (g2 != PADDING_SYMBOL)
      current_right->set_length(current_right->length()-1);
  }

  for (;;)
  {
    Transition_ID ti = transitions[depth] + 1;
    if (ti < nr_symbols && depth < MAX_WORD)
    {
      transitions[depth] = ti;
      State_ID nsi = state[depth+1] = fsa.new_state(state[depth],ti);
      if (nsi)
      {
        Ordinal g1 = left_generators[ti];
        Ordinal g2 = right_generators[ti];
        if (g1 != PADDING_SYMBOL)
          current_left->append(g1);
        if (g2 != PADDING_SYMBOL)
          current_right->append(g2);
        depth++;
        transitions[depth] = -1;
        return nsi;
      }
    }
    else
    {
      if (!depth)
      {
        state[0] = fsa.initial_state(ssi,false);
        if (!state[0])
          return 0;
        else
        {
          transitions[depth] = -1;
          return state[0];
        }
      }
      else
      {
        depth--;
        Ordinal g1 = left_generators[transitions[depth]];
        Ordinal g2 = right_generators[transitions[depth]];
        if (g1 != PADDING_SYMBOL)
          current_left->set_length(current_left->length()-1);
        if (g2 != PADDING_SYMBOL)
          current_right->set_length(current_right->length()-1);
      }
    }
  }
}

/**/

FSA_Common::FSA_Common(Container & container_,const Alphabet &alphabet) :
  FSA(container_,alphabet),
  accepting(* new Special_Subset(*this,SSF_All)),
  initial(* new Special_Subset(*this,SSF_Singleton)),
  nr_labels(0),
  label_nr(0),
  label_data(0),
  label_format(LT_Unlabelled),
  label_association(LA_Direct),
  compressor(0),
  state_definitions(0),
  accept_definitions(0),
  lengths(0),
  flags(0),
  label_alphabet__(&base_alphabet)
{
}

/**/

FSA_Common::~FSA_Common()
{
  if (compressor)
    delete compressor;
  if (state_definitions)
    delete [] state_definitions;
  if (accept_definitions)
    delete [] accept_definitions;
  if (lengths)
    delete [] lengths;
  delete &accepting;
  delete &initial;
  delete_labels();
  if (label_alphabet__ != &base_alphabet)
    label_alphabet__->detach();
}

/**/

void FSA_Common::allocate_compressor()
{
  if (!compressor)
    compressor = new Transition_Compressor(alphabet_size());
}

/**/

void FSA_Common::clear_accepting(bool expect_big_bitset)
{
  accepting.exclude_all();
  accepting.expect_big_bitset(expect_big_bitset);
}

void FSA_Common::clear_initial(bool expect_big_bitset)
{
  initial.exclude_all();
  initial.expect_big_bitset(expect_big_bitset);
}

/**/

void FSA_Common::create_definitions(bool fast)
{
  /* Find a path from an initial state to each state, if possible. The
     algorithm we use will find the bfs path */

  const State_Count nr_states = state_count();
  const Transition_ID nr_symbols = alphabet_size();
  if (state_definitions)
    delete [] state_definitions;
  if (lengths)
    delete [] lengths;
  state_definitions = new State_Definition[nr_states];
  memset(state_definitions,0,sizeof(State_Definition)*nr_states);
  if (fast)
    lengths = new Word_Length[nr_states];

  State_List sl;
  sl.reserve(nr_states,false);
  Special_Subset_Iterator ssi;
  for (State_ID si = initial_state(ssi,true);si;si = initial_state(ssi,false))
  {
    sl.append_one(si);
    if (fast)
      lengths[si] = 0;
  }

  State_List::Iterator sli(sl);
  for (State_ID si = sli.first(); si ; si = sli.next())
  {
    for (Transition_ID ti = 0; ti < nr_symbols; ti++)
    {
      State_ID ns = new_state(si,ti);
      if (is_valid_state(ns) && !is_initial(ns) && !state_definitions[ns].state)
      {
        state_definitions[ns].state = si;
        state_definitions[ns].symbol_nr = ti;
        if (fast)
          lengths[ns] = lengths[si] + 1;
        sl.append_one(ns);
      }
    }
  }
}

/**/

void FSA_Common::create_accept_definitions()
{
  /* Find a path from each state to an accept state if possible. The algorithm
     will find the BFS path to success from each state */

  const State_Count nr_states = state_count();
  const Transition_ID nr_symbols = alphabet_size();
  bool * succeeding = new bool[nr_states];
  State_Count found = 1;
  bool done = false;

  memset(succeeding,false,sizeof(bool)*nr_states);
  if (accept_definitions)
    delete [] accept_definitions;
  accept_definitions = new Accept_Definition[nr_states];
  memset(accept_definitions,0,sizeof(Accept_Definition)*nr_states);

  State_Subset_Iterator ssi;
  for (State_ID si = accepting_state(ssi,true);si;si = accepting_state(ssi,false))
  {
    succeeding[si] = true;
    found++;
    done = true;
  }
  unsigned pass = 0;
  while (done && found < nr_states)
  {
    done = false;
    pass++;
    for (State_ID si = 1; si < nr_states;si++)
    {
      if (!(char) si)
        container.status(2,1,"Computing paths to accept state. Pass %u. ("
                             FMT_ID " of " FMT_ID " to do)\n",
                         pass,nr_states-si,nr_states);
      if (!succeeding[si])
      {
        for (Transition_ID ti = 0; ti < nr_symbols; ti++)
        {
          State_ID ns = new_state(si,ti);
          if (is_valid_state(ns) && succeeding[ns] && accept_definitions[ns].distance+1==pass)
          {
            accept_definitions[si].distance = pass;
            accept_definitions[si].symbol_nr = ti;
            succeeding[si] = true;
            found++;
            done = true;
            break;
          }
        }
      }
    }
  }
  delete [] succeeding;
}

/**/

void FSA_Common::delete_labels()
{
  if (label_nr)
  {
    delete [] label_nr;
    label_nr = 0;
  }
  if (label_data)
  {
    for (Label_ID l = 1; l < nr_labels;l++)
      if (label_data[l])
        delete [] label_data[l];
    delete [] label_data;
    label_data = 0;
  }
}

/**/

void FSA_Common::set_accept_all()
{
  accepting.include_all();
}

/**/

void FSA_Common::set_initial_all()
{
  initial.include_all();
}

/**/

Element_Count FSA_Common::label_word_count(Label_ID label_id) const
{
  if (labels_are_words())
    return 0;
  const void * label = get_label_data(label_id);
  if (!label)
    return 0;
  if (label_type() == LT_Words)
    return 1;
  Word_List wl(*label_alphabet__);
  wl.unpack((const Byte *) label);
  return wl.count();
}

/**/

bool FSA_Common::set_is_accepting(State_ID state,bool is_accepting)
{
  return accepting.assign_membership(state,is_accepting);
}

/**/

bool FSA_Common::set_is_initial(State_ID state,bool is_initial)
{
  return initial.assign_membership(state,is_initial);
}

/**/

void FSA_Common::set_label_alphabet(const Alphabet & alphabet,bool repack)
{
  if (repack &&
      label_type() == LT_List_Of_Words &&
      label_alphabet__->packed_word_size(1) != alphabet.packed_word_size(1) ||
      label_type() == LT_Words &&
      label_alphabet__->extra_packed_density() != alphabet.extra_packed_density())
  {
    /* This code is needed by the code that generates subgroup presentations
       which can totally change the set of labels associated with an FSA.
       If the alphabets are packed differently the data needs to be
       reset */
    Label_Count nr_labels = label_count();
    Word_List new_wl(alphabet);
    Word_List old_wl(*label_alphabet__);
    for (Label_ID label = 1; label < nr_labels;label++)
    {
      label_word_list(&old_wl,label);
      new_wl.take(old_wl);
      label_alphabet__ = &alphabet;
      set_label_word_list(label,new_wl);
      label_alphabet__ = &old_wl.alphabet;
    }
  }

  if (label_alphabet__ != &base_alphabet)
    label_alphabet__->detach();
  label_alphabet__ = (Alphabet *) &alphabet;
  if (label_alphabet__ != &base_alphabet)
    label_alphabet__->attach();
}

/**/

bool FSA_Common::set_label_data(Label_ID label,const void * data)
{
  if (label_association == LA_Direct)
  {
    State_Count nr_states = state_count();
    if (label < nr_states && label >= nr_labels)
    {
      Byte ** new_label_data = new Byte *[nr_states];
      Label_ID li;
      for (li = 0; li < nr_labels;li++)
        new_label_data[li] = label_data[li];
      for (; li < nr_states;li++)
        new_label_data[li] = 0;
      if (label_data)
        delete [] label_data;
      label_data = new_label_data;
      nr_labels = nr_states;
    }
  }

  if (label && label < nr_labels)
  {
    if (!label_data)
    {
      label_data = new Byte *[nr_labels];
      memset(label_data,0,sizeof(Byte *) * nr_labels);
    }
    if (label_data[label])
      delete [] label_data[label];
    label_data[label] = (Byte *) data;
    return true;
  }
  return false;
}

/**/

bool FSA_Common::set_label_data(Label_ID label,const void * data,size_t label_size)
{
  Byte * clone = label_size ? new Byte[label_size] : 0;
  if (set_label_data(label,clone))
  {
    memcpy(clone,data,label_size);
    return true;
  }
  else
    delete [] clone;
  return false;
}

/**/

bool FSA_Common::set_label_nr(State_ID state,Label_ID label,bool grow)
{
  State_Count nr_states = state_count();
  if (!label_nr && (state != label || label_association == LA_Mapped))
  {
    label_nr = new Label_ID[nr_states];
    State_ID si = 0;
    if (label_association == LA_Direct)
      for (; si < nr_states && si < nr_labels;si++)
        label_nr[si] = si;
    for (; si < nr_states;si++)
      label_nr[si] = 0;
    label_association = LA_Mapped;
  }

  if (grow && label >= nr_labels)
  {
    if (label_data)
    {
      Byte ** new_label_data = new Byte *[label+1];
      Label_ID li = 0;
      for (; li < nr_labels;li++)
        new_label_data[li] = label_data[li];
      for (; li <= label;li++)
        new_label_data[li] = 0;
      delete [] label_data;
      label_data = new_label_data;
    }
    nr_labels = label+1;
  }

  if (state < nr_states && label < nr_labels)
  {
    if (!label_nr)
      return true;
    label_nr[state] = label;
    return true;
  }
  return false;
}

/**/

void FSA_Common::set_label_type(Label_Type label_type_)
{
  delete_labels();
  label_format = label_type_;
  nr_labels = 0;
}

/**/

bool FSA_Common::set_label_word(Label_ID label,const Word &word)
{
  /* This method now makes makes sure the label is packed using the
     current label_alphabet, so set_label_alphabet() now flips this
     when repacking labels. */
  if (label_type() == LT_Unlabelled)
    label_format = LT_Words;
  if (!labels_are_words())
    return false;
  void * data;
  if (label_format == LT_Words)
  {
    if (word.alphabet() == *label_alphabet__)
    {
      Extra_Packed_Word pw(word);
      data = pw.take();
    }
    else
    {
      Ordinal_Word ow(*label_alphabet__);
      ow = word;
      Extra_Packed_Word pw(ow);
      data = pw.take();
    }
  }
  else
  {
    Word_List wl(*label_alphabet__);
    wl.add(word);
    Packed_Word_List pwl(wl);
    data = pwl.take();
  }
  bool retcode = set_label_data(label,data);
  if (!retcode)
    delete [] (Byte *) data;
  return retcode;
}

/**/

bool FSA_Common::set_label_word_list(Label_ID label,const Word_List &wl)
{
  /* See comment in set_label_word() about alphabet. */
  if (label_type() == LT_Unlabelled)
    label_format = LT_List_Of_Words;
  if (label_format == LT_List_Of_Words)
  {
    Packed_Word_List pwl(wl);
    void * data = pwl.take();
    bool retcode = set_label_data(label,data);
    if (!retcode)
      delete [] (Byte *) data;
    return retcode;
  }
  if (label_format == LT_Words)
    return set_label_word(label,Entry_Word(wl,0));
  return false;
}

/**/

bool FSA_Common::set_label_statelist(Label_ID label,const State_List &sl)
{
  if (label_type() == LT_Unlabelled)
    label_format = LT_List_Of_Integers;
  if (label_type() != LT_List_Of_Integers)
    return false;
  Packed_State_List psl(sl);
  void * data = psl.take();
  bool retcode = set_label_data(label,data);
  if (!retcode)
    delete [] (Byte *) data;
  return retcode;
}

/**/

bool FSA_Common::set_transition(State_ID state,Transition_ID symbol_nr,
                                State_ID to_state)
{
  const Transition_ID nr_symbols = alphabet_size();
  bool retcode = false;
  const State_Count nr_states = state_count();
  if (state < nr_states && symbol_nr < nr_symbols && to_state < nr_states)

  {
    State_ID * transition = new State_ID[nr_symbols];
    get_transitions(transition,state);
    if (transition[symbol_nr] != to_state)
    {
      transition[symbol_nr] = to_state;
      retcode = set_transitions(state,transition);
    }
    else
      retcode = true;
    delete transition;
  }
  return retcode;
}

/**/

void FSA_Common::set_nr_labels(Label_Count nr_labels_,Label_Association la)
{
  delete_labels();
  if (la == LA_Auto)
    la = nr_labels_ == state_count() ? LA_Direct : LA_Mapped;
  label_association = la;
  nr_labels = nr_labels_;
}

/**/

void FSA_Common::sort_labels(bool trim_only)
{
  if (nr_labels)
  {
    Label_ID * new_numbers = new Label_ID[nr_labels];
    Label_Count new_nr_labels = 1;
    State_Count nr_states = state_count();
    int gap = 2;

    for (Label_Count i = 0;i < nr_labels;i++)
      new_numbers[i] = 0;
    State_ID si;
    for (si = 1;si < nr_states;si++)
    {
      Label_ID l = get_label_nr(si);
      if (l)
      {
        if (!(char) si &&
            container.status(2,gap,"Checking state labels (" FMT_ID " of " FMT_ID ")\n",
                             si,nr_states))
          gap = 1;
        if (!new_numbers[l])
          new_numbers[l] = new_nr_labels++;
        if (!trim_only)
          set_label_nr(si,new_numbers[l]);
      }
    }

    if (trim_only)
    {
      if (new_nr_labels == nr_labels)
      {
        delete [] new_numbers;
        return;
      }
      new_nr_labels = 1;
      for (Label_ID l = 1; l < nr_labels;l++)
        if (new_numbers[l])
          new_numbers[l] = new_nr_labels++;
      for (si = 1;si < nr_states;si++)
      {
        if (!(char) si &&
            container.status(2,gap,"Renumbering state labels (" FMT_ID " of " FMT_ID ")\n",si,nr_states))
          gap = 1;
        Label_ID l = get_label_nr(si);
        if (l)
          set_label_nr(si,new_numbers[l]);
      }
    }

    Byte ** new_label_data = new Byte * [new_nr_labels];
    for (Label_ID j = 0; j < nr_labels;j++)
    {
      if (!(char) j &&
          container.status(2,gap,"Rebuilding label data (" FMT_ID " of " FMT_ID ")\n",
                           j,nr_labels))
        gap = 1;
      if (j==0 || new_numbers[j])
        new_label_data[new_numbers[j]] = label_data[j];
      else if (label_data[j])
        delete [] label_data[j];
    }
    delete [] label_data;
    label_data = new_label_data;
    nr_labels = new_nr_labels;
    delete [] new_numbers;
  }
}

void FSA_Common::set_single_accepting(State_ID si)
{
  accepting.set_singleton_set(si);
}

void FSA_Common::set_single_initial(State_ID si)
{
  initial.set_singleton_set(si);
}

void FSA_Common::tidy()
{
  initial.expect_big_bitset(false);
  accepting.expect_big_bitset(false);
  initial.shrink(true);
  accepting.shrink(true);
}

/**/

Accept_Type FSA_Common::accept_type() const
{
  return accepting.get_format();
}

State_ID FSA_Common::accepting_state() const
{
  return accepting.first();
}

Word_Length FSA_Common::defining_length(State_ID state) const
{
  if (lengths)
    return lengths[state];
  return FSA::defining_length(state);
}


Label_ID FSA_Common::get_label_nr(State_ID state) const
{
  if (!is_valid_state(state))
    return 0;
  if (label_nr)
    return label_nr[state];
  if (label_association == LA_Direct && is_valid_label(state))
    return state;
  return 0;
}

Initial_Type FSA_Common::initial_type() const
{
  return initial.get_format();
}

State_ID FSA_Common::initial_state() const
{
  return initial.first();
}

/**/

bool FSA_Common::is_accepting(State_ID state) const
{
  return accepting.contains(state);
}

bool FSA_Common::is_initial(State_ID state) const
{
  return initial.contains(state);
}

/**/

size_t FSA_Common::label_size(Label_ID label) const
{
  const void * data = get_label_data(label);
  if (!data)
    return 0;
  /* This is bad code - we should be calling some method in the
     unpacking class for the label */
  switch (label_type())
  {
    case LT_Words:
      return label_alphabet__->extra_packed_word_size((const Byte *)data);
    case LT_Strings:
    case LT_Identifiers:
      return String((const Letter *) data).length()+1;
    case LT_List_Of_Integers:
      return (* ((State_ID *) data) + 1) * sizeof(State_ID);
    case LT_List_Of_Words:
      return (* (size_t *) data + sizeof(size_t));
    case LT_Unlabelled: // no labels
    case LT_Custom: // not supported yet
      return 0;
  }
  return 0;
}

/**/

String FSA_Common::label_text(String_Buffer * sb,Label_ID label,int item) const
{
  switch (label_format)
  {
    case LT_Unlabelled: // there is no label
    case LT_Custom:    // we can't understand the label
      sb->empty();
      break;
    case LT_Words:
      if (item <= 0)
      {
        Ordinal_Word word(*label_alphabet__);
        label_word(&word,label,0);
        label_alphabet__->format(sb,word);
      }
      break;
    case LT_Strings:
      if (item <= 0)
      {
        String value = (Letter *) get_label_data(label);
        sb->reserve(value.length() + 2);
        sb->set("\"");
        sb->append(value);
        sb->append("\"");
      }
      break;
    case LT_Identifiers:
      if (item <= 0)
      {
        String value = (Letter *) get_label_data(label);
        sb->set(value);
      }
      break;
    case LT_List_Of_Words:
      {
        Word_List wl(*label_alphabet__,0);
        wl.unpack((const Byte *) get_label_data(label));
        if (item == -1)
        {
          String_Buffer sb2;
          sb->set("[");
          for (Element_ID i = 0; i < wl.count() ;i++)
          {
            if (i != 0)
              sb->append(",");
            Entry_Word ew(wl,i);
            ew.format(&sb2);
            sb->append(sb2.get());
          }
          sb->append("]");
        }
        else
        {
          Entry_Word ew(wl,item);
          ew.format(sb);
        }
      }
      break;
    case LT_List_Of_Integers:
      {
        State_List sl;
        sl.unpack((const Byte *) get_label_data(label));
        if (item == -1)
        {
          String_Buffer sb2;
          State_List::Iterator sli(sl);
          bool started = false;
          sb->set("[");
          for (State_ID si = sli.first(); si ; si = sli.next())
          {
            if (started)
              sb->append(",");
            sb2.format(FMT_ID,si);
            sb->append(sb2.get());
            started = true;
          }
          sb->append("]");
        }
        else
        {
          State_List::Iterator sli(sl);
          State_ID si = sli.first();
          for (int i = 0; i < item ; i++,si = sli.next())
            ;
          sb->format(FMT_ID,si);
        }
      }
      break;
  }
  return sb->get();
}

/**/

State_Count FSA_Common::nr_initial_states() const
{
  return initial.count();
}

/**/

State_Count FSA_Common::nr_accepting_states() const
{
  return accepting.count();
}

/**/

void FSA_Common::print_accepting(Output_Stream * stream) const
{
  accepting.print(stream);
}

void FSA_Common::print_initial(Output_Stream * stream) const
{
  initial.print(stream);
}

/**/

Label_Count FSA_Common::copy_labels(const FSA & fsa_start,Label_Association la)
{
  Label_Count nr_labels = fsa_start.label_count();
  if (nr_labels > 1)
  {
    set_label_type(fsa_start.label_type());
    set_label_alphabet(fsa_start.label_alphabet());
    set_nr_labels(nr_labels,la);
    for (Label_ID j = 1; j < nr_labels;j++)

    {
      if (!(char) j)
        container.status(2,1,"Creating state labels (" FMT_ID " of " FMT_ID " done)\n",j,nr_labels);
      set_label_data(j,fsa_start.get_label_data(j),
                     fsa_start.label_size(j));
    }
  }
  return nr_labels;
}

/**/

FSA_Simple::FSA_Simple(Container & container,const Alphabet & alphabet,
                       State_Count nr_states_,Transition_ID nr_symbols_,
                       Transition_Storage_Format tsf) :
  FSA_Common(container,alphabet),
  nr_states(nr_states_),
  nr_symbols(nr_symbols_),
  dense_transitions(0),
  sparse_transitions(0),
  current_transition(0),
  current_state(0),
  locked_state(0)
{
  if (tsf == TSF_Default)
    tsf = nr_symbols * nr_states < 1024*1024 ? TSF_Dense : TSF_Sparse;

  if (tsf == TSF_Dense)
  {
    dense_transitions = new State_ID[nr_symbols*nr_states];
    memset(dense_transitions,0,nr_symbols*nr_states*sizeof(State_ID));
  }
  else
  {
    sparse_transitions = new Byte_Buffer[nr_states];
    current_transition = new State_ID[nr_symbols];
    allocate_compressor();
  }

}

int FSA_Simple::compare(const FSA_Simple & other) const
{
  /* Compares two FSAs for equality in terms of accepted states
     and transitions.
     This can be used to compare languages provided both FSAs have
     previously been standardised.
  */
  State_Count nr_states = state_count();
  Transition_ID nr_transitions = alphabet_size();
  if (nr_states != other.state_count() || nr_transitions != other.alphabet_size())
    return -1;

  for (State_ID si = 1; si < nr_states;si++)
  {
    if (is_accepting(si) != other.is_accepting(si))
      return -1;
    const State_ID * t1 = state_access(si);
    const State_ID * t2 = other.state_access(si);
    for (Transition_ID ti = 0; ti < nr_transitions;ti++)
      if (t1[ti] != t2[ti])
        return -1;
  }
  return 0;
}

/**/

void FSA_Simple::ensure_dense()
{
  if (!dense_transitions)
  {
    Transition_Realiser tr(*this,0x3000000);
    if (tr.transition_table())
    {
      if (sparse_transitions)
      {
        delete [] sparse_transitions;
        sparse_transitions = 0;
      }
      if (current_transition)
      {
        delete [] current_transition;
        current_transition = 0;
      }
      dense_transitions = tr.take();
    }
  }
}

/**/

bool FSA_Simple::set_transitions(State_ID si,const State_ID * transitions)
{
  if (dense_transitions)
  {
    State_ID *state = state_get(si);
    if (state != dense_transitions)
    {
#ifdef DEBUG
      for (Transition_ID ti = 0;ti < nr_symbols;ti++)
        if (!is_valid_target(transitions[ti]))
          return false;
#endif
      for (Transition_ID ti = 0;ti < nr_symbols;ti++)
         state[ti] = transitions[ti];
      return true;
    }
    return false;
  }
  if (is_valid_state(si))
  {
    if (si == current_state)
      memcpy(current_transition,transitions,nr_symbols*sizeof(State_ID));
    size_t size = compressor->compress(transitions);
    sparse_transitions[si].clone(compressor->cdata,size);
    return true;
  }
  return false;
}

/**/

void FSA_Simple::permute_states(const State_ID *perm, bool sort_sequence)
{
  /* permute the states of fsa, using perm
     perm should be a permutation of the integers 0 to nr_states-1.
     perm[0] should be 0, and perm[1] usually 1. If the conditions are not
     met then the FSA constructed will at probably have inaccessible or
     failing states and things might be worse than that.
     When sort_sequence is true, it is more or less OK to have some of the
     states mapped to 0. Then this function is working like filtered_copy()
     The most likely cause of this is that we have done a sort_bfs() and
     found some states that are inaccessible. But the FSA should be
     minimised afterwards.
     Any other violations makes this function like an unsafe version of
     FSA_Factory::merge()).
     This function should perhaps be called renumber_states() instead
     since what was state[n] becomes state[perm[n]]
     Thus if you want to sort the states in some way the input array should
     consist of the positions of the states within the sorted array, and
     not, as would probably be more natural, the number of the state in
     particular position. If sort_sequence is true then the method inverts
     the permutation, which is what is needed to turn a sort sequence into
     a renumbering sequence.
   */

  const State_Count nr_states = state_count();
  const Transition_ID nr_symbols = alphabet_size();
  State_ID i;
  State_ID * perm_inverse = 0;

  if (sort_sequence)
  {
    perm_inverse = new State_ID[nr_states];
    for (i = 0; i < nr_states;i++)
      perm_inverse[i] = 0;
    for (i = 0; i < nr_states;i++)
    {
      State_ID si = perm[i];
      if (perm_inverse[si] != 0 && si)
        MAF_INTERNAL_ERROR(container,("Invalid parameter in permute_states()\n"));
      perm_inverse[si] = i;
    }
    perm_inverse[0] = 0; // Failure state is not allowed to move.
    perm = perm_inverse;
  }

  if (nr_labels)
  {
    bool permute_labels = true;

    if (nr_labels == nr_states)
    {
      if (label_nr)
      {
        for (i = 1;i < nr_states;i++)
          if (label_nr[i] != i)
            break;
        permute_labels = i == nr_states;
      }
    }
    else
      permute_labels = false;
    if (permute_labels)
    {
      Byte ** label_data = new Byte *[nr_states];
      for (i = 0; i < nr_states;i++)
        label_data[perm[i]] = this->label_data[i];
      delete [] this->label_data;
      this->label_data = label_data;
    }
    else
    {
      Label_ID * label_nr = new Label_ID[nr_states];
      if (label_nr)
      {
        for (i = 0; i < nr_states;i++)
          label_nr[perm[i]] = this->label_nr[i];

        delete [] this->label_nr;
      }
      else
      {
        for (i = 0; i < nr_states;i++)
          label_nr[perm[i]] = i < nr_labels ? i: 0;
      }

      this->label_nr = label_nr;
    }
  }
  initial.renumber(perm);
  accepting.renumber(perm);
  /* We used to renumber the states using a huge array which replaced the
     dense transition table. But now we use a temporary FSA whose transition
     tables we will steal when we have finished. This means we do not have
     to worry about the format of the transitions except at once the whole
     transition table is complete.
  */
  FSA_Simple * temp = new FSA_Simple(container,base_alphabet,
                                     nr_states,nr_symbols);

  State_ID * transitions = new State_ID[nr_symbols];
  if (flags & GFF_RWS)
  {
    for (i = 1; i < nr_states;i++)
    {
      for (Transition_ID ti = 0; ti < nr_symbols;ti++)
      {
        State_ID nsi = new_state(i,ti);
        if (is_valid_state(nsi))
          transitions[ti] = perm[nsi];
        else
          transitions[ti] = nsi;
      }
      temp->set_transitions(perm[i],transitions);
    }
  }
  else
    for (i = 1; i < nr_states;i++)
    {
      for (Transition_ID j = 0; j < nr_symbols;j++)
        transitions[j] = perm[new_state(i,j)];
      temp->set_transitions(perm[i],transitions);
    }
  /* We are going to make sure the renumbering has not affected the failure
     state. This should not be necessary now, as the calls to
     set_transitions() should fail if we try to set anything in the failure
     state, but we may as well be defensive here in case anyone is tempted
     to put back the old code which attacked the dense transition table
     directly */
  for (Transition_ID j = 0; j < nr_symbols;j++)
    transitions[j] = 0;
  temp->set_transitions(0,transitions);
  delete [] transitions;
  /* Now steal the transition table from the temporary FSA */
  if (dense_transitions)
    delete [] dense_transitions;
  dense_transitions = temp->dense_transitions;
  temp->dense_transitions = 0;
  if (sparse_transitions)
    delete [] sparse_transitions;
  sparse_transitions = temp->sparse_transitions;
  temp->sparse_transitions = 0;
  delete temp;
  if (perm_inverse)
    delete perm_inverse;
  change_flags(0,GFF_BFS);
}

/**/

void FSA_Simple::sort_bfs()
{
  /* Put the fsa into bfs form. */

  const Transition_ID nr_symbols = alphabet_size();
  const State_Count nr_states = state_count();
  State_ID * perm = new State_ID[nr_states];
  bool * got = new bool[nr_states];
  State_ID si;

  for (si = 0;si < nr_states;si++)
  {
    got[si] = false;
    perm[si] = 0;
  }

  State_Count count = 1;
  State_Subset_Iterator ssi;
  for (si = initial_state(ssi,true);si;si = initial_state(ssi,false))
  {
    perm[count++] = si;
    got[si] = true;
  }

  for (si = 1;si < count;si++)
  {
    for (Transition_ID j = 0; j < nr_symbols;j++)
    {
      State_ID t = new_state(perm[si],j);
      if (t > 0 && !got[t])
      {
        perm[count++] = t;
        got[t] = true;
      }
    }
  }
  delete [] got;
  sort_states(perm);
  delete [] perm;
  change_flags(GFF_BFS,0);
}

/**/

void FSA_Simple::remove_rewrites()
{
  /* Turn an RWS FSA into an ordinary FSA */

  if (flags & GFF_RWS)
  {
    const Transition_ID nr_symbols = alphabet_size();
    const State_Count nr_states = state_count();
    FSA_Simple * temp = new FSA_Simple(container,base_alphabet,
                                       nr_states,nr_symbols);

    State_ID * transitions = new State_ID[nr_symbols];

    for (State_ID si = 1; si < nr_states;si++)
    {
      get_transitions(transitions,si);
      for (Transition_ID ti = 0; ti < nr_symbols;ti++)
      {
        State_ID nsi = transitions[ti];
        if (is_valid_state(nsi))
          transitions[ti] = nsi;
        else
          transitions[ti] = 0;
      }
      temp->set_transitions(si,transitions);
    }
    delete [] transitions;
    /* Now steal the transition table from the temporary FSA */
    if (dense_transitions)
      delete [] dense_transitions;
    dense_transitions = temp->dense_transitions;
    temp->dense_transitions = 0;
    if (sparse_transitions)
      delete [] sparse_transitions;
    sparse_transitions = temp->sparse_transitions;
    temp->sparse_transitions = 0;
    delete temp;
    change_flags(0,GFF_RWS|GFF_MINIMISED);
  }

}

/**/

Transition_Realiser::Transition_Realiser(const FSA & fsa_,size_t ceiling,
                                         State_Count nr_cached_rows_) :
  realised_transitions(0),
  cached_transitions(0),
  cached_rows(0),
  nr_cached_rows(nr_cached_rows_),
  fsa(fsa_)
{
  alphabet_size = fsa.alphabet_size();
  dense_table = fsa.dense_transition_table();
  if (!dense_table)
  {
    State_Count count = fsa.state_count();
    size_t required =  count * alphabet_size;
    if (ceiling && required <= ceiling)
    {
      realised_transitions = new State_ID[required];
      if (realised_transitions)
      {
        State_ID * buffer = realised_transitions;
        for (State_ID si = 0; si < count;si++,buffer += alphabet_size)
          fsa.get_transitions(buffer,si);
        dense_table = realised_transitions;
      }
    }
  }
  if (!dense_table)
  {
    cached_transitions = new State_ID[alphabet_size*nr_cached_rows];
    cached_rows = new State_ID[nr_cached_rows];
    for (State_Count i = 0; i < nr_cached_rows;i++)
      cached_rows[i] = -1;
  }
}

/**/

Transition_Compressor::Transition_Compressor(Transition_ID nr_symbols_) :
  nr_symbols(nr_symbols_),
  mask_size((nr_symbols+2+CHAR_BIT-1)/CHAR_BIT),
  buffer_size(1+(1+sizeof(State_ID))*nr_symbols_+1),
  cdata(new Byte[(buffer_size + sizeof(long)-1) & ~(sizeof(long)-1)]),
  expect1(true)
{
}

/**/

Transition_Compressor::~Transition_Compressor()
{
  delete [] (Byte *) cdata;
}

/**/

size_t Transition_Compressor::compress(const State_ID * buffer)
{
  /* We support three formats for storing transitions, stored in
     the lowest two bits of the first byte of the compressed
     transition information.

     format 0  - a dense row of transitions.

     format 1  - a bitmask beginning in the third bit of the first byte
     with a 1 for non zero transitions and a 0 for zero transitions.
     The non zero transitions are listed after the bit mask.

     format 2 - an array of byte/State_Id pairs terminated with byte value
     255. The byte indicates a following number of zero transitions that
     are followed by a single State_Id. The state might be zero in the
     case where there is a gap of more than 254 transitions between non
     zero transitions.

     format 3 is a pseudo-format for the case where there are no transitions.
     In this case we do not store a transition table at all.

     Another possible format would be byte byte State_Id*n where the first
     byte is interpreted as in format 2 and the second byte is n, a number
     of consecutive transitions. */

  Transition_ID j = 0;
  int count = 0;
  int gap = 0;
  size_t size;
  if (expect1)
  {
    memset(cdata,0,mask_size);
    for (Transition_ID i = 0; i < nr_symbols;i++)
      if (buffer[i])
      {
        cdata[ (i+2)/CHAR_BIT] |= 1 << ((i+2) % CHAR_BIT);
        memcpy(cdata+mask_size + j++*sizeof(State_ID),buffer+i,sizeof(State_ID));
        while (gap >= UCHAR_MAX)
        {
          count++;
          gap -= UCHAR_MAX;
        }
        gap = 0;
        count++;
     }
     else
       gap++;
    if (!count)
    {
      *cdata = 3;
      return 0;
    }
    size = mask_size + j*sizeof(State_ID);
    if (1 + count*(1+sizeof(State_ID)) + 1 < size)
    {
      expect1 = false;
      Byte * t = cdata;
      *t++ = 2;
      gap = 0;
      for (Transition_ID i = 0; count;i++)
        if (buffer[i])
        {
          while (gap >= UCHAR_MAX)
          {
            *t = UCHAR_MAX-1;
            t++;
            memset(t,0,sizeof(State_ID));
            t += sizeof(State_ID);
            gap -= UCHAR_MAX;
            count--;
          }
          *t++ = Byte(gap);
          memcpy(t,buffer+i,sizeof(State_ID));
          t +=  sizeof(State_ID);
          count--;
          gap = 0;
        }
        else
          gap++;
      *t++ = UCHAR_MAX;
      size = t - cdata;
    }
    else if (1 + nr_symbols*sizeof(State_ID) < size)
    {
      *cdata = 0;
      memcpy(cdata+1,buffer,size = nr_symbols*sizeof(State_ID));
      size++;
    }
    else
      *cdata |= 1;
  }
  else
  {
    Byte * t = cdata;
    *t++ = 2;
    gap = 0;
    for (Transition_ID i = 0; i < nr_symbols;i++)
      if (buffer[i])
      {
        while (gap >= UCHAR_MAX)
        {
          *t = UCHAR_MAX-1;
          t++;
         memset(t,0,sizeof(State_ID));
          t += sizeof(State_ID);
          gap -= UCHAR_MAX;
        }
        *t++ = Byte(gap);
        memcpy(t,buffer+i,sizeof(State_ID));
        t +=  sizeof(State_ID);
        count++;
        gap = 0;
      }
      else
        gap++;
    *t++ = UCHAR_MAX;
    size = t - cdata;
    if (!count)
    {
      *cdata = 3;
      return 0;
    }
    if (mask_size + count*sizeof(State_ID) <= size)
    {
      expect1 = true;
      size = mask_size + count*sizeof(State_ID);
      if (size <= 1 + nr_symbols*sizeof(State_ID))
      {
        Transition_ID i = cdata[1];
        memset(cdata,0,mask_size);
        *cdata = 1;
        for (; count;i++)
          if (buffer[i])
          {
            cdata[ (i+2)/CHAR_BIT] |= 1 << ((i+2) % CHAR_BIT);
            memcpy(cdata+mask_size + j++*sizeof(State_ID),buffer+i,sizeof(State_ID));
            count--;
          }
      }
      else if (1 + nr_symbols*sizeof(State_ID) < size)
      {
        *cdata = 0;
        memcpy(cdata+1,buffer,size = nr_symbols*sizeof(State_ID));
        size++;
      }
    }
  }
  return size;
}

/**/

size_t Transition_Compressor::key_for_decided_state(State_ID key)
{
  /* This method is used by FSA_Factory::minimise() to store a new
     hash key for states where we don't need to look at the transition
     table to allocate a state key. We create a dummy compressed transition
     using format 2 or 1.
  */

  if (!key)
  {
    *cdata = 3;
    return 0;
  }
  Byte * t = cdata;
  if (nr_symbols > 14)
  {
    *t++ = 2;
    *t++ = 0;
    memcpy(t,&key,sizeof(State_ID));
    t += sizeof(State_ID);
    *t++ = UCHAR_MAX;
  }
  else
  {
    *t++ = 5;
    if (nr_symbols > 6)
      *t++ = 0;
    memcpy(t,&key,sizeof(State_ID));
    t += sizeof(State_ID);
  }
  return t - cdata;
}

/**/

void Transition_Compressor::decompress(State_ID * buffer,
                                       const Byte * cdata) const
{
  if (!cdata)
  {
    memset(buffer,0,nr_symbols*sizeof(State_ID));
    return;
  }
  int format = cdata[0] & 3;

  if (format == 1)
  {
    Transition_ID j = 0;
    for (Transition_ID i = 0; i < nr_symbols;i++)
      if (cdata[(i+2)/CHAR_BIT] & (1 << ((i+2) % CHAR_BIT)))
        memcpy(buffer+i,cdata+mask_size+j++*sizeof(State_ID),sizeof(State_ID));
      else
        buffer[i] = 0;
  }
  else if (format == 2)
  {
    Transition_ID j = 0;
    cdata++;
    while (*cdata != UCHAR_MAX)
    {
      memset(buffer+j,0,*cdata*sizeof(State_ID));
      j += *cdata++;
      memcpy(buffer+j,cdata,sizeof(State_ID));
      j++;
      cdata += sizeof(State_ID);
    }
    memset(buffer+j,0,(nr_symbols-j)*sizeof(State_ID));
  }
  else
    memcpy(buffer,cdata+1,sizeof(State_ID)*nr_symbols);
}

/**/

State_ID Transition_Compressor::new_state(const Byte * cdata,Transition_ID ti) const
{
  if (!cdata)
    return 0;
  State_ID answer = 0;
  int format = cdata[0] & 3;

  if (format == 1)
  {
    if (cdata[(ti+2)/CHAR_BIT] & (1 << ((ti+2) % CHAR_BIT)))
    {
      Transition_ID j = 0;
      for (Transition_ID i = 0; i < nr_symbols;i++)
        if (cdata[(i+2)/CHAR_BIT] & (1 << ((i+2) % CHAR_BIT)))
          if (i == ti)
          {
            memcpy(&answer,cdata+mask_size+j++*sizeof(State_ID),sizeof(State_ID));
            break;
          }
          else
            j++;
    }
  }
  else if (format == 2)
  {
    Transition_ID j = 0;
    cdata++;
    while (*cdata != UCHAR_MAX && j <= ti)
    {
      j += *cdata++;
      if (j == ti)
      {
        memcpy(&answer,cdata,sizeof(State_ID));
        break;
      }
      j++;
      cdata += sizeof(State_ID);
    }
  }
  else
    memcpy(&answer,cdata+1+ti*sizeof(State_ID),sizeof(State_ID));
  return answer;
}

/**/

FSA_Simple * FSA_Factory::copy(const FSA & fsa_start,Transition_Storage_Format tsf)
{
  /* Returns a copy of the orginal FSA */
  State_Count nr_states = fsa_start.state_count();
  State_ID * states_1 = new State_ID[nr_states];

  states_1[0] = 0;
  for (State_ID i = 1;i < nr_states;i++)
    states_1[i] = 1;
  return filtered_copy(fsa_start,states_1,tsf);
}

/**/

FSA_Simple * FSA_Factory::filtered_copy(const FSA & fsa_start,
                                        State_ID * states_1,
                                        Transition_Storage_Format tsf)
{
  /* Returns an FSA in which states for which states_1[i] is 0  have been
     removed. Note that on entry states_1 is being use as though it where an
     array of bools, but is then reused as the map from old to new state
     numbers. Things are done this way because it is very likely that
     whatever has created states_1 initially needed more than just 1 bit of
     information per state to decide which states to keep, and we shall need
     a suitably array of State_ID values to create the new FSA, so we may
     as well specify the parameter using State_ID * as this will certainly save
     space and time for us, and very likely will for the caller as well

     With a little effort we could get this function to share code
     with merge(), but that performs a lot of extra checks that are not
     necessary here. */
  const Transition_ID nr_symbols = fsa_start.alphabet_size();
  State_ID * transition = new State_ID[nr_symbols];
  State_ID i;
  const State_Count nr_states = fsa_start.state_count();
  Container & container = fsa_start.container;

  /* First assign the new state numbers */
  states_1[0] = 0;
  State_Count final_count = 1;
  for (i = 1;i < nr_states;i++)
    if (states_1[i])
      states_1[i] = final_count++;

  /* Now we build the new FSA. */

  FSA_Simple * new_fsa = new FSA_Simple(container,
                                        fsa_start.base_alphabet,
                                        final_count,nr_symbols,tsf);
  new_fsa->change_flags(fsa_start.get_flags(),0);
  Label_Count nr_labels = new_fsa->copy_labels(fsa_start);
  Accept_Type accept = fsa_start.accept_type();
  bool set_accept = accept != SSF_Singleton && accept != SSF_All;
  bool set_initial = fsa_start.initial_type() != SSF_Singleton ||
                     fsa_start.initial_state() != 1;
  if (set_accept)
    new_fsa->clear_accepting(true);
  if (set_initial)
    new_fsa->clear_initial(true);
  int gap = 2;
  for (i = 1; i < nr_states;i++)
  {
    if (!(char) i &&
        container.status(2,gap,"Creating FSA states (" FMT_ID " of " FMT_ID " done)\n",
                         i,nr_states))
      gap = 1;
    if (states_1[i])
    {
      for (Transition_ID j = 0; j < nr_symbols;j++)
      {
        State_ID ns = fsa_start.new_state(i,j);
        transition[j] = ns > 0 ? states_1[ns] : ns;
      }
      new_fsa->set_transitions(states_1[i],transition);
      if (nr_labels > 1)
        new_fsa->set_label_nr(states_1[i],fsa_start.get_label_nr(i));
      if (set_accept && fsa_start.is_accepting(i))
        new_fsa->set_is_accepting(states_1[i],true);
      if (set_initial && fsa_start.is_initial(i))
        new_fsa->set_is_initial(states_1[i],true);
    }
  }
  if (accept == SSF_Singleton)
    new_fsa->set_single_accepting(states_1[fsa_start.accepting_state()]);
  new_fsa->sort_labels(true);
  new_fsa->tidy();
  delete [] transition;
  delete [] states_1;
  return new_fsa;
}

/**/

static void trim_inner(Transition_Realiser & tr,State_ID * states_1)
{
  /* This method sets each position in state_1 to 1 or 0 according to
     whether or not the state needs to be retained or not

     The method starts by flagging states which are an accept state with
     SUCCEEDING and all initial states with ACCESSIBLE. Then we going through
     all the transitions repeatedly, setting flag ACCESSIBLE when we find a
     state can be reached from a state that is already ACCESSIBLE and setting
     flag SUCCEEDING when we find we can reach a state which is SUCCEEDING. We
     also set flag ACCESSIBLE_NOT_SET on all states initially. This last flag is
     cleared for states whose transition table has been inspected since we
     learned the state is accessible. Each time we examine a state we set flag
     DYING if it is not a accept state, but clear it as soon as we find a
     transition to a state without flag DYING. So on the first pass the only
     DYING states will be states that are not accept states and from which there
     is no valid transition. ACCESSIBLE will spread forwards from the initial
     states, SUCCEEDING spread back from accepting states. DYING will also
     spread back, but from the failure state, and reluctantly.
     Eventually we shall find the set of states we should retain.
  */

  const int SUCCEEDING = 1;
  const int ACCESSIBLE = 2;
  const int GOOD_STATE = SUCCEEDING + ACCESSIBLE;
  const int ACCESSIBLE_NOT_SET = 4;
  const int DYING = 8;
  const FSA & fsa_start = tr.fsa;
  const Transition_ID nr_symbols = fsa_start.alphabet_size();
  State_ID i;
  const State_Count nr_states = fsa_start.state_count();
  State_Count final_count = 1;
  Container & container = fsa_start.container;
  bool found_accessible,found_succeeding;
  int gap = 2;
  states_1[0] = 0;

  for (i = 1;i < nr_states;i++)
  {
    states_1[i] = fsa_start.is_accepting(i) ? SUCCEEDING+ACCESSIBLE_NOT_SET : ACCESSIBLE_NOT_SET;
    if (fsa_start.is_initial(i))
      states_1[i] |= ACCESSIBLE;
  }

  do
  {
    if (container.status(2,gap,"FSA trim pass with " FMT_ID " states so far."
                     "(Original has " FMT_ID " states)\n",final_count,nr_states))
      gap = 1;
    found_succeeding = found_accessible = false;
    for (i = 1; i < nr_states;i++)
      if (states_1[i] != GOOD_STATE && !(states_1[i] & DYING))
      {
        bool succeeding = (states_1[i] & SUCCEEDING);
        bool dying = !succeeding;
        bool newly_accessible = states_1[i] & ACCESSIBLE &&
                                states_1[i] & ACCESSIBLE_NOT_SET;
        if (newly_accessible)
          states_1[i] &= ~ACCESSIBLE_NOT_SET;
        if (dying)
          states_1[i] |= DYING;
        const State_ID *transition = tr.realise_row(i);
        for (Transition_ID j = 0; j < nr_symbols;j++)
        {
          State_ID ns = transition[j];
          if (fsa_start.is_valid_state(ns))
          {
            states_1[i] |= states_1[ns] & SUCCEEDING;
            if (dying && !(states_1[ns] & DYING))
              dying = false;
            if (newly_accessible && !(states_1[ns] & ACCESSIBLE))
            {
              states_1[ns] |= ACCESSIBLE;
              found_accessible = true;
            }
          }
        }
        if (!dying)
          states_1[i] &= ~DYING;
        if (!found_succeeding)
          found_succeeding = !succeeding && states_1[i] & SUCCEEDING;
        if (states_1[i] == GOOD_STATE)
          final_count++;
      }
  }
  while (found_succeeding || found_accessible);
  for (i = 1; i < nr_states;i++)
    states_1[i] = states_1[i] == GOOD_STATE;
}

/**/

FSA_Simple * FSA_Factory::trim(const FSA & fsa_start)
{
  /* Returns an FSA in which states from which no accept state
     can be reached have been removed, as have states which cannot
     be reached at all.
  */

  State_ID * states_1 = new State_ID[fsa_start.state_count()];
  Transition_Realiser tr(fsa_start);

  trim_inner(tr,states_1);
  FSA_Simple * answer = filtered_copy(fsa_start,states_1);
  answer->change_flags(GFF_TRIM,0);
  return answer;
}

/**/

FSA_Simple * FSA_Factory::binop(const FSA & fsa_0,const FSA & fsa_1,
                                Binop_Flag opcode)
{
  if (fsa_0.initial_type() != SSF_Singleton)
  {
    FSA_Simple * fsa_temp = determinise(fsa_0);
    FSA_Simple * answer = binop(*fsa_temp,fsa_1,opcode);
    delete fsa_temp;
    return answer;
  }
  if (fsa_1.initial_type() != SSF_Singleton)
  {
    FSA_Simple * fsa_temp = determinise(fsa_1,DF_All,true);
    FSA_Simple * answer = binop(fsa_0,*fsa_temp,opcode);
    delete fsa_temp;
    return answer;
  }
  Container & container = fsa_0.container;
  const Transition_ID nr_symbols = fsa_0.alphabet_size();
  if (fsa_1.alphabet_size() != nr_symbols)
    return 0;
  bool trim_only = false;
  if (opcode == BF_And_Not_First_Trim)
  {
    opcode = BF_And_Not_First;
    trim_only = true;
  }

  State_ID key[2],old_key[2];
  State_ID fail_0 = opcode == BF_Not_And ? fsa_0.state_count() : 0;
  State_ID fail_1 = opcode == BF_And_Not || opcode == BF_And_Not_First ?
                    fsa_1.state_count() : 0;
  State_ID ceiling = 1;
  State_Count count = 2;
  Word_Length state_length = 0;
  key[0] = fsa_0.state_count();
  if (key[0] == fail_0)
    key[0]++;
  key[1] = fsa_1.state_count();
  if (key[1] == fail_1)
    key[1]++;
  Pair_Packer key_packer(key);
  State_ID *transition = new State_ID[nr_symbols];
  Keyed_FSA factory(container,fsa_0.base_alphabet,nr_symbols,
                    max(fsa_0.state_count()+fsa_1.state_count(),
                        State_Count(1024*1024+7)),
                    key_packer.key_size());
  void * key_area;

  /* Insert failure and initial states */
  key[0] = fail_0;
  key[1] = fail_1;
  factory.find_state(key_area = key_packer.pack_key(key));

  key[0] = fsa_0.initial_state();
  key[1] = fsa_1.initial_state();
  factory.find_state(key_packer.pack_key(key));

  State_ID binop_state = 0;
  while (factory.get_state_key(key_area,++binop_state))
  {
    key_packer.unpack_key(old_key);
    if (!(char) binop_state)
    {
      Word_Length length = binop_state >= ceiling ? state_length : state_length - 1;
      container.status(2,1,"Combining FSAs: State " FMT_ID ". (" FMT_ID " to do). Length %d\n",
                       binop_state,factory.state_count()-binop_state,length);
    }

    bool no_transitions = opcode == BF_And_Not_First &&
                          fsa_0.is_accepting(old_key[0]) &&
                          !fsa_1.is_accepting(old_key[1]) ||
                          opcode == BF_And_First &&
                          fsa_0.is_accepting(old_key[0]) &&
                          fsa_1.is_accepting(old_key[1]);

    for (Transition_ID i = 0; i < nr_symbols;i++)
    {
      if (old_key[0] == fail_0)
        key[0] = fail_0;
      else
        key[0] = fsa_0.new_state(old_key[0],i);
      if (old_key[1] == fail_1)
        key[1] = fail_1;
      else
        key[1] = fsa_1.new_state(old_key[1],i);
      switch (opcode)
      {
        case BF_Or: // nothing to do
          break;
        case BF_And_First:
          if (no_transitions)
            key[0] = key[1] = 0;
        case BF_And:
          if (!key[0] || !key[1])
            key[0] = key[1] = 0;
          break;
        case BF_Not_And:
          if (!key[0])
            key[0] = fail_0; /* we may as well put key[0] to fail_0 rather than
                                zero to save on calls to new_state() */
          if (!key[1])
          {
            key[0] = fail_0;
            key[1] = 0;
          }
          break;
        case BF_And_Not_First_Trim: /* unreachable, but we include it to
                                       make pedantic compilers happier and
                                       put the code that would have been
                                       executed if we hadn't change opcode */
        case BF_And_Not:
        case BF_And_Not_First:
          if (!key[1])
            key[1] = fail_1; /* we may as well put key[1] to fail_1 rather than
                                zero to save on calls to new_state() */
          if (!key[0] || no_transitions)
          {
            key[0] = 0;
            key[1] = fail_1;
          }
          break;
      }
      transition[i] = factory.find_state(key_packer.pack_key(key));
      if (transition[i] >= count)
      {
        count++;
        if (binop_state >= ceiling)
        {
          state_length++;
          ceiling = transition[i];
        }
      }
    }
    factory.set_transitions(binop_state,transition);
  }
  factory.copy_labels(fsa_0,LA_Mapped);
  binop_state = 0;
  factory.clear_accepting(true);
  while (factory.get_state_key(key_area,++binop_state))
  {
    key_packer.unpack_key(key);
    bool accept_0 = fsa_0.is_accepting(key[0]);
    bool accept_1 = fsa_1.is_accepting(key[1]);
    bool accepts = false;
    switch (opcode)
    {
      case BF_And_First:
      case BF_And:
        accepts = accept_0 && accept_1;
        break;
      case BF_Or:
        accepts = accept_0 || accept_1;
        break;
      case BF_And_Not:
      case BF_And_Not_First:
      case BF_And_Not_First_Trim: /* as above */
        accepts = accept_0 && !accept_1;
        break;
      case BF_Not_And:
        accepts = !accept_0 && accept_1;
        break;
    }
    if (accepts)
      factory.set_is_accepting(binop_state,true);
    factory.set_label_nr(binop_state,fsa_0.get_label_nr(key[0]));
  }
  if (transition)
    delete [] transition;
  factory.remove_keys();
  return trim_only ? trim(factory) : minimise(factory);
}

/**/

FSA_Simple * FSA_Factory::cartesian_product(const FSA & fsa_0,const FSA & fsa_1)
{
  const Transition_ID nr_symbols = fsa_0.alphabet_size();
  if (fsa_1.alphabet_size() != nr_symbols ||
      nr_symbols != fsa_0.base_alphabet.letter_count())
    return 0;

  if (fsa_0.initial_type() != SSF_Singleton)
  {
    FSA_Simple * fsa_temp = determinise(fsa_0);
    FSA_Simple * answer = cartesian_product(*fsa_temp,fsa_1);
    delete fsa_temp;
    return answer;
  }
  if (fsa_1.initial_type() != SSF_Singleton)
  {
    FSA_Simple * fsa_temp = determinise(fsa_1,DF_All,true);
    FSA_Simple * answer = cartesian_product(fsa_0,*fsa_temp);
    delete fsa_temp;
    return answer;
  }
  Container & container = fsa_0.container;
  Transition_ID nr_transitions = fsa_0.base_alphabet.product_alphabet_size();
  State_ID key[2],old_key[2];
  State_ID end0 = fsa_0.state_count();
  State_ID end1 = fsa_1.state_count();
  key[0] = end0+1;
  key[1] = end1+1;
  Pair_Packer key_packer(key);
  State_ID *transition = new State_ID[nr_transitions];
  Keyed_FSA factory(container,fsa_0.base_alphabet,nr_transitions,
                    fsa_0.state_count()*fsa_1.state_count(),
                    key_packer.key_size());
  void * key_area;

  /* Insert failure and initial states */
  key[0] = 0;
  key[1] = 0;
  factory.find_state(key_area = key_packer.pack_key(key));

  key[0] = fsa_0.initial_state();
  key[1] = fsa_1.initial_state();
  factory.find_state(key_packer.pack_key(key));

  State_ID binop_state = 0;
  while (factory.get_state_key(key_area,++binop_state))
  {
    key_packer.unpack_key(old_key);
    container.status(2,1,"Combining FSAs: Processing state " FMT_ID " of " FMT_ID "\n",
                     binop_state,factory.state_count());

    Transition_ID ti = 0;
    for (Ordinal g1 = 0;g1 <= nr_symbols;g1++)
    {
      if (old_key[0] == end0)
        key[0] = g1 == nr_symbols ? end0 : 0;
      else if (g1 == nr_symbols)
        key[0] = fsa_0.is_accepting(old_key[0]) ? end0 : 0;
      else
        key[0] = fsa_0.new_state(old_key[0],g1);
      for (Ordinal g2 = 0;g2 <= nr_symbols;g2++,ti++)
        if (ti < nr_transitions)
        {
          if (key[0])
          {
            if (old_key[1] == end1)
              key[1] = g2 == nr_symbols ? end1 : 0;
            else if (g2 == nr_symbols)
              key[1] = fsa_1.is_accepting(old_key[1]) ? end1 : 0;
            else
              key[1] = fsa_1.new_state(old_key[1],g2);
          }
          else
            key[1] = 0;
          if (!key[1])
            transition[ti] = 0;
          else
           transition[ti] = factory.find_state(key_packer.pack_key(key));
        }
    }
    factory.set_transitions(binop_state,transition);
  }
  binop_state = 0;
  factory.clear_accepting(true);
  while (factory.get_state_key(key_area,++binop_state))
  {
    key_packer.unpack_key(key);
    bool accept_0 = key[0] == end0 || fsa_0.is_accepting(key[0]);
    bool accept_1 = key[1] == end1 || fsa_1.is_accepting(key[1]);
    if (accept_0 && accept_1)
      factory.set_is_accepting(binop_state,true);
  }
  if (transition)
    delete [] transition;
  factory.remove_keys();
  return minimise(factory);
}

/**/

FSA_Simple * FSA_Factory::fsa_not(const FSA & fsa_start,bool first_only)
{
  /* Returns an FSA which accepts just those words the original FSA does
     not accept. Transitions to a failure state (including RWS negative
     state numbers representing rewrites) are replaced with transitions
     to a new accept state which once entered can never be left. */
  Container & container = fsa_start.container;
  const Transition_ID nr_symbols = fsa_start.alphabet_size();
  State_ID *transition = new State_ID[nr_symbols];
  const State_Count nr_states = fsa_start.state_count();
  FSA_Simple factory(container,fsa_start.base_alphabet,nr_states+1,
                     nr_symbols);

  bool found = false;
  bool set_initial = fsa_start.initial_type() != SSF_Singleton ||
                     fsa_start.initial_state() != 1;
  for (State_ID state = 1; state < nr_states;state++)
  {
    fsa_start.get_transitions(transition,state);
    for (Transition_ID ti = 0; ti < nr_symbols;ti++)
      if (!fsa_start.is_valid_state(transition[ti]))
      {
        transition[ti] = nr_states;
        found = true;
      }
    factory.set_transitions(state,transition);
    factory.set_is_accepting(state,!fsa_start.is_accepting(state));
    if (set_initial && fsa_start.is_initial(state))
      factory.set_is_initial(state,true);
  }
  if (first_only)
    for (Transition_ID ti = 0; ti < nr_symbols;ti++)
      transition[ti] = 0;
  else
  {
    for (Transition_ID ti = 0; ti < nr_symbols;ti++)
      transition[ti] = nr_states;
    factory.set_transitions(nr_states,transition);
  }
  /* The last state is normally an accept state. However if the original
     FSA never reaches the failure state this new state is inaccessible.
     In this case, if we make it a failure state then minimise() will merge
     it with the failure state, so we don't need to trim our FSA */
  factory.set_is_accepting(nr_states,found);
  factory.copy_labels(fsa_start,LA_Mapped);
  if (transition)
    delete [] transition;
  FSA_Simple * answer = minimise(factory);
  if (fsa_start.get_flags() & GFF_BFS)
    answer->sort_bfs(); /* The FSA we made was not BFS sorted even if the
                           original was since the added accept state
                           destroyed the BFS property. It is much simpler
                           to put this property back by sorting the final
                           FSA than to insert the new state when it is first
                           seen, since that would involve renumbering all
                           the later states. If the original FSA is not
                           BFS there is no reason ours should be */
  return answer;
}

/**/

class Label_Merger
{
  private:
    const FSA & fsa_start;
    const Alphabet & label_alphabet;
    Array_Of<Label_ID> labels;
    Word_List label_wl;
    Hash label_db;
    Word_List_DB wl_db;
  public:
    Label_Merger(const FSA & fsa_start_):
      fsa_start(fsa_start_),
      label_alphabet(fsa_start.label_alphabet()),
      label_db(1024,0),
      wl_db(1024,true),
      label_wl(label_alphabet)
    {
      Element_List key;
      key.empty();
      key.append_one(0);
      label_db.find_entry(key.buffer(),key.size());
      label_db.manage(labels);
    }
    void begin_label(Element_List &key)
    {
      key.empty();
    }
    void add_to_label(Element_List &key,Label_ID li)
    {
      key.insert(li);
    }
    Label_ID end_label(Element_List & key)
    {
      if (!key.count())
        return 0;
      key.append_one(0);
      Element_ID id = label_db.find_entry(key.buffer(),key.size());
      Label_ID li = labels[id];
      if (!li)
      {
        Sorted_Word_List swl(label_alphabet);
        Word_List label_wl(label_alphabet);
        for (Label_ID *old_label = (Label_ID *) key.buffer() ; *old_label; old_label++)
        {
          fsa_start.label_word_list(&label_wl,*old_label);
          Element_Count count = label_wl.count();
          for (Element_ID word_nr = 0; word_nr < count;word_nr++)
            swl.insert(Entry_Word(label_wl,word_nr));
        }
        label_wl = swl;
        li = wl_db.find_entry(label_wl);
        labels[id] = li;
      }
      return li;
    }
    bool get_label(Word_List * label_wl,Label_ID li)
    {
      return wl_db.get(label_wl,li);
    }
};

FSA_Simple * FSA_Factory::determinise(const FSA & fsa_start,Determinise_Flag partial,
                                      bool merge_labels,
                                      Transition_Storage_Format tsf)
{
  /* if partial is 0 then this this converts a MIDFA to a DFA
     accepting the same language.
     if partial is DF_Identical then we only merge initial states that have the
     same label. In this case, if merge_labels is false it is assumed that we
     won't reach accept states with different labels in a new state. This
     won't usually be true if fsa_start is a MIDFA multiplier which contains
     different words for equal multipliers, so in this case merge_labels should
     be true.
     If partial is DF_Equal then we merge initial states which have a
     label in common (which in the case of MIDFA multipliers means the
     initial labels are equal), which will usually result in many more initial
     states being merged. This is useful when composing MIDFA multipliers
     because initial states tend to proliferate.

     For perfomance reasons we do not use a Subset_Packer type key, but a
     State_List type key. This might result in the factory getting too big,
     but as usually there are only going to be a few initial states, and the
     key of any state can never have more than this number of states, and the
     number can only decrease this is very unlikely. In fact the key will
     very likely be smaller than it would be as a subset, since it is so
     sparse that the gaps would be too big not to need splitting */

  if (fsa_start.initial_type() == SSF_Singleton &&
      fsa_start.initial_state()==1)
    return minimise(fsa_start);
  Transition_ID nr_transitions = fsa_start.alphabet_size();
  Word_Length state_length = 0;
  Container & container = fsa_start.container;
  Keyed_FSA factory(container,fsa_start.base_alphabet,nr_transitions,
                    1024*1024+7,0);
  const Alphabet & label_alphabet = fsa_start.label_alphabet();
  State_List old_key;
  Element_List key; // initially this is used as a State_List, then as a Label_List

  if (partial)
  {
    if (fsa_start.label_count() == 1)
      partial = DF_All;
  }

  if (merge_labels)
    if (fsa_start.labels_are_words())
    {
      factory.set_label_type(LT_List_Of_Words);
      factory.set_label_alphabet(label_alphabet);
    }
    else
    {
      merge_labels = false;
      if (partial == DF_Equal)
        partial = DF_Identical;
    }

  /* Insert failure state */
  key.append_one(0);
  factory.find_state(key.buffer(),key.size());

  /* find out which states we need to retain */
  bool was_trim = (fsa_start.get_flags() & GFF_TRIM)!=0;
  State_ID * states_1 = !was_trim ? new State_ID[fsa_start.state_count()] : 0;

  /* Insert initial state(s).*/
  {
    /* First of all we build a list of the initial states that can lead
       to success. We ignore all the others */
    if (!was_trim)
    {
      Transition_Realiser tr(fsa_start);
      trim_inner(tr,states_1);
    }

    State_Subset_Iterator ssi;
    key.empty();
    for (State_ID si = fsa_start.initial_state(ssi,true);si;
         si = fsa_start.initial_state(ssi,false))
      if (was_trim || states_1[si])
        key.append_one(si);

    if (partial == DF_Identical)
    {
      /* In this case we only merge initial states with the same label */

      /* transfer the list of all initial states to another variable as
         we will re-use key, and old_key*/
      State_List istates;
      istates.take(key);
      State_List::Iterator initial_sli(istates);

      /* build a list of initial labels */
      for (State_ID si = initial_sli.first();si;si = initial_sli.next())
        old_key.insert(fsa_start.get_label_nr((si)));

      /* now build the new initial states */
      State_List::Iterator label_sli(old_key);
      for (Label_ID label = label_sli.first();label;label = label_sli.next())
      {
        key.empty();
        for (State_ID si = initial_sli.first();si;
             si = initial_sli.next())
          if (fsa_start.get_label_nr(si) == label)
            key.append_one(si);
        key.append_one(0);
        factory.set_is_initial(factory.find_state(key.buffer(),key.size()),true);
      }
    }
    else if (partial == DF_Equal)
    {
      /* transfer the list of succeeding initial states to old_key */
      old_key.take(key);
      State_List::Iterator sli(old_key);

      /* now build the new list of initial states by removing the first
         state repeatedly and merging later states with the new state
         if they have a label in common */
      for (State_ID si = sli.first();si;
           si = sli.first())/* we are going to consume the first item each time*/
      {
        old_key.remove(si);
        key.empty();
        key.append_one(si);
        Word_List label_wl(label_alphabet);
        container.status(2,1,"Examining labels (" FMT_ID " to do)\n",old_key.count());
        fsa_start.label_word_list(&label_wl,fsa_start.get_label_nr(si));
        Sorted_Word_List swl(label_wl);
        bool found = false;
        bool changed = false;
        for (State_ID merge_si = sli.first();merge_si;
             merge_si = changed ? sli.first() : (found ? sli.current() : sli.next()))
        {
          changed = found = false;
          fsa_start.label_word_list(&label_wl,fsa_start.get_label_nr(merge_si));
          Element_Count nr_words = label_wl.count();
          for (Element_ID word_nr = 0;word_nr < nr_words;word_nr++)
            if (swl.contains(Entry_Word(label_wl,word_nr)))
            {
              found = true;
              break;
            }
          if (found)
          {
            for (Element_ID word_nr = 0;word_nr < nr_words;word_nr++)
              if (swl.insert(Entry_Word(label_wl,word_nr)))
                changed = true;
            key.insert(merge_si);
            old_key.remove(merge_si);
          }
        }
        key.append_one(0);
        factory.set_is_initial(factory.find_state(key.buffer(),key.size()),true);
      }
    }
    else
    {
      /* The initial state is the set of all succeeding initial states of the
         original FSA*/
      key.append_one(0);
      factory.find_state(key.buffer(),key.size());
      old_key.reserve(key.count(),false);
    }
  }

  State_ID det_si = 0;
  State_Count count = factory.state_count();
  State_ID ceiling = 1;
  State_ID * transition = new State_ID[nr_transitions];
  while (factory.get_state_key(old_key.buffer(),++det_si))
  {
    Word_Length length = det_si >= ceiling ? state_length : state_length - 1;
    container.status(2,1,"Building determinised FSA states: ("
                         FMT_ID " of " FMT_ID " to do). Depth %u\n",
                     count-det_si,count,length);
    for (Transition_ID ti = 0; ti < nr_transitions;ti++)
    {
      key.empty();
      for (State_ID *si = old_key.buffer() ; *si; si++)
      {
        State_ID nsi = fsa_start.new_state(*si,ti,false);
        if (nsi > 0 && (was_trim || states_1[nsi]))
          key.insert(nsi);
      }
      key.append_one(0);
      transition[ti] = factory.find_state(key.buffer(),key.size());
      if (transition[ti] >= count)
      {
        if (det_si >= ceiling)
        {
          state_length++;
          ceiling = transition[ti];
        }
        count++;
      }
    }
    factory.set_transitions(det_si,transition);
  }
  if (states_1)
    delete [] states_1;
  delete [] transition;
  det_si = 0;
  if (fsa_start.accept_type() != SSF_All)
    factory.clear_accepting(true);
  Label_Merger label_merger(fsa_start);
  if (!merge_labels && partial == DF_Identical)
    factory.copy_labels(fsa_start);

  while (factory.get_state_key(old_key.buffer(),++det_si))
  {
    container.status(2,1,"Setting accept states of determinised FSA: "
                     "(" FMT_ID " of " FMT_ID " to do).\n",count-det_si,count);
    if (merge_labels)
      label_merger.begin_label(key);
    for (State_ID *si = old_key.buffer() ; *si; si++)
    {
      if (merge_labels)
      {
        Label_ID li = fsa_start.get_label_nr(*si);
        if (li)
          label_merger.add_to_label(key,li);
      }
      else if (partial == DF_Identical)
      {
        Label_ID li = fsa_start.get_label_nr(*si);
        if (li)
          factory.set_label_nr(det_si,li);
      }
      if (fsa_start.is_accepting(*si))
        factory.set_is_accepting(det_si,true);
    }
    if (merge_labels)
      factory.set_label_nr(det_si,label_merger.end_label(key),true);
  }
  factory.remove_keys();

  if (merge_labels)
  {
    Label_Count nr_labels = factory.label_count();
    Word_List label_wl(label_alphabet);
    for (Label_ID li = 1;li < nr_labels;li++)
    {
      container.status(2,1,"Creating labels of determinised FSA: "
                     "(" FMT_ID " of " FMT_ID " to do).\n",nr_labels-li,nr_labels);
      label_merger.get_label(&label_wl,li);
      factory.set_label_word_list(li,label_wl);
    }
  }

  factory.change_flags(GFF_BFS|GFF_ACCESSIBLE|GFF_TRIM,0);
  return minimise(factory,tsf,merge_labels && partial ? MLF_Non_Accepting : MLF_None);
}

/**/

FSA_Simple * FSA_Factory::composite(const FSA & fsa0,const FSA & fsa1,bool labelled_multiplier)
{
  /* fsa_0, and fsa_1 must be product FSAs on the same alphabet.
     The returned fsa accepts (u,v) if and only if there is some w such
     that fsa_0 accepts (u,w) and fsa_1 accepts (w,v) */
  const Alphabet & base_alphabet = fsa0.base_alphabet;
  const Alphabet & label_alphabet = fsa0.label_alphabet();
  Word_List_DB labels(1024,false);
  Ordinal_Word base_word(base_alphabet);
  Ordinal_Word label_word(label_alphabet);
  Transition_ID nr_transitions = fsa0.alphabet_size();
  State_ID * transition = new State_ID[nr_transitions];
  State_ID state;
  bool is_coset_multiplier = labelled_multiplier &&
                             label_alphabet.letter_count() > base_alphabet.letter_count() ||
                             fsa0.has_multiple_initial_states() ||
                             fsa1.has_multiple_initial_states();
  Container & container = fsa0.container;
  Ordinal nr_generators = base_alphabet.letter_count();

  State_Count count,initial_end;     //Declaring this here so that I don't
  Word_List label_wl(label_alphabet);  //need to declare them twice

  /* Initialise the new FSA */
  State_ID pair[2],old_pair[2];
  State_Pair_List key;
  Byte_Buffer packed_key;
  const void * old_packed_key;
  Transition_ID product_id;
  Ordinal g1,g2,g_middle;
  State_ID ceiling = 1;
  Word_Length state_length = 0;
  size_t key_size;
  Keyed_FSA factory(container,base_alphabet,nr_transitions,
                    fsa0.state_count(),0);

  /* Create the failure and initial states */
  key.empty();
  packed_key = key.packed_data(&key_size);
  factory.find_state(packed_key,key_size);

  State_Subset_Iterator ssi_0;
  State_Subset_Iterator ssi_1;

  bool all_dense = key.all_dense(fsa0.state_count(),
                                 fsa1.state_count());
  if (labelled_multiplier)
  {
    for (pair[0] = fsa0.initial_state(ssi_0,true); pair[0];
         pair[0] = fsa0.initial_state(ssi_0,false))
      for (pair[1] = fsa1.initial_state(ssi_1,true); pair[1];
             pair[1] = fsa1.initial_state(ssi_1,false))
      {
        key.insert(pair,all_dense);
        packed_key = key.packed_data(&key_size);
        factory.find_state(packed_key,key_size);
        key.empty();
      }
  }
  else
  {
    for (pair[0] = fsa0.initial_state(ssi_0,true); pair[0];
         pair[0] = fsa0.initial_state(ssi_0,false))
      for (pair[1] = fsa1.initial_state(ssi_1,true); pair[1];
             pair[1] = fsa1.initial_state(ssi_1,false))
        key.insert(pair,all_dense);
     packed_key = key.packed_data(&key_size);
     factory.find_state(packed_key,key_size);
     key.empty();
  }

  count = initial_end = factory.state_count();

  Transition_Realiser tr0(fsa0);
  Transition_Realiser *tr1 = &fsa0 == &fsa1 ? &tr0 : new Transition_Realiser(fsa1);

  /* Now create all the new states and the transition table */
  state = 0;
  State_Pair_List old_key;
  while ((old_packed_key = factory.get_state_key(++state))!=0)
  {
    Word_Length length = state >= ceiling ? state_length : state_length - 1;
    if (!(state & 255))
      container.status(2,1,"Composing FSA: state " FMT_ID " "
                           "(" FMT_ID " of " FMT_ID " to do). Length %u\n",
                       state,count-state,count,length);

    old_key.unpack((const Byte *) old_packed_key);
    State_Pair_List::Iterator old_key_pairs(old_key);
    /* We have to have some way of recognising when a padding character
       has been read on the left or right, in order to stop ourselves from
       accepting interior padding characters. Given that our original
       multiplier does this automatically this is somewhat surprising.
       The reason we have to do it is that when we take a middle $ generator
       we may find ourselves wanting to take a non-existent $,$ transition
       in either pair[0] or pair[1]. In this case we have no choice but to
       stay in in the current state, which is then unaware that it has
       read a padding character. */
    bool left_padded = old_key.is_left_padded();
    bool right_padded = old_key.is_right_padded();

    if (left_padded || right_padded)
      for (product_id = 0; product_id < nr_transitions;product_id++)
        transition[product_id] = 0;

    for (g1 = left_padded ? nr_generators : 0; g1 <= nr_generators;g1++)
      for (g2 = right_padded ? nr_generators : 0; g2 <= nr_generators;g2++)
      {
        product_id = base_alphabet.product_id(g1,g2);
        if (product_id < nr_transitions)
        {
          key.empty();
          /* To get the transition target we have to create the key
             of the new state by iterating over all the state pairs in
             the current state, for each possible middle generator */
          if (old_key_pairs.first(old_pair))
          {
            do
            {
              const State_ID * trow1 = tr0.realise_row(old_pair[0],0) +
                                        base_alphabet.product_base(g1);
              const State_ID * trow2 = tr1->realise_row(old_pair[1],1);
              for (g_middle = 0; g_middle < nr_generators; g_middle++)
              {
                if (!trow1[g_middle])
                  continue;
                pair[0] = trow1[g_middle];
                Transition_ID tid = base_alphabet.product_id(g_middle,g2);
                pair[1] = trow2[tid];
                if (pair[1])
                  key.insert(pair,all_dense);
              }
              {
                Transition_ID tid = base_alphabet.product_id(g1,g_middle);
                if (tid < nr_transitions)
                  pair[0] = trow1[g_middle];
                else
                  pair[0] = fsa0.is_accepting(old_pair[0]) ? old_pair[0] : 0;
                if (!pair[0])
                  continue;

                tid = base_alphabet.product_id(g_middle,g2);
                if (tid < nr_transitions)
                  pair[1] = trow2[tid];
                else
                  pair[1] = fsa1.is_accepting(old_pair[1]) ? old_pair[1] : 0;
                if (pair[1])
                  key.insert(pair,all_dense);
              }
            }
            while (old_key_pairs.next(old_pair));
          }
          if (g1 == nr_generators)
            key.set_left_padded();
          else if (g2 == nr_generators)
            key.set_right_padded();
          packed_key = key.packed_data(&key_size);
          transition[product_id] = factory.find_state(packed_key,key_size);
          if (transition[product_id] >= count)
          {
            if (state >= ceiling)
            {
              state_length++;
              ceiling = transition[product_id];
            }
            count++;
          }
        }
      }
    factory.set_transitions(state,transition);
  }
  /* we don't need this any more */
  delete [] transition;

  /* Set the labels,and initial and accept states in the new FSA */
  bool label01_is_identity = false;
  bool label11_is_identity = false;
  Sorted_Word_List swl(label_alphabet);
  Label_ID label_id;

  if (labelled_multiplier)
  {
    fsa0.label_word_list(&label_wl,1);
    label_word.set_length(0);
    label01_is_identity = label_wl.contains(label_word);
    fsa1.label_word_list(&label_wl,1);
    label11_is_identity = label_wl.contains(label_word);
    labels.empty();
    // Insert a "null" label, so our label numbers start from 1
    labels.Hash::find_entry(0,0);
    factory.set_label_alphabet(label_alphabet);
    factory.set_label_type(LT_List_Of_Words);
    factory.set_nr_labels(1,LA_Mapped);
  }

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
    State_Pair_List total_key((const Byte *) old_packed_key);
    State_Pair_List::Iterator old_key_pairs(old_key);
    swl.empty();
    while (old_key_pairs.first(old_pair))
    {
      do
      {
        bool accepting_pair = fsa0.is_accepting(old_pair[0]) && fsa1.is_accepting(old_pair[1]);
        if (accepting_pair)
          accepted = true;

        if (labelled_multiplier)
        {
          Label_ID label0 = fsa0.get_label_nr(old_pair[0]);
          Label_ID label1 = fsa1.get_label_nr(old_pair[1]);
          bool initial_pair = state < initial_end && fsa0.is_initial(old_pair[0]) && fsa1.is_initial(old_pair[1]);
          if (initial_pair || accepting_pair)
          {
            Word_List wl0(label_alphabet),wl1(label_alphabet);
            Ordinal_Word w0(label_alphabet),w1(label_alphabet);
            fsa0.label_word_list(&wl0,label0);
            fsa1.label_word_list(&wl1,label1);
            Element_Count count0 = wl0.count();
            Element_Count count1 = wl1.count();
            bool fix0 = false;
            bool fix1 = false;
            if (is_coset_multiplier)
            {
              /* label 1 is always attached to state 1, and almost always
                 represents the identity. We do not need to propagate these
                 labels, and not doing so will speed things up.
                 This is only valid with the MIDFA form of the multiplier
                 not for determinised ones.
                 Also a multiplier for a specific word will often have a
                 different label 1 */
              if (label0 == 1 && label01_is_identity)
              {
                count0 = 1;
                fix0 = true;
              }
              else if (label1 == 1 && label11_is_identity)
              {
                count1 = 1;
                fix1 = true;
              }
            }
            for (Element_ID word0 = 0; word0 < count0; word0++)
            {
              if (fix0)
                w0.set_length(0);
              else
                wl0.get(&w0,word0);
              for (Element_ID word1 = 0; word1 < count1; word1++)
              {
                if (fix1)
                  w1.set_length(0);
                else
                  wl1.get(&w1,word1);
                if (w0.length() > 0 && w1.length() > 0 &&
                    (w0.value(0) < nr_generators) != (w1.value(0) < nr_generators))
                  continue; // we do not want mixed labels
                label_word = w0 + w1;
                swl.insert(label_word);
              }
            }
          }
        }
      }
      while (old_key_pairs.next(old_pair) && (!accepted || labelled_multiplier));

      if (!accepted || labelled_multiplier)
      {
        /* We also need to check states accessible via $,g g,$ transitions
           since these states ought to be in our image set */
        old_key_pairs.first(old_pair);
        do
        {
          const State_ID * trow1 = tr0.realise_row(old_pair[0],0) + base_alphabet.product_base(PADDING_SYMBOL);
          const State_ID * trow2 = tr1->realise_row(old_pair[1],1);
          for (g_middle = 0; g_middle < nr_generators; g_middle++)
          {
            pair[0] = trow1[g_middle];
            if (!pair[0])
              continue;

            pair[1] = trow2[base_alphabet.product_id(g_middle,PADDING_SYMBOL)];
            if (pair[1])
            {
              if (total_key.insert(pair,all_dense))
                key.insert(pair,all_dense);
            }
          }
        }
        while (old_key_pairs.next(old_pair));
        old_key.take(key);
      }
      else
        break;
    }
    if (accepted)
      factory.set_is_accepting(state,true);
    if (state < initial_end)
      factory.set_is_initial(state,true);
    if (labelled_multiplier && (accepted || state < initial_end))
    {
      Element_ID id;
      label_wl = swl;
      labels.insert(&id,label_wl);
      factory.set_label_nr(state,id,true);
    }
  }
  tr0.unrealise();
  tr1->unrealise();
  if (tr1 != &tr0)
    delete tr1;
  factory.remove_keys();
  if (labelled_multiplier)
  {
    const void * packed_label;
    label_id = 0;
    while ((packed_label = labels.get_key(++label_id))!=0)
    {
      label_wl.unpack((const Byte *)packed_label);
      factory.set_label_word_list(label_id,label_wl);
    }
    labels.empty();
  }

  if (is_coset_multiplier)
  {
    // determinise here merges initial states that are proved equal
    // and then does a minimise
    return determinise(factory,FSA_Factory::DF_Equal,true);
  }
  else
    return minimise(factory);
}

/**/

FSA_Simple * FSA_Factory::exists(const FSA &fsa_start,bool sticky,bool swapped)
{
  /* fsa_start must be a product FSA. If "sticky" is false the returned FSA
     accepts w_1 if and only if fsa_start accepts(w_1,w_2) for some w_2,
     or if swapped is true if and only if fsa_start accepts(w_2,w_1) for some
     w_2.
     If "sticky" is true then if w_1 is accepted according to the rule above
     then we automatically accept all words of the form w_1*X. If we were
     trying to use exists() to calculate a word-acceptor this will be correct
     and much faster.

     As we build up the subset that represents a state, we form a key that
     represents its characteristic function. For n a state number of fsa_start,
     key[n] indicates whether or not the state n is present in the subset.

     This information is compressed in the stored key.
  */
  State_Count nr_product_states = fsa_start.state_count();
  Container &container = fsa_start.container;
  const Alphabet & alphabet = fsa_start.base_alphabet;
  Word_Length state_length = 0;
  State_ID ceiling = 1;
  Special_Subset key(fsa_start);
  Special_Subset old_key(fsa_start);
  Special_Subset::Packer key_packer(key);
  void * packed_key = key_packer.get_buffer();

  if (!fsa_start.is_product_fsa())
    return 0;
  Ordinal nr_generators = alphabet.letter_count();
  Keyed_FSA factory(container,alphabet,nr_generators,
                    nr_product_states,0);
  State_ID * transition = new State_ID[nr_generators];
  Transition_Realiser tr(fsa_start);

  /* Insert and skip failure state */
  key.empty();
  size_t size = key_packer.pack(key);
  factory.find_state(packed_key,size);
  /* Insert initial state */
  State_Subset_Iterator ssi;
  for (State_ID si = fsa_start.initial_state(ssi,true);
       si;si = fsa_start.initial_state(ssi,false))
    key.include(si);
  size = key_packer.pack(key);
  factory.find_state(packed_key,size);
  State_Count count = 2;
  State_ID exists_state = 0;
  Accept_Type accept_type = fsa_start.accept_type();
  State_ID product_accept_state = fsa_start.accepting_state();
  State_ID sticky_state = 0;
  State_List sl;

  if (accept_type != SSF_All)
    factory.clear_accepting(true);
  while (++exists_state < count)
  {
    Word_Length length = exists_state >= ceiling ? state_length : state_length - 1;
    if (exists_state == sticky_state)
    {
      for (Ordinal g1 = 0;g1 < nr_generators;g1++)
        transition[g1] = sticky_state;
    }
    else
    {
      old_key.unpack((const Byte *) factory.get_state_key(exists_state),
                     factory.get_key_size(exists_state));
      if (!(char) exists_state)
        container.status(2,1,"Building \"exists\" FSA state " FMT_ID
                             " (" FMT_ID " of " FMT_ID " to do). Length %u\n",
                         exists_state,count-exists_state,count,length);

      for (Ordinal g1 = 0;g1 < nr_generators;g1++)
      {
        /* Calculate action of generators on this state. To get the image, we
           have to apply (g1,g2) to each element in the subset of fsa_start
           corresponding to the state, for each generator g2 and for g2=Idword.
        */
        key.empty();
        bool accepts = false;
        for (State_ID si = ssi.first(old_key) ; si; si = ssi.next(old_key))
        {
          const State_ID * trow = tr.realise_row(si);
          for (Ordinal g2 = 0; g2 <= nr_generators;g2++)
          {
            Transition_ID product_id = swapped ? alphabet.product_id(g2,g1) :
                                                 alphabet.product_id(g1,g2);
            State_ID nsi = trow[product_id];
            if (!nsi)
              continue;
            key.include(nsi);
            if (sticky && fsa_start.is_accepting(nsi))
            {
              accepts = true;
              break;
            }
          }
          if (accepts)
            break;
        }
        if (accepts)
        {
          if (!sticky_state)
          {
            size = key_packer.pack(key);
            sticky_state = factory.find_state(packed_key,size);
          }
          transition[g1] = sticky_state;
        }
        else
        {
          size = key_packer.pack(key);
          transition[g1] = factory.find_state(packed_key,size);
        }
        if (transition[g1] >= count)
        {
          if (exists_state >= ceiling)
          {
            state_length++;
            ceiling = transition[g1];
          }
          count++;
        }
      }
    }
    factory.set_transitions(exists_state,transition);
  }
  if (accept_type != SSF_All)
  {
    exists_state = 0;
    while (++exists_state < count)
    {
      container.status(2,1,"Setting accept states for \"exists\" FSA state (" FMT_ID " of " FMT_ID " to do).\n",
                       count-exists_state,count);
      char accepts = 0;
      if (exists_state == sticky_state)
        accepts = 1;
      else
      {
        old_key.unpack((const Byte *) factory.get_state_key(exists_state),
                       factory.get_key_size(exists_state));
        for (;;)
        {
          if (accept_type != SSF_Singleton)
          {
            for (State_ID si = ssi.first(old_key) ; si; si = ssi.next(old_key))
            {
              if (fsa_start.is_accepting(si))
              {
                accepts = 1;
                break;
              }
            }
          }
          else
            accepts = old_key.contains(product_accept_state);
          if (accepts)
            break;
          key.empty();
          for (State_ID si = ssi.first(old_key); si; si = ssi.next(old_key))
          {
            Ordinal g1 = PADDING_SYMBOL;
            const State_ID * trow = tr.realise_row(si);
            for (Ordinal g2 = 0; g2 < nr_generators;g2++)
            {
              Transition_ID product_id = swapped ? alphabet.product_id(g2,g1) :
                                                   alphabet.product_id(g1,g2);
              State_ID nsi = trow[product_id];
              key.include(nsi);
            }
          }
          if (!key.count())
            break;
          old_key.take(key);
        }
      }
      if (accepts)
        factory.set_is_accepting(exists_state,true);
    }
  }
  tr.unrealise();
  factory.remove_keys();
  delete [] transition;
  container.progress(1,"Exists FSA with " FMT_ID " states prior to"
                       " minimisation constructed.\n",factory.state_count());
  FSA_Simple * answer = minimise(factory);
  return answer;
}

/**/

FSA_Simple * FSA_Factory::diagonal(const FSA &fsa_start)
{
  /* fsa_start must be a product FSA. The returned FSA accepts a word
     w if and only if fsa_start accepts (w,w)
  */
  State_Count nr_product_states = fsa_start.state_count();
  Container &container = fsa_start.container;
  const Alphabet & alphabet = fsa_start.base_alphabet;
  Word_Length state_length = 0;
  State_ID ceiling = 1;
  State_ID key,old_key;

  if (!fsa_start.is_product_fsa())
    return 0;
  Ordinal nr_generators = alphabet.letter_count();
  Keyed_FSA factory(container,alphabet,nr_generators,nr_product_states,sizeof(State_ID));
  State_ID * transition = new State_ID[nr_generators];
  Transition_Realiser tr(fsa_start);

  /* Insert and skip failure state */
  key = 0;
  factory.find_state(&key);
  /* Insert initial states */
  State_Subset_Iterator ssi;
  for (key = fsa_start.initial_state(ssi,true);
       key;key = fsa_start.initial_state(ssi,false))
    factory.set_is_initial(factory.find_state(&key),true);
  State_Count count = factory.state_count();
  State_ID diagonal_si = 0;
  Accept_Type accept_type = fsa_start.accept_type();

  if (accept_type != SSF_All)
    factory.clear_accepting(true);
  while (factory.get_state_key(&old_key,++diagonal_si))
  {
    Word_Length length = diagonal_si >= ceiling ? state_length : state_length - 1;
    if (!(char) diagonal_si)
      container.status(2,1,"Building diagonal FSA state " FMT_ID " ("
                            FMT_ID " of " FMT_ID " to do). Depth %u\n",
                       diagonal_si,count-diagonal_si,count,length);

    if (accept_type != SSF_All)
      factory.set_is_accepting(diagonal_si,fsa_start.is_accepting(old_key));
    const State_ID * trow = tr.realise_row(old_key);
    for (Ordinal g1 = 0;g1 < nr_generators;g1++)
    {
      transition[g1] = factory.find_state(&trow[alphabet.product_id(g1,g1)]);
      if (transition[g1] >= count)
      {
        if (diagonal_si >= ceiling)
        {
          state_length++;
          ceiling = transition[g1];
        }
        count++;
      }
    }
    factory.set_transitions(diagonal_si,transition);
  }
  tr.unrealise();
  factory.remove_keys();
  delete [] transition;
  return minimise(factory);
}

/**/

FSA_Simple * FSA_Factory::merge(const FSA & fsa_start,const State_ID *new_states)
{
  /* Returns an FSA in which states with the same number in the new_number
     array have been merged. The merge will fail if any two states that
     are supposed to be merged have contradicting transitions or different
     labels.
     Transitions for states i,j, contradict if for any x
     new_number[new_state(i,x)] and new_number[(new_state(j,x)] are both
     non-zero and different.

     The returned FSA is not minimised.
  */
  const Transition_ID nr_symbols = fsa_start.alphabet_size();
  State_ID si;
  const Label_ID nr_labels = fsa_start.label_count();
  const State_Count nr_states = fsa_start.state_count();
  State_Count new_nr_states = 0;
  // work out how many states are needed. It will usually be much faster
  // to go through the list completely looking for new states past where
  // we have got to, since we can then find multiple states on each pass.
  // If the list is really random and we need to many passes then
  // we build an element list and check it is contiguous from 0 to n.
  bool again;
  int pass = 0;
  do
  {
    again = false;
    for (si = 0; si < nr_states ;si++)
      if (new_states[si] == new_nr_states)
      {
        new_nr_states++;
        again = true;
      }
     pass++;
  }
  while (again && pass < 3);
  if (again)
  {
    Element_List el;
    el.reserve(nr_states,false);
    for (si = 0;si < nr_states;si++)
      el.insert(new_states[si]);
    Element_List::Iterator it(el);
    if (it.first() != 0 || it.last() != el.count()-1)
      return 0;
    new_nr_states = el.count();
  }
  else
  {
    // check the new_states array is valid
    for (si = 0; si < nr_states ;si++)
      if (new_states[si] >= new_nr_states)
        return 0;
  }

  /* Now we need to build the merged FSA. */
  FSA_Simple * new_fsa = new FSA_Simple(fsa_start.container,
                                        fsa_start.base_alphabet,
                                        new_nr_states, nr_symbols);

  new_fsa->change_flags(fsa_start.get_flags(),0);
  new_fsa->change_flags(0,GFF_BFS|GFF_MINIMISED);
  new_fsa->copy_labels(fsa_start,LA_Mapped);
  bool ok = true;
  Accept_Type accept = fsa_start.accept_type();
  bool set_accept = accept != SSF_Singleton && accept != SSF_All;
  bool set_initial = fsa_start.initial_type() != SSF_Singleton ||
                     fsa_start.initial_state() != 1;
  if (set_accept)
    new_fsa->clear_accepting(true);
  if (set_initial)
    new_fsa->clear_initial(false);

  for (si = 1; si < nr_states;si++)
  {
    State_ID state = new_states[si];
    if (state)
    {
      if (!(char) state)
        fsa_start.container.status(2,1,"Merging FSA states: state " FMT_ID
                                      ". (" FMT_ID " of " FMT_ID ") to do.\n",
                                   state,nr_states-state,nr_states);
      if (nr_labels > 1)
      {
        Label_ID old_label = new_fsa->get_label_nr(state);
        Label_ID new_label = fsa_start.get_label_nr(si);
        if (old_label && new_label && new_label != old_label)
        {
          ok = false;
          break;
        }
        if (new_label)
          new_fsa->set_label_nr(state,new_label);
      }

      State_ID * transition = new_fsa->state_lock(state);
      for (Transition_ID j = 0; j < nr_symbols;j++)
      {

        State_ID new_state = new_states[fsa_start.new_state(si,j)];
        if (transition[j] && new_state && transition[j] != new_state)
        {
          ok = false;
          break;
        }
        if (new_state)
          transition[j] = new_state;
      }
      new_fsa->state_unlock(state);
      if (set_accept && fsa_start.is_accepting(si))
        new_fsa->set_is_accepting(state,true);
      if (set_initial && fsa_start.is_initial(si))
        new_fsa->set_is_initial(state,true);
    }
    if (!ok)
      break;
  }
  if (!ok)
  {
    delete new_fsa;
    new_fsa = 0;
  }
  else
  {
    if (accept == SSF_Singleton)
      new_fsa->set_single_accepting(new_states[fsa_start.accepting_state()]);
    new_fsa->tidy();
  }
  return new_fsa;
}

/**/

FSA_Simple * FSA_Factory::minimise(const FSA & fsa_start,
                                   Transition_Storage_Format tsf,
                                   Merge_Label_Flag merge_labels)
{
  /* method to create the minimal FSA that accepts the same language as the
     original FSA and in which states have the same labels.
     This function works equally well for labelled and unlabelled FSAs, and
     works with RWS FSAs provided it is warned.
     This algorithm is supposedly an n^2 algorithm. In practice, for the FSA
     typically produced by MAF it is approximately n log n. The number of
     passes  required is typically only a few more than the furthest distance
     of any state from an initial state, which is typically logarithmic in the
     number of states (to base alphabet size, or alphabet_size-1).
     I have considered some of the algorithms discussed in the paper
     "On the performance of FSA minimisation algorithms". Unfortunately the
     experimental results of that paper are of little or no relevance to MAF,
     since the number of states in the FSAs considered by the paper is tiny and
     even the largest alphabet considered is smaller than the alphabet of a
     two generator word-difference machine.
     Two algorithms (Hopcroft's and Brzozowski's) are ruled out because
     they require a huge amount of extra storage - we would need to construct
     the reverse transition table for our FSA, which would require a huge
     array of variably sized arrays. (Actually our algorithm needs huge
     amounts of space as well, but we could easily use external storage
     if need be)
     Watson's and Daciuk's algorithm may be good, but I haven't found a
     comprehensible detailed description of it yet. Apparently it has
     theoretically worse asymptotic running time that the classical algorithm
     used here in any case. The primary advantage of that algorithm is that
     one can stop early, which might be a very useful time saving in the case
     of trial and correction automata. A major potential problem is that it
     potentially requires recursion to the depth of the number of states,
     so that it would certainly be essential to remove the recursion.

     For the huge FSA this code may be called upon to minimise a dominating
     factor in the time for each pass is the amount of data that is required
     in a state key. If we can reduce this signifantly each pass will run
     much quicker. This was the reasoning behind the code that looks for
     states that won't change in later passes of the algorithm. On its own
     this code would not be effective, because usually only a small proportion
     of states would be recognised as "decided", which means that the time and
     space savings from it would be marginal, and in fact the extra overhead
     typically would outweigh this.(On the other hand if we were unlucky enough
     to encounter an FSA which needs very many passes to minimise, this code
     will eventually start speeding passes up substantially, so will definitely
     have better worst case behaviour).

     But if we combine this with the main idea of Watson and Daciuk's algorithm
     we can do much better. It turns out that frequently a high proportion of
     states can be recognised as duplicates of one another, especially in the
     case of multipliers and word-acceptors. So it is well worth doing this,
     both because it potentially saves a huge amount of time per pass when
     we do the actual minimisation, and because a much higher proportion of
     states will be recognised as "decided" later on.
     This code won't always be beneficial. It is easy to come up with
     FSA for which the "outer minimisation" will be very expensive and not
     save any time at all later on, either because it cannot reduce the
     number of states, or because the minimisation would in fact complete
     in very few passes. Yet in practice it works very well.
  */

  Label_ID nr_labels = fsa_start.label_count();
  const Transition_ID nr_symbols = fsa_start.alphabet_size();
  State_ID * key = new State_ID[nr_symbols+1];
  State_ID i;
  const State_Count nr_states = fsa_start.state_count();
  State_ID * states_0 = new State_ID[nr_states];
  State_ID * states_1 = new State_ID[nr_states];
  Container & container = fsa_start.container;
  Transition_Realiser tr(fsa_start);
  Transition_Compressor tc(nr_symbols+1);
  State_ID trim_label;
  bool trim = false;
  bool was_trim = (fsa_start.get_flags() & GFF_TRIM)!=0;
  bool is_rws = (fsa_start.get_flags() & GFF_RWS)!=0;
  int gap = 2;

  if (!(fsa_start.get_flags() & GFF_ACCESSIBLE))
  {
    trim = true;
    was_trim = false;
  }

  if (nr_states > 200000 && fsa_start.accept_type() != SSF_All && !was_trim)
    trim = true; // This is not absolutely necessary but is usually much faster

  states_1[0] = 0;
  if (nr_labels==0 || merge_labels==MLF_All)
  {
    for (i = 1;i < nr_states;i++)
      states_1[i] = fsa_start.is_accepting(i);
    trim_label = 2;
  }
  else
  {
    /* We use a hash here to assign sensible states_1 values, rather than using
       2*label + is_accepting to ensure states_1[i] <= nr_states for all
       states. If we didn't do that we would have to treat the first pass
       differently */

    Hash hash(fsa_start.label_count() *2,0,fsa_start.label_count()*2);
    struct
    {
      Label_ID label;
      bool is_accepting;
    } key;

    key.label = 0;
    key.is_accepting = false;
    states_1[0] = hash.find_entry(&key,sizeof(Label_ID)+1);
    for (i = 1;i < nr_states;i++)
    {
      key.label = fsa_start.get_label_nr(i);
      key.is_accepting = fsa_start.is_accepting(i);
      if (key.is_accepting || merge_labels != MLF_Non_Accepting)
      {
        states_1[i] = hash.find_entry(&key,sizeof(Label_ID)+1);
        if (!key.is_accepting && states_1[i])
          trim = true;
      }
      else
        states_1[i] = 0;
    }
    trim_label = hash.count();
  }

  State_Count trim_count = nr_states;
  Element_Count hash_size = 0;
  if (trim)
  {
    if (!was_trim)
      trim_inner(tr,states_0);
    for (i = 1;i < nr_states;i++)
      if (!was_trim && !states_0[i])
      {
        trim_count--;
        states_1[i] = 0;
      }
      else if (!states_1[i])
        states_1[i] = trim_label;
  }
  else if (fsa_start.accept_type() == SSF_All)
    trim = true; /* In this case our FSA is trim, and state 0 is decided */

  Element_Count final_count = trim_count;
  /* Now we are going to look for duplicate states. It principle this
     code might not work, for example consider the following transition
     table, where all states are accepting.

     [2 3 4]
     [3 4 2]
     [4 2 3]
     [4 2 2]

     None of the states are recognised as duplicates, but the FSA minimises
     to [1 1 1].

     In practice this code often performs superbly, eliminating a very
     high proportion of the states in only a few passes. It should be noted
     that the hash created will be very big though.
  */

  State_ID * clone = new State_ID[nr_states];
  if (trim)
    for (i = 0; i < nr_states;i++)
      clone[i] = states_1[i] ? i : 0;
  else
    for (i = 0; i < nr_states;i++)
      clone[i] = i;

  {
    Element_Count initial_count;
    unsigned pass = 0;
    Hash hash(hash_size = trim_count,0,trim_count);
    do
    {
      initial_count = final_count;
      {
        State_ID * temp = states_0;
        states_0 = clone;
        clone = temp;
      }
      if (container.status(2,gap,"FSA pre-minimise pass %u with " FMT_ID " categories."
                                    "(Checking " FMT_ID " states)\n",pass++,final_count,
                                    initial_count))
        gap = 1;
      hash.empty(hash_size);
      memset(key,0,sizeof(State_ID)*(nr_symbols+1));
      size_t size = tc.compress(key);
      clone[0] = hash.find_entry(tc.cdata,size);
      for (i = 1; i < nr_states;i++)
      {
        if (states_0[i] != i)
        {
          clone[i] = clone[states_0[i]];
          continue;
        }

        Transition_ID j = 0;
        const State_ID * transition = tr.realise_row(i);
        if (is_rws)
        {
          for (; j < nr_symbols;j++)
          {
            State_ID si = transition[j];
            if (!fsa_start.is_valid_state(si))
              key[j] = si;
            else
              key[j] = states_0[si];
          }
        }
        else
          for (; j < nr_symbols;j++)
            key[j] = states_0[transition[j]];
        key[j] = states_1[i];
        size = tc.compress(key);
        clone[i] = hash.find_entry(tc.cdata,size);
      }

      hash_size = hash.count();
      /* clone currently contains the equivalence class number for the states.
         Instead we want the first representative for each state. We use
         this to reduce the number of states we need to examine on each pass
         We can reuse states_0, whose contents are no longer needed.
      */
      final_count = 0;
      /* first build the list of first representatives */
      for (i = 0; final_count < hash_size;i++)
        if (clone[i] == final_count)
          states_0[final_count++] = i;
      /* now convert clone to a map to state representatives */
      for (i = 0; i < nr_states;i++)
        clone[i] = states_0[clone[i]];
    }
    while (initial_count-final_count > initial_count/12 &&
           initial_count-final_count >= 1000);
    trim_count = final_count;
  }

  /* Now begins the minimisation proper. */
  {
    Element_Count initial_count;
    unsigned pass = 0;
    Hash hash(hash_size,0,trim_count);
    Element_Count flag_count = trim_count;
    if (flag_count <= trim_label) //since states_0 values can initially get to 
      flag_count = trim_label+1;  //trim_label we need to make sure flags is big 
                                  //enough when lots of labelled states got
                                  // pruned by the trim
    Byte * flags = new Byte[flag_count];
    /* We need two flags per state and both and old and new set of
       flags. Instead of flipping two variables we will flip the
       bits we use. This saves space and probably helps with cache utilisation
       since the positions will be close to stable after a while */
    const Byte Class_Is_Atom1 = 1;
    const Byte Class_Is_Decided1 = 2;
    const Byte Class_Is_AD1 = 3;
    const Byte Class_Is_Atom2 = 4;
    const Byte Class_Is_Decided2 = 8;
    const Byte Class_Is_AD2 = 12;
    Byte new_class_mask = 3;
    Byte old_class_mask;
    Byte new_decided_mask;
    Byte new_atom_mask;
    Byte decided;
    Byte atom;

    final_count = 0;
    memset(flags,0,flag_count);
    if (trim)
      flags[0] = new_class_mask;

    do
    {
      initial_count = final_count;
      if (container.status(2,gap,"FSA minimise pass %u with " FMT_ID
                                 " states so far. (Checking " FMT_ID
                                 " states)\n",pass++,final_count,trim_count))
        gap = 1;

      if (new_class_mask == Class_Is_AD1)
      {
        new_class_mask = Class_Is_AD2;
        old_class_mask = Class_Is_AD1;
        new_decided_mask = Class_Is_Decided2;
        new_atom_mask = Class_Is_Atom2;
      }
      else
      {
        new_class_mask = Class_Is_AD1;
        old_class_mask = Class_Is_AD2;
        new_decided_mask = Class_Is_Decided1;
        new_atom_mask = Class_Is_Atom1;
      }

      {
        State_ID * temp = states_1;
        states_1 = states_0;
        states_0 = temp;
      }

      hash.empty(hash_size);
      size_t size;
      if (!(flags[0] & old_class_mask))
      {
        memset(key,0,sizeof(State_ID)*(nr_symbols+1));
        size = tc.compress(key);
        decided = 0;
      }
      else
      {
        size = tc.key_for_decided_state(0);
        decided = new_decided_mask;
      }
      states_1[0] = hash.find_entry(tc.cdata,size);
      flags[0] = (flags[0] & old_class_mask) | new_atom_mask | decided;

      for (i = 1; i < nr_states;i++)
      {
        if (clone[i] != i)
        {
          states_1[i] = states_1[clone[i]];
          continue;
        }

        if (!states_0[i] && trim)
        {
          states_1[i] = 0;
          continue;
        }

        key[0] = states_0[i];
        decided = new_decided_mask;
        atom = new_atom_mask;

        if (!(flags[states_0[i]] & old_class_mask))
        {
          Transition_ID j = 0;
          const State_ID * transition = tr.realise_row(i);
          if (is_rws)
          {
            for (; j < nr_symbols;j++)
            {
              State_ID si = transition[j];
              if (!fsa_start.is_valid_state(si))
                key[j+1] = si;
              else
              {
                key[j+1] = states_0[si];
                if (!(flags[states_0[si]] & old_class_mask))
                {
                  decided = 0;
                  break;
                }
              }
            }
            while (++j < nr_symbols)
            {
              State_ID si = transition[j];
              if (!fsa_start.is_valid_state(si))
                key[j+1] = si;
              else
                key[j+1] = states_0[si];
            }
          }
          else
          {
            for (; j < nr_symbols;j++)
            {
              key[j+1] = states_0[transition[j]];
              if (!(flags[key[j+1]] & old_class_mask))
              {
                decided = 0;
                break;
              }
            }
            while (++j < nr_symbols)
              key[j+1] = states_0[transition[j]];
          }
          size = tc.compress(key);
        }
        else
          size = tc.key_for_decided_state(key[0]);

        if (!hash.insert(tc.cdata,size,&states_1[i]))
          atom = 0;

        flags[states_1[i]] = (flags[states_1[i]] & old_class_mask) |
                               decided | atom;
      }
      final_count = hash.count();
      hash_size = final_count + (final_count-initial_count)*2;
    }
    while (final_count > initial_count);
    delete [] flags;
    delete [] clone;
    delete [] states_0;
  }

  tr.unrealise();
  /* Now we need to build the minimised FSA. We could have got the data
     out of the Hash (if we added a suitable method) since the final keys
     contained the transitions but is probably just as quick to recreate it
     all */

  bool fudged = false;
  if (final_count == 1)
  {
    /* In this case there is an empty accepted language, and minimize is
       trying to create an FSA that has no initial states or transitions.
       This is not going to work well, so we change the FSA to one with
       an initial state and no transitions at all */
    final_count = 2;
    fudged = true;
    states_1[1] = 1;
  }
  FSA_Simple * new_fsa = new FSA_Simple(container,fsa_start.base_alphabet,
                                        final_count,nr_symbols,tsf);
  new_fsa->change_flags(fsa_start.get_flags(),0);
  if (!merge_labels)
    new_fsa->copy_labels(fsa_start,LA_Mapped);
  final_count = 1;
  Accept_Type accept = fsa_start.accept_type();
  bool set_accept = accept != SSF_Singleton && accept != SSF_All;
  bool set_initial = fsa_start.initial_type() != SSF_Singleton ||
                     fsa_start.initial_state() != 1;
  if (set_accept)
    new_fsa->clear_accepting(true);
  if (set_initial)
    new_fsa->clear_initial(true);
  for (i = 1; i < nr_states;i++)
  {
    if (states_1[i] == final_count)
    {
      final_count++;
      fsa_start.get_transitions(key,i);
      if (fudged)
        for (Transition_ID j = 0; j < nr_symbols;j++)
          key[j] = 0;
      else if (is_rws)
        for (Transition_ID j = 0; j < nr_symbols;j++)
          key[j] = fsa_start.is_valid_state(key[j]) ? states_1[key[j]] : key[j];
      else
        for (Transition_ID j = 0; j < nr_symbols;j++)
          key[j] = states_1[key[j]];
      new_fsa->set_transitions(states_1[i],key);
      if (nr_labels > 1 && !merge_labels)
        new_fsa->set_label_nr(states_1[i],fsa_start.get_label_nr(i));
      if (set_accept && fsa_start.is_accepting(i))
        new_fsa->set_is_accepting(states_1[i],true);
      if (set_initial && fsa_start.is_initial(i))
        new_fsa->set_is_initial(states_1[i],true);
    }
  }

  if (merge_labels && fsa_start.labels_are_words())
  {
    new_fsa->set_label_type(LT_List_Of_Words);
    new_fsa->set_label_alphabet(fsa_start.label_alphabet());
    Label_Merger label_merger(fsa_start);
    Element_List *keys = new Element_List[final_count];
    Word_List label_wl(fsa_start.label_alphabet());

    for (State_ID osi = 1; osi < nr_states;osi++)
    {
      if (!(char) osi)
        container.status(2,1,"Merging old labels (" FMT_ID " of " FMT_ID ")\n",osi,nr_states);
      if (states_1[osi])
      {
        Label_ID label = fsa_start.get_label_nr(osi);
        if (label)
          label_merger.add_to_label(keys[states_1[osi]],label);
      }
    }
    for (State_ID si = 1; si < (State_Count) final_count;si++)
    {
      if (!(char) si)
        container.status(2,1,"Assigning new labels (" FMT_ID " of " FMT_ID ")\n",si,final_count);
      new_fsa->set_label_nr(si,label_merger.end_label(keys[si]),true);
    }

    Element_Count nr_labels = new_fsa->label_count();
    for (Element_ID id = 1; id < nr_labels;id++)
    {
      if (!(char) id)
        container.status(2,1,"Creating new labels (" FMT_ID " of " FMT_ID ")\n",id,nr_labels);
      label_merger.get_label(&label_wl,id);
      new_fsa->set_label_word_list(id,label_wl);
    }
    delete [] keys;
  }
  if (accept == SSF_Singleton)
    new_fsa->set_single_accepting(states_1[fsa_start.accepting_state()]);
  delete [] key;
  delete [] states_1;
  if (!fudged)
    if (nr_labels)
      new_fsa->change_flags(GFF_TRIM|GFF_ACCESSIBLE,0);
    else
      new_fsa->change_flags(GFF_MINIMISED|GFF_TRIM|GFF_ACCESSIBLE,0);
  else
    new_fsa->change_flags(GFF_ACCESSIBLE,GFF_MINIMISED|GFF_TRIM);
  new_fsa->sort_labels(true);
  new_fsa->tidy();
  return new_fsa;
}

/**/

FSA_Simple * FSA_Factory::product_intersection(const FSA & fsa_0,
                                               const FSA & fsa_1,
                                               const FSA & fsa_2,
                                               String description,
                                               bool geodesic_reducer)
{
  /* fsa_0 is a product FSA, and fsa_1,fsa_2 are FSAs using its base alphabet.
     We construct the FSA which accepts (w1,w2) iff fsa_0 accepts (w1,w2) and
     fsa_1 accepts w1 and fsa_2 accepts w2. Since the product FSA also
     accepts padded words, but fsa_1, fsa_2 do not, we have to add an extra
     state to fsa_1 and fsa_2 to deal with this. The states are labelled with
     the labels of the product FSA.
     In theory we could construct this FSA by using method cartesian_product
     on fsa_1 and fsa_2. Then we could construct the fsa we want here, by
     using fsa_and(). But doing everything in one step is better because the
     intermediate FSA would probably be much larger than the FSA we finish up
     constructing.

     So the method constructs the fsa of which the states are triples
     (s0,s1,s2), with s0,s1,s2 states of fsa_0,fsa_1,fsa_2. The states
     are labelled with the states of fsa_0. If fsa_1 has states 1..n-1,
     then s1 may also be n meaning end of string has been read on w1 with
     w1 accepted, and similarly for fsa_2 and w2.
     The alphabet member (g1,g2) maps (s0,s1,s2) to (s0^(g1,g2),s1^g1,s2^g2)
     if all three components are nonzero, and to zero otherwise.
     The accept states are the ones in which all three states in the triple
     are accept states of their respective FSAs, or in the case of s1,s2
     their respective padded accepted state.
  */
  const Alphabet & alphabet = fsa_0.base_alphabet;
  Ordinal nr_generators = alphabet.letter_count();
  int nr_transitions = alphabet.product_alphabet_size();
  Container & container = fsa_0.container;

  if (fsa_0.alphabet_size() != alphabet.product_alphabet_size() ||
      fsa_1.alphabet_size() != alphabet.letter_count() ||
      fsa_2.alphabet_size() != alphabet.letter_count() ||
      fsa_1.has_multiple_initial_states() ||
      fsa_2.has_multiple_initial_states())
    return 0;

  State_ID key[3],old_key[3];
  State_ID end_of_lhs = fsa_1.state_count();
  State_ID end_of_rhs = fsa_2.state_count();
  key[0] = fsa_0.state_count();
  key[1] = end_of_lhs+1;
  key[2] = end_of_rhs+1;
  Triple_Packer key_packer(key);
  State_ID *transition = new State_ID[nr_transitions];
  Keyed_FSA factory(container,alphabet,nr_transitions,
                    key[0]/3+key[1]/3+key[2]/3,key_packer.key_size());
  void * packed_key;
  State_ID state;
  Word_Length state_length = 0;
  Transition_Realiser tr0(fsa_0);
  Transition_Realiser tr1(fsa_1);
  Transition_Realiser tr2(fsa_2);
  bool detect_failing = false;

  if (geodesic_reducer)
  {
    State_Subset_Iterator ssi;
    detect_failing = true;
    for (State_ID si = fsa_1.accepting_state(ssi,true); si && detect_failing;
         si = fsa_1.accepting_state(ssi,false))
      for (Ordinal g = 0; g < nr_generators && detect_failing;g++)
        if (fsa_1.new_state(si,g))
          detect_failing = false;
  }

  /* Insert failure and initial states */
  {
    key[0] = 0;
    key[1] = 0;
    key[2] = 0;
    factory.find_state(packed_key = key_packer.pack_key(key));

    State_Subset_Iterator ssi;
    key[1] = fsa_1.initial_state();
    key[2] = fsa_2.initial_state();
    for (key[0] = fsa_0.initial_state(ssi,true); key[0];
         key[0] = fsa_0.initial_state(ssi,false))
      factory.set_is_initial(factory.find_state(key_packer.pack_key(key)),true);
  }

  State_ID nr_states = factory.state_count();
  State_ID ceiling = 1;
  state = 0;
  while (factory.get_state_key(packed_key,++state))
  {
    Word_Length length = state >= ceiling ? state_length : state_length - 1;
    key_packer.unpack_key(old_key);
    if (!(char) state)
      container.status(2,1,"Building %s:state " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Length %u\n",
                       description.string(),state,nr_states-state,nr_states,length);
    Transition_ID product_id = 0;
    const State_ID * trow0 = tr0.realise_row(old_key[0]);
    const State_ID * trow1 = old_key[1] != end_of_lhs ? tr1.realise_row(old_key[1]) : 0;
    const State_ID * trow2 = old_key[2] != end_of_rhs ? tr2.realise_row(old_key[2]) : 0;
    for (Ordinal g1 = 0; g1 <= nr_generators;g1++)
      for (Ordinal g2 = 0; g2 <= nr_generators;g2++,product_id++)
      {
        if (product_id >= nr_transitions)
          continue;

        transition[product_id] = 0;

        key[0] = trow0[product_id];
        if (!key[0])
          continue;

        if (old_key[1] == end_of_lhs)
          key[1] = g1 == nr_generators ? end_of_lhs : 0;
        else if (g1 == nr_generators)
          key[1] = fsa_1.is_accepting(old_key[1]) && !geodesic_reducer ? end_of_lhs : 0;
        else
          key[1] = trow1[g1];
        if (!key[1])
          continue;

        if (old_key[2] == end_of_rhs)
          key[2] = g2 == nr_generators ? end_of_rhs : 0;
        else if (g2 == nr_generators)
          key[2] = fsa_2.is_accepting(old_key[2]) ? end_of_rhs : 0;
        else
          key[2] = trow2[g2];
        if (!key[2])
          continue;
        if (detect_failing && fsa_1.is_accepting(key[1]) &&
            (!fsa_0.is_accepting(key[0]) ||
              key[2] != end_of_rhs && !fsa_2.is_accepting(key[2])))
          continue;
        if (geodesic_reducer && old_key[2] == end_of_rhs &&
            (!fsa_0.is_accepting(key[0]) || !fsa_1.is_accepting(key[1])))
          continue;
        transition[product_id] = factory.find_state(key_packer.pack_key(key));
        if (transition[product_id] >= nr_states)
        {
          if (state >= ceiling)
          {
            state_length++;
            ceiling = transition[product_id];
          }
          nr_states++;
        }
      }
    factory.set_transitions(state,transition);
  }
  delete [] transition;

  factory.copy_labels(fsa_0,LA_Mapped);
  state = 0;
  factory.clear_accepting(true);
  while (factory.get_state_key(packed_key,++state))
  {
    key_packer.unpack_key(old_key);
    if (fsa_0.is_accepting(old_key[0]) &&
        (old_key[1] == end_of_lhs || fsa_1.is_accepting(old_key[1])) &&
        (old_key[2] == end_of_rhs || fsa_2.is_accepting(old_key[2])))
      factory.set_is_accepting(state,true);
    factory.set_label_nr(state,fsa_0.get_label_nr(old_key[0]));
  }
  factory.remove_keys();
  FSA_Simple * fsa = minimise(factory);
  fsa->sort_labels(true);
  return fsa;
}

/**/

FSA_Simple * FSA_Factory::prune(const FSA & fsa_start)
{
  /* Returns an FSA in which states from which the accepted language
     is finite have been removed */
  const Transition_ID nr_symbols = fsa_start.alphabet_size();
  State_ID i;
  const State_Count nr_states = fsa_start.state_count();
  State_ID * states_1 = new State_ID[nr_states];
  State_Count initial_count,final_count = 1;
  Container & container = fsa_start.container;
  unsigned pass = 0;

  states_1[0] = 0;
  for (i = 1;i < nr_states;i++)
  {
    states_1[i] = 1;
    final_count++;
  }

  do
  {
    initial_count = final_count;

    container.status(2,1,"FSA prune pass %u with " FMT_ID " states so far."
                     "(Original has " FMT_ID " states)\n",+pass,final_count,nr_states);
    for (i = 1; i < nr_states;i++)
      if (states_1[i])
      {
        bool found = false;
        for (Transition_ID j = 0; j < nr_symbols;j++)
          if (states_1[fsa_start.new_state(i,j)])
          {
            found = true;
            break;
          }
        if (!found)
        {
          final_count--;
          states_1[i] = 0;
        }
      }
  }
  while (final_count < initial_count);
  FSA_Simple * answer = filtered_copy(fsa_start,states_1);
  answer->change_flags(0,GFF_MINIMISED|GFF_TRIM); // These properties are lost
  return answer;
}


/**/

FSA_Simple * FSA_Factory::kernel(const FSA & fsa_start,unsigned flags)
{
  /* Modify an FSA so that states that cannot recur are no longer
     accepted */
  FSA_Simple * temp = fsa_start.get_flags() & GFF_TRIM ? copy(fsa_start) :
                                                         minimise(fsa_start);
  State_Count nr_states = temp->state_count();

  bool *recurrent = new bool[nr_states];
  temp->recurrent_states(recurrent,(flags & KF_INCLUDE_ALL_INFINITE)!=0);
  if (flags & KF_ACCEPT_ALL)
    for (State_ID si = 1; si <  nr_states; si++)
      temp->set_is_accepting(si,recurrent[si]);
  else
    for (State_ID si = 1; si <  nr_states; si++)
      if (!recurrent[si])
        temp->set_is_accepting(si,false);

  if (flags & KF_MIDFA)
  {
    State_ID si;
    for (si = 1; si < nr_states; si++)
      temp->set_is_initial(si,recurrent[si]);
    temp->change_flags(GFF_MIDFA,GFF_DFA|GFF_NFA);
    State_ID * states_1 = new State_ID[nr_states];
    for (si = 0;si < nr_states;si++)
      states_1[si] = recurrent[si];
    FSA_Simple * temp2 = filtered_copy(*temp,states_1);
    delete temp;
    temp = temp2;
  }

  FSA_Simple * answer = temp;
  if (!(flags & KF_NO_MINIMISE))
  {
    answer = minimise(*temp);
    delete temp;
  }
  delete [] recurrent;
  return answer;
}

/**/

State_Count FSA::recurrent_states(bool * recurrent,bool all_infinite) const
{
  State_Count nr_states = state_count();
  Transition_ID nr_transitions = alphabet_size();
  State_Count final_count = 0;
  State_ID *previous_state = new State_ID[nr_states]; // a chain of states
  State_ID *next_state = new State_ID[nr_states]; // queue of work

  memset(recurrent,0,nr_states);
  recurrent[0] = 1; // the failure state is always recurrent
  final_count++;
  memset(next_state,0,nr_states*sizeof(State_ID));
  memset(previous_state,0,nr_states*sizeof(State_ID));

  for (State_ID si = 1;si < nr_states;si++)
    if (!recurrent[si])
    {
      State_ID nsi;

      container.status(2,1,"Counting recurrent states. Found " FMT_ID " so far."
                           " (" FMT_ID " of " FMT_ID " to do)\n",
                       final_count,nr_states-si,nr_states);
      State_ID head_si = si;
      State_ID tail_si = si;
      next_state[tail_si] = 0;
      bool found = false;

      for (State_ID csi = head_si;csi ; csi = next_state[csi])
      {
        for (Transition_ID ti = 0; ti < nr_transitions;ti++)
        {
          State_ID nsi = new_state(csi,ti);
          if (is_valid_state(nsi)) /* This deals with RWS FSAs */
          {
            if (!previous_state[nsi])
            {
              previous_state[nsi] = csi;
              if (nsi == si)
              {
                found = true;
                break;
              }
              else
              {
                next_state[tail_si] = nsi;
                tail_si = nsi;
                next_state[tail_si] = 0;
              }
            }
          }
          if (found)
            break;
        }
      }

      if (found)
      {
        recurrent[si] = true;
        final_count++;
        if (all_infinite)
          for (nsi = head_si;nsi; nsi = next_state[nsi])
          {
            if (!recurrent[nsi])
            {
              final_count++;
              recurrent[nsi] = true;
            }
          }
        nsi = si;
        do
        {
          nsi = previous_state[nsi];
          if (!recurrent[nsi])
          {
            recurrent[nsi] = true;
            final_count++;
          }
        }
        while (nsi != si);
      }
      /* Clear results of previous_iteration */
      for (nsi = head_si;nsi; nsi = next_state[nsi])
        previous_state[nsi] = 0;
    }

  if (all_infinite)
  {
    State_Count initial_count;
    do
    {
      container.status(2,1,"Counting non-initial-only states."
                           " Found " FMT_ID " so far.\n",final_count);
      initial_count = final_count;
      for (State_ID si = 1;si < nr_states;si++)
        if (recurrent[si])
        {
          for (Transition_ID ti = 0; ti < nr_transitions;ti++)
          {
            State_ID nsi = new_state(si,ti);
            if (!is_valid_state(nsi))
              nsi = 0; /* This deals with RWS FSAs */
            if (!recurrent[nsi])
            {
              final_count++;
              recurrent[nsi] = true;
            }
          }
        }
    }
    while (final_count > initial_count);
  }
  delete [] next_state;
  delete [] previous_state;
  return final_count;
}

/**/

FSA_Simple * FSA_Factory::reverse(const FSA & fsa_start,bool create_midfa,bool labelled)
{
  State_Count nr_states = fsa_start.state_count();
  Transition_ID nr_transitions = fsa_start.alphabet_size();
  Special_Subset key(fsa_start);
  Special_Subset old_key(fsa_start);
  Special_Subset::Packer key_packer(key);
  void * packed_key = key_packer.get_buffer();
  size_t size;
  State_List ** subsets = new State_List *[nr_states*nr_transitions];
  Container & container = fsa_start.container;
  Keyed_FSA factory(container,fsa_start.base_alphabet,nr_transitions,
                    nr_states,0);
  State_Subset_Iterator ssi;

  memset(subsets,0,nr_states*nr_transitions*sizeof(State_List *));
  /* Insert the empty set */
  State_ID si;
  /* Create the subsets that give the pre-images of each transition into
     each state. This will speed up creation of the states later on.
     For good performance it is essential not to scan the states too often
     so we build all the subsets simultaneously.

     Since each transition can appear in at most one subset, we have a
     definite upper limit on the number of subsets that we are going to
     create, and also on their total size. It is possible for equal subsets
     to occur, so in theory it might save space to use a hash to "constant
     fold" the equal subsets. In practice it is likely to be the small subsets
     that are equal, so the saving will be marginal and probably would not
     outweigh the additional cost of the hash table and extra set of pointers.
  */
  for (si = 1; si < nr_states;si++)
    for (Transition_ID ti = 0; ti < nr_transitions;ti++)
    {
      container.status(2,1,"Calculating pre-images of states (" FMT_ID " of " FMT_ID " to do)\n",
                       nr_states - si,nr_states);
      State_ID nsi = fsa_start.new_state(si,ti,true);
      if (fsa_start.is_valid_state(nsi))
      {
        State_List * & subset = subsets[nsi*nr_transitions+ti];
        if (!subset)
          subset = new State_List;
        subset->append_one(si);
      }
    }

  for (si = 1; si < nr_states;si++)
  {
    container.status(2,1,"Shrinking pre-image database (" FMT_ID " of " FMT_ID " to do)\n",
                     nr_states - si,nr_states);
    for (Transition_ID ti = 0; ti < nr_transitions;ti++)
    {
      State_List * & subset = subsets[si*nr_transitions+ti];
      if (subset)
        subset->allocate(subset->count(),true);
    }
  }

  /* Now we are ready to build the FSA */
  Word_Length state_length = 0;
  /* Insert failure state */
  key.empty();
  size = key_packer.pack(key);
  factory.find_state(packed_key,size);

  /* Insert initial state. This consists of all accepting states of the
     original FSA */
  if (create_midfa)
  {
    factory.change_flags(GFF_MIDFA,GFF_DFA|GFF_NFA);
    for (si = 1; si < nr_states;si++)
    {
      if (fsa_start.is_accepting(si))
      {
        key.empty();
        key.include(si);
        size = key_packer.pack(key);
        factory.find_state(packed_key,size);
      }
    }
  }
  else
  {
    factory.change_flags(GFF_DFA,GFF_NFA|GFF_MIDFA);
    key.empty();
    for (si = 1; si < nr_states;si++)
      if (fsa_start.is_accepting(si))
        key.include(si);
    size = key_packer.pack(key);
    factory.find_state(packed_key,size);
  }
  State_ID reverse_si = 0;
  State_Count count = factory.state_count();
  State_ID ceiling = 1;
  State_Count last_initial_state = count-1;
  State_ID * transition = new State_ID[nr_transitions];

  while (++reverse_si < count)
  {
    Word_Length length = reverse_si >= ceiling ? state_length : state_length - 1;
    old_key.unpack((const Byte *) factory.get_state_key(reverse_si),factory.get_key_size(reverse_si));

    container.status(2,1,"Building reverse FSA states: (" FMT_ID " of " FMT_ID " to do)."
                    " Depth %u\n",count-reverse_si,count,length);
    for (Transition_ID ti = 0; ti < nr_transitions;ti++)
    {
      key.empty();

      for (State_ID si = ssi.first(old_key); si; si = ssi.next(old_key))
      {
        State_List * subset = subsets[si*nr_transitions+ti];
        if (subset)
        {
          State_List::Iterator sli(*subset);
          for (State_ID prefix_si = sli.last();prefix_si;prefix_si = sli.previous())
            key.include(prefix_si);
        }
      }

      if (!key.is_empty())
      {
        size = key_packer.pack(key);
        transition[ti] = factory.find_state(packed_key,size);
        if (transition[ti] >= count)
        {
          if (reverse_si >= ceiling)
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
    factory.set_transitions(reverse_si,transition);
  }

  /* Now we have finished with the subsets */
  for (si = 1; si < nr_states;si++)
    for (Transition_ID ti = 0; ti < nr_transitions;ti++)
    {
      container.status(2,1,"Purging pre-image database (" FMT_ID " of " FMT_ID " to do)\n",
                       nr_states - si,nr_states);
      State_List * & subset = subsets[si*nr_transitions+ti];
      if (subset)
        delete subset;
    }

  delete [] transition;
  delete [] subsets;
  State_ID old_initial = fsa_start.initial_state();
  Initial_Type initial_type = fsa_start.initial_type();
  factory.clear_accepting(true);
  State_List sl;
  reverse_si = 0;
  if (labelled)
  {
    factory.set_label_type(LT_List_Of_Integers);
    factory.set_nr_labels(count,LA_Direct);
  }
  while (++reverse_si < count)
  {
    container.status(2,1,"Setting accept states of reverse FSA: "
                     "(" FMT_ID " of " FMT_ID " to do).\n",count-reverse_si,count);
    old_key.unpack((const Byte *) factory.get_state_key(reverse_si),factory.get_key_size(reverse_si));
    if (labelled)
    {
      for (State_ID si = ssi.first(old_key); si; si = ssi.next(old_key))
        sl.append_one(si);
      factory.set_label_data(reverse_si,sl.packed_data());
      sl.empty();
    }
    if (initial_type == SSF_Singleton)
    {
      if (old_key.contains(old_initial))
        factory.set_is_accepting(reverse_si,true);
    }
    else
    {
      for (State_ID si = ssi.first(old_key); si; si = ssi.next(old_key))
      {
        if (fsa_start.is_initial(si))
        {
          factory.set_is_accepting(reverse_si,true);
          break;
        }
      }
    }
  }
  if (last_initial_state > 1)
  {
    for (reverse_si = 1; reverse_si <= last_initial_state;reverse_si++)
    {
      factory.set_is_initial(reverse_si,true);
      container.status(2,1,"Setting initial states of reverse FSA: "
                     "(" FMT_ID " of " FMT_ID " to do).\n",last_initial_state-reverse_si,last_initial_state);
    }
  }
  factory.remove_keys();
  factory.change_flags(GFF_BFS|GFF_ACCESSIBLE,0);
  return minimise(factory);
}

/**/

FSA_Simple * FSA_Factory::shortlex(Container & container,
                                   const Alphabet & alphabet,
                                   bool allow_interior_padding)
{
  /* There are five states:
     0 failure state - the right input is longer, or characters on the right
       have been read after a padding character and interior padding is disallowed.
     1 initial state - the input is equal so far
     2 the left input so far is greater than the right input so far
     3 the right input so far is greater than the left input so far
     4 the left input is longer.

     It is not entirely clear what the transitions in state 4 should be. Once
     a padding character has been read on the right it is the only legal
     character on the right, so we could say any other transition must be to
     the 0 failure state. On the other hand, if characters are read on both
     sides the left input is still longer than the right input, so we can
     justifying staying in state 4. If a padding character is read on the
     left once we are in state 4 we can no longer decide the order of the
     left and right words, and at any other time the left word becomes
     shorter than the RHS. So we decree that all such transitions are to 0.

     Since in MAF during diff reduction the word on the right is allowed to
     contain interior padding symbols we use the allow_interior_padding
     flag to determine what kind of transitions we see in state 4.
  */
  const Transition_ID nr_symbols = alphabet.letter_count();
  const Transition_ID nr_transitions = alphabet.product_alphabet_size();
  FSA_Simple * fsa = new FSA_Simple(container,alphabet,5,nr_transitions,TSF_Dense);
  State_ID * transition = new State_ID[nr_transitions];

  for (State_ID si = 1; si <= 4; si++)
  {
    Transition_ID ti = 0;
    for (Transition_ID g1 = 0; g1 <= nr_symbols; g1++)
      for (Transition_ID g2 = 0; g2 <= nr_symbols; g2++,ti++)
      {
        if (ti < nr_transitions)
        {
          if (g1 == nr_symbols)
            transition[ti] = 0;
          else if (g2 == nr_symbols)
            transition[ti] = 4;
          else if (si == 1)
          {
            if (g1 > g2)
              transition[ti] = 2;
            else if (g1 < g2)
              transition[ti] = 3;
            else
              transition[ti] = 1;
          }
          else if (si != 4 || allow_interior_padding)
            transition[ti] = si;
          else
            transition[ti] = 0;
        }
      }
    fsa->set_transitions(si,transition);
  }
  fsa->clear_accepting(true);
  fsa->set_is_accepting(2,true);
  fsa->set_is_accepting(4,true);
  fsa->change_flags(GFF_MINIMIZED|GFF_TRIM|GFF_ACCESSIBLE,GFF_BFS);
  delete [] transition;
  return fsa;
}

/**/

FSA_Simple * FSA_Factory::pad_language(Container & container,
                                       const Alphabet & alphabet)
{
  /* There are four states:
     0 failure state - either the left or the right word contained interior
       padding.
     1 No pad characters so far
     2 Right word is padded.
     3 Left word is padded.
  */
  const Transition_ID nr_symbols = alphabet.letter_count();
  const Transition_ID nr_transitions = alphabet.product_alphabet_size();
  FSA_Simple * fsa = new FSA_Simple(container,alphabet,4,nr_transitions,TSF_Dense);
  State_ID * transition = new State_ID[nr_transitions];

  for (State_ID si = 1; si <= 3; si++)
  {
    Transition_ID ti = 0;
    for (Transition_ID g1 = 0; g1 <= nr_symbols; g1++)
      for (Transition_ID g2 = 0; g2 <= nr_symbols; g2++,ti++)
      {
        if (ti < nr_transitions)
        {
          if (g1 == nr_symbols)
            transition[ti] = si==1 || si == 3 ? 3 : 0;
          else if (g2 == nr_symbols)
            transition[ti] = si == 1 || si == 2 ? 2 : 0;
          else
            transition[ti] = si == 1 ? 1 : 0;
        }
      }
    fsa->set_transitions(si,transition);
  }
  fsa->set_accept_all();
  fsa->change_flags(GFF_MINIMIZED|GFF_TRIM|GFF_ACCESSIBLE|GFF_BFS,0);
  delete [] transition;
  return fsa;
}

/**/

FSA_Simple * FSA_Factory::universal(Container & container,
                                    const Alphabet & alphabet)
{
  Transition_ID nr_transitions = alphabet.letter_count();
  FSA_Simple * fsa = new FSA_Simple(container,alphabet,2,nr_transitions,TSF_Dense);
  State_ID * transition = new State_ID[nr_transitions];

  for (Transition_ID ti = 0; ti < nr_transitions; ti++)
    transition[ti] = 1;
  fsa->set_transitions(1,transition);
  fsa->clear_accepting(false);
  fsa->set_is_accepting(1,true);
  delete [] transition;
  return fsa;
}

/**/

FSA_Simple * FSA_Factory::markov(Container & container,
                                 const Alphabet & alphabet)
{
  Transition_ID nr_transitions = alphabet.letter_count();
  FSA_Simple * fsa = new FSA_Simple(container,alphabet,nr_transitions+2,nr_transitions,TSF_Dense);
  State_ID * transition = new State_ID[nr_transitions];
  fsa->set_label_type(LT_Words);
  fsa->set_nr_labels(nr_transitions+2,LA_Direct);
  Ordinal_Word ow(alphabet,1);
  ow.set_length(0);
  fsa->set_label_word(1,ow);

  for (Transition_ID ti = 0; ti < nr_transitions; ti++)
  {
    transition[ti] = ti+2;
    ow.set_length(0);
    ow.append(Ordinal(ti));
    fsa->set_label_word(ti+2,ow);
  }

  for (State_ID si = 1; si < nr_transitions+2;si++)
    fsa->set_transitions(si,transition);
  fsa->set_accept_all();
  delete [] transition;
  return fsa;
}

/**/

FSA_Simple * FSA_Factory::all_words(Container & container,
                                    const Alphabet & alphabet,int max_length)
{
  Transition_ID nr_transitions = alphabet.letter_count();
  State_Count nr_states = max_length+2;
  if (max_length < 0)
  {
    container.error_output("Invalid length %d passed to all_words()\n",max_length);
    return 0;
  }
  FSA_Simple * fsa = new FSA_Simple(container,alphabet,nr_states,nr_transitions,TSF_Dense);
  State_ID * transition = new State_ID[nr_transitions];
  State_ID si;
  for (si = 1; si+1 < nr_states;si++)
  {
    for (Transition_ID ti = 0; ti <= nr_transitions; ti++)
      transition[ti] = si+1;
    fsa->set_transitions(si,transition);
  }
  for (Transition_ID ti = 0; ti <= nr_transitions; ti++)
    transition[ti] = 0;
  fsa->set_transitions(si,transition);
  fsa->set_accept_all();
  fsa->change_flags(GFF_BFS|GFF_MINIMISED|GFF_BFS|GFF_DFA|GFF_TRIM|GFF_ACCESSIBLE,0);

  delete [] transition;
  return fsa;
}

/**/

FSA_Simple * FSA_Factory::transpose(const FSA &fsa_start)
{
  /* returns an fsa that accepts(u,v) if and only if fsa_start()
     accepts(v,u) */
  Transition_ID nr_transitions = fsa_start.alphabet_size();
  const Alphabet &alphabet = fsa_start.base_alphabet;

  Ordinal nr_generators = alphabet.letter_count();
  if (nr_transitions != alphabet.product_alphabet_size())
    return 0;
  FSA_Simple &answer = *copy(fsa_start);
  State_Count nr_states = answer.state_count();
  for (State_ID si = 1; si < nr_states;si++)
  {
    State_ID * transition = answer.state_lock(si);
    for (Ordinal g1 = PADDING_SYMBOL; g1 < nr_generators;g1++)
      for (Ordinal g2 = g1+1; g2 < nr_generators;g2++)
      {
        Transition_ID ti = alphabet.product_id(g1,g2);
        Transition_ID nti = alphabet.product_id(g2,g1);
        State_ID temp = transition[nti];
        transition[nti] = transition[ti];
        transition[ti] = temp;
      }
    answer.state_unlock(si);
  }
  if (answer.get_flags() & GFF_BFS)
  {
    answer.change_flags(0,GFF_BFS);
    answer.sort_bfs();
  }
  return &answer;
}

/**/

FSA_Simple * FSA_Factory::truncate(const FSA & fsa_start,
                                   Word_Length max_definition_length,
                                   State_Count max_states,
                                   int round)
{
    /* truncate() creates an approximation to fsa_start which is correct for
       words up to the specified length. The function is only meaningful for
       word-acceptor FSAs in which all states are accept states and in which
       any subword of an accepted word is accepted.
       For words longer than the specified length, some state information may
       be lost, and the new FSA accepts a word iff the original FSA accepted
       the trailing subword of length max_definition_length.

       You can pass WHOLE_WORD for max_definition_length, in which case
       max_states must be non zero. In any case if a non-zero value is
       specified for max_states, max_definition_length is ignored. In this
       case truncate() approximates states when a state with a state number
       >= max_states would be reached. Also in this case the round parameter
       determines whether to then recalculate max_states based on the
       defining_length of that state. If round is -1 max_states is rounded
       down so that approximation begins for all states of this length.
       if round is 0 rounding begins exactly at the specified state number.
       If round is 1 rounding begins for states longer than this.
       The returned FSA is minimised so may have fewer states than you expect.
       If round is 0 or 1 it may also have more states.
    */
  const Transition_ID nr_symbols = fsa_start.alphabet_size();
  if (max_states >= fsa_start.state_count() ||
      fsa_start.accept_type() != SSF_All)
    return 0;
  if (!max_states)
  {
    if (max_definition_length == WHOLE_WORD)
      return 0;
    max_states = fsa_start.nr_accessible_states(max_definition_length);
  }
  else
    max_definition_length = WHOLE_WORD;

  if (max_definition_length == WHOLE_WORD)
  {
    max_definition_length = fsa_start.defining_length(max_states-1);
    if (round)
      max_states = fsa_start.nr_accessible_states(max_definition_length);
  }
  Container &container = fsa_start.container;
  Transition_ID * values = new Transition_ID[max_definition_length];
  Keyed_FSA factory(container,fsa_start.base_alphabet,nr_symbols,
                    max_states,sizeof(State_ID));
  State_Definition definition = {0,0}; // to shut up compiler warnings;

  State_ID si = 0;
  State_ID osi;
  State_Count count = 2;
  State_ID ceiling = 1;
  State_ID suffix_state = fsa_start.initial_state();
  Word_Length state_length = 0;

  // Insert failure and initial states
  factory.find_state(&si,sizeof(State_ID));
  si = fsa_start.initial_state();
  factory.find_state(&si,sizeof(State_ID));
  si = 0;
  State_ID * transition = new State_ID[nr_symbols];
  while (++si < count)
  {
    Word_Length length = si >= ceiling ? state_length : state_length - 1;
    container.status(2,1,"Truncating FSA at state " FMT_ID ". State " FMT_ID " "
                       "(" FMT_ID " of " FMT_ID " to do)\n",
                     max_states-1,si,count-si,count);
    factory.get_state_key(&osi,si);
    if (length == max_definition_length)
    {
      Word_Length i = 1;
      State_ID prefix_state = si;
      while (prefix_state != 1)
      {
        factory.get_definition(&definition,prefix_state);
        values[length-i++] = definition.symbol_nr;
        prefix_state = definition.state;
      }
      suffix_state = fsa_start.initial_state();
      for (i = 1; i < length;i++)
        suffix_state = fsa_start.new_state(suffix_state,values[i]);
    }

    for (Transition_ID ti = 0; ti < nr_symbols;ti++)
    {
      State_ID nsi = fsa_start.new_state(osi,ti);
      if (nsi >= max_states)
        nsi = fsa_start.new_state(suffix_state,ti);
      transition[ti] = factory.find_state(&nsi,sizeof(State_ID));
      if (transition[ti] >= count)
        count++;
    }
    factory.set_transitions(si,transition);
  }
  delete [] transition;
  delete [] values;
  factory.copy_labels(fsa_start);
  si = 0;
  while (factory.get_state_key(&osi,++si))
    factory.set_label_nr(si,fsa_start.get_label_nr(osi));
  factory.remove_keys();
  return minimise(factory);
}

/**/

FSA_Simple * FSA_Factory::unaccepted_prefix(const FSA & fsa_start)
{
  /* Returns an FSA which accepts just those words which were prefixes
     of accepted words, but not themselves accepted in the original FSA.
     This is useful if an FSA should be prefix closed but currently is
     not.*/
  FSA_Simple * fsa_temp = copy(fsa_start);
  State_Count nr_states = fsa_start.state_count();

  for (State_ID si = 1; si < nr_states;si++)
    fsa_temp->set_is_accepting(si,!fsa_temp->is_accepting(si));
  FSA_Simple * answer = minimise(*fsa_temp);
  delete fsa_temp;
  return answer;
}

/**/

FSA_Simple * FSA_Factory::star(const FSA & fsa_start)
{
  /* The star of a language L, L* is the set of all strings of 0 or more
     accepted words of the language. So the star of an FSA is the FSA
     which accepts the star of its language.
     We can find this FSA by creating a new FSA where the states of the new
     FSA are subsets of the states of the first. The initial state is the
     set consisting of the initial states of the first, but made accepting.
     In the new FSA if a state s contains elements (s0,s1,...sn) from the
     original FSA then the transition s^g goes to a state (s0^g,s1^g,...sn^g)
     unionised with the set of initial states of the first in the case where
     any of the s^g are accept states.
  */

  if (fsa_start.has_multiple_initial_states())
  {
    /*
     To make life easier for ourselves, we first determinise the input FSA
     if it is a MIDFA, as then we only have to unionise with a single element
     size when we see an accept state.
    */
    FSA_Simple * temp = determinise(fsa_start);
    FSA_Simple * answer = star(*temp);
    delete temp;
    return answer;
  }

  Accept_Type accept_type = fsa_start.accept_type();
  State_ID initial = fsa_start.initial_state();

  /* If an FSA has the property that it has one accept state, and it is
     the initial state, then the FSA is unchanged by the star operator. */
  if (accept_type == SSF_Singleton && fsa_start.is_accepting(initial))
    return copy(fsa_start);

  State_ID accepting_state = fsa_start.accepting_state();
  Container &container = fsa_start.container;
  Transition_ID nr_transitions = fsa_start.alphabet_size();
  State_Count nr_states = fsa_start.state_count();

  if (accept_type == SSF_Singleton)
  {
    /* If an FSA has the property that it has one accept state, and all
       the transations from it are to the failure state (which is not
       uncommon), then merging the initial state and the accept state
       gives the star */
    State_ID si = accepting_state;
    bool ok = true;
    for (Transition_ID ti = 0; ti < nr_transitions; ti++)
      if (fsa_start.new_state(si,ti) != 0)
      {
        ok = false;
        break;
      }
    if (ok)
    {
      State_ID * states = new State_ID[nr_states];
      for (si = 0; si < accepting_state;si++)
        states[si] = si;
      states[si] = initial;
      while (++si < nr_states)
        states[si] = si-1;
      FSA_Simple * answer = merge(fsa_start,states);
      delete [] states;
      answer->set_label_type(LT_Unlabelled);
      answer->sort_bfs();
      /* We might have an unminimised FSA here, but only if original was
         unminimised too */
      return answer;
    }
  }

  const Alphabet & alphabet = fsa_start.base_alphabet;

  /* If an FSA has the property that all the transitions from the
     initial state are accept states, then the star is the FSA that
     accepts every word in the alphabet! */
  {
    State_ID si = fsa_start.initial_state();
    bool ok = true;
    for (Transition_ID ti = 0; ti < nr_transitions; ti++)
      if (!fsa_start.is_accepting(fsa_start.new_state(si,ti)))
      {
        ok = false;
        break;
      }
    if (ok)
    {
      FSA_Simple * answer = new FSA_Simple(container,alphabet,2,
                                           nr_transitions,TSF_Dense);
      State_ID * transition = answer->state_lock(1);
      for (Transition_ID ti = 0; ti < nr_transitions;ti++)
        transition[ti] = 1;
      answer->set_transitions(1,transition);
      answer->state_unlock(1);
      answer->change_flags(GFF_MINIMISED|GFF_TRIM|GFF_ACCESSIBLE|GFF_BFS,0);
      return answer;
    }
  }

  Word_Length state_length = 0;
  State_ID ceiling = 1;

  Special_Subset key(fsa_start);
  Special_Subset old_key(fsa_start);
  Special_Subset::Packer key_packer(key);
  void * packed_key = key_packer.get_buffer();

  Keyed_FSA factory(container,alphabet,nr_transitions,nr_states,0);
  State_ID * transition = new State_ID[nr_transitions];

  /* Insert and skip failure state */
  key.empty();
  size_t size = key_packer.pack(key);
  factory.find_state(packed_key,size);
  /* Insert initial state */
  key.include(initial);
  size = key_packer.pack(key);
  factory.find_state(packed_key,size);

  State_Count count = 2;
  State_ID star_state = 0;
  State_List sl;

  if (accept_type != SSF_All)
    factory.clear_accepting(true);
  Special_Subset_Iterator ssi;

  while (++star_state < count)
  {
    Word_Length length = star_state >= ceiling ? state_length : state_length - 1;
    container.status(2,1,"Building \"star\" FSA state " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Depth %u\n",
                     star_state,count-star_state,count,length);

    old_key.unpack((const Byte *) factory.get_state_key(star_state),
                   factory.get_key_size(star_state));
    for (Transition_ID ti = 0;ti < nr_transitions;ti++)
    {
      /* Calculate action of generators on this state. To get the image, we
         have to apply g1 to each element in the subset of fsa_start
         corresponding to the state. If any of the target states is an
         accept state we have to add the initial state into the set
      */
      key.empty();
      for (State_ID si = ssi.first(old_key); si; si = ssi.next(old_key))
      {
        State_ID new_state = fsa_start.new_state(si,ti,false);
        if (!new_state)
          continue;
        key.include(new_state);
        if (fsa_start.is_accepting(new_state))
          key.include(initial);
      }
      size = key_packer.pack(key);
      transition[ti] = factory.find_state(packed_key,size);
      if (transition[ti] >= count)
      {
        if (star_state >= ceiling)
        {
          state_length++;
          ceiling = transition[ti];
        }
        count++;
      }
    }
    factory.set_transitions(star_state,transition);
  }
  if (accept_type != SSF_All)
  {
    star_state = 0;
    while (++star_state < count)
    {
      old_key.unpack((const Byte *) factory.get_state_key(star_state),
                     factory.get_key_size(star_state));
      container.status(2,1,"Setting accept states for \"star\" FSA state (" FMT_ID " of " FMT_ID " to do).\n",
                       count-star_state,count);
      char accepts = 0;
      {
        if (accept_type != SSF_Singleton)
        {
          for (State_ID si = ssi.first(old_key) ; si; si = ssi.next(old_key))
          {
            if (fsa_start.is_accepting(si))
            {
              accepts = 1;
              break;
            }
          }
        }
        else
          accepts = old_key.contains(accepting_state);
      }
      if (accepts)
        factory.set_is_accepting(star_state,true);
    }
    factory.set_is_accepting(1,true);
  }
  factory.remove_keys();
  delete [] transition;
  factory.change_flags(GFF_BFS|GFF_ACCESSIBLE,0);
  return minimise(factory);
}

FSA_Simple * FSA_Factory::concat(const FSA & fsa_0,const FSA & fsa_1)
{
  /* the concat of two FSAs on the same alphabet is the FSA which accepts all
     words which consist of an accepted word of the first FSA's language
     followed by an accepted word of the second FSA's language. The code is
     generally similar to that for star, but now the states of the new FSA
     are subsets of the union of the states of the starting FSAs. The initial
     state is the union of all the initial states of the first. The target of
     a transition is again the union of all the sn^g, where sn denotes one of
     the elements from the states of the original FSAs present in the starting
     state. When one of the sn^g is an accept state of the first FSA we
     unionise with the initial states of the second FSA.
     One minor complication is that we have to renumber all the states
     of the second FSA upwards to avoid confusion with the first FSA. */

  Transition_ID nr_transitions = fsa_0.alphabet_size();
  if (fsa_1.alphabet_size() != nr_transitions ||
      fsa_1.base_alphabet != fsa_0.base_alphabet)
    return 0;

  if (fsa_1.has_multiple_initial_states())
  {
    /* To make life easier we first determinise the second FSA if need be*/
    FSA_Simple * temp = determinise(fsa_1);
    FSA_Simple * answer = concat(fsa_0,*temp);
    delete temp;
    return answer;
  }

  Container &container = fsa_0.container;
  State_Count nr_states_0 = fsa_0.state_count();
  State_Count nr_states_1 = fsa_1.state_count();
  State_Count nr_states = nr_states_0 + nr_states_1;
  State_ID initial_1 = fsa_1.initial_state() + nr_states_0;
  const Alphabet & alphabet = fsa_0.base_alphabet;
  Word_Length state_length = 0;
  State_ID ceiling = 1;
  Simple_Finite_Set state_set(container,nr_states);
  Special_Subset key(state_set);
  Special_Subset old_key(state_set);
  Special_Subset::Packer key_packer(key);
  void * packed_key = key_packer.get_buffer();

  Keyed_FSA factory(container,alphabet,nr_transitions,nr_states,0);
  State_ID * transition = new State_ID[nr_transitions];

  /* Insert and skip failure state */
  key.empty();
  size_t size = key_packer.pack(key);
  factory.find_state(packed_key,size);
  /* Insert initial state - we haven't ruled out a MIDFA here */
  State_Subset_Iterator ssi;
  for (State_ID si = fsa_0.initial_state(ssi,true);
       si;si = fsa_0.initial_state(ssi,false))
  {
    key.include(si);
    if (fsa_0.is_accepting(si))
      key.include(initial_1);
  }
  size = key_packer.pack(key);
  factory.find_state(packed_key,size);

  State_Count count = 2;
  State_ID concat_state = 0;

  factory.clear_accepting(true);
  while (++concat_state < count)
  {
    Word_Length length = concat_state >= ceiling ? state_length : state_length - 1;
    container.status(2,1,"Building \"concat\" FSA state " FMT_ID " (" FMT_ID " of " FMT_ID " to do). Depth %u\n",
                     concat_state,count-concat_state,count,length);

    old_key.unpack((const Byte *) factory.get_state_key(concat_state),
                   factory.get_key_size(concat_state));
    for (Transition_ID ti = 0;ti < nr_transitions;ti++)
    {
      /* Calculate action of generators on this state. To get the image, we
         have to apply g1 to each element in the subset of fsa_0 U fsa_1
         corresponding to the state. If any of the target states is an
         accept state of fsa_0 we have to add the initial state of fsa_1
         into the set
      */
      key.empty();
      for (State_ID si = ssi.first(old_key); si; si = ssi.next(old_key))
      {
        State_ID new_state;
        if (si < nr_states_0)
        {
          new_state = fsa_0.new_state(si,ti,false);
          if (!fsa_0.is_valid_state(new_state))
            continue;
          if (fsa_0.is_accepting(new_state))
            key.include(initial_1);
        }
        else
        {
          new_state = fsa_1.new_state(si-nr_states_0,ti,false);
          if (!fsa_1.is_valid_state(new_state))
            continue;
          new_state += nr_states_0;
        }
        key.include(new_state);
      }
      size = key_packer.pack(key);
      transition[ti] = factory.find_state(packed_key,size);
      if (transition[ti] >= count)
      {
        if (concat_state >= ceiling)
        {
          state_length++;
          ceiling = transition[ti];
        }
        count++;
      }
    }
    factory.set_transitions(concat_state,transition);
  }
  /* The accept type is never going to be SSF_All a priori */
  {
    concat_state = 0;
    State_ID accepting_state = fsa_1.accepting_state() + nr_states_0;
    Accept_Type accept_type = fsa_1.accept_type();

    while (++concat_state < count)
    {
      if (!(char) concat_state)
        container.status(2,1,"Setting accept states for \"concat\" FSA state (" FMT_ID " of " FMT_ID " to do).\n",
                         count-concat_state,count);
      char accepts = 0;
      {
        old_key.unpack((const Byte *) factory.get_state_key(concat_state),
                       factory.get_key_size(concat_state));
        if (accept_type != SSF_Singleton)
        {
          for (State_ID si = fsa_1.accepting_state(ssi,true); si;
               si = fsa_1.accepting_state(ssi,false))
            if (old_key.contains(si+nr_states_0))
            {
              accepts = 1;
              break;
            }
        }
        else
          accepts = old_key.contains(accepting_state);
      }
      if (accepts)
        factory.set_is_accepting(concat_state,true);
    }
  }
  factory.remove_keys();
  delete [] transition;
  factory.change_flags(GFF_BFS|GFF_ACCESSIBLE,0);
  return minimise(factory);
}

/**/

FSA_Simple * FSA_Factory::cut(const FSA & fsa_original)
{
  /* Make the mininimum number of changes to an FSA necessary to make its
     accepted language finite.
     The method works by building up a list of states back to which there is
     no transition, starting from the initial states (to which any transitions
     are removed).  Whenever we get stuck we find the earliest recurrent state
     and replace the offending transitions by transitions to the failure state
  */

  FSA_Simple & fsa_work = * (fsa_original.get_flags() & GFF_MINIMISED ?
                           copy(fsa_original) : minimise(fsa_original));

  if (!(fsa_work.get_flags() & GFF_BFS))
    fsa_work.sort_bfs();

  Transition_ID nr_transitions = fsa_work.alphabet_size();
  State_Count nr_states = fsa_work.state_count();
  State_ID si,ns;
  Transition_ID ti;
  Container & container = fsa_work.container;
  bool * accessible = new bool[nr_states];

  /* We first count the number of transitions into each state */
  State_Count * in_degree = new State_Count[nr_states];
  State_ID * ordered_state_list = new State_ID[nr_states];
  State_Count count = 1;
  State_ID best_state = 1;

  ordered_state_list[0] = 0;
  for (si = 0; si < nr_states;si++)
    in_degree[si] = 0;
  for (si = 1;si < nr_states;si++)
  {
    container.status(2,1,"Counting transitions. (" FMT_ID " of " FMT_ID " done)\n",count,nr_states);
    for (ti = 0; ti < nr_transitions;ti++)
    {
      ns = fsa_work.new_state(si,ti);
      if (fsa_work.is_valid_state(ns))
        in_degree[ns]++;
    }
  }

  for (si = 1;si < nr_states;si++)
    if (!in_degree[si])
      ordered_state_list[count++] = si;

  /* Now we build up our list of non-recurring states */
  State_Count i = 1;
  unsigned long changes = 0;
  while (count < nr_states)
  {
    for (;i < count ;i++)
    {
      container.status(2,1,"Removing recurrences. State " FMT_ID ". (" FMT_ID
                           " of " FMT_ID " states done). %ld changes.\n",best_state,count,
                       nr_states,changes);
      si = ordered_state_list[i];
      for (ti = 0;ti < nr_transitions;ti++)
      {
        ns = fsa_work.new_state(si,ti);
        if (fsa_work.is_valid_state(ns))
        {
          in_degree[ns]--;
          if (in_degree[ns]==0)
            ordered_state_list[count++] = ns;
        }
      }
    }
    if (count < nr_states)
    {
      for (; best_state < nr_states;best_state++)
        if (in_degree[best_state] != 0)
        {
          State_List sl;
          State_List::Iterator sli(sl);

          sl.reserve(nr_states,false);
          memset(accessible,0,nr_states);
          sl.append_one(best_state);
          bool found = false;
          for (State_ID si = sli.first();si && !found;si = sli.next())
          {
            container.status(2,1,"Removing recurrences. State " FMT_ID ". ("
                                 FMT_ID " of " FMT_ID " states done). %ld changes.\n",
                             best_state,count,nr_states,changes);
            for (Transition_ID ti = 0; ti < nr_transitions;ti++)
            {
              ns = fsa_work.new_state(si,ti);
              if (ns == best_state)
              {
                fsa_work.set_transition(si,ti,0);
                changes++;
                if (!--in_degree[ns])
                {
                  found = true;
                  ordered_state_list[count++] = ns;
                  break;
                }
              }
              else if (fsa_work.is_valid_state(ns) && !accessible[ns])
              {
                sl.append_one(ns);
                accessible[ns] = true;
              }
            }
          }
          if (found)
            break;
        }
    }
  }
  delete [] accessible;
  delete [] in_degree;
  delete [] ordered_state_list;
  FSA_Simple * answer = minimise(fsa_work);
  delete & fsa_work;
  return answer;
}

/**/

FSA_Simple * FSA_Factory::restriction(const FSA & fsa_start,const Alphabet & new_alphabet)
{
 /* restriction() returns an FSA based on that part of a starting
    FSA which is common between the original alphabet and the new
    alphabet. The translation between the old and new alphabet is
    based on string comparison of the glyphs.
    No attempt is made to translate the labels if there be any.
    They are copied unchanged and keep their old alphabet. */

  Container & container = fsa_start.container;
  Ordinal nr_letters = new_alphabet.letter_count();
  Transition_ID nr_transitions = fsa_start.is_product_fsa() ?
                                 new_alphabet.product_alphabet_size() : nr_letters;
  Ordinal * xlat_ordinal = new Ordinal[nr_letters+1];
  const Alphabet & old_alphabet = fsa_start.base_alphabet;
  Ordinal old_nr_letters = old_alphabet.letter_count();
  Ordinal ng,og; // new and old ordinals
  // construct translation between old and new alphabets
  for (ng = 0; ng < nr_letters;ng++)
  {
    xlat_ordinal[ng] = -1;
    for (og = 0; og < old_nr_letters;og++)
      if (strcmp(new_alphabet.glyph(ng),old_alphabet.glyph(og))==0)
      {
        xlat_ordinal[ng] = og;
        break;
      }
  }
  // and padding character
  xlat_ordinal[nr_letters] = old_nr_letters;

  // now construct translation between Transition_IDs
  Transition_ID * xlat_transition = new Transition_ID[nr_transitions];
  Transition_ID nti,oti; // new and old transition numbers
  if (nr_transitions == nr_letters)
    for (ng = 0; ng < nr_letters;ng++)
      xlat_transition[ng] = xlat_ordinal[ng];
  else
  {
    for (Ordinal g1 = 0;g1 <= nr_letters; g1++)
      for (Ordinal g2 = 0;g2 <= nr_letters; g2++)
      {
        Transition_ID nti = new_alphabet.product_id(g1,g2);
        Ordinal og1 = xlat_ordinal[g1];
        Ordinal og2 = xlat_ordinal[g2];
        if (nti < nr_transitions)
          if (og1 >= 0 && og2 >= 0)
            xlat_transition[nti] = old_alphabet.product_id(og1,og2);
          else
            xlat_transition[nti] = -1;
      }
  }

  State_Count nr_states = fsa_start.state_count();
  FSA_Simple * temp_fsa = new FSA_Simple(container,new_alphabet,nr_states,
                                         nr_transitions);
  // now create all the states
  for (State_ID state = 1;state < nr_states;state++)
  {
    State_ID * trow = temp_fsa->state_lock(state);
    for (nti = 0; nti < nr_transitions;nti++)
    {
      oti = xlat_transition[nti];
      if (oti >=0)
        trow[nti] = fsa_start.new_state(state,oti);
      else
        trow[nti] = 0;
    }
    temp_fsa->state_unlock(state);
    temp_fsa->set_is_initial(state,fsa_start.is_initial(state));
    temp_fsa->set_is_accepting(state,fsa_start.is_accepting(state));
  }
  temp_fsa->copy_labels(fsa_start);
  temp_fsa->change_flags(fsa_start.get_flags(),0);
  temp_fsa->change_flags(0,GFF_TRIM|GFF_ACCESSIBLE|GFF_MINIMISED);
  FSA_Simple * answer = minimise(*temp_fsa);
  delete temp_fsa;
  answer->sort_bfs();
  answer->sort_labels();
  delete [] xlat_ordinal;
  delete [] xlat_transition;
  return answer;
}

/**/

FSA_Simple * FSA_Factory::finite_language(const Word_Collection & words,bool labelled)
{
  const Alphabet & alphabet = words.alphabet;
  Container & container = alphabet.container;
  const Transition_ID nr_symbols = alphabet.letter_count();
  Element_Count count = words.count();
  Ordinal_Word ow(alphabet);
  Word_DB wdb(alphabet,count);
  Element_ID word_nr;

  /* first build a database containing all the prefixes and full words
     we need to recognise */
  for (word_nr = 0; word_nr < count;word_nr++)
  {
    if (words.get(&ow,word_nr))
    {
      Word_Length l = ow.length();
      for (Word_Length i = 0; i <= l ;i++)
        wdb.enter(Subword(ow,0,i));
    }
  }
  /* next we build the transitions */
  FSA_Simple * temp = new FSA_Simple(container,alphabet,wdb.count()+1,nr_symbols);
  count = wdb.count();
  for (word_nr = 0; word_nr < count;word_nr++)
  {
    if (wdb.get(&ow,word_nr))
    {
      Word_Length l = ow.length();
      State_ID si = word_nr+1;
      State_ID * transition = temp->state_lock(si);
      for (Ordinal g = 0; g< nr_symbols;g++)
      {
        ow.set_length(l);
        ow.append(g);
        Element_ID new_word_nr;
        if (wdb.find(ow,&new_word_nr))
          transition[g] = new_word_nr+1;
      }
      temp->state_unlock(si);
    }
  }
  /* next we set the accept states */
  count = words.count();
  temp->clear_accepting(true);
  if (labelled)
  {
    temp->set_label_type(LT_Words);
    temp->set_nr_labels(count+1);
    temp->set_label_alphabet(alphabet);
  }

  for (word_nr = 0; word_nr < count;word_nr++)
  {
    if (words.get(&ow,word_nr))
    {
      Element_ID whole_word_nr;
      wdb.find(ow,&whole_word_nr);

      temp->set_is_accepting(whole_word_nr+1,true);
      if (labelled)
      {
        temp->set_label_word(word_nr+1,ow);
        temp->set_label_nr(whole_word_nr+1,word_nr+1);
      }
    }
  }
  temp->change_flags(GFF_TRIM,GFF_BFS);
  if (labelled)
  {
    temp->sort_bfs();
    return temp;
  }
  FSA_Simple * answer = minimise(*temp);
  delete temp;
  return answer;
}

/**/

#if 0

/*
I had hoped the code below would make it possible to do Felsch style
coset enumeration more quickly, but have decided against using it for now.
*/

FSA_Simple * FSA_Factory::two_way_scanner(const Word_Collection & wc,bool labelled)
{
  /* Construct a two-variable FSA whose accepted language consists of
     all word pairs (u,v) such that u*rev(v) is in wc where
     rev(v) is the reversed word for v */

  const Alphabet & alphabet = wc.alphabet;
  Container & container = alphabet.container;
  const Transition_ID nr_generators = alphabet.letter_count();
  Element_Count word_count = wc.count();
  /* We use a state pair list for the state key.
     The list contains a pair for each word we can still potentially match
     with the first State_ID being the Element_ID of the word within the
     list, and the second number being the end of the subword that has
     not been matched. There is one additional pair with the first State_ID
     being word_count (so it comes first), and the second number the
     start of the subword that has not been matched (which is the same
     for all words) */

  Transition_ID nr_transitions = alphabet.product_alphabet_size();
  State_ID *transition = new State_ID[nr_transitions];
  Keyed_FSA factory(container,alphabet,nr_transitions,0,0);
  // key stuff
  bool dense = word_count <= USHRT_MAX && MAX_WORD <= USHRT_MAX;
  State_Pair_List key,old_key;
  Byte_Buffer packed_key;
  const void * old_packed_key;
  size_t key_size;

  Ordinal_Word ow(alphabet);
  Element_ID word_nr;

  /* Insert failure state */
  key.empty();
  packed_key = key.packed_data(&key_size);
  factory.find_state(packed_key,key_size);

  /* Insert initial state */
  key.reserve(word_count+1,false);
  old_key.reserve(word_count+1,false);
  for (word_nr = 0; word_nr < word_count;word_nr++)
    if (wc.get(&ow,word_nr))
      key.insert(word_nr,ow.length(),dense);
  key.insert(word_count,0,dense);
  packed_key = key.packed_data(&key_size);
  factory.find_state(packed_key,key_size);

  State_Count nr_states = 2;
  Word_Length state_length = 0;
  State_ID scanner_state = 0;
  State_ID ceiling = 1;
  while ((old_packed_key = factory.get_state_key(++scanner_state))!=0)
  {
    if (!(char) scanner_state)
    {
      Word_Length length = scanner_state >= ceiling ? state_length : state_length - 1;
      container.status(2,1,"Constructing two way scanner FSA: State " FMT_ID ". (" FMT_ID " to do). Length %d\n",
                       scanner_state,nr_states-scanner_state,length);
    }

    old_key.unpack((const Byte *) old_packed_key);
    State_Pair_List::Iterator old_key_pairs(old_key);
    State_ID old_pair[2];
    State_ID old_start,old_end,start,end;

    bool left_padded = old_key.is_left_padded();
    bool right_padded = old_key.is_right_padded();
    old_key_pairs.last(old_pair);
    old_start = old_pair[1];
    Transition_ID product_id = 0;
    for (Ordinal g1 = 0; g1 <= nr_generators;g1++)
      for (Ordinal g2 = 0; g2 <= nr_generators;g2++,product_id++)
      {
        if (product_id >= nr_transitions)
          continue;

        transition[product_id] = 0;

        if (left_padded && g1 != nr_generators)
          continue;

        if (right_padded && g2 != nr_generators)
          continue;

        bool found = false;
        start = old_start;

        key.empty();
        if (g1 == nr_generators)
          key.set_left_padded();
        else
          start++;
        if (g2 == nr_generators)
          key.set_right_padded();
        /* To get the transition target we have to create the key
           of the new state by iterating over all the state pairs in
           the current state, for each possible middle generator */
        if (old_key_pairs.first(old_pair))
        {
          do
          {
            word_nr = old_pair[0];
            end = old_end = old_pair[1];
            wc.get(&ow,word_nr);
            if (g1 != nr_generators &&
                (old_start >= old_end || ow.value(old_start) != g1))
              continue;
            if (g2 != nr_generators)
              if (end == start || g2 != ow.value(--end))
                continue;
            found = true;
            key.insert(word_nr,end,dense);
          }
          while (old_key_pairs.next(old_pair) && old_pair[0] < word_count);
        }
        if (found)
        {
          key.insert(word_count,start,dense);
          packed_key = key.packed_data(&key_size);
          transition[product_id] = factory.find_state(packed_key,key_size);
        }
        if (transition[product_id] >= nr_states)
        {
          if (scanner_state >= ceiling)
          {
            state_length++;
            ceiling = transition[product_id];
          }
          nr_states++;
        }
      }
    factory.set_transitions(scanner_state,transition);
  }
  delete [] transition;

  scanner_state = 0;
  factory.clear_accepting(true);
  Hash label_hash(nr_states,0);
  if (labelled)
    factory.set_label_type(LT_List_Of_Integers);
  while ((old_packed_key = factory.get_state_key(++scanner_state))!=0)
  {
    if (!(char) scanner_state)
      container.status(2,1,"Labelling two way scanner FSA: State " FMT_ID ". (" FMT_ID " to do).\n",
                       scanner_state,nr_states-scanner_state);

    old_key.unpack((const Byte *) old_packed_key);
    State_Pair_List::Iterator old_key_pairs(old_key);
    State_ID old_pair[2];
    old_key_pairs.last(old_pair);
    State_ID old_start = old_pair[1];
    Element_List el;

    if (old_key_pairs.first(old_pair))
    {
      do
      {
        if (old_pair[1] == old_start)
          el.append_one(old_pair[0]);
      }
      while (old_key_pairs.next(old_pair) && old_pair[0] < word_count);
    }
    if (el.count())
    {
      Packed_Element_List pel(el);
      factory.set_is_accepting(scanner_state,true);
      if (labelled)
      {
        Label_ID label_nr;
        int inserted = label_hash.bb_insert(pel,pel.size(),false,&label_nr);
        factory.set_label_nr(scanner_state,label_nr,true);
        if (inserted)
          factory.set_label_data(label_nr,pel.take());
      }
    }
  }
  factory.tidy();
  factory.remove_keys();
  return FSA_Factory::minimise(factory);
}

#endif

/**/

FSA_Simple * FSA_Factory::determinise_multiplier(const FSA & fsa_start)
{
  /* Create a determinised version of a coset system general multiplier */

  /* We need to remove any labels from initial states that are not accept
     states */
  FSA_Simple * adjusted = copy(fsa_start);
  State_Subset_Iterator ssi;
  for (State_ID state = adjusted->initial_state(ssi,true);
         state;
         state = adjusted->initial_state(ssi,false))
    if (!adjusted->is_accepting(state))
      adjusted->set_label_nr(state,0);

  FSA_Simple * answer = determinise(*adjusted,FSA_Factory::DF_All,true);
  delete adjusted;
  Word_List wl(fsa_start.label_alphabet());
  Word_List new_wl(fsa_start.label_alphabet());
  Ordinal_Word word(fsa_start.label_alphabet());

  Ordinal nr_generators = fsa_start.base_alphabet.letter_count();
  for (Label_ID label = 1; label < answer->label_count();label++)
  {
    answer->label_word_list(&wl,label);
    Element_Count count = wl.count();
    new_wl.empty();
    for (Element_ID word_nr = 0;word_nr < count;word_nr++)
    {
      wl.get(&word,word_nr);
      if (word.length()==0 || word.value(0) < nr_generators)
        new_wl.add(word);
    }
    answer->set_label_word_list(label,new_wl);
  }
  answer->set_label_alphabet(fsa_start.base_alphabet);
  return answer;
}

/**/

FSA_Simple * FSA_Factory::overlap_language(const FSA & L1_acceptor)
{
  /* Builds the FSA which accepts words which have a minimally reducible
     prefix and a minimally reducible trailing subword which overlap
  */

  const Ordinal nr_generators = Ordinal(L1_acceptor.alphabet_size());
  Word_Length state_length = 0;
  Container & container = L1_acceptor.container;
  Keyed_FSA factory(container,L1_acceptor.base_alphabet,nr_generators,
                    SUGGESTED_HASH_SIZE,0);
  State_List old_key;
  State_List key;
  State_ID accept_state = L1_acceptor.accepting_state();

  /* Insert failure and initial states */
  key.append_one(0);
  factory.find_state(key.buffer(),key.size());
  key.empty();
  key.append_one(L1_acceptor.initial_state());
  key.append_one(0);
  factory.find_state(key.buffer(),key.size());

  State_ID overlap_si = 0;
  State_Count count = factory.state_count();
  State_ID ceiling = 1;
  State_ID * transition = new State_ID[nr_generators];
  old_key.reserve(key.count(),false);
  while (factory.get_state_key(old_key.buffer(),++overlap_si))
  {
    Word_Length length = overlap_si >= ceiling ? state_length : state_length - 1;
    container.status(2,1,"Building overlap acceptor states: (" FMT_ID " of " FMT_ID " to do)."
                    " State Length %d\n",count-overlap_si,count,length);
    for (Ordinal g = 0; g < nr_generators;g++)
    {
      key.empty();
      key.insert(-1); // place holder for left state - we will put left state
                      // here when all the right states are done
      State_ID left_si = *old_key.buffer();
      State_ID nleft_si = left_si == accept_state ? accept_state : L1_acceptor.new_state(left_si,g,false);
      if (!nleft_si)
        transition[g] = 0;
      else
      {
        if (nleft_si != accept_state)
          key.insert(1);
        for (State_ID *si = old_key.buffer()+1 ; *si; si++)
        {
          State_ID nsi = L1_acceptor.new_state(*si,g,false);
          if (nsi == accept_state)
          {
            /* since some suffix is reducible we must already have
               reached the accept state of L1 in a prefix because
               otherwise we would just have reached the failure state. */
            key.empty();
            key.append_one(accept_state);
            key.append_one(accept_state);
            break;
          }
          else if (nsi > 0)
            key.insert(nsi);
        }
        key.append_one(0);
        *key.buffer() = nleft_si;
        transition[g] = factory.find_state(key.buffer(),key.size());
        if (transition[g] >= count)
        {
          old_key.reserve(key.count(),true);
          if (overlap_si >= ceiling)
          {
            state_length++;
            ceiling = transition[g];
          }
          count++;
        }
      }
    }
    factory.set_transitions(overlap_si,transition);
  }
  delete [] transition;

  key.empty();
  key.append_one(accept_state);
  key.append_one(accept_state);
  key.append_one(0);
  factory.set_single_accepting(factory.find_state(key.buffer(),key.size()));
  factory.remove_keys();

  factory.change_flags(GFF_BFS|GFF_ACCESSIBLE,0);
  return FSA_Factory::minimise(factory);
}

/**/

FSA_Simple * FSA_Factory::separate(const FSA & fsa_original)
{
  /* Builds an fsa which accepts the same language as the original
     but in which each state is only reached by one generator */
  Container & container = fsa_original.container;
  const Transition_ID nr_symbols = fsa_original.alphabet_size();

  State_ID key[2],old_key[2];
  State_ID ceiling = 1;
  State_Count count = 2;
  Word_Length state_length = 0;
  key[0] = fsa_original.state_count();
  key[1] = nr_symbols+1;
  Pair_Packer key_packer(key);
  State_ID *transition = new State_ID[nr_symbols];
  Keyed_FSA factory(container,fsa_original.base_alphabet,nr_symbols,
                    fsa_original.state_count(),
                    key_packer.key_size());
  void * key_area;

  /* Insert failure and initial states */
  key[0] = 0;
  key[1] = 0;
  factory.find_state(key_area = key_packer.pack_key(key));

  Special_Subset_Iterator ssi;
  for (State_ID si = fsa_original.initial_state(ssi,true);si;
       si = fsa_original.initial_state(ssi,false))
  {
    key[0] = si;
    key[1] = 0;
    factory.find_state(key_area = key_packer.pack_key(key));
  }

  State_ID nsi = 0;
  while (factory.get_state_key(key_area,++nsi))
  {
    key_packer.unpack_key(old_key);
    if (!(char) nsi)
    {
      Word_Length length = nsi >= ceiling ? state_length : state_length - 1;
      container.status(2,1,"Separating FSA: State " FMT_ID ". (" FMT_ID " to do). Length %d\n",
                       nsi,factory.state_count()-nsi,length);
    }

    for (Transition_ID i = 0; i < nr_symbols;i++)
    {
      key[0] = fsa_original.new_state(old_key[0],i);
      key[1] = i+1;
      transition[i] = factory.find_state(key_packer.pack_key(key));
      if (transition[i] >= count)
      {
        count++;
        if (nsi >= ceiling)
        {
          state_length++;
          ceiling = transition[i];
        }
      }
    }
    factory.set_transitions(nsi,transition);
  }
  bool new_labels = false;
  if (fsa_original.label_type() != LT_Unlabelled)
    factory.copy_labels(fsa_original,LA_Mapped);
  else if (!fsa_original.is_product_fsa())
  {
    new_labels = true;
    factory.set_label_type(LT_Words);
    Ordinal_Word ow(fsa_original.base_alphabet);

    for (Ordinal i = 0; i < nr_symbols;i++)
    {
      ow.set_length(0);
      ow.append(i);
      factory.set_label_word(i+1,ow);
    }
  }
  nsi = 0;
  factory.clear_accepting(true);
  while (factory.get_state_key(key_area,++nsi))
  {
    key_packer.unpack_key(key);
    bool accepts = fsa_original.is_accepting(key[0]);
    if (accepts)
      factory.set_is_accepting(nsi,true);
    if (new_labels)
      factory.set_label_nr(nsi,key[1]);
    else
      factory.set_label_nr(nsi,fsa_original.get_label_nr(key[0]));
  }
  if (transition)
    delete [] transition;
  factory.remove_keys();
  return trim(factory);
}
