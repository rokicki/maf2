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


// $Log: maf_dr.cpp $
// Revision 1.15  2010/06/29 11:21:21Z  Alun
// seen_at needed in right_shortlex case for groups as well as coset systems
// Revision 1.14  2010/06/10 13:57:35Z  Alun
// All tabs removed again
// Revision 1.13  2010/04/15 17:28:36Z  Alun
// size_t replaced by Element_ID in various places to save memory in 64-bit
// version.
// seen_at is now used in shortlex case as well if we have a coset word-difference
// machine, because reduction was not always correct in that case previously
// Revision 1.12  2009/11/08 22:18:55Z  Alun
// Type corrected
// Revision 1.11  2009/10/10 09:56:24Z  Alun
// typo in comment corrected
// Revision 1.10  2009/09/14 06:30:27Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.9  2008/11/12 00:28:14Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.12  2008/11/12 01:28:14Z  Alun
// coset reduction could get wrong initial state in non-shortlex case
// Revision 1.11  2008/10/24 08:44:43Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.10  2008/10/12 19:47:14Z  Alun
// Reduction to longer words was unreliable
// Revision 1.9  2008/10/09 16:51:14Z  Alun
// In case where coset dm lacks h generators the h word translation was previously to the right of the coset symbol
// Simplified coset reduction generally
// Revision 1.8  2008/09/20 09:22:03Z  Alun
// Final version built using Indexer.
// Revision 1.3  2007/11/15 22:58:08Z  Alun
//

/* See maf_dr.h for an introduction to the classes in this module.

   Diff_Reduce() caters for both shortlex and non-shortlex orderings. The
   code for the ordinary shortlex case is simpler because whenever we find
   p(u)^-1*h1*v1 = p(u)^-1*h2*v2 we can be sure that whichever of v1 and v2
   is found second is no better than the other word, so we do not need to
   keep it. Similarly when we find an equality we can easily tell whether or
   not it is a reduction. In the coset case a reduction found later might
   actually be better than an earlier one. In this case we don't rate
   reductions, but just classify them according to the "greater than" state.
   This means we might not get the best reduction first time, but won't
   miss reductions.

   In the case of non shortlex orderings we have a lot of extra things to
   worry about. In the first place we cannot enumerate v words in an order
   that allows us to be sure of getting the best v word first. So when we
   see a repeated difference we have to compare the two v words and decide
   which to keep. When we find an equality we need to check whether it is
   a reduction. Finally we need to remember that reduction can increase
   length, so that the process of applying a reduction is more complex, and
   also we have to carry on reading letters after the u word is exhausted.

   For the time being I have not attempted to make any optimisations for
   non shortlex orderings since I don't want this code to be concerned with
   the details of the orderings. If this proves to be desirable the correct
   way to go about things would be to use the Alphabet:gt_ methods
   to keep track of which vwords to keep.
*/
#include "mafbase.h"
#include "mafword.h"
#include "fsa.h"
#include "container.h"
#include "maf.h"
#include "maf_dr.h"
#include "maf_el.h"

/**/

const Word_Length LHS_Better = WHOLE_WORD;
const Word_Length LHS_Equal = WHOLE_WORD-1;
const Word_Length RHS_Better = 0;

struct Diff_Reduce::Node
{
  Element_ID prefix; /* index into previous level of Node_List */
  Ordinal rvalue;
  Word_Length better; /* LHS_Better means no padding symbols read and
                         candidate is currently worse than current word
                         LHS_Equal means candidate is equal to current word
                         so far.
                         RHS_Better means candidate is of equal length so
                         far to current word, but is a better word.
                         Other positive value n means padding symbol read n
                         times, so candidate is shorter (hence better) than
                         the current word.
                         Note that if ever we get to a reduction, the value
                         of better is equal to the number of padding symbols
                         read, and so represents the reduction in length.

                         In the non shortlex case better just indicates
                         the number of padding characters and we have
                         to extract the word to see if it is a reduction.
                      */
  State_ID dm_si;
};

struct Diff_Reduce::Node_List
{
  State_Count nr_allocated;
  State_Count nr_nodes;
  Ordinal lvalue;
  Ordinal rvalue;
  Node * node;
  Node_List() :
    nr_allocated(0),
    nr_nodes(0),
    lvalue(INVALID_SYMBOL),
    rvalue(INVALID_SYMBOL),
    node(0)
  {}
  void add_state(Element_ID prefix,Ordinal rvalue,State_ID dm_si,
                 Word_Length better,Element_Count max_states)
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
    node[nr_nodes].dm_si = dm_si;
    node[nr_nodes].better = better;
    nr_nodes++;
  }
  void replace_state(Element_ID prefix,Ordinal rvalue,State_ID dm_si,
                     Word_Length better,Element_ID node_nr)
  {
    node[node_nr].prefix = prefix;
    node[node_nr].rvalue = rvalue;
    node[node_nr].dm_si = dm_si;
    node[node_nr].better = better;
  }
};

/**/

Diff_Reduce::Diff_Reduce(const FSA * difference_machine) :
  dm(*difference_machine),
  max_depth(0),
  stack(0),
  valid_length(0),
  seen(0),
  seen_at(0),
  nr_seen(0),
  equality_si(difference_machine->accepting_state()),
  initial_si(difference_machine->initial_state()),
  is_right_shortlex(difference_machine->base_alphabet.order_type() == WO_Right_Shortlex),
  is_shortlex(is_right_shortlex || difference_machine->base_alphabet.order_is_effectively_shortlex())
{
  can_lengthen = !is_shortlex && !dm.base_alphabet.order_is_geodesic();
  if (dm.alphabet_size() != dm.base_alphabet.product_alphabet_size() ||
      !dm.is_accepting(equality_si))
    MAF_INTERNAL_ERROR(dm.container,
                     ("Bad FSA specified as dm in Diff_Reduce\n"));
}

/**/

Diff_Reduce::~Diff_Reduce()
{
  if (max_depth)
  {
    for (Word_Length i = 0; i < max_depth;i++)
      if (stack[i].node)
        delete [] stack[i].node;
    delete [] stack;
    if (seen)
      delete [] seen;
    if (seen_at)
      delete [] seen_at;
  }
}

/**/

void Diff_Reduce::invalidate()
{
  /* This method is needed in the case where the dm attached to the
     class is being modified. Diff_Reduce itself cannot deal with this
     but the user of the class can call invalidate() if it knows that
     changes have occurred since the last call to reduce().
     This is mainly for use by Strong_Diff_Reduce() */
  valid_length = dm.has_multiple_initial_states() ? INVALID_LENGTH : 0;
}

/**/

void Diff_Reduce::extract_word(Ordinal_Word * word,Element_ID node_nr,
                               Word_Length full_length,
                               State_ID * initial_state,bool cached)
{
  Word_Length padding = stack[full_length].node[node_nr].better;
  /* test below is provided in case extract_word() is ever used in the
     shortlex case. It is not required at the moment */
//  if (padding >= LHS_Equal)
//    padding = 0;
  Word_Length l = full_length-padding;
  /* previous is used in the case where cached is true. When we
     get to a point where the previous word and the one being read now
     have the same ancestor then we know that the earlier characters
     are correct */
  State_ID previous = node_nr -1;
  if (cached && !node_nr)
    cached = false;
  word->allocate(l,cached);
  Ordinal * values = word->buffer();
  while (l)
  {
    Ordinal value = stack[full_length].rvalue = stack[full_length].node[node_nr].rvalue;
    node_nr = stack[full_length].node[node_nr].prefix;
    if (value != PADDING_SYMBOL)
      values[--l] = value;
    if (cached)
    {
      previous = stack[full_length--].node[previous].prefix;
      if (previous == node_nr)
        break;
    }
    else
      full_length--;
  }
  if (stack[0].nr_nodes != 1)
  {
    while (full_length)
      node_nr = stack[full_length--].node[node_nr].prefix;
  }
  else
    node_nr = 0;
  *initial_state = stack[0].node[node_nr].dm_si;
}

/**/

unsigned Diff_Reduce::reduce(Word * answer,const Word & start_word,
                             const unsigned flags,const FSA * wa)
{
  /* This function uses a word-difference FSA to reduce a word.
     It should always be able to reduce a word at least as well as a
     Rewriter_Machine, assuming all the states and transitions that can
     be deduced from the latter's equations are present.
     It works by constructing the set of all potential equations for
     each leading subword of the word being reduced and remembering the best
     potential equation for each difference.

     This method differs considerably from KBMAG because it does not care
     where padding characters come in the v word. It is perfectly happy
     to accept any number of padding characters anywhere. The reason for this
     is that although we can be sure that if our word-difference machine is
     complete we could get by without doing this, there is no obvious reason
     why this should be the case when the word-difference machine is incomplete.
     In fact this is also helpful when calculating the word-acceptor, because
     if we find Uv1=Uv2 and v2 < v1, then we would like v2 to keep just v2
     and not v1 in the state stack of Diff_Reduce or the word-acceptor, whereas
     if we disallow interior padding we cannot do so.

     When the word-difference machine is complete we will usually want to
     reduce words with the multiplier instead, as this should be much faster.
     Since any v word with a pad character is certainly a reduction (at least
     in the shortlex case), it seems silly to not spot such reductions. On
     difficult presentations this does seem to help to find the missing word
     differences.

     The potential substitutions are calculated approximately in shortlex
     order so any reduction we find is usually the best.
  */
  Word_Length length = start_word.length();
  unsigned retcode = 0;
  const Alphabet & alphabet = dm.base_alphabet;
  const Ordinal nr_generators = alphabet.letter_count();
  const State_Count nr_differences = dm.state_count();
  Ordinal rvalue = PADDING_SYMBOL; // initialised to shut up compilers
  Word_Length prefix_length = 0;
  Ordinal_Word initial_part(dm.label_alphabet());
  Ordinal_Word *subgroup_word = 0;
  bool need_seen_at = false;

  if (answer != &start_word)
  {
    answer->set_length(length);
    word_copy(*answer,start_word,length);
  }
  Ordinal * values = answer->buffer();
  if (!length)
    return false;
  if (flags & WR_PREFIX_ONLY)
  {
    length--;
    rvalue = values[length];
  }

  /* See whether this is a coset word reduction. We can work out what
     to do, and don't need any special flags or extra parameters to handle
     it! We are relying on the user not to pass us anything too unexpected
     however.
  */

  if (values[0] >= nr_generators)
  {
    while (values[prefix_length] > nr_generators) // skip the H word
      prefix_length++;
    subgroup_word = &initial_part;
    *subgroup_word = Subword(*answer,0,prefix_length);
    if (values[prefix_length] == nr_generators)  // skip the _H
      prefix_length++;
  }

  if (wa)
  {
    if (&wa->base_alphabet != &dm.base_alphabet ||
        wa->alphabet_size() != dm.base_alphabet.letter_count())
      MAF_INTERNAL_ERROR(dm.container,
                         ("Bad FSA specified as wa in Diff_Reduce\n"));
    if (flags & DR_CHECK_FIRST && wa->accepts(values+prefix_length,
                                                    length-prefix_length))
      return false;
  }

  if (prefix_length)
  {
    *answer = Subword(*answer,prefix_length,length);
    length -= prefix_length;
    values = answer->buffer();
  }

  if (!max_depth)
  {
    /* Create the stack if need be */
    max_depth = length+1;
    stack = new Node_List[max_depth];
    valid_length = INVALID_LENGTH;
  }

  if (valid_length == INVALID_LENGTH)
  {
    /* Set up the initial state(s) */
    stack[0].nr_nodes = 0;
    if (!dm.has_multiple_initial_states())
    {
      nr_initial_states = 1;
      stack[0].add_state(0,PADDING_SYMBOL,dm.initial_state(),
                         is_shortlex ? LHS_Equal : 0,1);
    }
    else
    {
      State_Subset_Iterator ssi;
      State_ID si;
      nr_initial_states = dm.nr_initial_states();
      for (si = dm.initial_state(ssi,true);si;si = dm.initial_state(ssi,false))
        stack[0].add_state(0,PADDING_SYMBOL,si,
                           is_shortlex ? LHS_Equal : 0,
                           nr_initial_states);
    }
    valid_length = 0;
  }

  /* If prefix length is non zero then we are reducing an h*_H*g word
     and include all the initial states. Otherwise we are doing an
     ordinary g word reduction and only include the first initial state.  */
  need_seen_at = nr_initial_states > 1 || is_right_shortlex;
  if (subgroup_word)
  {
    if (stack[0].nr_nodes != nr_initial_states)
      valid_length = 0;
    stack[0].nr_nodes = nr_initial_states;
  }
  else
  {
    if (stack[0].nr_nodes != 1)
      valid_length = 0;
    stack[0].nr_nodes = 1;
  }

  if (nr_seen != nr_differences)
  {
    if (is_shortlex)
    {
      if (seen)
        delete [] seen;
      seen = new char[nr_seen = nr_differences];
      memset(seen,0,nr_differences);
    }

    if (need_seen_at || !is_shortlex)
    {
      if (seen_at)
        delete [] seen_at;
      seen_at = new State_ID[nr_seen = nr_differences];
      memset(seen_at,0,nr_differences*sizeof(State_ID));
    }
  }

  if (is_shortlex)
  {
    if (!subgroup_word && !is_right_shortlex)
      need_seen_at = false;

    for (Word_Length i = 0; i < length;)
    {
      Ordinal lvalue = values[i];
      if (i + 1 >= max_depth)
      {
        max_depth = length+1;
        Node_List * new_stack = new Node_List[max_depth];
        for (Word_Length j = 0; j <= i;j++)
          new_stack[j] = stack[j];
        delete [] stack;
        stack = new_stack;
      }
      bool reduced = false;
      if (i >= valid_length || lvalue != stack[i+1].lvalue)
      {
        Node_List & current = stack[i];
        Node_List & next = stack[i+1];
        next.lvalue = lvalue;

        if (lvalue >= nr_generators)
          MAF_INTERNAL_ERROR(dm.container,("Bad word passed to Diff_Reduce. Word contains Ordinal %d\n",lvalue));
        next.nr_nodes = 0;

        State_Count max_states = min((State_Count)
                                     (current.nr_nodes*(nr_generators+1)),
                                     nr_differences);
        for (State_Count prefix = 0;prefix < current.nr_nodes; prefix++)
        {
          State_ID new_dm_si = dm.new_state(current.node[prefix].dm_si,
                                            alphabet.product_id(lvalue,PADDING_SYMBOL),
                                            false);
          Word_Length better = current.node[prefix].better;
                   /* && below is to exclude the extraordinary case where
                        we have already managed to read LHS_Equal-1 padding
                        characters, and are trying to read another. */
          if (new_dm_si && better != LHS_Equal-1)
          {
            if (better >= LHS_Equal)
              better = 1;
            else
              better++;
            if (new_dm_si == equality_si)
            {
              if (!(flags & WR_CHECK_ONLY))
                apply_reduction(values,&length,subgroup_word,i,
                                better,prefix,PADDING_SYMBOL);
              reduced = true;
              retcode++;
              break;
            }
            else if (!seen[new_dm_si])
            {
              next.add_state(prefix,PADDING_SYMBOL,new_dm_si,better,max_states);
              seen[new_dm_si] = 3;
            }
          }
        }

        if (!reduced && current.nr_nodes)
        {
          Ordinal g2 = 0;
          State_Count prefix = 0;
          bool normal_order = (i != 0 || current.nr_nodes==1) && !is_right_shortlex;
          /* We extend all the candidate reduction words by each generator.
             In the coset case we need to be careful to enumerate all the
             words starting with a particular generator, otherwise our words
             won't be listed in the right order, and, what is worse, we
             might exclude the only possible reduction, because seen[] might
             have been set for a difference by a worse word.
             So the loop for going through the candidates is a little complex.
             In fact, in the coset case our enumeration method does not quite
             work anyway because our candidate words will be arranged something
             like
             aa ab haa hab jaa jab ba bb hba hbb
             when what is actually needed is
             aa haa jaa ab hab jab ba hba bb hbb.
             So we have to test seen.
          */
          for (;;)
          {
            State_ID old_dm_si = current.node[prefix].dm_si;
            State_ID new_dm_si = dm.new_state(old_dm_si,
                                              alphabet.product_id(lvalue,g2),
                                              normal_order);
            if (old_dm_si == 1 && !new_dm_si && lvalue==g2)
              new_dm_si = 1; /* kludge for machines without initial (x,x) transitions */

            if (new_dm_si)
            {
              Word_Length better = current.node[prefix].better;
              if (g2 != lvalue &&
                  (better == LHS_Equal || is_right_shortlex &&
                  (better == LHS_Better || better == RHS_Better)))
                better = g2 < lvalue ? RHS_Better : LHS_Better;
              char seen_flag;
              if (better == LHS_Better)
                seen_flag = 1;
              else if (better == LHS_Equal)
                seen_flag = 2;
              else
                seen_flag = 3;
              bool wanted = seen_flag > seen[new_dm_si];
              if (new_dm_si == equality_si)
              {
                if (better == LHS_Better)
                  wanted = false; /* the word is worse and so can be discarded */
                else if (better != LHS_Equal)
                {
                  if (!(flags & WR_CHECK_ONLY))
                    apply_reduction(values,&length,subgroup_word,
                                    i,better,prefix,g2);
                  reduced = true;
                  retcode++;
                  break;
                }
                /* if we get here then v is identical to u so far and
                   needs to be included in the list of states */
              }

              if (wanted)
              {
                if (!seen[new_dm_si])
                {
                  next.add_state(prefix,g2,new_dm_si,better,max_states);
                  seen[new_dm_si] = seen_flag;
                  if (need_seen_at)
                    seen_at[new_dm_si] = next.nr_nodes;
                }
                else
                {
                  next.replace_state(prefix,g2,new_dm_si,better,seen_at[new_dm_si]-1);
                  seen[new_dm_si] = seen_flag;
                }
              }
            }

            if (normal_order)
            {
              if (++g2 == nr_generators)
              {
                g2 = 0;
                if (++prefix == current.nr_nodes)
                  break;
              }
            }
            else
            {
              if (++prefix == current.nr_nodes)
              {
                prefix = 0;
                if (++g2 == nr_generators)
                  break;
              }
            }
          }
        }
        /* We always clear out seen after each loop to avoid
           constant memsets() on it, which could be slow if
           there are very many word-differences */
        {
          for (State_Count j = 0;j < next.nr_nodes;j++)
            seen[next.node[j].dm_si] = 0;
          if (need_seen_at)
            for (State_Count j = 0;j < next.nr_nodes;j++)
              seen_at[next.node[j].dm_si] = 0;
        }
        if (reduced)
        {
          Word_Length save = i;
          i = valid_length;
          if (wa && wa->accepts(values,length) || flags & WR_ONCE+WR_CHECK_ONLY)
          {
            valid_length = save;
            break;
          }
        }
        else
          valid_length = ++i;
      }
      else
        i++;
    }
  }
  else
  {
    Word & uword = *answer; /* current state of reduced word
                                            ignoring any prefix */
    Ordinal_Word vword(alphabet,length); /* new candidate for difference history */
    Ordinal_Word other_vword(alphabet,length); /* equal (or coset equal) vword */
    State_ID istate;

    values = uword.buffer();
    Word_Length limit = can_lengthen && !(flags & WR_ASSUME_L2) ? 1 : length;
    if (limit <= length)
    {
      for (Word_Length i = 0;;)
      {
        Ordinal lvalue = i < limit ? values[i] : PADDING_SYMBOL;
        if (i + 1 >= max_depth)
        {
          max_depth = max(Word_Length(i+1),length)+1;
          Node_List * new_stack = new Node_List[max_depth];
          for (Word_Length j = 0; j <= i;j++)
            new_stack[j] = stack[j];
          delete [] stack;
          stack = new_stack;
        }
        Word_Length used = i+1;
        if (used > limit)
          used = limit;
        bool reduced = false;
        if (i >= valid_length || lvalue != stack[i+1].lvalue)
        {
          Node_List & current = stack[i];
          Node_List & next = stack[i+1];
          next.lvalue = lvalue;
          bool found = false;
          valid_length = i;
          if (lvalue >= nr_generators)
            MAF_INTERNAL_ERROR(dm.container,("Bad word passed to Diff_Reduce\n"));
          next.nr_nodes = 0;
          State_Count max_states = min((State_Count)
                                       (current.nr_nodes*(nr_generators+1)),
                                       nr_differences);
          vword.allocate(i+1);
          if (lvalue == PADDING_SYMBOL && !(flags & DR_ASSUME_DM_CORRECT))
          {
            /* A particularly nasty aspect of the non-shortlex case is
               that once u has been exhausted we might see an infinite
               chain of v words v1,v2,... such that vn < u and d^(u,vn) is
               never 0. In this case the d values must eventually recur.
               Rather than looking back into previous levels of the
               history to detect this I pad words to cause the history to
               be repeated at the next stage. This wastes some memory and
               time, but it makes the test for whether to include a state
               a lot simpler.
               The found variable is set true when a proper state has been
               found, so we will eventually break out once only padded
               states remain.
               In fact this should not happen, and won't if we use the
               "correct" difference machine, because the $,g transitions
               are fixed for each state. However, if the difference machine
               contains multiplier transitions, or computed ones, then
               it will.
            */
            for (State_Count prefix = 0;prefix < current.nr_nodes;prefix++)
            {
              Ordinal padding = current.node[prefix].better+1;
              State_ID old_dm_si = current.node[prefix].dm_si;
              State_ID new_dm_si = old_dm_si < 0 ? -old_dm_si : old_dm_si;
              if (i == limit)
              {
                extract_word(&vword,prefix,i,&istate,true);
                if (Subword(uword,0,limit).compare(vword) <= 0)
                  new_dm_si = 0; //vword cannot lead to a reduction
              }
              if (new_dm_si)
              {
                next.add_state(prefix,PADDING_SYMBOL,-new_dm_si,padding,
                               max_states);
                seen_at[new_dm_si] = next.nr_nodes;
              }
            }
          }

          Ordinal * vvalues = vword.buffer();
          for (State_Count prefix = 0;prefix < current.nr_nodes;prefix++)
          {
            extract_word(&vword,prefix,i,&istate,true);
            State_ID old_dm_si = current.node[prefix].dm_si;
            for (Ordinal g2 = lvalue == PADDING_SYMBOL ? 0 : PADDING_SYMBOL;
                 g2 < nr_generators;g2++)
            {
              Transition_ID ti = alphabet.product_id(lvalue,g2);
              State_ID new_dm_si = old_dm_si < 0 ? 0 :
                                                   dm.new_state(old_dm_si,ti);
              if (old_dm_si == 1 && !new_dm_si && lvalue==g2)
                new_dm_si = 1; /* kludge for machines without initial (x,x)
                                  transitions */

              if (new_dm_si)
              {
                Word_Length padding = current.node[prefix].better;
                bool wanted = !seen_at[new_dm_si];

                if (g2 != PADDING_SYMBOL)
                {
                  vword.adjust_length(i+1-padding);
                  vvalues[i - padding] = g2;
                }
                else
                  padding++;

                if (i >= limit && Subword(uword,0,limit).compare(vword) <= 0)
                  continue; //vword cannot lead to a reduction

                if (new_dm_si == equality_si)
                {
                  int cmp = i >= limit? 1 : Subword(uword,0,used).compare(vword);
                  if (cmp < 0)
                  {
                    continue; /* the word is worse than what it would replace
                                 and so can be discarded */
                  }
                  else if (cmp > 0)
                  {
                    /* we have found a reduction */
                    if (!(flags & WR_CHECK_ONLY))
                    {
                      Word_Length l = used < vword.length() ?
                                                    used : vword.length();
                      for (i = 0;i < l;i++) /* see where the change began*/
                        if (vword.value(i) != values[i])
                          break;
                      if (can_lengthen && limit > i+1)
                        limit = i+1;
                      if (used < length)
                        uword = vword + Subword(uword,used,length);
                      else
                        uword = vword;
                      values = uword.buffer();
                      length = uword.length();
                      if (limit > length)
                        limit = length;
                      if (istate != initial_si)
                      {
                        Ordinal_Word next_prefix(subgroup_word->alphabet());
                        if (dm.label_word(&next_prefix,istate,1))
                          *subgroup_word += next_prefix;
                        else if (dm.label_word(&next_prefix,istate,0))
                          *subgroup_word += next_prefix;
                      }
                    }
                    reduced = true;
                    retcode++;
                    break;
                  }
                  /* if we get here then v is identical to u so far and
                     needs to be included in the list of states */
                }

                if (wanted)
                {
                  next.add_state(prefix,g2,new_dm_si,padding,max_states);
                  seen_at[new_dm_si] = next.nr_nodes;
                  found = true;
                }
                else
                {
                  State_ID other_istate;
                  extract_word(&other_vword,seen_at[new_dm_si]-1,i+1,&other_istate,false);
                  if (other_vword.compare(vword)==1)
                  {
                    next.replace_state(prefix,g2,new_dm_si,padding,
                                       seen_at[new_dm_si]-1);
                    found = true;
                  }
                }
              }
            }
            if (reduced)
              break;
          }
          /* We always clear out seen_at after each loop to avoid
             constant memsets() on it, which could be slow if
             there are very many word-differences */
          {
            for (State_Count j = 0;j < next.nr_nodes;j++)
            {
              State_ID dm_si = next.node[j].dm_si;
              seen_at[dm_si < 0 ? -dm_si : dm_si] = 0;
            }
          }
          if (reduced)
          {
            if (wa && wa->accepts(values,length) ||
                flags & WR_ONCE+WR_CHECK_ONLY)
              break;
            continue;
          }
          else
            valid_length = ++i;

          if (!found)
          {
            if (limit++ >= length)
              break;
            i = limit-1;
          }
        }
        else
          i++;
        if (valid_length >= length && !can_lengthen)
          break;
      }
    }
  }

  answer->set_length(length);
  if (flags & WR_PREFIX_ONLY)
  {
    answer->append(rvalue);
    length++;
  }

  if (subgroup_word)
  {
    if (subgroup_word->length() && subgroup_word->value(0) < nr_generators)
    {
      if (flags & DR_NO_G_PREFIX)
        subgroup_word->set_length(0);
      else
        reduce(subgroup_word,*subgroup_word);
    }

    prefix_length = subgroup_word->length()+1;
    if (length+prefix_length > MAX_WORD)
      prefix_length = 1;
    answer->set_length(length + prefix_length);
    values = answer->buffer();
    memmove(values + prefix_length,values,length*sizeof(Ordinal));
    memcpy(values,subgroup_word->buffer(),(prefix_length-1)*sizeof(Ordinal));
    values[prefix_length-1] = nr_generators;
  }
  return retcode;
}

/**/

void Diff_Reduce::apply_reduction(Ordinal * values, Word_Length *length,
                                  Ordinal_Word * subgroup_word,
                                  Word_Length nr_accepted, Word_Length nr_pads,
                                  State_Count prefix,Ordinal rvalue)
{
  /* We provisionally accepted nr_accepted symbols,
     and upon reading the next symbol decided to replace these
     nr_accepted+1 symbols with the v word.
     This may contain embedded padding symbols which we must remove */

  Word_Length substitution_length = nr_accepted + 1 - nr_pads;
  State_ID initial_state = initial_si;

  if (nr_pads)
  {
    // memcpy below is OK because this method is only used when the word
    // ordering method is effectively shortlex or right shortlex
    memcpy(values + substitution_length,values+nr_accepted+1,
           (*length - (nr_accepted+1))*sizeof(Ordinal));
    *length -= nr_pads;
  }
  valid_length = substitution_length;
  if (substitution_length)
  {
    if (rvalue != PADDING_SYMBOL)
      values[--valid_length] = rvalue;
    while (valid_length > 0)
    {
      Ordinal value = stack[nr_accepted].node[prefix].rvalue;
      if (stack[nr_accepted].node[prefix].better == LHS_Equal)
        break;
      prefix = stack[nr_accepted].node[prefix].prefix;
      if (value != PADDING_SYMBOL)
        values[--valid_length] = value;
      --nr_accepted;
    }
  }
  if (subgroup_word)
  {
    if (stack[nr_accepted].node[prefix].dm_si != initial_si)
    {
      /* In this case we must be doing a coset reduction, and must have
         started from one of the subgroup words, so we need to look back
         to the beginning of the stack to see where we started */
      while (nr_accepted)
        prefix = stack[nr_accepted--].node[prefix].prefix;
      initial_state = stack[0].node[prefix].dm_si;
    }
    /* The sensible thing to do is to ensure that our word
       difference machines have both G and H alphabet labels for the
       subgroup words. But if we are reducing using a KBMAG produced
       midiff file this information is missing.
    */
    if (initial_state != initial_si)
    {
      Ordinal_Word next_prefix(subgroup_word->alphabet());
      if (dm.label_word(&next_prefix,initial_state,1))
        *subgroup_word += next_prefix;
      else if (dm.label_word(&next_prefix,initial_state,0))
        *subgroup_word += next_prefix;
    }
  }
}

/**/

struct Diff_Equate::Node
{
  State_Count prefix;
  Ordinal rvalue;
  State_ID dm_si;
  State_ID wa_si;
};

struct Diff_Equate::Node_List
{
  State_Count nr_allocated;
  State_Count nr_nodes;
  Ordinal lvalue;
  Node * node;
  Node_List() :
    nr_allocated(0),
    nr_nodes(0),
    lvalue(-2),
    node(0)
  {}
  void add_state(State_Count prefix,Ordinal rvalue,State_ID dm_si,
                 State_ID wa_si,State_Count max_states);
};

/**/

void Diff_Equate::Node_List::add_state(State_Count prefix,Ordinal rvalue,State_ID dm_si,
                                       State_ID wa_si,State_Count max_states)
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
  node[nr_nodes].dm_si = dm_si;
  node[nr_nodes].wa_si = wa_si;
  nr_nodes++;
}

/**/

Diff_Equate::Diff_Equate(const FSA * difference_machine,
                         const FSA * word_acceptor) :
  dm(*difference_machine),
  wa(*word_acceptor),
  max_depth(0),
  stack(0),
  valid_length(0),
  seen(0),
  nr_seen(0),
  equality_si(difference_machine->accepting_state())
{
  if (dm.alphabet_size() != dm.base_alphabet.product_alphabet_size() ||
      !dm.is_accepting(equality_si))
    MAF_INTERNAL_ERROR(dm.container,
                     ("Bad FSA specified as dm in Diff_Equate\n"));
  if (&wa.base_alphabet != &dm.base_alphabet ||
      wa.alphabet_size() != wa.base_alphabet.letter_count())
    MAF_INTERNAL_ERROR(dm.container,
                       ("Bad FSA specified as wa in Diff_Equate\n"));
}

/**/

Diff_Equate::~Diff_Equate()
{
  if (max_depth)
  {
    for (Word_Length i = 0; i < max_depth;i++)
      if (stack[i].node)
        delete [] stack[i].node;
    delete [] stack;
    delete [] seen;
  }
}

/**/

bool Diff_Equate::equate(Ordinal_Word * lhs_word,
                         Ordinal_Word * rhs_word,
                         const Word & root_word)
{
  /* This function uses a word-difference FSA to find two
     accepted words that are in fact equal as group elements.
     If the word-difference machine is incorrect we may find that
     for three accepted words u,v1,v2 we have Uv1 and Uv2 are both
     equal to the same word-difference, so that v1==v2.

     Although the code is generally similar to Diff_Reduce::reduce()
     we do not care at all about finding words equal to u.
     u is simply a "catalyst" that allows us to discover an equation
     provable from the word-difference machine that we did not know about
     (and which therefore certainly contains an unknown primary difference)
  */

  Word_Length length = root_word.length();
  const Alphabet & alphabet = dm.base_alphabet;
  const Ordinal nr_generators = alphabet.letter_count();
  const State_Count nr_differences = dm.state_count();
  const State_Count nr_wa_states = wa.state_count();
  const State_ID twice_padded = nr_wa_states+1;
  const State_ID thrice_padded = nr_wa_states+2;
  const Ordinal * values = root_word.buffer();

  if (!max_depth)
  {
    max_depth = length+1;
    stack = new Node_List[max_depth];
    /* We are not currently dealing with the coset system case. In that case we
       would have Uh1v1==Uh2v2==d, which only implies h1v1==h2v2 not that
       v1==v2. But it is still the case that the coset word-acceptor should
       have rejected one of v1 and v2. As it stands the code can only cope
       with the case where the coset acceptor is accepting two genuinely equal
       words. */
//    if (dm.initial_type() == IT_Initial_Single)
    {
      stack[0].nr_allocated = stack[0].nr_nodes = 1;
      stack[0].node = new Node;
      stack[0].node[0].dm_si = dm.initial_state();
      stack[0].node[0].rvalue = -1;
      stack[0].node[0].prefix = 0;
      stack[0].node[0].wa_si = wa.initial_state();
    }
#if 0
    else
    {
      State_List sl;
      State_ID si;
      for (si = 1; si < nr_differences;si++)
        if (dm.is_initial(si))
          sl.append_one(si);
      State_List::Iterator sli(sl);
      int nr_initial_states = sl.count();
      for (si = sli.first();si;si = sli.next())
        stack[0].add_state(0,PADDING_SYMBOL,si,1,nr_initial_states);
    }
#endif
    seen = new State_ID[nr_seen = nr_differences];
    memset(seen,0,nr_differences*sizeof(State_ID));
  }
  if (nr_seen != nr_differences)
  {
    delete [] seen;
    seen = new State_ID[nr_seen = nr_differences];
    memset(seen,0,nr_differences*sizeof(State_ID));
  }

  for (Word_Length i = 0; i < length;i++)
  {
    Ordinal lvalue = values[i];
    if (i + 1 >= max_depth)
    {
      max_depth = length+1;
      Node_List * new_stack = new Node_List[max_depth];
      for (Word_Length j = 0; j <= i;j++)
        new_stack[j] = stack[j];
      delete [] stack;
      stack = new_stack;
    }
    if (i >= valid_length || lvalue != stack[i+1].lvalue)
    {
      Node_List & current = stack[i];
      Node_List & next = stack[i+1];
      valid_length = i;
      next.lvalue = lvalue;
      next.nr_nodes = 0;

      State_Count max_states = min((State_Count)
                              (current.nr_nodes*(nr_generators+1)),
                              nr_differences);
      for (State_Count prefix = 0;prefix < current.nr_nodes;prefix++)
      {
        State_ID new_dm_si = dm.new_state(current.node[prefix].dm_si,
                                     alphabet.product_id(lvalue,PADDING_SYMBOL),
                                          false);

        if (new_dm_si)
        {
          State_ID old_wa_si = current.node[prefix].wa_si;
          State_ID new_wa_si;
          if (old_wa_si < nr_wa_states)
            new_wa_si = nr_wa_states;
          else if (old_wa_si >= twice_padded)
            new_wa_si = thrice_padded;
          else
            new_wa_si = twice_padded;
          if (new_wa_si)
          {
            if (!seen[new_dm_si])
            {
              next.add_state(prefix,PADDING_SYMBOL,new_dm_si,new_wa_si,
                             max_states);
              seen[new_dm_si] = next.nr_nodes;
            }
            else
            {
              extract_equation(lhs_word,rhs_word,i,seen[new_dm_si]-1,prefix,
                               PADDING_SYMBOL);
              return true;
            }
          }
        }
      }

      for (State_Count prefix = 0;prefix < current.nr_nodes;prefix++)
      {
        State_ID old_wa_si = current.node[prefix].wa_si;
        if (old_wa_si < nr_wa_states)
        {
          State_ID old_dm_si = current.node[prefix].dm_si;
          for (Ordinal g2 = 0; g2 < nr_generators;g2++)
          {
            State_ID new_wa_si = wa.new_state(old_wa_si,g2);
            if (new_wa_si)
            {
              State_ID new_dm_si = dm.new_state(old_dm_si,
                                                alphabet.product_id(lvalue,g2));
              if (new_dm_si)
              {
                if (!seen[new_dm_si])
                {
                  next.add_state(prefix,g2,new_dm_si,new_wa_si,max_states);
                  seen[new_dm_si] = next.nr_nodes;
                }
                else
                {
                  extract_equation(lhs_word,rhs_word,i,seen[new_dm_si]-1,
                                   prefix,g2);
                  return true;
                }
              }
            }
          }
        }
      }
      /* We always clear out seen after each loop to avoid
         constant memsets() on it, which could be slow if
         there are very many word-differences */
      {
        for (State_Count j = 0;j < next.nr_nodes;j++)
          seen[next.node[j].dm_si] = 0;
      }
    }
  }
  return false;
}

/**/

void Diff_Equate::extract_equation(Ordinal_Word *lhs,Ordinal_Word * rhs,
                                   Word_Length prefix_length,
                                   State_Count next_index,State_Count current_index,
                                   Ordinal rvalue)
{
  const Node & rhs_node = stack[prefix_length+1].node[next_index];
  extract_word(rhs,prefix_length,rhs_node.prefix,rhs_node.rvalue);
  extract_word(lhs,prefix_length,current_index,rvalue);
  /* Clear out seen as we are going to return early */
  Node_List & next = stack[prefix_length+1];
  for (State_Count j = 0;j < next.nr_nodes;j++)
    seen[next.node[j].dm_si] = 0;
}

/**/

void Diff_Equate::extract_word(Ordinal_Word * word,
                               Word_Length prefix_length,State_Count prefix,
                               Ordinal rvalue)
{
  /* Compute the length of the word */
  Word_Length length = 0;
  if (rvalue != PADDING_SYMBOL)
    length++;
  Word_Length i = prefix_length;
  State_Count p = prefix;
  while (i)
  {
    Node & node = stack[i--].node[p];
    if (node.rvalue != PADDING_SYMBOL)
      length++;
    p = node.prefix;
  }

  /* Now extract it */
  word->allocate(length,false);
  Ordinal * values = word->buffer();
  i = prefix_length;
  p = prefix;
  if (rvalue != PADDING_SYMBOL)
    values[--length] = rvalue;
  while (i)
  {
    Node & node = stack[i--].node[p];
    if (node.rvalue != PADDING_SYMBOL)
      values[--length] = node.rvalue;
    p = node.prefix;
  }
}
