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


/*
$Log: maf_dr.h $
Revision 1.6  2010/04/15 17:24:12Z  Alun
Now uses Element_ID rather than size_t for indexing differences as this
saves a lot of memory in 64-bit builds (unless Element_ID is long long
of course)
Revision 1.5  2009/09/12 18:48:34Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.4  2008/10/24 07:41:54Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.5  2008/10/24 08:41:53Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.4  2008/10/09 12:27:02Z  Alun
Parameter removed from private method() apply_reduction()
Revision 1.3  2008/07/14 01:51:28Z  Alun
"Aug 2008 snapshot"
Revision 1.2  2007/11/15 22:58:12Z  Alun
*/
#pragma once
#ifndef MAF_DR_INCLUDED
#define MAF_DR_INCLUDED 1
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif
// classes referred to and defined elsewhere

class FSA;
class Word;
class Ordinal_Word;

// classes defined in this header
class Diff_Reduce;
class Diff_Equate;

/*
These two classes provide functionality for word-difference machines
and word-acceptors.

They are separate classes because it is useful for these methods
to be available to FSAs that are logically word-difference machines or
word-acceptors, but have various C++ types, for example factories
being built into other types of FSA.

Diff_Reduce() implements the quadratic time algorithm for word reduction
using a word-difference machine. However, to improve performance, it
remembers its state between calls, so that when it is called repeatedly
for similar words that are either not reducible, or have a longish common
prefix with their reduction there will be a considerable saving in time.

Diff_Equate() is used to discover equations that are implied, but not
recognised by the word-difference machine, i.e. triples of words u,v1,v2
such that Uv1==Uv2==d so that v1==v2, even if V1v2 is not accepted.
*/

/* flags for Diff_Reduce::reduce() */
const unsigned DR_CHECK_FIRST = 0x100; /* If specified, and wa is non-zero see
                                          if wa accepts word, and if so don't
                                          try to reduce word. */
const unsigned DR_ASSUME_DM_CORRECT = 0x400;
const unsigned DR_NO_G_PREFIX = 0x200;
const unsigned DR_ONCE = WR_ONCE;
const unsigned DR_PREFIX_ONLY = WR_PREFIX_ONLY; /* Remove last letter from word and then
                                      put it back once reduction is complete*/
const unsigned DR_ALTERNATE = 0x800; /* flag for Strong_Diff_Reduce() */
class Diff_Reduce : public Word_Reducer
{
  private:
    struct Node;
    struct Node_List;
  public:
    const FSA & dm;
  private:
    // We are going to cater for the situation where the initial and accepting
    // states are not the same. It would be advantageous for a word-difference
    // machine to have terminal accepting states by doing a built in and with
    // the shortlex automaton, and killing all the transitions out of its
    // accept states.
    const State_ID initial_si; // the initial state
    const State_ID equality_si;
    Word_Length max_depth;
    Word_Length valid_length;
    Node_List * stack;
    char * seen;
    Element_ID * seen_at;
    State_Count nr_seen;
    State_Count nr_initial_states;
    bool can_lengthen;
    const bool is_right_shortlex;
    const bool is_shortlex;
  public:
    Diff_Reduce(const FSA * difference_machine);
    ~Diff_Reduce();
    /* Invalidate() is needed in the case where the difference machine
         attached to the class is being modified. Diff_Reduce itself cannot
         deal with this but the user of the class can call invalidate() if
         it knows that changes have occurred since the last call to reduce().
         This is mainly for use by Strong_Diff_Reduce() */
    void invalidate();

    unsigned reduce(Word * answer,const Word & start_word,
                    unsigned flags = 0,const FSA * wa = 0);
  private:
    void apply_reduction(Ordinal * values, Word_Length *length,
                         Ordinal_Word * subgroup_prefix,
                         Word_Length nr_accepted, Word_Length nr_pads,
                         State_Count prefix,Ordinal rvalue);
    void extract_word(Ordinal_Word * word,Element_ID node_nr,
                      Word_Length full_length,State_ID * initial_state,bool cached);

};

/**/

class Diff_Equate
{
  private:
    struct Node;
    struct Node_List;
  public:
    const FSA & dm;
    const FSA & wa;
  private:
    const State_ID equality_si;
    Word_Length max_depth;
    Word_Length valid_length;
    Node_List * stack;
    State_ID * seen;
    State_Count nr_seen;
  public:
    Diff_Equate(const FSA * difference_machine,const FSA * word_acceptor);
    ~Diff_Equate();
    bool equate(Ordinal_Word * lhs,Ordinal_Word * rhs,
                const Word &catalyst_word);
  private:
    void extract_equation(Ordinal_Word * lhs,Ordinal_Word * rhs,
                          Word_Length prefix_length,State_Count next_index,
                          State_Count current_index,Ordinal rvalue);

    void extract_word(Ordinal_Word * word,
                      Word_Length prefix_length,State_Count prefix,
                      Ordinal rvalue);
};

#endif

