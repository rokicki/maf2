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
$Log: mafword.h $
Revision 1.12  2010/06/10 13:58:04Z  Alun
All tabs removed again
Revision 1.11  2010/04/26 19:43:16Z  Alun
Many changes made with the hope of reducing number of calls through vtable
in performance critical code. Fast_Word type added. So far this is only
used in maf_tc.cpp (the coset enumerator), but this type might be useful in
other places in future
Revision 1.10  2009/09/13 19:45:41Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.9  2009/07/04 10:29:54Z  Alun
append() method added to Sorted_Word_List
Revision 1.8  2008/11/03 01:15:56Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.7  2008/09/24 02:39:11Z  Alun
Final version built using Indexer.
Revision 1.4  2007/11/15 22:58:11Z  Alun
*/
#pragma once
#ifndef MAFWORD_INCLUDED
#define MAFWORD_INCLUDED 1

/* This header file prototypes the classes used for managing words
   (in generators) in MAF. Or rather, the classes used for managing words
   outside the context of any algebraic structure beyond what is implied
   by the given set of generators and inverses.
*/
#include <string.h>
#ifndef AWCC_INCLUDED
#include "awcc.h"
#endif
#ifndef AWDEFS_INCLUDED
#include "awdefs.h"
#endif
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif
#ifndef ALPHABET_INCLUDED
#include "alphabet.h"
#endif

typedef String Glyph; /* Human format */

/* Utility functions for copying and comparing words
   MAF ensures all the words it stores in equations are terminated,
   with -1 if the word uses ordinals, or 0 if the word uses a text format.
   If it is safe to do so it terminates temporary words it creates as well.
   However it can manipulate words that are not terminated, provided
   the length is specified. In fact most of the time it does not rely on the
   terminator at all, which is inserted to make it easier to work out what
   is going on if you need to spy on the code with a debugger. */

inline Ordinal *word_copy(Ordinal *target,const Ordinal *source,
                          size_t length)
{
  /* NB. This copies the specified number of letters, regardless of apparent
     word length */
//  for (size_t i = 0; i < length;i++)
//    target[i] = source[i];
//  return target;
  return (Ordinal *) memcpy(target,source,length*sizeof(Ordinal));
}

inline bool words_differ(const Ordinal *w1,const Ordinal *w2,
                         Word_Length length)
{
  /* NB. This function only returns whether the specific number of
     letters in a word match. It does not return any lexicographic information.
     For that you need to use compare().
     The code using memcmp() has been commented out because it is much slower
     with MS compiler. */
//  return memcmp(w1,w2,sizeof(Ordinal)*length)!=0;
  for (Word_Length i = 0; i < length;i++)
    if (w1[i] != w2[i])
      return true;
  return false;
}

// classes referred to but defined elsewhere
class Alphabet;

// classes defined in this header file
class Entry_Word;
class Ordinal_Word;
class Word;
class Subword;
class Packed_Word;
class Extra_Packed_Word;
class Packed_Word_List;
class Sorted_Word_List;
class Word_Collection;
class Word_List;

class Word
{
  // This is the minimal interface that all objects that are "words" must
  // support
  public:
    virtual ~Word() {}
    virtual void allocate(Word_Length length,bool keep = false) = 0;
    virtual Ordinal *buffer() = 0;
    virtual const Ordinal *buffer() const
    {
      return ((Word *)this)->buffer();
    }
    virtual const Alphabet & alphabet() const = 0;
    void append(Ordinal g);
    const Ordinal *data(Word_Length position) const
    {
      return buffer()+position;
    }
    int compare(const Word &rhs) const
    {
      // return 1,0,-1 according as *this > rhs *this == rhs, *this < rhs in
      // alphabet ordering.
      return alphabet().compare(*this,rhs);
    }
    bool operator!=(const Word & rhs) const
    {
      Word_Length l = length();
      return l != rhs.length() || words_differ(*this,rhs,l);
    }
    bool operator==(const Word & rhs) const
    {
      return !operator!=(rhs);
    }
    Word_Length find_symbol(Ordinal g,Word_Length start = 0) const
    {
      const Ordinal * values = buffer();
      Word_Length l = length();
      for (Word_Length i = start; i < l ; i++)
        if (values[i] == g)
          return i;
      return INVALID_LENGTH;
    }
    Word_Length extra_unpack(const Byte * packed_buffer);
    size_t format(Letter * output_buffer,size_t buffer_size) const
    {
      return alphabet().format_raw(output_buffer,buffer_size,*this);
    }
    String format(String_Buffer *sb) const
    {
      alphabet().format(sb,*this);
      return sb->get();
    }
    Ordinal *data(Word_Length position)
    {
      return buffer()+position;
    }
    virtual Word_Length length() const = 0;
    size_t pack(Byte * packed_buffer) const
    {
      return alphabet().pack(packed_buffer,buffer(),length());
    }
    PACKED_DATA extra_packed_data(size_t * size = 0) const;
    size_t extra_packed_size() const
    {
      return alphabet().extra_packed_word_size(length());
    }

    PACKED_DATA packed_data(size_t * size = 0) const
    {
      Byte * packed_buffer = new Byte[packed_size()];
      if (size)
        *size = pack(packed_buffer);
      else
        pack(packed_buffer);
      return packed_buffer;
    }
    size_t packed_size() const
    {
      return alphabet().packed_word_size(length());
    }
    void set_code(Word_Length position,Ordinal value)
    {
      *data(position) = value;
    }
    void set_length(Word_Length length)
    {
      allocate(length,true);
    }
    void set_multiple(Word_Length position,const Ordinal *values,Word_Length length)
    {
      Ordinal *  buffer__ = data(position);
      for (Word_Length i = 0;i < length;i++)
        buffer__[i] = values[i];
    }
    Word_Length unpack(const Byte * packed_buffer)
    {
      const Alphabet & a = alphabet();
      Word_Length l = packed_buffer ? a.word_length(packed_buffer) : 0;
      allocate(l,false);
      if (l)
        a.unpack(buffer(),packed_buffer,l);
      return l;
    }
    Ordinal value(Word_Length position) const
    {
      return *data(position);
    }
    operator const Ordinal *() const
    {
      return buffer();
    }
    operator Ordinal *()
    {
      return buffer();
    }
    int lexcmp(const Word &rhs) const;
    void print(Container & container,Output_Stream * stream) const;
    Ordinal_Word operator+(const Word & other) const;
    Word & operator=(const Word & other);
    /* set the word to the next word in BFS order (i.e. shortlex) */
    void next_bfs();
};

class Subword : public Word
{
  private:
    Word * word;
    Word_Length start;
    Word_Length length__;
  public:
    Subword(Word & word_,Word_Length start_ = 0,Word_Length end = WHOLE_WORD) :
      word(&word_),
      start(start_)
    {
      if (end==WHOLE_WORD)
        end = word->length();
      length__ = end - start;
    }
    Subword(const Subword & original) :
      Word(original),
      word(original.word),
      start(original.start),
      length__(original.length__)
    {
    }
    Subword() :
      word(0)
    {
    }
    void allocate(Word_Length length,bool keep = true)
    {
      if (length+start > word->length())
        word->allocate(length + start,keep);
      length__ = length;
    }
    Ordinal *buffer()
    {
      return word->data(start);
    }
    const Ordinal *buffer() const
    {
      return ((Subword *)this)->buffer();
    }
    const Alphabet & alphabet() const
    {
      return word->alphabet();
    }
    Word_Length length() const
    {
      return length__;
    }
};

class Ordinal_Word : public Word
{
  protected:
    const Alphabet & alphabet__;
    Ordinal *buffer__;
    Word_Length word_length;
    Word_Length allocated;
  public:
    Ordinal_Word(const Alphabet & alphabet_,Word_Length length = 0);
    Ordinal_Word(const Alphabet & alphabet_,const Byte * packed_word);
    Ordinal_Word(const Ordinal_Word & word);
    Ordinal_Word(const Word & word);
    ~Ordinal_Word();
    void allocate(Word_Length length,bool keep = false);
    const Alphabet & alphabet() const
    {
      return alphabet__;
    }
    Ordinal *buffer()
    {
      return buffer__;
    }
    const Ordinal *buffer() const
    {
      return buffer__;
    }
    Word_Length length() const
    {
      return word_length;
    }
    Ordinal_Word & operator+=(const Word & other);
    Ordinal_Word & operator=(const Word & other);
#ifdef __GNUG__
    Ordinal_Word & operator=(const Ordinal_Word & other)
    {
      return operator=((const Word &) other);
    }
#endif
    bool adjust_length(Word_Length new_length)
    {
      if (new_length < allocated)
      {
        word_length = new_length;
        buffer__[word_length] = TERMINATOR_SYMBOL;
        return true;
      }
      return false;
    }
    /* It is worth repeating some methods to try to improve optimisation*/
    void set_code(Word_Length position,Ordinal value)
    {
      buffer__[position] = value;
    }
    Ordinal value(Word_Length position) const
    {
      return buffer__[position];
    }
    const Ordinal *data(Word_Length position) const
    {
      return buffer__+position;
    }
    Ordinal *data(Word_Length position)
    {
      return buffer__+position;
    }
    operator const Ordinal *() const
    {
      return buffer__;
    }
    operator Ordinal *()
    {
      return buffer__;
    }
};

/* As well as single words, we will often want to form a collection of
   related words. The next classes allow us to manage such a collection
   so that we only to have to remember a pointer to the start of each word.
   saving 12 bytes of storage per word compared with an Ordinal_Word (or
   4 when we have an explicit Entry_Word for it).
*/

class Word_List;

struct Fast_Word
{
  const Ordinal * buffer;
  Word_Length length;
};

class Word_Collection
{
  public:
    const Alphabet &alphabet;
  public:
    Word_Collection(const Alphabet &alphabet_) :
      alphabet(alphabet_)
    {}
    virtual ~Word_Collection() {}
    /* add() Must ensure the specified word is in the collection.
       The implementing class may or may not allow for duplicates */
    virtual void add(const Word & word) = 0;
    virtual void empty() = 0;
    virtual bool contains(const Word & word) const
    {
      return find(word);
    }
    /* count() must returns a number at least as big as the number of words in
       the collection. Whenever possible the answer should be exact, but
       if necessary it can be inexact if holes are created when words are
       deleted. */
    virtual Element_Count count() const = 0;
    /* find() must return true if there is an equal word in the collection
       and false otherwise. If word_nr is non-zero then it is set to a
       number for which get() will currently return an equal word.
       The value might change after a subsequent call to add() */
    virtual bool find(const Word &word,Element_ID * word_nr=0) const = 0;
    /* get() must retrieve the word_nr th word in the collection, assign it to
       *word and return true. The return value is false if there is no word
       with the specified index, and in this case the contents of word
       are unspecified. */
    virtual bool get(Word * word,Element_ID word_nr) const = 0;
    String format(String_Buffer * sb) const;
    void print(Container & container,Output_Stream * stream) const;
};


class Sized_Word_Collection : public Word_Collection
{
  public:
    Sized_Word_Collection(const Alphabet &alphabet) :
      Word_Collection(alphabet)
    {}
    virtual size_t total_length() const = 0;
};

/* Word_List is suited to lists that once created are constant, and where
   words are kept in the order they are added. A Word_List can contain
   duplicates. If a Word_List going to be very big consider using a
   Word_DB instead. */

class Word_List : public Sized_Word_Collection
{
  friend class Entry_Word;
  friend class Packed_Word_List;
  private:
    BLOCKED(Word_List)
    Ordinal *buffer;
    Ordinal ** words;
    size_t buffer_length;
    size_t used;
    Element_Count max_words;
    Element_Count nr_words;
  public:
    Word_List(const Alphabet & alphabet_,Element_Count max_words_= 0);
    Word_List(const Sorted_Word_List & swl);
    Word_List(const Alphabet & alphabet_,const Packed_Word_List & pwl);
    Word_List & operator=(const Sorted_Word_List & other);
    // += concatenates the word lists without checking for duplicates
    Word_List & operator+=(const Sized_Word_Collection & other);
    void take(Word_List & other);
    ~Word_List();
    void add(const Word & word)
    {
      add(word,false);
    }
    /* Adds the word, after optionally checking if it is not already present
       in the list, and  returns the index of the new word in the list.
       If the word is already present and no_duplicate is true returns -1 */
    int add(const Word & new_word,bool no_duplicate);
    void empty()
    {
      nr_words = 0;
      used = 0;
      if (words)
        words[0] = 0;
    }
    void grow(Element_Count count);

    // serialises the data into an array of bytes allocated from the heap
    PACKED_DATA packed_data(size_t * packed_size = 0) const;
    void unpack(const Byte * data);

    Element_Count count() const
    {
      return nr_words;
    }
    bool find(const Word & word,Element_ID * word_nr = 0) const;
    bool get(Word * word,Element_ID word_nr) const;
    size_t total_length() const
    {
      return used - nr_words;
    }
    inline const Entry_Word word(Element_ID word_nr) const;
    void get_fast_word(Fast_Word * fw,Element_ID word_nr) const
    {
      fw->buffer = words[word_nr];
      fw->length = word_length(word_nr);
    }
    Word_Length word_length(Element_ID word_nr) const
    {
      if (word_nr < nr_words)
        return (words[word_nr+1] - words[word_nr]) - 1;
      return 0;
    }
  private:
    void reserve(Word_Length length);
};

class Entry_Word : public Word
{
  /* Entry_Word provides the Word interface for a word in a Word_List.
     Unlike an Ordinal_Word it is a temporary object that does not own the
     data inside it. */
  private:
    const Word_List &word_list;
    const Element_ID index;
  public:
    Entry_Word(const Word_List & word_list_,Element_ID index_) :
      word_list(word_list_),
      index(index_)
    {}
    const Ordinal *buffer() const
    {
      return word_list.words[index];
    }
    const Alphabet & alphabet() const
    {
      return word_list.alphabet;
    }
    Word_Length length() const
    {
      return word_list.word_length(index);
    }
  private:
    // Virtual functions required by Word
    void allocate(Word_Length,bool)
    {}
    Ordinal *buffer()
    {
      return word_list.words[index];
    }
};

const Entry_Word Word_List::word(Element_ID word_nr) const
{
  return Entry_Word(*this,word_nr);
}

/* Sorted_Word_List allows a list of words to be kept ordered according to
the word ordering specified in the alphabet. Words can only occur once
in a Sorted_Word_List */

class Sorted_Word_List : public Sized_Word_Collection
{
  private:
    NO_ASSIGNMENT(Sorted_Word_List)
    Ordinal_Word ** words;
    Element_Count max_words;
    Element_Count nr_words;
    size_t total_length__;
  public:
    Sorted_Word_List(const Alphabet & alphabet_,Element_Count max_words_= 0);
    Sorted_Word_List(const Word_List & wl);
    Sorted_Word_List(const Sorted_Word_List & wl);
    ~Sorted_Word_List();
    Sorted_Word_List & operator=(const Word_List &wl);
    void add(const Word & word)
    {
      insert(word,0);
    }
    /* append() inserts the word into the correct position in the list.
       But first sees if new_word is greater than any existing item
    */
    bool append(const Word & new_word,Element_ID * word_nr = 0);
    // empty() throws away the entire contents of the list
    void empty();
    /* Inserts the word into the correct position in the list */
    bool insert(const Word & new_word,Element_ID * word_nr = 0);
    void grow(Element_Count count);
    /* Removes a word from the list if present */
    bool remove(const Word & new_word,Element_ID * word_nr = 0);
    void take(Sorted_Word_List &other);
    bool contains(const Word & word) const
    {
      return find(word);
    }
    Element_Count count() const
    {
      return nr_words;
    }
    bool find(const Word & word,Element_ID * word_nr = 0) const;
    bool get(Word * word,Element_ID word_nr) const
    {
      const Ordinal_Word * answer = this->word(word_nr);
      if (answer)
        *word = *answer;
      return answer != 0;
    }
    size_t total_length() const
    {
      return total_length__;
    }
    const Ordinal_Word * word(Element_ID word_nr) const
    {
      return word_nr < nr_words ? words[word_nr] : 0;
    }
};


PACKED_CLASS(Packed_Word,Word);
PACKED_CLASS_EX(Extra_Packed_Word,Word,extra_packed_data);
PACKED_CLASS(Packed_Word_List,Word_List);

#endif
