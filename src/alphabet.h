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
$Log: alphabet.h $
Revision 1.16  2010/06/10 13:57:52Z  Alun
All tabs removed again
Revision 1.15  2010/06/08 06:33:52Z  Alun
Changes to allow recovery from parse errors in some circumstances
rename_letter() method added, though this is only implemented for Word_Alphabet
Revision 1.14  2009/11/11 00:17:14Z  Alun
Types widened on various functions so we can detect attempt to create
over-sized alphabet
Revision 1.13  2009/09/13 23:51:13Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Added order_supports_automation() method.

Revision 1.12  2009/06/18 11:19:14Z  Alun
Various changes intended to improve performance on very large alphabets
Revision 1.12  2008/11/05 01:09:00Z  Alun
Another new word ordering
Revision 1.11  2008/11/03 01:15:13Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.10  2008/09/23 21:51:15Z  Alun
Final version built using Indexer.
Revision 1.6  2007/12/20 23:25:45Z  Alun
*/
#pragma once
#ifndef ALPHABET_INCLUDED
#define ALPHABET_INCLUDED 1

/* This header file prototypes the class used for managing word ordering,
   I/O (parsing/format), and alphabet related issues in MAF. This class is
   called "Alphabet" although it might have been better for it to have been
   called "Ordered_Alphabet", or even "Word_Manager". However, the name
   "Alphabet" better reflects the way the class is usually used.

   There are three values associated with each "letter" of the alphabet.
   1. The string representing the letter. This is given type "Glyph" and
   is ordinary char * (or wchar_t * if code is compiled to use Unicode)
   so we can use C string functions.

   2. The value representing the shortlex order and transition index of the
   letter within the alphabet. This is  given type  "Ordinal". The ordinal
   value of the first generator is 0. (Actually a different logical type is
   used for transition indexes, since the need to support "product" alphabets
   means a wider native type is needed for FSAs, but the values are always the
   same, except for the padding symbol).

   3. The representation of the letter. For Word classes this is
   just the Ordinal for the generator, but for classes that pack word data
   it is different. Alphabet supports the simple packing scheme used
   by Packed_Word which uses one byte per symbol for small alphabets and
   two for large ones. However, this is in the process of being supplanted
   by Extra_Packed_Word which packs as many symbols into an unsigned long
   as can be done without using a complex arithmetic coding scheme.
   Packed_Words can usually be interpreted by any words using any alphabet,
   but Extra_Packed_Words will be misinterpreted by a word from an alphabet
   of a different size.

   If MAF's AT_Char alphabet has been used MAF packs generators using
   the appropriate character. The original reason for this was that it
   made debugging with a visual debugger far easier, since the content
   of memory was easily readable, but this is largely irrelevant now since
   almost all of MAF uses AT_String alphabets.

   If the AT_String or AT_Simple alphabets are used we cannot conveniently
   represent packed words internally using the actual generators. In this case
   packed words are encoded in native char or short values starting from 1 for
   the first generator. Whether char or short is used depends on number of
   generators.

   There are three cases:

   Group presented using "a","A","b" as generators etc, and AT_Char
   option selected.

   "Code" really is char, and uses values 'a','A','b'
   Ordinal is lexical order "a"=0,"A"=1,"b"=2.
   In this case there must be < 256 generators.

   Group presented using words, e.g. "left","right","down" as generators etc.
   Alphabet type AT_String must be selected, otherwise creation of alphabet
   fails.

   if < 256 generators

   Code is char, and uses values "left"=1,"right"=2,etc

   >= 256 generators

   Code is short, and uses values "left"=1,"right"=2,etc

   The values for Code and Ordinal are different so that we can keep 0
   available as a string terminator internally, but still base arrays
   of transitions from 0.

   Several special Ordinal values are defined.
   -1 represents IdWord, but never occurs within a word. It is used as the
   terminating symbol for words in Ordinal representation, and is used to
   request the padding symbol in product FSAs.
   -2 represents "invalid generator". This should never occur, except that
   it is the return value from inverse() for generators that are not
   invertible to generators.
   generator_count also represents the "padding symbol".

   To minimise the overhead of having a flexible packed representation of
   words it is best to use unpacked words whenever possible. Packed words
   are intended only for use in FSA state keys or databases.
*/

#ifndef AWCC_INCLUDED
#include "awcc.h"
#endif
#ifndef AWDEFS_INCLUDED
#include "awdefs.h"
#endif
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

typedef String Glyph; /* Human format */

// Classes referred to but defined elsewhere
class Word;
class Presentation;
class Container;
class Ordinal_Word;
class Parse_Error_Handler;

/**/

/* Since the padding symbol usually only occurs at the ends of words, and does
   not change the group element the word maps to, it makes sense to use the
   same ordinal for all three of the following. It does cause us some minor
   issues in ensuring compatibility with KBMAG, but it is much less confusing
   than the alternatives. From the point of view of having the word pairs
   in a product FSA come out in shortlex order this order would be best,
   but KBMAG wants the padding symbol at the end, presumably because it uses 0
   for epsilon transitions.

   Some MAF algorithms extend words by iterating from -1 up to but not
   including nr_generators, others are more like KBMAG and go from 0 up to and
   including nr_generators. I prefer the former. It should be noted that
   the coset symbol _H uses the value nr_generators would have had in the
   original presentation's alphabet, and that many of the automata in a coset
   system still use the original alphabet. So all in all it is best to avoid
   using nr_generators as though it were a terminator.
 */

const Ordinal IdWord = -1; /* Do not change this */
const Ordinal PADDING_SYMBOL = IdWord;
const Ordinal TERMINATOR_SYMBOL = PADDING_SYMBOL;
const Ordinal INVALID_SYMBOL = -2;

// flag for print() method
enum Alphabet_Print_Format
{
  APF_Bare,
  APF_GAP_Normal,
  APF_GAP_Product
};


class Alphabet
{
  struct Rank_Work_Area;
  public:
    enum Order_Kind
    {
      Order_Is_Geodesic,   // a word cannot be greater than a longer word
      Order_Limited_Increase,  // a word can be greater than a
                               // longer word, but any word can have only
                               // finite number of predecessors
      Order_Indefinite_Increase// a word can have infinitely many predecessors
    };
    enum Greater_Than_Kind
    {
      Greater_Than_Not_Implemented, // the gt_ methods are not implemented and
                                    // so it is not possible to build a word
                                    // acceptor
      Greater_Than_Finite,  // The gt_methods are implemented and have an FSA
      Greater_Than_Infinite // The gt_methods are implemented, but may not have an FSA
    };
    enum Order_Flags // Since this is used as a set of flags and not an enum
    {                // I have followed my naming convention for flags
      ORDER_HAS_WEIGHTS = 1,
      ORDER_HAS_LEVELS = 2,
      ORDER_WEIGHT_MOVES_LEFT = 4,
      ORDER_WEIGHT_MOVES_RIGHT = 8
    };
  protected:
    size_t code_size;
    Ordinal nr_letters;
    unsigned density; // number of symbols that can be packed into an unsigned long
    unsigned *weights;
  private:
    unsigned *levels;
    Ordinal *ranks;
    Ordinal *ilevels;
    Ordinal *representatives;
    Rank_Work_Area *rwa;
    Ordinal nr_levels;
    Word_Ordering word_ordering;
    Ordinal coset_symbol;
    mutable unsigned long reference_count;
    mutable Private_Byte_Buffer gt_state;
    const bool need_operator_symbol;
  public:
    static struct Order_Properties
    {
      Word_Ordering order;
      Order_Kind order_kind;
      Greater_Than_Kind gt_kind;
      unsigned flags;
      size_t gt_key_size;
      bool has_weights() const
      {
        return (flags & ORDER_HAS_WEIGHTS)!=0;
      }
      bool has_levels() const
      {
        return (flags & ORDER_HAS_LEVELS)!=0;
      }
      bool weight_moves_left() const
      {
        return (flags & ORDER_WEIGHT_MOVES_LEFT)!=0;
      }
      bool weight_moves_right() const
      {
        return (flags & ORDER_WEIGHT_MOVES_RIGHT)!=0;
      }
    } const order_properties[];
    Container & container;
    virtual ~Alphabet();
    void attach() const
    {
      reference_count++;
    }
    void detach() const
    {
      if (!--reference_count)
        delete (Alphabet *) this;
    }
    // Function to instantiate alphabet
    static Alphabet * create(Alphabet_Type format,Container & container);

    // KBMAG style strings for naming word_orderings
    static String const * word_orderings();

    // set_letters() sets all the generators in one go
    // for AT_Letter it is a simple string such as "aAbB"
    // for AT_String it must be comma separated: "a,A,b,B"
    virtual bool set_letters(String identifiers) = 0;
    /* set_nr_letters()/set_next_letter() can be used where it is
       desired to set the letters of the alphabet one at a time.
       set_nr_letters() completely removes any information, and
       must be called first. Individual letters can only be set
       consecutively */
    virtual bool set_nr_letters(Element_Count nr_letters_) = 0;
    virtual bool set_next_letter(Glyph glyph) = 0;
    virtual bool rename_letter(Ordinal,Glyph)
    {
      return false;
    }
    /* set_similar_letters("_p",n) sets the glyphs for the next
       n letters to be _p1,_p2,... _pn.
       Obviously it is only supported for AT_String alphabets. */
    bool set_similar_letters(Glyph prefix, Ordinal nr_letters);

    // methods to do with word order

    // set_coset_order() is called to inform the alphabet that a coset system
    // will be processed. It is needed to allow coset systems in which the
    // g generators are at different levels to be processed.
    bool set_coset_order(Ordinal coset_symbol);

    bool set_level(Ordinal g,unsigned level)
    {
      return set_level(g,level,true);
    }
    bool set_weight(Ordinal g,unsigned weight)
    {
      if (is_generator(g) && weights)
      {
        weights[g] = weight;
        return true;
      }
      return false;
    }
    void set_word_ordering_from(const Alphabet & other);
    APIMETHOD void set_word_ordering(Word_Ordering word_ordering_);
    /* All remaining methods are const. */

    // capacity() returns number of letters supported by this alphabet type
    virtual Element_Count capacity() const = 0;

    /* format() creates a string representation of a word.
       Normally you would not use either of these methods
       directly. They are provided for Word to use in its
       implementation of format().

       buffer_size is the length of the buffer in characters,
       The return value is the length the returned string should have.
       If buffer_size <= return value the answer has been truncated.
       There is no 0 terminator unless buffer_size > return_value.
       To make life easier use the String_Buffer version of format() */
    virtual size_t format_raw(Letter * output_buffer,size_t buffer_size,
                              const Word & word) const = 0;
    APIMETHOD size_t format(String_Buffer * sb,const Word & word) const;
    virtual Glyph glyph(Ordinal value) const = 0;
    virtual size_t pack(Byte * buffer,const Ordinal *values,
                        Word_Length length) const = 0;
    virtual Ordinal parse_identifier(const Letter *&input_string,
                                     bool allow_caret = false) const = 0;
    /* truncated_alphabet() creates a new alphabet consisting of the first
       nr_letters generators from the original alphabet. This is used in
       coset systems to create the alphabet conisting of just the G generators.
       This method is only implemented for AT_String alphabets since
       coset systems always use AT_String */
    virtual Alphabet * truncated_alphabet(Element_Count nr_letters) const = 0;
    virtual void unpack(Ordinal *value,const Byte * buffer,
                        Word_Length length) const = 0;
    virtual Ordinal value(const Byte *data) const = 0;
    // returns the length of a packed word
    virtual Word_Length word_length(const Byte *data) const = 0;
    // non virtual methods
    /* operator== just checks number of letters and glyphs for corresponding
       letters match - word_ordering may differ. */
    APIMETHOD bool operator==(const Alphabet & other) const;
    bool operator!=(const Alphabet & other) const
    {
      return !operator==(other);
    }
    // compare returns -1 is lhs < rhs, 0 if lhs == rhs, 1 if lhs > rhs
    // in the ordering set for the alphabet.
    int compare(const Word &lhs,const Word &rhs) const;
    /* the gt_ methods provide helper functions for implementing a
       greater_than automaton, and a method for use in building a word
       acceptor
       the first three methods return a pointer to memory containing the
       key for the product state automaton that can decide whether u > v
       in the selected word ordering.
       These state keys would normally be entered into a Hash so that
       State_ID values can be used instead.
       gt_failure_state provides a key for the failure state needed in this
       case, so that the initial state can be entered as 1. However, if the
       automaton does reach the failure state, which normally will happen
       only when padding characters are read on the left, a 0 pointer is
       returned.
    */
    const void *gt_failure_state(size_t * size) const;
    const void *gt_initial_state(size_t * size) const;
    const void *gt_new_state(size_t *size,const void * key,Transition_ID ti) const;
      // returns a fixed size if the key does not vary in size and 0 otherwise
    size_t gt_key_size() const;
    bool gt_is_accepting(const void * key) const
    {
      return gt_order(key) > 0;
    }
    /* gt_compare() determines whether two state keys can be compared
       so that one or other is unequivocally "better" than the other in
       a word-acceptor. key1 is "better" than key2 if all possible paths
       to reduction (i.e. "no transition" in the word-acceptor) from key2
       are also paths to reduction in key1, and key1 has some other paths
       to reduction as well. It is assumed that the u word that has been
       read is the same in both cases.
       The return value is as follows:
       1, key1 is better
       0  key1 and key2 are equivalent
       2  key2 is better
       -1 neither key is better than the other, but they are not equivalent.
    */
    int gt_compare(const void * state_key1,const void * state_key2) const;
    /* gt_order returns >0,0,< 0 according as u > v,u===v,u < v so far */
    int gt_order(const void * key) const;
    size_t gt_size(const void * state_key) const;

    bool order_is_shortlex() const
    {
      return word_ordering == WO_Shortlex;
    }
    bool is_generator(Ordinal g) const
    {
      return g >= 0 && g < nr_letters;
    }
    Ordinal letter_count() const
    {
      return nr_letters;
    }
    unsigned level(Ordinal g) const
    {
      return levels && is_generator(g) ? levels[g] : 1;
    }
    // order_is_effectively_shortlex() is needed by Diff_Reduce to
    // help it decide whether it can perform various optimisations
    // that are only valid in the case where the word is shortlex
    bool order_is_effectively_shortlex() const;
    bool order_is_geodesic() const
    {
      return order_properties[word_ordering].order_kind == Order_Is_Geodesic;
    }
    bool order_needs_moderation() const
    {
      return order_properties[word_ordering].order_kind == Order_Indefinite_Increase;
    }
    bool order_supports_automation() const
    {
      return order_properties[word_ordering].gt_kind != Greater_Than_Not_Implemented;
    }
    Word_Ordering order_type() const
    {
      return word_ordering;
    }
    size_t packed_word_size(size_t length) const
    {
      return code_size*(length+1);
    }
    size_t packed_word_size(const Byte *data) const
    {
      return packed_word_size(word_length(data));
    }
    Word_Length extra_packed_word_length(const Byte *data) const
    {
      if (data)
        return * (Word_Length *) data;
      return 0;
    }
    unsigned extra_packed_density() const
    {
      return density;
    }
    size_t extra_packed_word_size(Word_Length length) const
    {
      if (!density)
        return sizeof(Word_Length);
      return ((length+density-1)/density)*sizeof(unsigned long) + sizeof(Word_Length);
    }
    size_t extra_packed_word_size(const Byte * data) const
    {
      return extra_packed_word_size(extra_packed_word_length(data));
    }
    APIMETHOD Ordinal_Word * parse(String string,String_Length length,
                                   const Presentation * presentation,
                                   Parse_Error_Handler & error_handler) const;
    bool parse(Ordinal_Word *word,String string,String_Length length,
               const Presentation * presentation,
               Parse_Error_Handler & error_handler) const;
    // Output a GAP alphabet record
    void print(Container &container,Output_Stream * stream,Alphabet_Print_Format format) const;
    void print_ordering(Container &container,Output_Stream * stream) const;

    Transition_ID product_alphabet_size() const
    {
      return (nr_letters+1)*(nr_letters+1)-1;
    }
    Transition_ID product_base(Ordinal g1) const
    {
      return product_id(g1,0);
    }
    Transition_ID product_id(Ordinal g1,Ordinal g2) const
    {
      if (g1 < 0)
        g1 = nr_letters;
      if (g2 < 0)
        g2 = nr_letters;
      return g1*(nr_letters+1)+g2;
    }
    void product_generators(Ordinal *g1,Ordinal *g2,Transition_ID ti) const
    {
      *g2 = ti % (nr_letters + 1);
      *g1 = ti / (nr_letters + 1);
      if (*g2 == nr_letters)
        *g2 = PADDING_SYMBOL;
      if (*g1 == nr_letters)
        *g1 = PADDING_SYMBOL;
    }
    // length of an ordinal word
    Word_Length word_length(const Ordinal *data) const
    {
      Word_Length i;
      for (i = 0;data[i] >= 0;i++)
        ;
      return i;
    }
    Word_Length word_length(size_t size) const
    {
      return size/code_size-1;
    }
  protected:
    Alphabet(Container & container_,bool need_operator_symbol);
  private:
    bool set_level(Ordinal g,unsigned level,bool set_ranks)
    {
      if (is_generator(g) && levels)
      {
        levels[g] = level;
        if (set_ranks)
          compute_ranks();
        return true;
      }
      return false;
    }
    APIMETHOD void compute_ranks();
    void remove_order();
    int shortlex_compare(const Word &lhs,const Word &rhs) const;
    int wtlex_compare(const Word &lhs,const Word &rhs) const;
    int wtshortlex_compare(const Word &lhs,const Word &rhs) const;
    int accented_compare(const Word &lhs,const Word &rhs,bool multi_level) const;
    int wreath_compare(const Word &lhs,const Word &rhs) const;
    int right_wreath_compare(const Word &lhs,const Word &rhs) const;
    int recursive_compare(const Word &lhs,const Word &rhs) const;
    int right_recursive_compare(const Word &lhs,const Word &rhs) const;
    int short_recursive_compare(const Word &lhs,const Word &rhs) const;
    int short_right_recursive_compare(const Word &lhs,const Word &rhs) const;
    int short_wtlex_compare(const Word &lhs,const Word &rhs) const;
    int short_fptp_compare(const Word &lhs,const Word &rhs) const;
    int right_shortlex_compare(const Word &lhs,const Word &rhs) const;
    int ranked_compare(const Word &lhs,const Word &rhs) const;
    int nestedrank_compare(const Word &lhs,const Word &rhs) const;
    int grouped_compare(const Word &lhs,const Word &rhs) const;
    int coset_compare(const Word &lhs,const Word &rhs) const;
};

#endif
