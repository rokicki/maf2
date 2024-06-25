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
$Log: maf_ew.h $
Revision 1.11  2010/06/10 13:58:07Z  Alun
All tabs removed again
Revision 1.10  2010/05/29 08:35:08Z  Alun
Changes for new style Node_Manager interface. All AE_ flags moved to here
from maf_we.h
Revision 1.9  2009/09/12 18:48:35Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.8  2008/09/02 18:14:30Z  Alun
"Early Sep 2008 snapshot"
Revision 1.6  2007/11/15 22:58:12Z  Alun
*/
#pragma once
#ifndef MAF_EW_INCLUDED
#define MAF_EW_INCLUDED 1
#ifndef MAFWORD_INCLUDED
#include "mafword.h"
#endif
#ifndef CERTIFICATE_INCLUDED
#include "certificate.h"
#endif
#ifndef NODELIST_INCLUDED
#include "nodelist.h"
#endif
#ifndef NODEBASE_INCLUDED
#include "nodebase.h"
#endif

// Classes referred to and defined elsewhere
class Node;
class Node_Manager;
class Rewriter_Machine;
typedef Node Equation_Node;

//Classes defined in this header
class Equation_Word;

/* flags for Rewriter_Machine::add_equation(),
             Working_Equation::normalise()
             Working_Equation::prepare_for_insert() */
const unsigned AE_KEEP_LHS = 1;  /* only reduce LHS to L2 */
const unsigned AE_REDUCE = 2;    /* reduce LHS and RHS - forced except on
                                    normalise() */
const unsigned AE_COPY_LHS = 4;  /* the RHS is just the known reduction of
                                    the LHS */
const unsigned AE_SECONDARY = 8; /* don't insert the equation if RHS and LHS
                                    have common prefix - normally this will
                                    be removed and remaining equation gets
                                    inserted. */
const unsigned AE_UPDATE = 16; /* add_equation() should call update_machine() */
const unsigned AE_MUST_SIMPLIFY = 32; /* call check_transitions() even if the
                                         LHS is already present,*/
const unsigned AE_TIGHT = 64;  /* don't allow the equation to be inserted if
                                  it is too big even if interesting (so it
                                  will be pooled) */
const unsigned AE_DISCARDABLE = 128; /* discard this equation if it is big and
                                        not interesting. This must only be
                                        used for equations not found by Knuth
                                        Bendix. These can always be recreated
                                        later */
const unsigned AE_INSERT = 256; /* Don't perform size checks - just insert
                                   the equation. (It may still be omitted
                                   if other conditions do not hold -
                                   but it will not be pooled) */
const unsigned AE_SAFE_REDUCE = 512; /* Don't throw an internal error if
                                        "reduction" causes length to exceed
                                        maximum allowed */
const unsigned AE_SAVE_AXIOM = 1024; /* The equation being added is a raw
                                        axiom, and we want to save the
                                        polished version of it */
const unsigned AE_IGNORE_PRIMARY = 2048; /* Ignore whether equation is primary when
                                            considering interest */
const unsigned AE_INSERT_DESIRABLE = 4096; /* less strong version of AE_INSERT */
const unsigned AE_PROVOKE_COLLAPSE = 8192;
const unsigned AE_KEEP_FAILED = 16384; /* Keep non-reducible equation */
const unsigned AE_CORRECTION = 32768; /* Don't insert equation if not interesting */
const unsigned AE_NO_UNBALANCE = 65536;
const unsigned AE_PRE_CANCEL = 0x20000;
const unsigned AE_NO_REPEAT = 0x40000;
const unsigned AE_NO_BALANCE = 0x80000;
const unsigned AE_POOL = 0x100000;
const unsigned AE_HIGHLY_DESIRABLE = 0x200000;
const unsigned AE_RIGHT_CONJUGATE = 0x400000;
const unsigned AE_NO_SWAP = 0x800000;
const unsigned AE_STRONG_INSERT = 0x1000000;
const unsigned AE_VERY_DISCARDABLE = 0x2000000;
const unsigned AE_WANT_WEAK_INSERT = 0x4000000;

class Equation_Word : public Word
{
  friend class Working_Equation;
  friend class Rewriter_Machine;
#ifdef DEBUG
    String_Buffer debug_word;
#endif
  public:
    Node_Manager &nm;
  protected:
    Language language;
    Word_Length allocated;
    Word_Length word_length;
    mutable Word_Length valid_length;
    Node_ID *state;
    Ordinal *values_base;
    Ordinal *values;
    Node_List reductions;
    mutable Certificate certificate;
  public:
    Equation_Word(Node_Manager & nm_,const Word & word);
    Equation_Word(Node_Manager & nm_,Node_Handle state_,Ordinal transition=-1);
    Equation_Word(Node_Manager & nm_,Word_Length length = 0);
    Equation_Word(const Equation_Word & other);
    ~Equation_Word();
    // implementation of Word methods
    void allocate(Word_Length length,bool keep = false);
    const Alphabet & alphabet() const;
    Word_Length length() const
    {
      return word_length;
    }
    Word_Length fast_length() const /* not virtual */
    {
      return word_length;
    }
    const Node_ID *states() const
    {
      return state+1;
    }
    const Ordinal *buffer()  const
    {
      return values;
    }
  private:
    // methods from Word we want to hide
    Ordinal *buffer()
    {
      return values;
    }
    void set_code(Word_Length position,Ordinal value);
    void set_multiple(Word_Length position,const Ordinal *values,Word_Length length)
    {
      Word::set_multiple(position,values,length);
    }
    Word & operator+=(const Word & other);
    Word_Length unpack(const Byte * packed_buffer);
    operator Word &() { return *this;}
  public:
    //queries
    operator const Word &() const { return *this;}
    Language get_language() const
    {
      return language;
    }
    Ordinal first_letter() const
    {
      return values[0];
    }
    Ordinal last_letter() const
    {
      return values[word_length-1];
    }
    Node_Reference state_lhs(bool full_match = true) const;
    Node_Reference state_word(bool full_match = true) const;
    Node_Reference state_prefix() const;
    // commands
    void append(Ordinal rvalue);
    Node_Reference get_node(Word_Length length,
                            bool lhs,Word_Length * new_prefix_length);
    void join(const Equation_Word & first_word,
              const Equation_Word & second_word,Word_Length offset = WHOLE_WORD);
    Node_Reference difference(State state,Ordinal g1,Ordinal g2,bool create = true);
    Node_Reference left_half_difference(Node_Handle state,Ordinal g,bool create);
    Node_Reference right_half_difference(Node_Handle state,Ordinal g,bool create);
    Equation_Word & operator=(const Word &word);
    Equation_Word & operator=(Equation_Word &word);
    void pad(Word_Length new_length);
    bool read_inverse(Node_Reference node,bool read_state,bool remember = false);
    void read_word(Node_Handle node,bool read_state);
    Node_Reference realise_state();
    Node_Reference re_realise_state(Node_Handle state,bool attach = true);
    /* unless shortlex ordering is used reduce() can increase the word length,
       and it is possible that the maximum permitted length might be exceeded.
       When this happens, MAF will usually terminate with an internal error.
       If you pass a non zero no_fail value then instead reduce() will return
       false and set *no_fail to true. The caller must initialise *no_fail to
       false if required - the idea being that the caller can perform several
       reductions with a no_fail that is initially false, and then test the
       value afterwards.
    */
    bool reduce(unsigned flags = 0,bool * no_fail = 0);
  private:
    void invalidate() const
    {
      valid_length = 0;
      certificate = Certificate();
    }
    void validate() const;

    // slice() chops a subword out of the word and sets the word to that
    Word_Length slice(Word_Length start,Word_Length end);
    void unpack(const Word & word);
    /* hard_reduce() implements reduction for words containing
       reductions that grow the word */
    bool hard_reduce(unsigned flags = 0,bool * no_fail = 0);
};
#endif
