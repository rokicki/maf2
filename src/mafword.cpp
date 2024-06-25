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


// $Log: mafword.cpp $
// Revision 1.13  2010/06/10 13:57:34Z  Alun
// All tabs removed again
// Revision 1.12  2010/04/23 12:43:21Z  Alun
// June 2010 version
// Revision 1.11  2009/11/08 23:06:54Z  Alun
// Various minor type changes to allow for possible future change of base type
// for Word_Length and Total_Length
// Revision 1.10  2009/09/13 19:14:56Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.9  2008/12/07 12:22:50Z  Alun
// append() method can now return position of new item
// Revision 1.8  2008/11/02 18:57:14Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.7  2008/09/24 02:38:15Z  Alun
// Final version built using Indexer.
// Revision 1.4  2007/11/15 22:58:09Z  Alun
//

#include <memory.h>
#include "mafword.h"
#include "container.h"

void Word::append(Ordinal g)
{
  Word_Length l = length();
  if (l < MAX_WORD)
  {
    set_length(l+1);
    *data(l) = g;
  }
  else
    MAF_INTERNAL_ERROR(alphabet().container,("Maximum word length exceeded in Word::append()\n"));
}

int Word::lexcmp(const Word &rhs) const
{
  /* Not shortlex - beware - this ignores parts of the word
     where one or other word has ended, i.e. a and ab are equal! */
  const Ordinal * buffer__ = buffer();
  const Ordinal * rbuffer__ = rhs.buffer();
  for (Word_Length i = 0;;i++)
  {
    if (buffer__[i] < 0 || rbuffer__[i] < 0)
      return 0;
    if (buffer__[i] != rbuffer__[i])
      return buffer__[i] > rbuffer__[i] ? 1 : -1;
  }
}

/**/

void Word::print(Container & container,Output_Stream * stream) const
{
  size_t slength = format(0,0);
  Letter *string = new Letter[slength+1];
  format(string,slength+1);
  container.output(stream,"%s",string);
  delete string;
}

/**/

Ordinal_Word::Ordinal_Word(const Alphabet & alphabet_,Word_Length length/* = 0*/) :
  alphabet__(alphabet_),
  allocated(0),
  buffer__(0)
{
  allocate(length,false);
}

Ordinal_Word::Ordinal_Word(const Alphabet & alphabet_,const Byte * packed_word) :
  alphabet__(alphabet_),
  allocated(0),
  buffer__(0)
{
  unpack(packed_word);
}

/**/

Ordinal_Word::Ordinal_Word(const Word & word_) :
  alphabet__(word_.alphabet()),
  allocated(0),
  buffer__(0)
{
  allocate(word_.length(),false);
  word_copy(buffer__,word_.buffer(),word_length);
}

Ordinal_Word::~Ordinal_Word()
{
  if (buffer__)
    delete [] buffer__;
}

/**/

Ordinal_Word::Ordinal_Word(const Ordinal_Word & word_) :
  Word(word_), // needed to shut up g++ - does nothing
  alphabet__(word_.alphabet__),
  allocated(0),
  buffer__(0)
{
  /* C++'s stupid inability to explicitly invoke a constructor
     forces me to duplicate this code, so I might as well devirtualise it */
  allocate(word_.word_length,false);
  word_copy(buffer__,word_.buffer__,word_length);
}

/**/

void Ordinal_Word::allocate(Word_Length length,bool keep)
{
  if (length >= allocated)
  {
    Ordinal * new_buffer = new Ordinal[length+1];
    if (keep)
      memcpy(new_buffer,buffer__,(word_length+1)*sizeof(Ordinal));
    if (buffer__)
      delete buffer__;
    buffer__ = new_buffer;
    allocated = length+1;
  }
  buffer__[word_length = length] = TERMINATOR_SYMBOL;
}

/**/

Ordinal_Word Word::operator+(const Word & other) const
{
  Ordinal_Word answer(*this);
  return answer += other;
}

/**/

Ordinal_Word & Ordinal_Word::operator+=(const Word & other)
{
  Word_Length old_length = word_length;
  Word_Length other_length = other.length();
  Total_Length new_length = old_length + other_length;
  if (new_length > MAX_WORD)
    MAF_INTERNAL_ERROR(alphabet().container,
                       ("Maximum word length exceeded in Ordinal_Word::operator+=()\n"));
  set_length(Word_Length(new_length));
  word_copy(buffer__ +old_length,other.buffer(),other_length);
  return *this;
}

/**/

Word & Word::operator=(const Word & other)
{
  Word_Length l = other.length();
  if (l > length() || !buffer())
    allocate(l,false);
  word_copy(buffer(),other.buffer(),l);
  set_length(l);
  return *this;
}

/**/

Ordinal_Word & Ordinal_Word::operator=(const Word & other)
{
  Word_Length l = other.length();
  if (l >= allocated)
    allocate(l,false);
  word_copy(buffer__,other.buffer(),l);
  buffer__[word_length = l] = TERMINATOR_SYMBOL;
  return *this;
}

Word_List::Word_List(const Alphabet & alphabet_,
                     const Packed_Word_List & pwl) :
  buffer(0),
  words(0),
  buffer_length(0),
  used(0),
  nr_words(0),
  max_words(0),
  Sized_Word_Collection(alphabet_)
{
  unpack(pwl);
}

Word_List::Word_List(const Sorted_Word_List &swl) :
  buffer(0),
  words(0),
  buffer_length(0),
  used(0),
  nr_words(0),
  max_words(0),
  Sized_Word_Collection(swl.alphabet)
{
  grow(swl.count());
  reserve(swl.total_length() + swl.count());
  for (Element_ID i = 0;i < max_words;i++)
    add(*swl.word(i));
}

/**/

Word_List & Word_List::operator=(const Sorted_Word_List &swl)
{
  empty();
  Element_Count count = swl.count();
  grow(count);
  reserve(swl.total_length() + count);
  for (Element_ID i = 0;i < count;i++)
    add(*swl.word(i));
  return *this;
}

/**/

Word_List & Word_List::operator+=(const Sized_Word_Collection &other)
{
  Element_Count other_count = other.count();
  grow(count() + other_count);
  reserve(other.total_length() + other_count);

  Ordinal_Word ow(alphabet);
  for (Element_ID i = 0;i < other_count;i++)
    if (other.get(&ow,i))
      add(ow);
  return *this;
}

/**/

void Word_List::take(Word_List & other)
{
  if (buffer)
    delete [] buffer;
  if (words)
    delete [] words;
  buffer = other.buffer;
  words = other.words;
  buffer_length = other.buffer_length;
  used = other.used;
  max_words = other.max_words;
  nr_words = other.nr_words;
  other.max_words = 0;
  other.buffer = 0;
  other.words = 0;
  other.used = 0;
  other.nr_words = 0;
  other.buffer_length = 0;
}

/**/

int Word_List::add(const Word & new_word,bool no_duplicate)
{
  /* Adds the word if it is not already present in the list, and
     returns the index of the word in the list. If the word is already
     present and no_duplicate is true returns -1 */


  if (no_duplicate)
    if (find(new_word))
      return -1;
  Word_Length length = new_word.length();
  reserve(length); /* Also ensures there is room for a new word */
  word_copy(words[nr_words],new_word.buffer(),length);
  words[nr_words][length] = TERMINATOR_SYMBOL;
  nr_words++;
  used += length+1;
  words[nr_words] = buffer + used;
  return nr_words-1;
}

/**/

bool Word_List::find(const Word & search_word,Element_ID *word_nr) const
{
  /* Returns the index of the word if it is already present in the list, and
     optionally adds it if not. This will be more convenient if we don't
     much care whether a word is new to the list or not */
  Word_Length length = search_word.length();

  for (Element_ID i = 0; i < nr_words;i++)
  {
    if (word_length(i) == length &&
        !words_differ(words[i],search_word.buffer(),length))
    {
      if (word_nr)
        *word_nr = i;
      return true;
    }
  }
  return false;
}

/**/

void Word_List::grow(Element_Count new_max_words)
{
  if (new_max_words > max_words)
  {
    max_words = new_max_words;
    Ordinal ** new_words = new Ordinal *[max_words+1];
    Element_ID i;
    for (i = 0; i < nr_words;i++)
      new_words[i] = words[i];
    new_words[i++] = buffer + used;
    for (; i < max_words;i++)
      new_words[i] = 0;
    if (words)
      delete [] words;
    words = new_words;
  }
}

/**/

void Word_List::reserve(Word_Length length)
{
  /* reserve() used to be public and allow the caller to gain temporary
     direct access to the word list, creating an Entry_Word at the end
     of the list of the specified length.
     Now it is just used to internally, to make room for a new word */
  if (nr_words >= max_words)
    grow(nr_words < 2 ? nr_words+1 : nr_words+4);
  words[nr_words] = buffer + used;
  if (used+length+1 > buffer_length)
  {
    buffer_length = used+length+1;
    if (max_words > 2)
      buffer_length = (buffer_length+1023) & ~1023;
    Ordinal * new_buffer = new Ordinal[buffer_length];
    word_copy(new_buffer,buffer,used);
    /* <= intentional below */
    for (Element_ID i = 0; i <= nr_words;i++)
      words[i] = new_buffer + (words[i]-buffer);
    if (buffer)
      delete [] buffer;
    buffer = new_buffer;
  }
}

/**/

bool Word_List::get(Word * word,Element_ID word_nr) const
{
  if (word_nr >= nr_words)
    return false;
  *word = Entry_Word(*this,word_nr);
  return true;
}

/**/

String Word_Collection::format(String_Buffer * sb) const
{
  String_Buffer sb2;
  Ordinal_Word ow(alphabet);
  sb->set("[");
  Element_Count nr_words = count();
  for (Element_ID i = 0; i < nr_words ;i++)
  {
    if (i != 0)
      sb->append(",");
    if (get(&ow,i))
    {
      ow.format(&sb2);
      sb->append(sb2.get());
    }
  }
  sb->append("]");
  return sb->get();
}

/**/

void Word_Collection::print(Container & container,Output_Stream * stream) const
{
  String_Buffer sb;
  container.output(stream,"%s",format(&sb).string());
}

/**/

PACKED_DATA Word_List::packed_data(size_t * size) const
{
  size_t needed = used*alphabet.packed_word_size(size_t(0));
  if (!used)
  {
    if (size)
      *size = 0;
    return 0;
  }
  Byte * packed_buffer = new Byte [needed+sizeof(size_t)];
  Byte * current = packed_buffer+sizeof(size_t);
  for (Element_ID i = 0; i < nr_words;i++)
  {
    Entry_Word ew(*this,i);
    if (ew.value(0) != INVALID_SYMBOL)
    {
      ew.pack(current);
      current += ew.packed_size();
    }
  }
  needed = (current - packed_buffer) - sizeof(size_t);
  memcpy(packed_buffer,&needed,sizeof(size_t));
  if (size)
    *size = current - packed_buffer;
  return packed_buffer;
}

/**/

Word_List::Word_List(const Alphabet & alphabet_,Element_ID max_words_/*= 0*/) :
  buffer(0),
  words(0),
  buffer_length(0),
  used(0),
  nr_words(0),
  max_words(0),
  Sized_Word_Collection(alphabet_)
{
  grow(max_words_);
}

Word_List::~Word_List()
{
  if (buffer)
    delete [] buffer;
  if (words)
    delete [] words;
}

void Word_List::unpack(const Byte * data)
{
  empty();
  size_t buffer_size;

  if (data)
  {
    memcpy(&buffer_size,data,sizeof(size_t));
    data += sizeof(size_t);
    const Byte * data_end = data + buffer_size;
    while (data < data_end)
    {
      Word_Length l = alphabet.word_length(data);
      reserve(l);
      alphabet.unpack(words[nr_words],data,l);
      words[nr_words][l] = TERMINATOR_SYMBOL;
      nr_words++;
      used += l+1;
      words[nr_words] = buffer + used;
      size_t size = alphabet.packed_word_size(l);
      data += size;
    }
  }
}

/**/

Sorted_Word_List::Sorted_Word_List(const Alphabet & alphabet_,Element_ID max_words_/*= 0*/) :
  words(0),
  nr_words(0),
  max_words(0),
  total_length__(0),
  Sized_Word_Collection(alphabet_)
{
  grow(max_words_);
}

/**/

Sorted_Word_List::Sorted_Word_List(const Word_List &wl) :
  words(0),
  total_length__(0),
  nr_words(0),
  max_words(0),
  Sized_Word_Collection(wl.alphabet)
{
  grow(wl.count());
  for (Element_ID i = 0;i < max_words;i++)
    insert(Entry_Word(wl,i));
}

/**/

Sorted_Word_List::Sorted_Word_List(const Sorted_Word_List &other) :
  words(0),
  nr_words(0),
  max_words(0),
  total_length__(0),
  Sized_Word_Collection(other.alphabet)
{
  Element_Count count = other.count();
  grow(count);
  for (Element_ID i = 0; i < count;i++)
    append(*other.word(i));
}

/**/

void Sorted_Word_List::take(Sorted_Word_List &other)
{
  if (this != &other)
  {
    empty();
    if (words)
      delete [] words;
    max_words = other.max_words;
    words = other.words;
    nr_words = other.nr_words;
    total_length__ = other.total_length__;
    other.max_words = other.nr_words = 0;
    other.total_length__ = 0;
    other.words = 0;
  }
}

/**/

Sorted_Word_List::~Sorted_Word_List()
{
  empty();
  if (words)
    delete [] words;
}

/**/

Sorted_Word_List & Sorted_Word_List::operator=(const Word_List &wl)
{
  Element_Count count = wl.count();
  empty();
  grow(count);
  for (Element_ID i = 0;i < count;i++)
    insert(Entry_Word(wl,i));
  return *this;
}

/**/

void Sorted_Word_List::empty()
{
  if (words)
    for (Element_ID i = 0; i < nr_words; i++)
      delete words[i];
  nr_words = 0;
  total_length__ = 0;
}

/**/

void Sorted_Word_List::grow(Element_Count new_max_words)
{
  if (new_max_words > max_words)
  {
    max_words = new_max_words;
    Ordinal_Word ** new_words = new Ordinal_Word *[max_words];
    Element_ID i;
    for (i = 0; i < nr_words;i++)
      new_words[i] = words[i];
    if (words)
      delete [] words;
    words = new_words;
  }
}

bool Sorted_Word_List::insert(const Word & new_word,Element_ID *word_nr)
{
  grow(nr_words+1);

  Element_ID low = 0;
  Element_ID high = nr_words;

  while (low < high)
  {
    Element_ID mid = low + (high-low)/2;
    if (new_word.compare(*words[mid]) > 0)
      low = mid+1;
    else
      high = mid;
  }

  if (word_nr)
    *word_nr = low;
  if (low < nr_words)
  {
    if (new_word.compare(*words[low]) == 0)
      return false;
    memmove(words+low+1,words+low,(nr_words-low)*sizeof(Ordinal_Word *));
  }
  total_length__ += new_word.length();
  words[low] = new Ordinal_Word(new_word);
  nr_words++;
  return true;
}

/**/

bool Sorted_Word_List::append(const Word & new_word,Element_ID *word_nr)
{
  grow(nr_words+1);

  if (nr_words)
  {
    int cmp = new_word.compare(*words[nr_words-1]);
    if (cmp <= 0)
    {
      if (cmp == 0)
        return false;
      return insert(new_word,word_nr);
    }
  }
  total_length__ += new_word.length();
  words[nr_words] = new Ordinal_Word(new_word);
  if (word_nr)
    *word_nr = nr_words;
  nr_words++;
  return true;
}

/**/

bool Sorted_Word_List::find(const Word & new_word,Element_ID *word_nr) const
{
  Element_ID low = 0;
  Element_ID high = nr_words;

  while (low < high)
  {
    Element_ID mid = low + (high-low)/2;
    if (new_word.compare(*words[mid]) > 0)
      low = mid+1;
    else
      high = mid;
  }
  if (word_nr)
    *word_nr = low;
  return low < nr_words && new_word.compare(*words[low]) == 0;
}

/**/

bool Sorted_Word_List::remove(const Word & word,Element_ID * word_nr)
{
  Element_ID low = 0;
  Element_ID high = nr_words;

  while (low < high)
  {
    Element_ID mid = low + (high-low)/2;
    if (word.compare(*words[mid]) > 0)
      low = mid+1;
    else
      high = mid;
  }

  if (low < nr_words && word.compare(*words[low])==0)
  {
    if (word_nr)
      *word_nr = low;
    total_length__ -= word.length();
    delete words[low];
    memmove(words+low,words+low+1,(--nr_words-low)*sizeof(Ordinal_Word *));
    return true;
  }
  return false;
}

/**/

void Word::next_bfs()
{
  Ordinal * values = buffer();
  Word_Length len = length();
  Ordinal max_letter = alphabet().letter_count()-1;
  Word_Length i;

  for (i = length(); i > 0;i--)
    if (values[i-1] != max_letter)
      break;
  if (i > 0)
    values[i-1]++;
  else
  {
    set_length(++len);
    values = buffer();
    i = 0;
  }
  for (; i < len;i++)
    values[i] = 0;
}

/**/

void * Word::extra_packed_data(size_t *size) const
{
  Word_Length l = length();
  size_t packed_size = alphabet().extra_packed_word_size(l);
  Byte * answer = new Byte[packed_size];
  unsigned density = alphabet().extra_packed_density();
  unsigned simple_code_size = alphabet().packed_word_size(size_t(0));
  memcpy(answer,&l,sizeof(l));
  if (density)
  {
    const Ordinal * values = buffer();
    if (density > sizeof(unsigned long)/simple_code_size)
    {
      unsigned long base = alphabet().letter_count();
      Byte * to = answer + sizeof(l);
      for (Total_Length i = 0; i < l; i += density) /* i must not be Word_Length here */
      {
        unsigned long v = 1;
        unsigned long c = 0;
        for (Word_Length j = 0;j < density && i+j < l ;j++)
        {
          c +=  values[i+j] * v;
          v *= base;
        }
        memcpy(to,&c,sizeof(unsigned long));
        to += sizeof(unsigned long);
      }
    }
    else
    {
      if (simple_code_size == 1)
      {
        Byte * to = answer + sizeof(l);
        for (Word_Length i = 0; i < l; i++)
        {
          Byte c = values[i];
          *to++ = c;
        }
      }
      else
        memcpy(answer+sizeof(l),values,l*sizeof(Ordinal));
    }
  }
  if (size)
    *size = packed_size;
  return answer;
}

/**/

Word_Length Word::extra_unpack(const Byte * data)
{
  Word_Length i,l;
  if (data)
    memcpy(&l,data,sizeof(l));
  else
    l = 0;

  unsigned density = alphabet().extra_packed_density();
  unsigned simple_code_size = alphabet().packed_word_size(size_t(0));
  allocate(l,false);
  Ordinal * values = buffer();

  if (density)
  {
    if (density > sizeof(unsigned long)/simple_code_size)
    {
      unsigned long base = alphabet().letter_count();
      const Byte * from = data + sizeof(l);
      for (Total_Length i = 0; i < l; i += density) /* i must not be Word_Length here */
      {
        unsigned long c;
        memcpy(&c,from,sizeof(unsigned long));
        from += sizeof(unsigned long);
        for (Word_Length j = 0;j < density && i+j < l ;j++)
        {
          values[i+j] = c % base;
          c /= base;
        }
      }
    }
    else
    {
      if (simple_code_size == 1)
      {
        const Byte * from = data + sizeof(l);
        for (Word_Length i = 0; i < l; i++)
          values[i] = *from++;
      }
      else
        memcpy(values,data+sizeof(l),l*sizeof(Ordinal));
    }
  }
  else
    for (i = 0;i < l;i++)
      values[i] = 0;
  return l;
}
