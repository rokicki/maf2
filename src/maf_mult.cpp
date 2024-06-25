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


// $Log: maf_mult.cpp $
// Revision 1.20  2011/06/11 12:03:20Z  Alun
// detabbed!
// Revision 1.19  2011/06/02 14:40:17Z  Alun
// subgroup_relators did not work if the -schreier option was selected or a
// Serial_Compositor object was used to build the relator multipliers
// Revision 1.18  2011/05/20 10:17:00Z  Alun
// detabbed
// Revision 1.17  2010/06/29 11:50:16Z  Alun
// subgroups relators computation went wrong if some main generators were trivial
// Revision 1.16  2010/06/11 10:20:00Z  Alun
// Code marked previously marked as experimental no longer is
// Revision 1.15  2010/06/10 17:15:13Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.14  2010/06/10 13:57:38Z  Alun
// All tabs removed again
// Revision 1.13  2010/06/07 21:53:33Z  Alun
// Composite construction now always done in "axiom checking" mode.
// Wrong alphabet was used for unpacking labels in some places which caused
// problems with subgroup presentations sometimes.
// Revision 1.12  2009/11/08 22:27:22Z  Alun
// type of local variable changed
// Revision 1.11  2009/10/12 17:33:19Z  Alun
// typo in comment corrected
// Revision 1.10  2009/09/13 18:54:53Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Language_Size data type introduced and used where appropriate
// Revision 1.9  2009/08/23 19:37:31Z  Alun
// stdio.h removed
// Revision 1.8  2009/05/06 15:09:32Z  Alun
// Axiom checking code now uses FSA::compare(). Experimental code added which
// adds extra relators for Schreier generators
// Revision 1.8  2008/11/03 18:33:55Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.7  2008/10/10 07:17:43Z  Alun
// Various coset system bugs fixed

/* This module contains code for using a multiplier to perform multiplication,
   for forming composite multipliers, and for checking a multiplier really is
   correct.

   In MAF a multiplier is any 2-variable FSA with labels that are lists of
   words which accepts word pairs (u,v) with a label that contains the word w
   if uw=v, or, in the coset case, if uw=hv where h is the label of the
   initial state.

   Each Multiplier is equipped with a Word_List, listing the words that
   appear in the accept states.

   If a multiplier accepts (u,v) with label x and (v,w) with label y,then in
   the composite accepts (u,w). Since ux=v and vy=w, uxy=w. Hence the
   accept states of a composite multiplier should be labelled by multiplying
   the labels of the original, and we can find the multiplier for any word
   by repeated composition. But since states in a composite can arise from
   more than one pair of states in the original FSA, we may need to supply
   multiple labels. In fact all the words that appear in a label are equal
   unless we are dealing with a determinised coset multiplier.

   There are two different ways we can perform composition to build up
   a multiplier for a long word.

      We can keep composing one big multiplier with itself retaining as
      labelled accept states only the words we are interested in (so
      that some accept states from the composite cease to be accept states).
      In this case there is little point worrying about trying to split
      the words up efficiently, because we will be calculating
      all possible composites at each step anyway.

      We can extract individual multipliers and perform composition
      operations one at a time. In this case it is worth trying to
      minimise the number of operations by looking for common words.
      However, this is by no means easy to do, and we only make some
      simple optimisations.

      Which of these is faster for a particular group can only be found out
      by trying both.

      The code for forming composites used to operate in two modes
      "axiom checking mode" where composites were formed properly, and another
      mode in which the word(s) describing the requested multiplier(s) were
      first reduced using the general multiplier for the group.
      This mode has been removed, and all composites are now formed "as is",
      and no attempt is made to omit any of the processing needed to label
      the states properly. In the former mode of operation it was not possible
      to form composite coset multipliers properly unless the automatic structure
      for the group had been computed.
*/

#include "mafword.h"
#include "keyedfsa.h"
#include "maf.h"
#include "container.h"
#include "maf_spl.h"
#include "maf_wdb.h"
#include "maf_el.h"
#include "maf_ss.h"
#include "equation.h"

/* Multiplier::Node and Multiplier::Node_List are used in performing
   multiplication*/

struct Multiplier::Node
{
  size_t prefix;
  State_ID multiplier_si;
  Ordinal rvalue;
};

struct Multiplier::Node_List
{
  State_Count nr_allocated;
  State_Count nr_nodes;
  Ordinal lvalue;
  Node * node;
  Node_List() :
    nr_allocated(0),
    nr_nodes(0),
    lvalue(INVALID_SYMBOL),
    node(0)
  {}
  void add_state(size_t prefix,Ordinal rvalue,State_ID multiplier_si,
                 size_t max_states)
  {
    if (nr_nodes == nr_allocated)
    {
      nr_allocated = max_states;
      Node * new_node = new Node[nr_allocated];
      for (State_Count k = 0; k < nr_nodes;k++)
        new_node[k] = node[k];
      if (node)
        delete [] node;
      node = new_node;
    }
    node[nr_nodes].prefix = prefix;
    node[nr_nodes].rvalue = rvalue;
    node[nr_nodes].multiplier_si = multiplier_si;
    nr_nodes++;
  }
};

/**/

Multiplier::Multiplier(const Multiplier &other) :
  Delegated_FSA(other.container,other.base_alphabet),
  containing_label(0),
  multipliers(*new Sorted_Word_List(other.base_alphabet)),
  max_depth(0),
  stack(0),
  valid_length(0)
{
  fsa__ = FSA_Factory::copy(*other.fsa());
  set_multipliers();
}

/**/

Multiplier::Multiplier(FSA &other,bool owner_) :
  Delegated_FSA(other.container,other.base_alphabet),
  multipliers(* new Sorted_Word_List(other.base_alphabet)),
  containing_label(0),
  max_depth(0),
  stack(0),
  valid_length(0)
{
  owner = owner_;

  // Try to rule out as many FSAs as possible that are not multipliers
  if (other.alphabet_size() != base_alphabet.product_alphabet_size() ||
      !other.labels_are_words() ||
      other.get_flags() & GFF_RWS )
  {
    if (owner)
      delete &other;
  }
  else
  {
    fsa__ = &other;
    set_multipliers();
  }
}

/**/

bool Multiplier::is_multiplier(const Word & word,Element_ID * multiplier_nr) const
{
  /* Check a word is one of the supported multipliers, and returning its
     index in the list of multipliers */
  return multipliers.find(word,multiplier_nr);
}

const Word * Multiplier::multiplier(Element_ID word_nr) const
{
  return multipliers.word(word_nr);
}

/**/

bool Multiplier::label_contains(Label_ID label,Element_ID multiplier_nr) const
{
  /* Check whether the specified label contains the required multiplier */
  if (multiplier_nr >= nr_multipliers)
    return false;
  if (containing_label)
    return containing_label[multiplier_nr] == label;
  /* If we get here then the labels were not disjoint and we have more
     work to do */
  Word_List wl(label_alphabet());
  label_word_list(&wl,label);
  const Ordinal_Word & multiplier = *multipliers.word(multiplier_nr);
  return wl.contains(multiplier);
}

/**/

void Multiplier::set_multipliers()
{
  /* Determine the set of multipliers supported by a Multiplier.

     We attempt to construct a list that for each multiplier lists the
     unique label that contains that multiplier in member containing_label.
     However, if Multiplier is a determinised coset multiplier then a
     multiplier may appear in more than one label. In that case we set
     disjoint false, and perform a more expensive test each time */

  State_Count nr_labels = label_count();
  Label_ID label;
  bool disjoint = true;

  multipliers.empty();
  Word_List wl(label_alphabet());
  State_List sl;
  State_Subset_Iterator ssi;
  /* First build a list of labels that are attached to accept states. We
     do not care about initial only labels */
  for (State_ID state = accepting_state(ssi,true);
       state;
       state = accepting_state(ssi,false))
    sl.insert(get_label_nr(state));

  State_List::Iterator sli(sl);
  for (label = sli.first(); label;label = sli.next())
  {
    label_word_list(&wl,label);
    Element_Count count = wl.count();
    Ordinal_Word word(label_alphabet());
    for (Element_ID word_nr = 0; word_nr < count;word_nr++)
    {
      wl.get(&word,word_nr);
      if (word.value(0) < base_alphabet.letter_count())
        if (!multipliers.insert(word))
          disjoint = false;
    }
  }

  nr_multipliers = multipliers.count();
  if (containing_label)
    delete [] containing_label;
  if (disjoint)
  {
    containing_label = new Label_ID[nr_multipliers];
    for (label = 1; label < nr_labels;label++)
    {
      label_word_list(&wl,label);
      Element_Count count = wl.count();
      for (Element_ID word_nr = 0; word_nr < count;word_nr++)
      {
        Element_ID multiplier;
        if (multipliers.find(Entry_Word(wl,word_nr),&multiplier))
          containing_label[multiplier] = label;
      }
    }
  }
  else
    containing_label = 0;
}

/**/

Multiplier::~Multiplier()
{
  if (max_depth)
  {
    for (Word_Length i = 0; i < max_depth;i++)
      if (stack[i].node)
        delete [] stack[i].node;
    delete [] stack;
  }
  if (containing_label)
    delete [] containing_label;
  delete &multipliers;
}

/**/

bool Multiplier::multiply(Ordinal_Word * answer,const Word & start_word,
                          Element_ID multiplier_nr,State_ID * initial_state) const
{
  /* This function uses a multiplier FSA to multiply a reduced word by
     one of the multipliers it supports.
  */
  Word_Length length = start_word.length();
  bool found = false;
  const FSA & multiplier = *fsa__;
  const Alphabet & alphabet = multiplier.base_alphabet;
  Ordinal nr_generators = alphabet.letter_count();
  State_Count nr_states = multiplier.state_count();

  if (!max_depth)
  {
    /* Create the stack if need be */
    max_depth = length+1;
    stack = new Node_List[max_depth];
    valid_length = INVALID_LENGTH;
  }
  valid_length = INVALID_LENGTH;
  if (valid_length == INVALID_LENGTH)
  {
    /* Set up the initial state(s) */
    stack[0].nr_nodes = 0;
    if (!multiplier.has_multiple_initial_states())
      stack[0].add_state(0,PADDING_SYMBOL,multiplier.initial_state(),1);
    else
    {
      State_Subset_Iterator ssi;
      State_ID si;
      State_Count nr_initial_states = multiplier.nr_initial_states();
      for (si = multiplier.initial_state(ssi,true);si;si = multiplier.initial_state(ssi,false))
        stack[0].add_state(0,PADDING_SYMBOL,si,nr_initial_states);
    }
    valid_length = 0;
  }


  if (length == 0)
  {
    /* If the multiplicand has length 0, one of the initial states may
       be accepting. We have to check for this up front because the
       main loop assumes a transition is necessary */
    Node_List & current = stack[0];
    for (State_Count prefix = 0;prefix < current.nr_nodes;prefix++)
    {
      State_ID new_multiplier_si = current.node[prefix].multiplier_si;
      if (is_accepting(new_multiplier_si))
      {
        Label_ID label = multiplier.get_label_nr(new_multiplier_si);
        if (label_contains(label,multiplier_nr))
        {
          if (initial_state)
            *initial_state = new_multiplier_si;
          found = true;
          break;
        }
      }
    }
    if (found)
    {
      answer->set_length(0);
      return true;
    }
  }

  const Ordinal * values = start_word.buffer();
  for (Word_Length i = 0;;)
  {
    Ordinal lvalue,rvalue;
    if (i < length)
    {
      lvalue = values[i];
      rvalue = PADDING_SYMBOL;
    }
    else
    {
      lvalue = PADDING_SYMBOL;
      rvalue = 0;
    }
    i++;
    if (i >= max_depth)
    {
      max_depth = Word_Length(max(i+1,length + 1));
      Node_List * new_stack = new Node_List[max_depth];
      for (Word_Length j = 0; j < i;j++)
        new_stack[j] = stack[j];
      delete [] stack;
      stack = new_stack;
    }
    if (i > valid_length || lvalue != stack[i].lvalue || i >= length)
    {
      Node_List & current = stack[i-1];
      Node_List & next = stack[i];
      next.lvalue = lvalue;

      if (lvalue >= nr_generators)
        MAF_INTERNAL_ERROR(multiplier.container,
                           ("Bad word passed to Multiplier::multiply\n"));
      next.nr_nodes = 0;

      State_Count max_states = min((State_Count)
                              (current.nr_nodes*(nr_generators+1)),
                              nr_states);
      for (State_Count prefix = 0;prefix < current.nr_nodes;prefix++)
      {
        State_ID old_multiplier_si = current.node[prefix].multiplier_si;
        for (Ordinal g2 = rvalue; g2 < nr_generators;g2++)
        {
          State_ID new_multiplier_si = multiplier.new_state(old_multiplier_si,
                                            alphabet.product_id(lvalue,g2));
          if (new_multiplier_si)
          {
            next.add_state(prefix,g2,new_multiplier_si,max_states);
            if (i >= length && is_accepting(new_multiplier_si))
            {
              Label_ID label = multiplier.get_label_nr(new_multiplier_si);
              if (label_contains(label,multiplier_nr))
              {
                found = true;
                break;
              }
            }
          }
        }
        if (found)
          break;
      }
      if (found)
      {
        valid_length = i - 1; // stack[i] is no longer fully filled.
        length = i;
        answer->set_length(length);
        Ordinal * values = answer->buffer();
        State_Count prefix = stack[i].nr_nodes-1;
        for (i = length;i > 0; i--)
        {
          Ordinal value = stack[i].node[prefix].rvalue;
          if (value == PADDING_SYMBOL)
            length--;
          else
            values[i-1] = value;
          prefix = stack[i].node[prefix].prefix;
        }
        if (initial_state)
          *initial_state = stack[0].node[prefix].multiplier_si;
        answer->set_length(length);
        break;
      }
      else
      {
        valid_length = i;
        if (!next.nr_nodes)
          break; /* We can't perform the requested multiplication.
                    Either the multiplicand is not accepted, or Multiplier
                    is incomplete */

      }
    }
  }
  return found;
}

FSA_Simple * Multiplier::inverse(const MAF & maf) const
{
  /* Construct the multiplier which supports multiplication by the inverses of
     this multiplier's multipliers. */
  FSA_Simple * answer = FSA_Factory::transpose(*this);
  Word_List old_wl(answer->label_alphabet());
  Word_List new_wl(answer->label_alphabet());
  Ordinal_Word ow(answer->label_alphabet());
  Label_Count nr_labels = answer->label_count();
  for (Label_ID label = 1; label < nr_labels;label++)
  {
    answer->label_word_list(&old_wl,label);
    Element_Count nr_words = old_wl.count();
    new_wl.empty();
    for (Element_ID word_nr = 0;word_nr < nr_words;word_nr++)
    {
      old_wl.get(&ow,word_nr);
      maf.invert(&ow,ow);
      new_wl.add(ow);
    }
    answer->set_label_word_list(label,new_wl);
  }
  return answer;
}

/**/

FSA_Simple * Multiplier::kbmag_multiplier(const MAF & maf,const Word &multiplier,
                                          const String prefix)
{
  /*
     This method extracts the multiplier for a single generator from
     the coset MIDFA general multiplier in kbmag format. kbmag creates
     these multipliers using a new alphabet, and the accept states are
     unlabelled, unless they happen to be initial states as well.

     We form a normal composite multiplier, and then massage its labels
     afterwards.

     In the initial multiplier the first state is certain to be labelled
     by IdWord. It might have some other labels, if any main generators
     are trivial.

     Any other initial states have distinct Schreier generator labels.
  */
  FSA_Simple * answer = composite(maf,multiplier);
  State_Subset_Iterator ssi;
  Word_List_DB label_db(label_count(),true);
  Word_List wl(label_alphabet());

  // build a hash of the original initial labels
  for (State_ID si = initial_state(ssi,true);si;si = initial_state(ssi,false))
  {
    label_word_list(&wl,get_label_nr(si));
    label_db.insert(wl);
  }
  // create the new alphabet
  Ordinal new_nr_generators = label_db.count() - 2;
  Alphabet & new_alphabet = * Alphabet::create(AT_String,container);
  new_alphabet.set_nr_letters(new_nr_generators);
  new_alphabet.set_similar_letters(prefix,new_nr_generators);

  Word_List new_wl(new_alphabet);
  Ordinal_Word new_label(new_alphabet,1);
  //find out which initial states have been preserved and create a new label
  //for each of them
  Element_List el;
  for (State_ID si = answer->initial_state(ssi,true); si;
       si = answer->initial_state(ssi,false))
  {
    answer->label_word_list(&wl,answer->get_label_nr(si));
    Element_ID x_gen = label_db.find_entry(wl,false);
    if (x_gen != 1)
    {
      new_label.set_length(1);
      new_label.set_code(0,x_gen-2);
    }
    else
      new_label.set_length(0);

    String_Buffer sb;
    new_label.format(&sb);
    new_wl.add(new_label);
  }
  // put the new labels on the FSA
  answer->set_label_type(LT_List_Of_Words);
  answer->set_label_alphabet(new_alphabet);
  answer->set_nr_labels(new_wl.count()+1);
  int label_nr = 1;
  int word_nr = 0;
  for (State_ID si = answer->initial_state(ssi,true); si;
       si = answer->initial_state(ssi,false))
  {
    new_wl.get(&new_label,word_nr++);
    answer->set_label_nr(si,label_nr);
    answer->set_label_word(label_nr++,new_label);
  }
  return answer;
  // we do not clean up the alphabet. It will go away when the FSA gets deleted
}

/**/

class Operator_Word;

/* Operator_Word is used by methods that build composite multipliers.
   Each word begins and ends with INVALID_SYMBOL, which, roughly
   speaking represents a composition to be performed. Initially the
   word abc looks like * a * b * c * . If we form the composite ab then
   the word is changed to * ab * c * . A word is not "correct" until
   the only * characters are at the beginning and end of the word.
   Putting * characters at the ends simplifies finding composite
   operations to perform. for example once we have * ab * c* it is
   not worth forming * bc *, and the presence of the extra * characters
   makes this test simple to perform.

   Since the class is only used in the construction of composites some
   methods expect a "Compositor" parameter, which can implement callback
   methods to do useful stuff with the word.
*/

class Compositor
{
  friend class Operator_Word;
  private:
    virtual void add_work(const Ordinal * /*before*/,size_t /*length*/,bool /*complete*/) {};
    virtual Element_Count perform_inner(const Operator_Word &/*before*/,
                                        size_t /*offset*/,unsigned /*delta*/,
                                        bool /*do_inverse*/ = false,
                                        const FSA * /*inverse*/ = 0)
    {
      return 0;
    }
  public:
    virtual ~Compositor() {};
    virtual const FSA_Simple * multiplier(const Word &word) = 0;
    virtual const Multiplier * short_multiplier() const = 0;
    virtual void fix_multipliers() = 0;
};

class Operator_Word
{
  public:
    size_t allocated;
    size_t length;
    Word_Length pending;
    Ordinal * data;
  public:
    Operator_Word(const Word & start_word)
    {
      /* constructor to construct * a * ... * z * from a..z.
         In this case we assume we are starting from the General Multiplier */
      length = start_word.length();
      data = 0;
      pending = 0;
      if (length)
      {
        data = new Ordinal[allocated = length*2 + 1];
        data[0] = INVALID_SYMBOL;
        const Ordinal * values = start_word.buffer();
        for (Word_Length i = 0; i < length;i++)
        {
          data[i*2+1] = values[i];
          data[i*2+2] = INVALID_SYMBOL;
        }
        pending = length-1;
        length = 2*length+1;
      }
    }

    Operator_Word(const Word & start_word,
                  const Sorted_Word_List &available_multipliers,
                  bool * impossible) :
      data(0),
      allocated(0),
      pending(0),
      length(0)
    {
      /* constructor to construct * ab * cde * ... * yz * from a..z
         where each chunk is as large as possible given a list
         of available_multipliers.
         We use a "greedy" algorithm and make the longest possible word
         at each stage. We work from the right since that simplifies
         calculating where in our working word we need to look for
         * characters to get rid of. It is assumed that all the
         single letter multipliers are available, since no attempt is
         deal with abcd as ab*cd if bcd is available, even if a is not. */
      Operator_Word work(start_word);
      if (work.length)
      {
        Word_Length l = start_word.length();
        for (Word_Length i = l; i !=0;)
        {
          Word_Length segment_length = 0;
          for (Word_Length j = i; j-- > 0;)
            if (available_multipliers.contains(Subword((Word &)start_word,j,i)))
              segment_length = i - j;
          if (!segment_length)
          {
            String_Buffer sb;
            start_word.format(&sb);
            start_word.alphabet().container.error_output("Can't do %s\n",
                                                         sb.get().string());
            *impossible = true;
            return;
          }
          Word_Length start = i - segment_length;
          if (segment_length > 1)
          {
            /* initially the nth character in a word occurs at position 2*n+1.
               We know this is a valid character so we skip it.
               If the segment is n characters long it has n-1 * characters
               to be removed. */
            int to_do = segment_length -1;
            for (size_t k = start*2+2; to_do!=0;k++)
              if (work.data[k] == INVALID_SYMBOL)
              {
                memmove(work.data + k,work.data + k + 1,
                        (work.length - k - 1)*sizeof(Ordinal));
                work.pending--;
                work.length--;
                to_do--;
              }
          }
          i = start;
        }
        set(work.data,work.length,work.pending);
      }
    }

    Operator_Word()
    {
      allocated = length = pending = 0;
      data = 0;
    }

    Operator_Word(const Word & w1,const Word & w2)
    {
      /* Constructor to construct *az* from a and z. This is the
         constructor we use when we want to construct the Operator_Word
         for a composite that has been formed already */

      Word_Length l1 = w1.length();
      Word_Length l2 = w2.length();
      length =  l1 + l2 + 3;
      data = new Ordinal[allocated = length];
      data[0] = INVALID_SYMBOL;
      word_copy(data+1,w1.buffer(),l1);
      data[l1+1] = INVALID_SYMBOL;
      word_copy(data+l1+2,w2.buffer(),l2);
      data[length-1] = INVALID_SYMBOL;
      pending = 0;
    }

    ~Operator_Word()
    {
      if (data)
        delete [] data;
    }

    bool extract_word(Ordinal_Word * word) const
    {
      /* Extract the interior of an Operator_Word into an ordinary Word.
         This makes no sense until all the composites have been formed */
      if (pending)
        return false;
      word->set_length(length ? length - 2 : 0);
      if (length)
        word->set_multiple(0,data+1,length-2);
      return true;
    }

    Operator_Word & operator=(const Operator_Word & other)
    {
      set(other.data,other.length,other.pending);
      return *this;
    }

    void pre_pair(Sorted_Word_List &useful_pairs)
    {
      /* Extract each group of two letters from an operator word
         and add it to the list */
      for (size_t i = 0; i + 5 <= length; i += 4)
      {
        Ordinal_Word ow(useful_pairs.alphabet,2);
        ow.set_code(0,data[i+1]);
        ow.set_code(1,data[i+3]);
        useful_pairs.add(ow);
      }
    }
    bool pair(Compositor & compositor,Operator_Word * before)
    {
      /* convert a word of the form * a * b * c * d * ...
         to * ab * cd * ... */

      bool retcode = false;
      for (size_t i = 0; i + 5 <= length; i += 3)
      {
        before->set(data+i,5);
        memmove(data+i+2,data+i+3,(length-i-3)*sizeof(Ordinal));
        length--;
        if (!--pending)
          retcode = true;
        compositor.perform_inner(*before,2,1);
      }
      return retcode;
    }

    bool perform(Element_Count * found,
                 const Ordinal * before,
                 size_t before_length,
                 size_t offset)
    {
      /* On entry "before" contains a sequence of the form * w1 * w2 *.
         We look for this sequence in the word, and if found replace it
         with * w1w2 *. We count the number of times we do this, and
         also how many composite operations are still required for this
         word */
      bool retcode = false;
      for (size_t i = 0; i + before_length <= length;i++)
        if (memcmp(data+i,before,before_length*sizeof(Ordinal))==0)
        {
          memmove(data+i+offset,data+i+offset+1,
                  (length - i - offset-1)*sizeof(Ordinal));
          i += before_length-3;
          length--;
          *found += 1;
          if (--pending == 0)
            retcode = true;
        }
      return retcode;
    }

    bool enumerate_work(Compositor & cb,bool parallel = false)
    {
      /* enumerate_work lists the composites still waiting to be formed
         in a word
         If parallel is true then instead it lists the pairs that can be
         formed, and any remaining word */
      if (pending)
      {
        Word_Length done = 0;
        for (size_t i = 0; i < length;i++)
          if (data[i] == INVALID_SYMBOL)
          {
            int found = 1;
            for (size_t j = i+2;;j++)
            {
              if (data[j] == INVALID_SYMBOL)
                found++;
              if (found==3)
              {
                if (!parallel || !(done & 1))
                  cb.add_work(data+i,j-i+1,pending==1);
                done++;
                break;
              }
            }
            if (done == pending)
              break;
          }
      }
      if (parallel && length)
      {
        if (!(pending & 1))
        {
          size_t i;
          for (i = length-3;data[i] != INVALID_SYMBOL;i--)
            ;
          cb.add_work(data+i,length-i,i==0);
        }
        int seen = 0;
        for (size_t i = 1; i < length-1;i++)
          if (data[i] == INVALID_SYMBOL)
          {
            if (++seen & 1 && pending)
            {
              memmove(data+i,data+i+1,(length - i-1)*sizeof(Ordinal));
              length--;
              pending--;
            }
          }
      }
      return pending!=0;
    }
    Element_Count pending_count() const
    {
      return pending;
    }
    void set(const Ordinal * new_data,size_t new_length,Word_Length new_count = 0)
    {
      if (allocated < new_length)
      {
        if (data)
          delete [] data;
        data = new Ordinal[allocated = new_length];
      }
      pending = new_count;
      length = new_length;
      word_copy(data,new_data,new_length);
    }

};

/* Multiplier DB is a Word_DB in which each word has two associated fields:
   0:  a pointer representing the reference count for the multiplier
   1:  a pointer to the FSA for the multiplier */

class Multiplier_DB : public Word_DB
{
   Array_Of<unsigned> counts;
   Array_Of<FSA_Simple *> fsas;
  public:
    Multiplier_DB(const Alphabet & alphabet,Element_Count hash_size) :
      Word_DB(alphabet,hash_size)
    {
      manage(counts);
      manage(fsas);
    }
    ~Multiplier_DB()
    {
      Element_Count nr_multipliers = count();
      for (Element_ID m = 0; m < nr_multipliers;m++)
        set_multiplier(m,0);
    }
    void down_count(Element_ID multiplier_nr,unsigned delta)
    {
      counts[multiplier_nr] -= delta;
      if (!counts[multiplier_nr])
        set_multiplier(multiplier_nr,0);
    }
    void up_count(Element_ID multiplier_nr,unsigned delta)
    {
      counts[multiplier_nr] += delta;
    }
    FSA_Simple * multiplier(Element_ID multiplier_nr)
    {
      return fsas[multiplier_nr];
    }
    const FSA_Simple * multiplier(const Word & word)
    {
      return multiplier(enter(word));
    }
    void set_multiplier(Element_ID multiplier_nr,FSA_Simple * fsa)
    {
      FSA_Simple * was = multiplier(multiplier_nr);
      fsas[multiplier_nr] = fsa;
      if (was)
        delete was;
    }
    FSA_Simple * make_multiplier(const MAF & maf,
                                 const Word & word,
                                 const Multiplier &basis)
    {
      Element_ID m = enter(word);
      FSA_Simple * fsa = multiplier(m);
      if (!fsa)
        set_multiplier(m,fsa = basis.composite(maf,word));
      return fsa;
    }
    void extract_inverse_multipliers(const MAF & maf,const Sorted_Word_List & inverse_multipliers,bool transpose_only)
    {
      Element_Count nr_multipliers = inverse_multipliers.count();
      for (Element_ID word_nr = 0;word_nr < nr_multipliers;word_nr++)
      {
        const Ordinal_Word & ow = *inverse_multipliers.word(word_nr);
        const FSA_Simple * fsa = multiplier(ow);
        if (!fsa)
        {
          Ordinal_Word iow(ow);
          maf.invert(&iow,iow);
          fsa = multiplier(iow);
          if (fsa)
          {
            Element_ID m_id = enter(ow);
            if (!transpose_only)
            {
              Multiplier m((FSA &) *fsa,false);
              set_multiplier(m_id,m.inverse(maf));
            }
            else
              set_multiplier(m_id,FSA_Factory::transpose(*fsa));
          }
        }
      }
    }

    void fix_multipliers()
    {
      /* Adjusts a collection of multipliers so that they are suitable for
         axiom checking */
      Element_Count nr_multipliers = count();
      for (Element_ID m = 0; m < nr_multipliers;m++)
      {
        FSA_Simple * fsa = multiplier(m);
        if (fsa)
        {
          if (fsa->has_multiple_initial_states())
            set_multiplier(m,fsa = FSA_Factory::determinise(*fsa));
          if (fsa->label_count() > 2)
          {
            /* If the FSA has more than two labels then we are probably dealing
               with a coset multiplier in which labels that used to be different
               are now the same. We need to remove the labels to ensure the
               equality test is correct
            */
            fsa->set_label_type(LT_Unlabelled);
            set_multiplier(m,FSA_Factory::minimise(*fsa));
          }
        }
      }
    }
};

class Parallel_Compositor : public Compositor
{
  private:
    const MAF & maf;
    Multiplier * gm2;
    FSA_Simple * answer;
    Sorted_Word_List next_multipliers;
    Sorted_Word_List needed_multipliers;
    Operator_Word ** words;
    Element_Count nr_multipliers;
    Multiplier_DB multiplier_db;
    bool want_composite;
  public:
    const Alphabet & base_alphabet;
    Parallel_Compositor(const MAF & maf,
                        const Multiplier & source_multiplier,
                        const Word_Collection & new_multipliers,
                        const Word_Collection & short_multipliers,
                        bool want_composite_ = false);
    ~Parallel_Compositor()
    {
      if (gm2)
        delete gm2;
      for (Element_ID i = 0; i < nr_multipliers;i++)
        delete words[i];
      delete [] words;
    }
    const FSA_Simple * multiplier(const Word & word)
    {
      const FSA_Simple * fsa = multiplier_db.multiplier(word);
      if (fsa)
        return fsa;
      /* It may be the case that a multiplier was listed as one of the
         desired short multipliers, and hence not constructed as an individual
         multiplier, but then requested again afterwards. So we need to be
         prepared to build it in this case */
      if (gm2)
        fsa = multiplier_db.make_multiplier(maf,word,*gm2);
      return fsa;
    }
    const Multiplier * short_multiplier() const
    {
      return gm2;
    }
    void fix_multipliers()
    {
      multiplier_db.fix_multipliers();
    }
    FSA_Simple * take()
    {
      return answer;
    }
  private:
    void add_work(const Ordinal * before,size_t length,bool complete)
    {
      Ordinal_Word ow(base_alphabet,--length-2);
      ow.set_length(0);
      Word_Length operator_position = INVALID_LENGTH;
      for (Word_Length i = 1;i < length;i++)
        if (before[i] != INVALID_SYMBOL)
          ow.append(before[i]);
        else
          operator_position = i-1;
      if (operator_position != INVALID_LENGTH || want_composite || !complete)
        next_multipliers.insert(ow);
      if (operator_position != INVALID_LENGTH)
      {
        needed_multipliers.insert(Subword(ow,0,operator_position));
        needed_multipliers.insert(Subword(ow,operator_position));
      }
      else if (!complete || want_composite)
      {
        needed_multipliers.insert(ow);
        ow.set_length(0);
        needed_multipliers.insert(ow);
      }
    }
    void extract_multipliers(FSA_Simple * candidate,
                             const Sorted_Word_List & actual_multipliers)
    {
      Multiplier m(*candidate,false);
      for (Element_ID word_nr = 0;word_nr < nr_multipliers;word_nr++)
        if (!words[word_nr]->pending_count())
        {
          const Ordinal_Word & ow = *actual_multipliers.word(word_nr);
          const FSA_Simple * fsa = multiplier_db.multiplier(ow);
          if (!fsa)
            multiplier_db.make_multiplier(maf,ow,m);
        }
    }
};

Parallel_Compositor::Parallel_Compositor(const MAF & maf_,
                                         const Multiplier & source_multiplier,
                                         const Word_Collection & new_multipliers,
                                         const Word_Collection & short_multipliers,
                                         bool want_composite_) :
  maf(maf_),
  answer(0),
  words(0),
  base_alphabet(source_multiplier.base_alphabet),
  next_multipliers(source_multiplier.base_alphabet),
  needed_multipliers(source_multiplier.base_alphabet),
  multiplier_db(source_multiplier.base_alphabet,new_multipliers.count()),
  want_composite(want_composite_),
  gm2(0)
{
  /* This method constructs the multiplier(s) for the list of words specified
     in new_multipliers. i.e. the product FSA(s) which accepts a word pair
     (u,v) if and only if u and v are both reduced words and uw == v for
     some w in new_multipliers. It is the caller's responsibility to ensure
     that the Multiplier being asked to form a composite is capable of
     generating the target Multiplier. The method can construct either a
     single multiplier with labelled accept states or a collection of separate
     multipliers.

     This method is also used to perform the relatively trivial task of
     extracting the multiplier for a single multiplier from a composite
     multiplier. This is not inefficient, because the method will very
     quickly discover that all it has to do is to change the accept states
     and labels.

     It works by starting from the current multiplier and repeatedly
     composing its current iteration with itself, and increasing the
     number of known multipliers and accepted word pairs in the following
     manner:
     Suppose the current iteration accepts (u,x) with an accept state
     whose label contains w0, and accepts (x,v) with an accept state
     whose label contains w1. Then uw0w1==xw1=v. Therefore we can
     accept the pair (u,v) with a label including w0w1. We can construct
     this FSA by the mapping (s1,s2)^(g1,g2) -> s1^(s1,A+)*s2^(A+,s2) where
     where A+ is the padded alphabet.
     On each iteration we only add a word w0w1 to the list of known
     multipliers if w0w1 is one of the words w in our target list, or
     a useful subword of such a word. Unless we want to build a composite
     multiplier then we extract each single multiplier from the composite
     at the earliest opportunity, so that we can remove accept states and
     transitions from what we are composing.
     Eventually our current iteration will inevitably accept all the
     (remaining) words in our target list. At that point we recalculate the
     accept states only to include the target list of words.
     This method is primarily intended for use by the General_Multiplier,
     and indeed might fail erronously if asked to create a
     sub-subgroup_multiplier (because reductions to the intermediate
     multipliers might make it appear that necessary composites were
     not required.) It is here because it makes it much easier to create
     a "general subgroup" multiplier and then extract the individual
     multipliers from it.


     In coset multipliers the accept states correspond to the equations
     ux=hv, not ux=v. So in this case the pair of accept states present in
     a composite accept state is ux=h1w  wy=h2v, or uxy=h1wy=h1h2v.
     In this case we must also examine and label the initial states.
     Also if asked to do deal with a determinized coset multiplier, we have
     to cater for the fact that each accept state can have more than one
     reduced word in its label.

     We cater for having g and h labels for the initial states of composites.
     This is required to allow us to create a nice presentation of the
     subgroup.
  */

  const Alphabet & base_alphabet = source_multiplier.base_alphabet;
  const Alphabet & label_alphabet = source_multiplier.label_alphabet();
  Word_List_DB labels(1024,false);
  Ordinal_Word base_word(base_alphabet);
  Ordinal_Word label_word(label_alphabet);
  unsigned pass_nr = 0;
  Transition_ID nr_transitions = source_multiplier.alphabet_size();
  State_ID * transition = new State_ID[nr_transitions];
  State_ID state;
  bool is_coset_multiplier = label_alphabet.letter_count() > base_alphabet.letter_count() ||
                             source_multiplier.has_multiple_initial_states();
  bool can_invert = maf.alphabet == label_alphabet;
  Container & container = source_multiplier.container;
  Sorted_Word_List available_multipliers(source_multiplier.multiplier_list());
  Sorted_Word_List inverse_multipliers(source_multiplier.base_alphabet);
  bool force_identity = false;

  Element_Count pending = 0; /* number of words whose multiplier
                                has not yet been formed */
  Element_Count nr_multipliers_desired = new_multipliers.count();

  Sorted_Word_List actual_multipliers(base_alphabet);
  /* Sort the list of multipliers, and clean it if we are allowed to */
  {
    for (Element_ID word_nr = 0;word_nr < nr_multipliers_desired;word_nr++)
    {
      new_multipliers.get(&base_word,word_nr);
      Ordinal_Word iword(base_word);
      maf.invert(&iword,iword);
      if (!short_multipliers.contains(base_word))
      {
        if (want_composite || iword.length() <= 1 || !can_invert || !actual_multipliers.contains(iword))
          actual_multipliers.insert(base_word);
        else
          inverse_multipliers.insert(base_word);
      }
    }
  }
  /* Build the list of Operator_Words that will tell us where we have got
     to so far */
  {
    nr_multipliers = actual_multipliers.count();
    words = new Operator_Word *[nr_multipliers];
    bool impossible = false;
    base_word.set_length(0);
    for (Element_ID word_nr = 0;word_nr < nr_multipliers;word_nr++)
    {
      const Ordinal_Word & ow = *actual_multipliers.word(word_nr);
      if (!ow.length() && want_composite)
        force_identity = true;
      words[word_nr] = new Operator_Word(ow,available_multipliers,&impossible);
      if (words[word_nr]->pending_count()!=0)
        pending++;
    }
    if (impossible)
    {
      container.error_output("Bad call to Parallel_Compositor::Parallel_Compositor().\n"
                             "One or more required multipliers missing from initial multipliers\n");
      return;
    }
  }

  /* Check the identity element is included in source_multiplier,
     otherwise this method will not work.
     (Actually this is not quite true, but the "impossible" test
     for this case would be more difficult) */
  base_word.set_length(0);
  if (pending && !source_multiplier.is_multiplier(base_word,0))
  {
    container.error_output("Bad call to Parallel_Compositor::Parallel_Compositor().\n"
                           "Initial multipliers must include identity\n");
    return;
  }

  FSA_Simple * candidate = FSA_Factory::copy(*source_multiplier.fsa(),TSF_Dense);
  Ordinal nr_generators = base_alphabet.letter_count();

  State_Count count,initial_end;     //Declaring this here so that I don't
  Word_List label_wl(label_alphabet);  //need to declare them twice
  pending += short_multipliers.count();

  while (pending)
  {
    /* extract any multipliers that are ready */
    if (!want_composite)
      extract_multipliers(candidate,actual_multipliers);
    container.progress(1,"Composite multiplier for " FMT_ID " words. "
                         " Starting pass %u with " FMT_ID " multipliers with\n "
                         FMT_ID " still to find by composition\n",
                       nr_multipliers_desired,
                       ++pass_nr,available_multipliers.count(),pending);
    next_multipliers.empty();
    needed_multipliers.empty();
    pending = 0;
    for (Element_ID word_nr = 0;word_nr < nr_multipliers;word_nr++)
      if (words[word_nr]->enumerate_work(*this,true))
        pending++;
    if (pending || force_identity)
    {
      // we always require identity while work is pending, as it is complicated
      // to work out when it will not be needed again and it almost always
      // will be needed. But on the last step we can remove it
      base_word.set_length(0);
      needed_multipliers.insert(base_word);
      next_multipliers.insert(base_word);
    }
    if (short_multipliers.count() && !gm2)
    {
      for (Element_ID word_nr = 0;word_nr < short_multipliers.count();word_nr++)
      {
        short_multipliers.get(&base_word,word_nr);
        if (base_word.length() <= 1)
        {
          needed_multipliers.insert(base_word);
          needed_multipliers.insert(Subword(base_word,1,1));
        }
        else
        {
          needed_multipliers.insert(Subword(base_word,0,1));
          needed_multipliers.insert(Subword(base_word,1,2));
        }
        next_multipliers.insert(base_word);
      }
    }
    /* Eliminate any multipliers we don't need any more. This may save a
       lot of time and will rarely take more than a few seconds*/
    if (needed_multipliers.count() < available_multipliers.count())
    {
      Word_List wl(needed_multipliers);
      Multiplier m(*candidate,true);
      candidate = m.composite(maf,wl);
      // previous candidate will go away when m gets destroyed
    }

    /* Initialise the new candidate FSA */
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
                      candidate->state_count(),0);

    /* Create the failure and initial states */
    key.empty();
    packed_key = key.packed_data(&key_size);
    factory.find_state(packed_key,key_size);

    State_Subset_Iterator ssi_0;
    State_Subset_Iterator ssi_1;

    bool all_dense = key.all_dense(candidate->state_count(),
                                   candidate->state_count());
    for (pair[0] = candidate->initial_state(ssi_0,true); pair[0];
         pair[0] = candidate->initial_state(ssi_0,false))
      for (pair[1] = candidate->initial_state(ssi_1,true); pair[1];
           pair[1] = candidate->initial_state(ssi_1,false))
      {
        key.insert(pair,all_dense);
        packed_key = key.packed_data(&key_size);
        factory.find_state(packed_key,key_size);
        key.empty();
      }
    count = initial_end = factory.state_count();

    /* Now build lists of which labels appear in accept states.
       Since the number of labels not associated with any accept
       state and the number of words in the labels tends to grow
       exponentially this code can be extremely slow */
    Label_Set_Owner lso(*candidate);
    Special_Subset accept_labels(lso);
    State_Count nr_states = candidate->state_count();
    for (State_ID si = 1; si < nr_states;si++)
      if (candidate->is_accepting(si))
        accept_labels.assign_membership(candidate->get_label_nr(si),true);
    Special_Subset left_ssl(lso);
    Special_Subset right_ssl(lso);
    Special_Subset left_ss(*candidate);
    Special_Subset right_ss(*candidate);
    Word_List right_label_wl(label_alphabet);
    Special_Subset_Iterator left_label_ssi,right_label_ssi;
    for (Label_ID left_label = left_label_ssi.item(accept_labels,true);
         left_label;
         left_label = left_label_ssi.item(accept_labels,false))
    {
      candidate->label_word_list(&label_wl,left_label);
      Element_Count left_nr_words = label_wl.count();
      for (Label_ID right_label = right_label_ssi.item(accept_labels,true);
           right_label;
           right_label = right_label_ssi.item(accept_labels,false))
      {
        container.status(2,1,"Examining labels " FMT_ID " " FMT_ID "\n",
                         left_label,right_label);
        candidate->label_word_list(&right_label_wl,right_label);
        Element_Count right_nr_words = right_label_wl.count();
        for (Element_ID left_word_nr = 0;
             left_word_nr < left_nr_words;left_word_nr++)
        {
          for (Element_ID right_word_nr = 0;
               right_word_nr < right_nr_words;
               right_word_nr++)
          {
            label_word = Entry_Word(label_wl,left_word_nr) +
                         Entry_Word(right_label_wl,right_word_nr);
            if (next_multipliers.contains(label_word))
            {
              left_ssl.assign_membership(left_label,true);
              right_ssl.assign_membership(right_label,true);
            }
          }
        }
      }
    }
    left_ssl.set_fast_random_access();
    right_ssl.set_fast_random_access();
    for (State_ID si = 1; si < nr_states;si++)
    {
      if (candidate->is_accepting(si))
      {
        if (left_ssl.contains(candidate->get_label_nr(si)))
          left_ss.assign_membership(si,true);
        if (right_ssl.contains(candidate->get_label_nr(si)))
          right_ss.assign_membership(si,true);
      }
    }
    left_ss.set_fast_random_access();
    right_ss.set_fast_random_access();
    candidate->label_word_list(&label_wl,1);
    label_word.set_length(0);
    bool label1_is_identity = label_wl.contains(label_word);

    Transition_Realiser tr(*candidate);
    /* Now create all the new states and the transition table */
    state = 0;
    State_Pair_List old_key;
    while ((old_packed_key = factory.get_state_key(++state))!=0)
    {
      Word_Length length = state >= ceiling ? state_length : state_length - 1;
      if (!(state & 255))
        container.status(2,1,"Composing multiplier: pass %u state " FMT_ID " "
                             "(" FMT_ID " of " FMT_ID " to do). Length %u\n",
                         pass_nr,state,count-state,count,length);

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
                const State_ID * trow1 = tr.realise_row(old_pair[0],0) +
                                          base_alphabet.product_base(g1);
                const State_ID * trow2 = tr.realise_row(old_pair[1],1);
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
                  /* Below we test whether states are in our "left_ss" or
                     "right_ss" set, rather than merely accepting, which is
                     what the typical composite() algorithm does, and it is
                     not entirely obvious that this is OK, since later on
                     we use ($,g)(g,$) transitions to increase the number
                     of accepted states. But we can see that this must be OK
                     for two reasons:
                     1) In the first place we could have legitimately formed
                     the composite using different left and right FSAs with
                     the appropriate set of accept states, and then we would
                     have made the same decision.
                     2) Say we take a ($,$) transition to get pair[1]. Then
                     even though pair[1] is in a state where it may accept
                     a (g,$) transition, pair[0] definitely is not in a state
                     where it will accept a ($,g) transition. Its right word
                     is known to be padded, so only (g,$) transitions will
                     get anywhere on the left. The same argument applies the
                     other way round. */
                  Transition_ID tid = base_alphabet.product_id(g1,g_middle);
                  if (tid < nr_transitions)
                    pair[0] = trow1[g_middle];
                  else
                    pair[0] = left_ss.contains(old_pair[0]) ? old_pair[0] : 0;
                  if (!pair[0])
                    continue;

                  tid = base_alphabet.product_id(g_middle,g2);
                  if (tid < nr_transitions)
                    pair[1] = trow2[tid];
                  else
                    pair[1] = right_ss.contains(old_pair[1]) ? old_pair[1] : 0;
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

    /* Set the labels,and initial and accept states in the new FSA */
    Sorted_Word_List swl(label_alphabet);
    Word_List wl0(label_alphabet),wl1(label_alphabet);
    Ordinal_Word w0(label_alphabet),w1(label_alphabet);
    labels.empty();
    Label_ID label_id;
    // Insert a "null" label, so our label numbers start from 1
    labels.Hash::find_entry(0,0);
    state = 0;
    factory.clear_accepting(true);
    factory.set_label_alphabet(label_alphabet);
    factory.set_label_type(LT_List_Of_Words);
    factory.set_nr_labels(1,LA_Mapped);
    key.empty();

    while ((old_packed_key = factory.get_state_key(++state))!=0)
    {
      if (!(char) state)
        container.status(2,1,"Labelling states: pass %u state " FMT_ID " "
                             "(" FMT_ID " of " FMT_ID " to do).\n",
                         pass_nr,state,count-state,count);
      bool needed = false;
      State_Pair_List old_key((const Byte *) old_packed_key);
      State_Pair_List total_key((const Byte *) old_packed_key);
      State_Pair_List::Iterator old_key_pairs(old_key);
      swl.empty();
      for (;;)
      {
        if (old_key_pairs.first(old_pair))
        {
          do
          {
            Label_ID label0 = candidate->get_label_nr(old_pair[0]);
            Label_ID label1 = candidate->get_label_nr(old_pair[1]);
            if (state < initial_end && label0 && label1 ||
                left_ss.contains(old_pair[0]) && right_ss.contains(old_pair[1]))
            {
              candidate->label_word_list(&wl0,label0);
              candidate->label_word_list(&wl1,label1);
              Element_Count count0 = wl0.count();
              Element_Count count1 = wl1.count();
              bool have_g = false;
              bool have_h = false;
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
                if (label0 == 1 && label1_is_identity)
                {
                  count0 = 1;
                  fix0 = true;
                }
                else if (label1 == 1 && label1_is_identity)
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
                  bool include = state < initial_end &&
                                  candidate->is_initial(old_pair[0]) &&
                                  candidate->is_initial(old_pair[1]);
                  if (w0.length() > 0 && w1.length() > 0 &&
                      (w0.value(0) < nr_generators) != (w1.value(0) < nr_generators))
                    continue; // we do not want mixed labels
                  label_word = w0 + w1;
                  if (label_word.length())
                  {
                    if (label_word.value(0) < nr_generators)
                    {
                      if (include && have_g)
                        include = false;
                      if ((!include || !needed) && next_multipliers.contains(label_word))
                         needed = include = true;
                    }
                    else if (have_h)
                      include = false;
                  }
                  else if (next_multipliers.contains(label_word))
                    include = needed = true;
                  if (include)
                  {
                    if (label_word.length())
                      if (label_word.value(0) < nr_generators)
                        have_g = true;
                      else
                        have_h = true;
                    swl.insert(label_word);
                  }
                }
              }
            }
          }
          while (old_key_pairs.next(old_pair));
          /* We also need to check states accessible via $,g g,$ transitions
             since these states ought to be in our image set */
          old_key_pairs.first(old_pair);
          do
          {
            const State_ID * trow1 = tr.realise_row(old_pair[0],0) + base_alphabet.product_base(PADDING_SYMBOL);
            const State_ID * trow2 = tr.realise_row(old_pair[1],1);
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
      if (needed || state < initial_end)
      {
        Element_ID id;
        label_wl = swl;
        labels.insert(&id,label_wl);
        if (state < initial_end)
          factory.set_is_initial(state,true);
        if (needed)
          factory.set_is_accepting(state,true);
        factory.set_label_nr(state,id,true);
      }
    }
    tr.unrealise();
    factory.remove_keys();
    const void * packed_label;
    label_id = 0;
    while ((packed_label = labels.get_key(++label_id))!=0)
    {
      label_wl.unpack((const Byte *)packed_label);
      factory.set_label_word_list(label_id,label_wl);
    }
    labels.empty();
    delete candidate;
    if (is_coset_multiplier)
    {
      // determinise here merges initial states that are proved equal
      // and then does a minimise
      candidate = FSA_Factory::determinise(factory,FSA_Factory::DF_Equal,true,TSF_Dense);
    }
    else
      candidate = FSA_Factory::minimise(factory,TSF_Dense);
    available_multipliers.take(next_multipliers);
    if (short_multipliers.count() && !gm2)
    {
      FSA_Simple * temp = FSA_Factory::copy(*candidate);
      gm2 = new Multiplier(*temp,true);
    }
  }
  /* we don't need this any more */
  delete [] transition;
  if (want_composite)
  {
    /* Now we have an FSA with all the accept states we need, but it probably
       has too many accept states at the moment */
    count = candidate->state_count();

    /* The composite's labels should be the proved labels that we are
       interested in. However we need to preserve the initial state
       labels as well. */
    State_List initial_labels;
    if (is_coset_multiplier)
    {
      State_Subset_Iterator ssi;
      for (State_ID si = candidate->initial_state(ssi,true);si;
           si = candidate->initial_state(ssi,false))
        initial_labels.insert(candidate->get_label_nr(si));
    }
    Label_Count nr_labels = candidate->label_count();
    Word_List wl(label_alphabet);
    for (Label_ID label_id = 1; label_id < nr_labels;label_id++)
      if (!initial_labels.contains(label_id))
      {
        candidate->label_word_list(&label_wl,label_id);
        wl.empty();
        Element_Count count = label_wl.count();
        for (Element_ID word_nr = 0; word_nr < count;word_nr++)
        {
          label_wl.get(&label_word,word_nr);
          if (actual_multipliers.find(label_word))
            wl.add(label_word);
        }
        candidate->set_label_word_list(label_id,wl);
      }

    for (state = 1;state < count;state++)
    {
      if (candidate->is_accepting(state))
      {
        Label_ID label_id = candidate->get_label_nr(state);
        candidate->label_word_list(&label_wl,label_id);
        bool ok = false;
        Element_Count nr_words = label_wl.count();
        for (Element_ID word_nr = 0; word_nr < nr_words;word_nr++)
           if (actual_multipliers.contains(Entry_Word(label_wl,word_nr)))
           {
             ok = true;
             break;
           }
        if (!ok)
        {
          candidate->change_flags(0,GFF_TRIM);
          candidate->set_is_accepting(state,false);
          if (!candidate->is_initial(state))
            candidate->set_label_nr(state,0);
        }
      }
    }

    candidate->sort_labels();
    answer = FSA_Factory::minimise(*candidate,TSF_Default,
                                   nr_multipliers > 1 ? FSA_Factory::MLF_None :
                                                        FSA_Factory::MLF_All);
    delete candidate;
    if (nr_multipliers_desired > 1)
      container.progress(1,"Composite multiplier with " FMT_ID
                           " states computed\n",answer->state_count());
    else if (nr_multipliers_desired)
    {
      String_Buffer sb;
      new_multipliers.get(&base_word,0);
      base_word.format(&sb);
      container.progress(1,"%s multiplier with " FMT_ID " states computed\n",
                         sb.get().string(),answer->state_count());
    }
  }
  else
  {
    extract_multipliers(candidate,actual_multipliers);
    multiplier_db.extract_inverse_multipliers(maf,inverse_multipliers,false);
    delete candidate;
  }
}

/**/

class Serial_Compositor : public Compositor
{
  protected:
    const MAF & maf;
    const General_Multiplier & gm;
    Multiplier * gm2;
    /* multiplier_db contains a list of words for which we have multipliers,
       Each multiplier is reference counted */
    Multiplier_DB multiplier_db;
    Array_Of<unsigned> counts;
    Hash work_db; /* work_db is used to help us to decide what to do next */
    Operator_Word ** words;
    Element_Count nr_words;
    Element_Count work_count;
    bool require_labels;
    bool can_invert;
  public:
    Serial_Compositor(const MAF & maf_,const General_Multiplier & gm_,
                      const Word_List &desired_multipliers,
                      const Word_List &short_multipliers,
                      bool require_labels_,
                      Compositor_Algorithm algorithm = CA_Hybrid) :
      maf(maf_),
      gm(gm_),
      multiplier_db(gm_.base_alphabet,1024),
      work_db(1024,0),
      gm2(0),
      require_labels(require_labels_),
      can_invert(maf.alphabet == gm.label_alphabet())
    {
      Element_Count pending = 0; /* number of words whose multiplier
                                    has not yet been formed */
      work_db.manage(counts);
      Element_ID word_nr;
      nr_words = desired_multipliers.count();
      const Alphabet & base_alphabet = gm.base_alphabet;
      Ordinal_Word base_word(base_alphabet);

      /* Clean up the list of multipliers and then convert it to a list
         of Operator_Words */
      Sorted_Word_List new_multipliers(desired_multipliers.alphabet,nr_words);
      Sorted_Word_List inverse_multipliers(desired_multipliers.alphabet,nr_words);
      {
        for (word_nr = 0;word_nr < nr_words;word_nr++)
        {
          desired_multipliers.get(&base_word,word_nr);
          if (!short_multipliers.contains(base_word) || algorithm != CA_Hybrid)
          {
            Ordinal_Word iword(base_word);
            maf.invert(&iword,iword);
            if (iword.length()==1 || !can_invert || !new_multipliers.contains(iword))
              new_multipliers.insert(base_word);
            else
              inverse_multipliers.insert(base_word);
          }
        }

        /* Construct the list of Operator_Words for the multipliers we need
           to create */
        words = new Operator_Word *[nr_words = new_multipliers.count()];
        for (word_nr = 0;word_nr < nr_words;word_nr++)
          if (set_work(word_nr,*new_multipliers.word(word_nr)))
            pending++;
      }

      if (pending || short_multipliers.count())
      {
        Operator_Word before;
        if (algorithm == CA_Hybrid)
        {
          /* If there is something to do construct the multiplier for
             useful words of length 2.
             This usually saves time, (though by no means always) */
          Ordinal_Word word(gm.base_alphabet);
          Sorted_Word_List swl(gm.base_alphabet);
          word.set_length(0);
          swl.add(word);
          /* Find out which pairs we need */
          for (word_nr = 0; word_nr < nr_words;word_nr++)
            words[word_nr]->pre_pair(swl);
          /* Also include any other short words specified */
          for (word_nr = 0; word_nr < short_multipliers.count();word_nr++)
            swl.add(Entry_Word(short_multipliers,word_nr));

          FSA_Simple * fsa = gm.composite(maf,swl);
          gm2 = new Multiplier(*fsa,true);
          for (word_nr = 0; word_nr < nr_words;word_nr++)
          {
            /* First perform as many pairs as possible. We do this
               to stop b*c being picked from a*b*c*d */
            if (words[word_nr]->pair(*this,&before))
              pending--;
          }
        }

        while (pending)
        {
          bool found = false;
          for (word_nr = 0; word_nr < nr_words;word_nr++)
          {
            /* If there are any words we can complete we should,
               because these composites are definitely needed */
            if (words[word_nr]->pending_count() == 1)
            {
              before = *words[word_nr];
              pending -= perform(before,can_invert);
              found = true;
            }
          }

          if (found)
            continue;
          /* It is unclear what to do next. Find a composite we can
           form now which is referred to at least as often as any other */
          work_db.empty();
          work_count = 0;
          for (word_nr = 0; word_nr < nr_words;word_nr++)
            words[word_nr]->enumerate_work(*this,false);
          extract_work(&before);
          pending -= perform(before,can_invert);
        }
      }
      // make sure any words of length 0 or 1 are dealt with
      for (word_nr = 0; word_nr < nr_words;word_nr++)
        make_multiplier(*new_multipliers.word(word_nr));
      // make sure any inverse words are dealt with
      multiplier_db.extract_inverse_multipliers(maf,inverse_multipliers,!require_labels);
    }

    ~Serial_Compositor()
    {
      for (Element_ID i = 0; i < nr_words;i++)
        delete words[i];
      delete [] words;
      if (gm2)
        delete gm2;
    }

    const FSA_Simple * multiplier(const Word & word)
    {
      const FSA_Simple * fsa = multiplier_db.multiplier(word);
      if (fsa)
        return fsa;
      /* It may be the case that a multiplier was listed as one of the
         desired short multipliers, and hence not constructed as an individual
         multiplier, but then requested again afterwards. So we need to be
         prepared to build it in this case */
      if (gm2)
        fsa = multiplier_db.make_multiplier(maf,word,*gm2);
      return fsa;
    }

    const Multiplier * short_multiplier() const
    {
      return gm2;
    }
    void fix_multipliers()
    {
      multiplier_db.fix_multipliers();
    }

  private:
    FSA_Simple * make_multiplier(const Word & word)
    {
      return multiplier_db.make_multiplier(maf,word,gm);
    }

    bool set_work(Element_ID word_nr,const Word & word)
    {
      /* On entry word is either the LHS or RHS word in one of the defining
         relations. We form its Operator_Word, and any single multipliers
         needed for it */

      words[word_nr] = new Operator_Word(word);
      Word_Length l = word.length();
      Word_List wl(gm.base_alphabet);

      for (Word_Length i = 0; i < l;i++)
      {
        Subword sw((Word &) word,i,i+1);
        Element_ID multiplier_nr = multiplier_db.enter(sw);
        multiplier_db.up_count(multiplier_nr,1);
      }
      return words[word_nr]->pending_count()!=0;
    }

    Element_Count perform_inner(const Operator_Word & before,size_t offset,
                                unsigned delta,
                                bool do_inverse = false,const FSA * inverse = 0)
    {
      /* On entry before contains a word of the form OP word1 OP word2 OP
         with the second OP at offset.
         We form the composite OP word1 word2 OP */

      /* Split out the words to compose */
      Ordinal_Word left(gm.base_alphabet,offset-1);
      left.set_multiple(0,before.data+1,offset-1);
      Element_ID m1 = multiplier_db.enter(left);
      Ordinal_Word right(gm.base_alphabet,before.length-offset-2);
      right.set_multiple(0,before.data+offset+1,before.length-offset-2);
      Element_ID m2 = multiplier_db.enter(right);
      /* Now form the composite and set its count */
      Ordinal_Word new_word(gm.base_alphabet);
      String_Buffer sb1,sb2,sb3;
      new_word = left + right;
      Element_ID multiplier_nr = multiplier_db.enter(new_word);
      FSA_Simple * fsa = multiplier_db.multiplier(multiplier_nr);
      if (!fsa)
      {
        if (!inverse && new_word.length() > 1 && can_invert)
        {
          Ordinal_Word inew_word(gm.base_alphabet);
          maf.invert(&inew_word,new_word);
          inverse = multiplier_db.multiplier(inew_word);
        }
        /* The composite does not exist yet */
        if (inverse) /* In this case it is the inverse of *inverse
                        Note, in this case the labels are WRONG, so
                        this is only a valid thing to do when we don't
                        care about  */
        {
          if (require_labels)
          {
            Multiplier m(* (FSA *) inverse,false);
            fsa = m.inverse(maf);
          }
          else
            fsa = FSA_Factory::transpose(*inverse);
        }
        else if (new_word.length() != 2 || !gm2)
        {
          /* hard case */
          new_word.format(&sb1);
          left.format(&sb2);
          right.format(&sb3);
          FSA_Simple * fsa1 = make_multiplier(left);
          FSA_Simple * fsa2 = make_multiplier(right);
          maf.container.progress(1,"Building multiplier %s from %s and %s\n",
                                 sb1.get().string(),sb2.get().string(),sb3.get().string());
          fsa = FSA_Factory::composite(*fsa1,*fsa2,require_labels);
        }
        else
        {
          /* easy case - extract from gm2 */
          fsa = gm2->composite(maf,new_word);
        }
        multiplier_db.set_multiplier(multiplier_nr,fsa);
      }
      multiplier_db.up_count(multiplier_nr,delta);
      /* garbage collect the old multipliers */
      multiplier_db.down_count(m1,delta);
      multiplier_db.down_count(m2,delta);
      if (do_inverse)
      {
        maf.invert(&left,left);
        maf.invert(&right,right);
        Operator_Word inverse(right,left);
        return perform(inverse,false,fsa);
      }
      return 0;
    }

    Element_Count perform(const Operator_Word & before,bool do_inverse,
                          FSA * inverse = 0)
    {
      /* On entry before contains a word of the form OP word1 OP word2 OP
         We form the composite OP word1 word2 OP and update all the words
         accordingly */

      Element_Count retcode = 0;
      Element_Count delta = 0;

      /* Find where the composite is formed*/
      size_t offset;
      for (offset = 1; offset < before.length;offset++)
        if (before.data[offset] == INVALID_SYMBOL)
          break;

      /* count up the number of times this operation affects word1 and word2
         usage, and the amount of work still left to do after this,
         and update the words */
      for (Element_ID word_nr = 0; word_nr < nr_words;word_nr++)
        if (words[word_nr]->perform(&delta,before.data,before.length,offset))
          retcode++;

      /* Split out the two words to compose and compose them*/
      if (delta)
        retcode += perform_inner(before,offset,delta,do_inverse,inverse);

      return retcode;
    }

    void add_work(const Ordinal * before,size_t length,bool)
    {
      /* add a possible work item */
      Element_ID item = work_db.find_entry(before,length*sizeof(Ordinal));
      if (item >= work_count)
      {
        counts[item] = 0;
        work_count++;
      }
      /* increment the count of the number of times this item is needed */
      counts[item]++;
    }

    void extract_work(Operator_Word * work)
    {
      /* find the most heavily used unformed composite that we can
         form now. If two are of equal rank pick the lightest */
      unsigned best_count = counts[0];
      size_t best_weight = work_db.get_key_size(0);
      Element_ID answer = 0;
      for (Element_ID i = 1; i < work_count;i++)
      {
        unsigned test = counts[i];
        size_t test_weight = work_db.get_key_size(i);
        if (test > best_count)
        {
          answer = i;
          best_count = test;
          best_weight = test_weight;
        }
        else if (test == best_count && test_weight <= best_weight)
        {
          answer = i;
          best_count = test;
          best_weight = test_weight;
        }
      }
      work->set((const Ordinal *) work_db.get_key(answer),
                work_db.get_key_size(answer)/sizeof(Ordinal));
    }

};

/*
Axiom_Checker checks that a multiplier that has passed the consistency
check is really associative.

All the work is done in the constructor!
*/

class Axiom_Checker
{
  private:
    bool ok;
  public:
    void check_relations(Word_List & axiom_words,Compositor &c)
    {
      /* Check the group relations using the multipliers contained
         in a compositor */
      ok = true;
      Ordinal_Word lhs_word(axiom_words.alphabet);
      Ordinal_Word rhs_word(axiom_words.alphabet);
      String_Buffer sb1,sb2;
      Container & container = axiom_words.alphabet.container;
      const Multiplier *gm2 = c.short_multiplier();
      c.fix_multipliers();

      Element_Count nr_words = axiom_words.count();
      for (Element_ID word_nr = 0; ok && word_nr < nr_words; word_nr += 2)
      {
        axiom_words.get(&lhs_word,word_nr);
        axiom_words.get(&rhs_word,word_nr+1);
        lhs_word.format(&sb1);
        rhs_word.format(&sb2);
        container.progress(1,"Checking relation %s=%s\n",sb1.get().string(),sb2.get().string());
        if (lhs_word.length() <= 2 && rhs_word.length() <= 2 && gm2)
        {
          /* Check the multipliers always occur together */
          Label_Count nr_labels = gm2->label_count();
          Word_List label_wl(gm2->label_alphabet());

          for (Label_ID label = 1;label < nr_labels;label++)
          {
            gm2->label_word_list(&label_wl,label);
            int found = 0;
            if (label_wl.contains(lhs_word))
              found++;
            if (label_wl.contains(rhs_word))
              found++;
            if (found == 1)
            {
              container.progress(1,"Check fails!\n");
              ok = false;
            }
          }
        }
        else
        {
          const FSA_Simple * fsa1 = c.multiplier(lhs_word);
          const FSA_Simple * fsa2 = c.multiplier(rhs_word);
          if (!fsa1 || !fsa2)
            MAF_INTERNAL_ERROR(container,("An FSA is unexpectedly missing!\n"));
          if (fsa1->compare(*fsa2)!=0)
          {
            container.progress(1,"Check fails!\n");
            ok = false;
          }
        }
      }
    }
    Axiom_Checker(const MAF & maf,const General_Multiplier & gm,Compositor_Algorithm algorithm,
                  bool check_inverses)
    {
      /* On entry gm_ is a general multiplier that is presumed to have
         passed the General_Multiplier::valid(). This means it defines
         a closed multiplication on the accepted language. However, it
         is not necessarily associative. On the other hand the multipliers
         themselves do form a group: the generators are the individual
         multipliers, which inevitably satisfy MxMX=M$ MXMx=M$ (so that
         it is not necessary to verify the inverse relators).
         The multipliers are correct if and only if the composites satisfy
         the group axioms. So to prove the multipiers correct we
         construct the multiplier for the LHS and RHS of each relation,
         and show they have the same accepted language. */

      const Linked_Packed_Equation *axiom;
      const Presentation_Data & pd = maf.properties();

      /* Construct the list of words for the Compositor */
      Word_List axiom_words(gm.base_alphabet);
      Word_List short_words(gm.base_alphabet);

      if (check_inverses)
      {
        Ordinal_Word ow(maf.alphabet);
        for (Ordinal g = 0; g < gm.base_alphabet.letter_count();g++)
        {
          ow.append(g);
          ow.append(maf.inverse(g));
          short_words.add(ow);
          axiom_words.add(ow);
          ow.set_length(0);
          short_words.add(ow);
          axiom_words.add(ow);
        }
      }

      for (axiom = maf.first_axiom();axiom;axiom = axiom->get_next())
      {
        Simple_Equation se(maf.alphabet,*axiom);
        if (se.lhs_word.value(0) < pd.coset_symbol)
        {
          if (se.lhs_length & se.rhs_length & 1)
          {
            /* try to reduce the amount of work by converting equations that
               have odd length on both sides to ones that are even on both */
            if (se.lhs_length > se.rhs_length)
            {
              se.rhs_word.append(maf.inverse(se.lhs_word.value(se.lhs_length-1)));
              se.lhs_word.set_length(--se.lhs_length);
              se.rhs_length++;
            }
            else
            {
              se.lhs_word.append(maf.inverse(se.rhs_word.value(se.rhs_length-1)));
              se.rhs_word.set_length(--se.rhs_length);
              se.lhs_length++;
            }
          }
          if (se.lhs_length <= 2)
            short_words.add(se.lhs_word);
          if (se.rhs_length <= 2)
            short_words.add(se.rhs_word);
          axiom_words.add(se.lhs_word);
          axiom_words.add(se.rhs_word);
        }
      }

      if (algorithm == CA_Parallel)
      {
        Parallel_Compositor pc(maf,gm,axiom_words,short_words,false);
        check_relations(axiom_words,pc);
      }
      else
      {
        Serial_Compositor sc(maf,gm,axiom_words,short_words,false,algorithm);
        check_relations(axiom_words,sc);
      }
    }

    bool passed() const
    {
      return ok;
    }
};

/**/

bool MAF::check_axioms(const General_Multiplier &gm,
                       Compositor_Algorithm algorithm,
                       bool check_inverses) const
{
  Axiom_Checker check(*this,gm,algorithm,check_inverses);
  return check.passed();
}

/**/

void MAF::subgroup_relators(const General_Multiplier &gm,
                            Word_List * relator_wl,
                            bool use_schreier_generators)
{
  /* find all the h-relators of the subgroup for a coset system.
     These are the h-labels of the initial states of the relator multipliers.
     We also need to add additional h-relators by computing the h-words for
     the subgroup generators themselves, which will not necessarily be
     reduced.

     If the parameter use_schreier_generators is true then the multiplier
     is a KBMAG style coset multiplier, not a MAF multiplier.

     This means that the h-labels are new generators for H and are simply
     h1=g-label of initial state 2,...hn-1=g-label of initial state n. In
     this case we do not need to compute the multiplier for the sub-generators.
  */

  Word_List multiplier_wl(gm.base_alphabet);
  const Alphabet &alphabet = gm.label_alphabet();

  Word_List label_wl(alphabet);
  Ordinal_Word word(alphabet);
  Ordinal first_sub_generator = (coset_symbol+1);
  Sorted_Word_List h_relators(alphabet);
  // we use Language_Size below because we are going to do arithmetic
  // involving a language size with this.
  Language_Size nr_schreier_generators = gm.nr_initial_states()-1;
  bool try_parallel = true;

  /* First build the list of relators */
  relator_wl->empty();
  for (Ordinal g = 0; g < coset_symbol;g++)
    if (inverse(g) != INVALID_SYMBOL)
    {
      word.set_length(0);
      word.append(g);
      word.append(inverse(g));
      multiplier_wl.add(word);
    }

  for (const Linked_Packed_Equation *axiom = first_axiom();axiom;
       axiom = axiom->get_next())
  {
    Simple_Equation se(this->alphabet,*axiom);
    if (se.lhs_word.value(0) < coset_symbol)
    {
      se.relator(&word,*this);
      multiplier_wl.add(word);
    }
  }
  Element_Count nr_relators = multiplier_wl.count();
  const FSA * wa = load_fsas(GA_WA);
  bool need_multiplier(nr_schreier_generators > 0);

  if (wa && need_multiplier)
  {
    Total_Length length = max_relator_length;
    int passes = 0;
    Language_Size expected_states = nr_schreier_generators;
    while (length > 1)
    {
      passes++;
      length = (length+1)/2;
      if (expected_states < LS_HUGE/expected_states)
        expected_states *= expected_states;
      else
        expected_states = LS_HUGE;
    }
    if (expected_states > Language_Size(MAX_STATES))
      try_parallel = false;
    Language_Size language_size = wa->language_size(true);
    if (language_size < LS_HUGE &&
        (language_size < expected_states/nr_relators ||
        language_size * nr_relators / nr_schreier_generators / gm.state_count() < passes*nr_schreier_generators))
    {
      need_multiplier = false;
      FSA::Word_Iterator wi(*wa);
      Language_Size remaining = language_size;
      for (State_ID si = wi.first();
           si;
           si = wi.next(true),remaining--)
      {
        if (wa->is_accepting(si))
        {
          Ordinal_Word test(alphabet);
          for (Element_ID i = 0; i < nr_relators;i++)
          {
            String_Buffer sb;
            test = wi.word + Entry_Word(multiplier_wl,i);
//          test.format(&sb);
//            printf("%s\n",sb.get());
            gm.reduce(&test,test,use_schreier_generators ? WR_SCHREIER_LABEL : WR_H_LABEL);
//          test.format(&sb);
//            printf("%s\n",sb.get());
            for (Word_Length j = 0;;j++)
              if (test.value(j) == coset_symbol)
              {
                if (Subword(test,j+1) != wi.word)
                  * (char *) 0 = 0;
                if (j != 0)
                  h_relators.insert(Subword(test,0,j));
                break;
              }
          }
        }
        container.status(2,1,"Multiplying coset representatives by relators"
                             " (" FMT_LS " of " FMT_LS " to do)\n",
                             remaining,language_size);
      }
    }
  }

  /* Now compute the multipliers and find their h-labels, which
     are necessarily relators */
  if (need_multiplier)
  {
    Compositor *c;
    if (try_parallel)
      c = new Parallel_Compositor(*this,gm,multiplier_wl,*relator_wl);
    else
      c = new Serial_Compositor(*this,gm,multiplier_wl,*relator_wl,true);
    Element_Count nr_relators = multiplier_wl.count();
    for (Element_ID relator_nr = 0; relator_nr < nr_relators;relator_nr++)
    {
      const FSA_Simple * fsa = c->multiplier(Entry_Word(multiplier_wl,relator_nr));
      State_Subset_Iterator ssi;
      State_ID si;
      for (si = fsa->initial_state(ssi,true);si;si = fsa->initial_state(ssi,false))
      {
        if (fsa->label_word_list(&label_wl,fsa->get_label_nr(si)))
        {
          Element_Count nr_labels = label_wl.count();
          for (Element_ID word_nr = 0;word_nr < nr_labels;word_nr++)
          {
            label_wl.get(&word,word_nr);
            if (word.length() && word.value(0) >= first_sub_generator)
              h_relators.insert(word);
          }
        }
      }
    }
    delete c;
  }

  /* Now build the list of g-words for the subgroup generators and
     compute their h-words. */
  if (!use_schreier_generators)
  {
    Ordinal nr_generators = generator_count();
    multiplier_wl.empty();
    State_Subset_Iterator ssi;
    Sorted_Word_List h_generators(alphabet);
    Word_List h_multipliers(alphabet);

    for (Ordinal h = first_sub_generator; h < nr_generators;h++)
    {
      word.set_length(1);
      word.set_code(0,h);
      h_generators.insert(word);
    }

    for (State_ID si = gm.initial_state(ssi,true);si;si = gm.initial_state(ssi,false))
    {
      gm.label_word_list(&label_wl,gm.get_label_nr(si));
      Element_Count count = label_wl.count();
      for (Element_ID word_nr = 0;word_nr < count;word_nr++)
        h_generators.remove(Entry_Word(label_wl,word_nr));
    }

    for (Ordinal h = first_sub_generator; h < nr_generators;h++)
    {
      word.set_length(1);
      word.set_code(0,h);
      if (h_generators.contains(word))
      {
        h_multipliers.add(word);
        word.set_code(0,coset_symbol);
        word += Ordinal_Word(gm.base_alphabet,group_word(h));
        gm.reduce(&word,word,WR_H_LABEL);
        Word_Length l = word.length();
        word.set_code(l-1,inverse(h));
        h_relators.insert(word);
      }
    }
  }
  else
  {
    /* The code in this else is not strictly necessary.
       Any extra axioms generated cannot be necessary in theory, but
       may possibly be useful. simplify can sometimes do better when
       this code is present.
       We have to miss out state 1 because otherwise trivial g-generators
       will cause a spurious H-relator to be created
    */
    State_Subset_Iterator ssi;
    for (State_ID si = gm.initial_state(ssi,true);
         (si = gm.initial_state(ssi,false))!=0;)
    {
      gm.label_word_list(&label_wl,gm.get_label_nr(si));
      Element_Count count = label_wl.count();
      for (Element_ID word_nr = 0;word_nr < count;word_nr++)
      {
        Ordinal_Word hgen(Entry_Word(label_wl,word_nr));
        if (hgen.length() && hgen.value(0) < coset_symbol)
        {
          /* We calculate the free inverse of the G-word for the
             Schreier generator h, Clearly this is equal to h^-1.
             So when we reduce it we will get some H-word that is equal to
             h^-1, and we can add another axiom for it. This might take some
             time and achieve nothing, so it is not clear this code should be
             here. Possibly it should be optional - more testing is needed */
          invert(&word,hgen);
          gm.reduce(&word,word,WR_SCHREIER_LABEL);
          Word_Length l = word.length();
          word.set_length(l-1); /* Some H-word equal to h^-1 */
          word.append(si-1 + coset_symbol);/* *h to get new relator */
          h_relators.insert(word);
        }
      }
    }
  }
  *relator_wl = h_relators;
}

/**/

FSA_Simple * Multiplier::composite(const MAF & maf,
                                   const Word_Collection & new_multipliers) const
{
  Word_List short_wl(new_multipliers.alphabet);
  Parallel_Compositor pc(maf,*this,new_multipliers,short_wl,true);
  return pc.take();
}

/**/

FSA_Simple * Multiplier::composite(const MAF & maf,
                                   const Word & new_multiplier) const
{
  Word_List wl(new_multiplier.alphabet(),1);
  Word_List short_wl(new_multiplier.alphabet());
  wl.add(new_multiplier);
  Parallel_Compositor pc(maf,*this,wl,short_wl,true);
  return pc.take();
}

/**/

General_Multiplier::General_Multiplier(const General_Multiplier &other) :
  Multiplier(other),
  multiplier_nr(0)
{
  set_multiplier_nrs();
}

/**/

General_Multiplier::General_Multiplier(FSA_Simple &other,bool owner_) :
  Multiplier(other,owner_),
  multiplier_nr(0)
{
  set_multiplier_nrs();
}

/**/

bool General_Multiplier::label_for_generator(Label_ID label,Ordinal g) const
{
  return label_contains(label,multiplier_nr[g]);
}

/**/

void General_Multiplier::set_multiplier_nrs()
{
  Ordinal_Word word(base_alphabet,1);
  Ordinal nr_generators = base_alphabet.letter_count();

  word.set_length(0);
  if (!multiplier_nr)
    multiplier_nr = new Element_ID[nr_generators+1];
  is_multiplier(word,&multiplier_nr[nr_generators]);
  word.set_length(1);
  for (Ordinal g = 0; g < nr_generators;g++)
  {
    word.set_code(0,g);
    is_multiplier(word,&multiplier_nr[g]);
  }
}

/**/

General_Multiplier::~General_Multiplier()
{
  delete [] multiplier_nr;
}
