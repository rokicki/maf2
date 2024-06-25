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


// $Log: alphabet.cpp $
// Revision 1.21  2011/06/11 15:45:18Z  Alun
// Failed to raise parse error if there was an unmatched closing parenthesis
// Revision 1.20  2010/06/17 21:58:07Z  Alun
// Typo fixed that allowed creation of alphabets that are too big
// Revision 1.19  2010/06/10 16:39:02Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.18  2010/06/08 06:33:53Z  Alun
// rename_letter() method added.
// Improved handling of parse errors
// Revision 1.17  2009/11/12 12:06:45Z  Alun
// Minor changes to ensure clean compilation with GNU
// Revision 1.16  2009/11/08 22:56:59Z  Alun
// Various methods changed to accept wider integer types
// so that subgroup presentation has a chance to fail gracefully
// if limit on number of generators is exceeded.
// Various minor changes to improve portability, in particular to allow
// for possible future change to allow more than 2^15 generators
// Revision 1.15  2009/09/16 07:42:17Z  Alun
// Additional source code changes needed to get clean compilation on latest GNU
// Revision 1.14  2009/09/14 09:57:23Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// automation implemented for grouped word order
// Revision 1.13  2009/01/01 21:23:44Z  Alun
// Changes to avoid performance problems on very large alphabets
// Revision 1.14  2008/11/05 01:16:49Z  Alun
// Now have two varieties of "accented order", both automisable
// Revision 1.13  2008/11/03 01:12:18Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.12  2008/10/10 07:14:50Z  Alun
// Simple_Alphabet should be derived from Word_Alphabet
// Bug in parsing identifiers containing ^-1 fixed
// Revision 1.11  2008/09/29 21:38:59Z  Alun
// Switch to using Hash+Collection_Manager as replacement for Indexer.
// Currently this is about 10% slower, but this is a more flexible
// architecture.
// Revision 1.6  2007/12/20 23:25:42Z  Alun
//

#include <string.h>
#include <stdio.h>
#include "maf.h"
#include "mafword.h"
#include "mafctype.h"
#include "container.h"
#include "maf_sdb.h"

struct Alphabet::Rank_Work_Area
{
  Word_Length l1; // length of subword at this level
  Word_Length l2;
  Word_Length start1; // start of subword in sorted word
  Word_Length start2;
  Word_Length p1; // where we got to so far
  Word_Length p2;
};

const Alphabet::Order_Properties Alphabet::order_properties[] =
{
 {WO_Shortlex,Order_Is_Geodesic,Greater_Than_Finite,0,1},
 {WO_Weighted_Lex,Order_Limited_Increase,Greater_Than_Infinite,ORDER_HAS_WEIGHTS,1+sizeof(int)},
 {WO_Weighted_Shortlex,Order_Limited_Increase,Greater_Than_Infinite,ORDER_HAS_WEIGHTS,1+sizeof(int)},
 {WO_Accented_Shortlex,Order_Is_Geodesic,Greater_Than_Infinite,ORDER_HAS_LEVELS,1+sizeof(Ordinal)},
 {WO_Multi_Accented_Shortlex,Order_Is_Geodesic,Greater_Than_Finite,ORDER_HAS_LEVELS,1+sizeof(Ordinal)},
 {WO_Recursive,Order_Indefinite_Increase,Greater_Than_Infinite,ORDER_WEIGHT_MOVES_LEFT,0},
 {WO_Right_Recursive,Order_Indefinite_Increase,Greater_Than_Infinite,ORDER_WEIGHT_MOVES_RIGHT,0},
 {WO_Wreath_Product,Order_Indefinite_Increase,Greater_Than_Infinite,ORDER_HAS_LEVELS|ORDER_WEIGHT_MOVES_LEFT,0},
 {WO_Right_Wreath_Product,Order_Indefinite_Increase,Greater_Than_Infinite,ORDER_HAS_LEVELS|ORDER_WEIGHT_MOVES_RIGHT,0},
 {WO_Short_Recursive,Order_Is_Geodesic,Greater_Than_Infinite,0,0},
 {WO_Short_Right_Recursive,Order_Is_Geodesic,Greater_Than_Infinite,0,0},
 {WO_Short_Weighted_Lex,Order_Is_Geodesic,Greater_Than_Infinite,ORDER_HAS_WEIGHTS,1+sizeof(int)},
 {WO_Right_Shortlex,Order_Is_Geodesic,Greater_Than_Finite,0,1},
 {WO_Short_FPTP,Order_Is_Geodesic,Greater_Than_Finite,0,1+sizeof(Ordinal)},
 {WO_Grouped,Order_Indefinite_Increase,Greater_Than_Infinite,ORDER_HAS_LEVELS,0},
 {WO_Ranked,Order_Indefinite_Increase,Greater_Than_Not_Implemented,ORDER_HAS_LEVELS|ORDER_WEIGHT_MOVES_LEFT,0},
 {WO_NestedRank,Order_Indefinite_Increase,Greater_Than_Not_Implemented,ORDER_HAS_LEVELS|ORDER_WEIGHT_MOVES_LEFT,0},
 {WO_Coset,Order_Indefinite_Increase,Greater_Than_Not_Implemented,ORDER_HAS_LEVELS|ORDER_WEIGHT_MOVES_LEFT,0}
};

class Letter_Alphabet : public Alphabet
{
  private:
    Letter * letter_area;
    Ordinal values[UCHAR_MAX+1];

  public:
    Letter_Alphabet(Container & container) :
       Alphabet(container,false)
    {
      letter_area = 0;
      code_size = 1;
    }

    ~Letter_Alphabet()
    {
      if (letter_area)
        delete letter_area;
    }

    void delete_letters(bool reallocate)
    {
      if (letter_area)
        delete [] letter_area;
      if (reallocate)
      {
        letter_area = new Letter[nr_letters*2];
        memset(letter_area,0,nr_letters*2*sizeof(Letter));
      }
      else
        letter_area = 0;
    }

    bool set_letter(Ordinal value,Glyph identifier)
    {
      if (!identifier[0] || identifier[1] || strchr("*()^,[] ",identifier[0]) ||
          value < 0 || value >= nr_letters)
        return false;
      letter_area[value*2] = identifier[0];
      if (values[identifier[0]] != -2)
        return false;
      values[identifier[0]] = value;
      return true;
    }

    bool set_letters(String letters_)
    {
      bool retcode = true;
      Letter glyph[2];
      size_t l = letters_.length();
      if (l > size_t(capacity()) || !set_nr_letters(Element_Count(l)))
        return false;
      delete_letters(true);
      values[0] = IdWord;
      for (size_t i = 1; i <= UCHAR_MAX;i++)
        values[i] = -2;
      glyph[1] = 0;
      for (Ordinal g = 0; g < nr_letters;g++)
      {
        glyph[0] = letters_[g];
        if (!set_letter(g,glyph))
          retcode = false;
      }
      return retcode;
    }

    Element_Count capacity() const
    {
      return UCHAR_MAX;
    }
    bool set_nr_letters(Element_Count nr_letters)
    {
      if (!Alphabet::set_nr_letters(nr_letters))
        return false;
      delete_letters(true);
      set_word_ordering(order_type());
      return true;
    }

    bool set_next_letter(Glyph glyph)
    {
      Ordinal i = 0;
      if (letter_area)
        while (i < nr_letters && letter_area[i*2])
          i++;
      if (i < nr_letters)
        return set_letter(i,glyph);
      return false;
    }

    size_t format_raw(Letter * output_buffer,size_t buffer_size,const Word & word) const
    {
      Word_Length l = word.length();
      size_t answer = l;
      if (size_t(l) > buffer_size)
        l = Word_Length(buffer_size);
      Word_Length i = 0;
      for (; i < l;i++)
        output_buffer[i] = letter_area[word.value(i)*2];
      if (i < buffer_size)
        output_buffer[i] = 0;
      return answer;
    }

    Glyph glyph(Ordinal value) const
    {
      if (value < IdWord || value >= nr_letters)
        return "";
      return value == IdWord ? "IdWord" : letter_area+value*2;
    }

    size_t pack(Byte * buffer,const Ordinal *value,Word_Length length) const
    {
      Word_Length i = 0;
      for (; i < length;i++)
        buffer[i] = letter_area[value[i]*2];
      buffer[i] = 0;
      return i+1;
    }

    Ordinal parse_identifier(const Letter *&input_string,bool) const
    {
      return value((Byte *) input_string++);
    }

    Alphabet * truncated_alphabet(Element_Count) const
    {
      return 0;
    }

    void unpack(Ordinal *ordinals,const Byte * buffer,Word_Length length) const
    {
      for (Word_Length i = 0; i < length;i++)
        ordinals[i] = values[(unsigned char) buffer[i]];
    }

    Ordinal value(const Byte *data) const
    {
      return values[* (unsigned char *) data];
    }

    Ordinal value(const Byte *data,Word_Length offset) const
    {
      return values[((unsigned char *) data)[offset]];
    }

    Word_Length word_length(const Byte *data) const
    {
      Word_Length i = 0;
      if (data)
        for (; data[i];i++)
          ;
      return i;
    }

};

class Word_Alphabet : public Alphabet
{
  private:
    Letter ** letters;
    String_DB * strings;
  public:

    Word_Alphabet(Container & container) :
      Alphabet(container,true),
      strings(0)
    {
      letters = 0;
    }

    virtual ~Word_Alphabet()
    {
      delete_letters();
    }

    void delete_letters()
    {
      if (strings)
      {
        delete strings;
        strings = 0;
      }
      if (letters)
      {
        for (Ordinal g = 0; g < nr_letters;g++)
          delete [] letters[g];
        delete [] letters;
      }
    }

    bool set_letter(Ordinal value,Glyph identifier,size_t length)
    {
      if (value < 0 || value >= nr_letters || letters[value])
        return false;
      letters[value] = new Letter[length+1];
      memcpy(letters[value],identifier,length*sizeof(Letter));
      letters[value][length] = 0;
      Element_ID g;
      if (strings->find(letters[value],length,&g))
      {
        container.input_error("%s has already been used for generator "
                              FMT_ID "\n",letters[value],g);
        return false;
      }
      strings->insert(letters[value],length,&g);
      if (g != value+1)
        return false;
      return true;
    }

    bool rename_letter(Ordinal value,Glyph identifier)
    {
      if (value < 0 || value >= nr_letters || !letters[value])
        return false;
      size_t length = identifier.length();
      Element_ID g;
      if (strings->find(identifier,length,&g))
      {
        container.input_error("%s has already been used for generator "
                              FMT_ID "\n",identifier.string(),g);
        return false;
      }
      delete letters[value];
      letters[value] = identifier.clone();
      return strings->set_key(value+1,letters[value],length);
    }

    bool set_letters(String letters_)
    {
      bool counted = false;
      bool retcode = true;

      for (;;)
      {
        const Letter * s = letters_;
        Element_Count found = 0;
        for (;;)
        {
          while (is_white(*s))
            s++;
          const Letter *start = s;
          if (!is_initial(*s))
            break;
          while (is_flagged(*s,CC_Subsequent|CC_Dot))
            s++;
          if (memcmp(s,"^-1",3) == 0)
            s += 3;
          if (counted)
            if (!set_letter(Ordinal(found),start,s-start))
              retcode = false;
          found++;
          while (is_white(*s))
            s++;
          if (*s == ',')
            s++;
        }
        if (found > capacity())
          container.input_error("Maximum alphabet size (" FMT_ID ") exceeded. "
                                FMT_ID " symbols found.\n",capacity(),found);
        if (*s || found > capacity())
          return false;
        if (counted)
          break;
        set_nr_letters(found);
        counted = true;
      }
      return retcode;
    }

    bool set_next_letter(Glyph glyph)
    {
      Ordinal i = strings ? strings->count()-1 : 0;
      while (i < nr_letters && letters[i])
        i++;
      if (i < nr_letters)
        return set_letter(i,glyph,glyph.length());
      return false;
    }

    Element_Count capacity() const
    {
      return SHRT_MAX-1;
    }

    bool set_nr_letters(Element_Count nr_letters)
    {
      if (!Alphabet::set_nr_letters(nr_letters))
        return false;
      delete_letters();
      letters = new Letter *[nr_letters];
      strings = new String_DB(nr_letters);
      strings->insert("IdWord",6);
      for (Ordinal g = 0; g < nr_letters;g++)
        letters[g] = 0;
      set_word_ordering(order_type());
      return true;
    }

    size_t format_raw(Letter * output_buffer,size_t buffer_size,const Word & word) const
    {
      Word_Length l(word.length());
      const Ordinal * values = word.buffer();
      Ordinal old_g = values[0];
      Glyph s(glyph(old_g));
      size_t was_answer_size(0);
      size_t gl(s.length());
      size_t answer_size(gl);
      if (answer_size <= buffer_size)
        memcpy(output_buffer,s,answer_size);
      for (Word_Length i = 1; i < l;i++)
      {
        was_answer_size = answer_size;
        Ordinal g = values[i];
        if (g == old_g)
        {
          int power = 1;
          char exponent[12];
          while (i + power < l && values[i+power] == g)
            power++;
          i += power++ - 1;
          int el = sprintf(exponent,"%d",power);
          if (strcmp(s + gl -3,"^-1")==0)
          {
            answer_size += el-1;
            if (answer_size <= buffer_size)
              memcpy(output_buffer+was_answer_size-1,exponent,el);
          }
          else
          {
            answer_size += el+1;
            if (answer_size <= buffer_size)
            {
              output_buffer[was_answer_size] = '^';
              memcpy(output_buffer+was_answer_size+1,exponent,el);
            }
          }
        }
        else
        {
          answer_size++;
          if (answer_size <= buffer_size)
            output_buffer[was_answer_size] = '*';
          s = glyph(old_g = g);
          was_answer_size = answer_size;
          gl = s.length();
          answer_size += gl;
          if (answer_size <= buffer_size)
            memcpy(output_buffer+was_answer_size,s,gl);
        }
      }
      if (answer_size < buffer_size)
        output_buffer[answer_size] = 0;
      return answer_size;
    }

    Glyph glyph(Ordinal value) const
    {
      if (value < IdWord || value >= nr_letters)
        return "";
      return value == IdWord ? "IdWord" : letters[value];
    }

    size_t pack(Byte * buffer,const Ordinal *values,Word_Length length) const
    {
      if (nr_letters > UCHAR_MAX)
      {
        short * b = (short *) buffer;
        Word_Length i = 0;
        for (; i < length;i++)
          b[i] = values[i]+1;
        b[i] = 0;
        return (i+1)*2;
      }
      else
      {
        Word_Length i = 0;
        for (; i < length;i++)
          buffer[i] = values[i]+1;
        buffer[i] = 0;
        return i+1;
      }
    }

    Ordinal parse_identifier(const Letter *&input_string,bool allow_inverse) const
    {
      Word_Length l = 0;
      const Letter * start = input_string;
      if (is_initial(*input_string))
      {
        l++;
        input_string++;
      }
      for (;;)
      {
        Letter c = *input_string;
        if (!is_flagged(c,CC_Subsequent|CC_Dot))
          break;
        l++;
        input_string++;
      }
      if (allow_inverse && memcmp(input_string,"^-1",3) == 0)
      {
        l += 3;
        input_string += 3;
      }
      Element_ID g;
      if (strings->find(start,l,&g))
        return g-1;
      return INVALID_SYMBOL;
    }

    Alphabet * truncated_alphabet(Element_Count nr_letters) const
    {
      if (nr_letters < 0 || nr_letters > this->nr_letters)
        return 0;
      Word_Alphabet * truncation = new Word_Alphabet(container);
      truncation->set_nr_letters(nr_letters);
      truncation->code_size = code_size; // We are going to force the code size
                                         // to be the same so that the new
                                         // alphabet can understand G words from
                                         // this alphabet, and indeed pack and
                                         // unpack any word in this alphabet
      for (Ordinal g = 0; g < nr_letters; g++)
        truncation->letters[g] = glyph(g).clone();
      truncation->set_word_ordering_from(*this);
      return truncation;
    }

    void unpack(Ordinal *values,const Byte * buffer,Word_Length length) const
    {
      if (nr_letters > UCHAR_MAX)
      {
        short * b = (short *) buffer;
        for (Word_Length i = 0; i < length;i++)
          values[i] = b[i]-1;
      }
      else
      {
        for (Word_Length i = 0; i < length;i++)
          values[i] = buffer[i]-1;
      }
    }

    Ordinal value(const Byte *buffer) const
    {
      if (nr_letters <= UCHAR_MAX)
        return * (unsigned char *) buffer -1;
      return * (short *) buffer - 1; // safe because buffer is aligned
    }

    Ordinal value(const Byte *buffer,Word_Length offset) const
    {
      if (nr_letters <= UCHAR_MAX)
        return ((unsigned char *) buffer)[offset] -1;
      return ((short *) buffer)[offset] - 1; // safe because buffer is aligned
    }

    Word_Length word_length(const Byte *data) const
    {
      if (code_size == 1)
      {
        Word_Length i = 0;
        if (data)
          for (; data[i];i++)
            ;
        return i;
      }
      const short * d = (short *) data;
      Word_Length i = 0;
      if (d)
        for (; d[i];i++)
          ;
      return i;
    }
};

class Simple_Alphabet : public Word_Alphabet
{
  public:

    Simple_Alphabet(Container & container) :
      Word_Alphabet(container)
    {
    }

    bool set_letters(String)
    {
      return false;
    }

    bool set_next_letter(Glyph)
    {
      return false;
    }

    bool set_nr_letters(Element_Count nr_letters)
    {
      if (!Alphabet::set_nr_letters(nr_letters))
        return false;
      set_word_ordering(order_type());
      return true;
    }
    Glyph glyph(Ordinal value) const
    {
      if (value < IdWord || value >= nr_letters)
        return "";
      if (value == IdWord)
        return "IdWord";
      static Letter g[32];
      sprintf(g,"g%d",value);
      return g;
    }

};

/**/

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4127)
#endif

Alphabet * Alphabet::create(Alphabet_Type format,Container & container)
{
  switch (format)
  {
    case AT_Char:
      if (sizeof(Letter) == 1)
        return new Letter_Alphabet(container);
    case AT_String:
      return new Word_Alphabet(container);
    case AT_Simple:
      return new Simple_Alphabet(container);
    default:
      return 0;
  }
}

/**/

bool Alphabet::set_nr_letters(Element_Count nr_letters_)
{
  if (nr_letters_ < 0 || nr_letters_ > capacity())
    return false;
  nr_letters = Ordinal(nr_letters_);
  code_size = nr_letters <= UCHAR_MAX ? 1 : 2;
  density = 0;
  unsigned long v = 1;
  if (nr_letters > 1)
  {
    while (v <= ULONG_MAX/nr_letters)
    {
      v *= nr_letters;
      density++;
    }
    if (v <= ULONG_MAX/(nr_letters-1) &&
        ULONG_MAX-v*(nr_letters-1) >= v-1)
      density++;
  }
  return true;
}

/**/

#ifdef _MSC_VER
#pragma warning(pop)
#endif

Alphabet::Alphabet(Container & container_,bool need_operator_symbol_) :
  weights(0),
  levels(0),
  ranks(0),
  rwa(0),
  word_ordering(WO_Shortlex),
  container(container_),
  reference_count(0),
  ilevels(0),
  representatives(0),
  need_operator_symbol(need_operator_symbol_)
{
}

/**/

void Alphabet::remove_order()
{
  if (weights)
  {
    delete [] weights;
    weights = 0;
  }
  if (levels)
  {
    delete [] levels;
    levels = 0;
  }
  if (ilevels)
  {
    delete [] ilevels;
    ilevels = 0;
  }
  if (representatives)
  {
    delete [] representatives;
    representatives = 0;
  }
  if (ranks)
  {
    delete [] ranks;
    ranks = 0;
  }
  if (rwa)
  {
    delete [] rwa;
    rwa = 0;
  }
}

Alphabet::~Alphabet()
{
  remove_order();
}

void Alphabet::compute_ranks()
{
  /* ranks[] essentially duplicates the information in levels, but
     with reversed values, so that generators with a higher value in levels[]
     have a lower value in ranks[]. Also the values in ranks[] are guaranteed
     to be contiguous from 0.
     The values are reversed so that the ranked_compare() and grouped_compare()
     methods encounter higher level generators first in the rwa.

     ilevels[] essentially duplicates the information in levels, but with
     the levels adjusted to be contiguous, so that the values are as small
     as is consistent with the ordering. Also we count the number of generators
     at each level, since this affects how much data needs to be stored in
     gt_state.

     We could probably get by with just one of ranks and ilevels, but having
     both makes the code for the relevant orders simpler to understand.
  */

  if (ranks && levels)
  {
    unsigned done_level = ~0u;
    Ordinal done = 0;
    nr_levels = 0;
    while (done < nr_letters)
    {
      Ordinal i;
      unsigned worst_level = 0;
      for (i = 0;i < nr_letters;i++)
        if (levels[i] > worst_level && levels[i] < done_level)
          worst_level = levels[i];
      for (i = 0;i < nr_letters;i++)
        if (levels[i] == worst_level)
        {
          ranks[i] = nr_levels;
          done++;
        }
      done_level = worst_level;
      nr_levels++;
    }
    if (rwa)
      delete [] rwa;
     rwa = new Rank_Work_Area[nr_levels];
  }

  if (!ranks || word_ordering == WO_Grouped)
  {
    /* This is for the various wreath like options, and recursive orderings */
    if (!ilevels)
      ilevels = new Ordinal[nr_letters];
    unsigned done_level = 0u;
    Ordinal done = 0;
    nr_levels = 0;
    if (levels)
    {
      while (done < nr_letters)
      {
        Ordinal i;
        unsigned best_level = ~0u;
        for (i = 0;i < nr_letters;i++)
          if (levels[i] < best_level && levels[i] > done_level)
            best_level = levels[i];
        for (i = 0;i < nr_letters;i++)
          if (levels[i] == best_level)
          {
            ilevels[i] = nr_levels+1;
            done++;
          }
        done_level = best_level;
        nr_levels++;
      }
    }
    else
    {
      for (Ordinal g = 0;g < nr_letters;g++)
        ilevels[g] = g+1;
      nr_levels = nr_letters;
    }
    if (!ranks)
    {
      if (representatives)
        delete [] representatives;
      representatives = new Ordinal[nr_levels+1];
      for (Ordinal g = 0; g < nr_letters;g++)
        representatives[ilevels[g]] = g;
    }
  }
}

/**/

bool Alphabet::set_similar_letters(Glyph prefix,Ordinal nr_similar_letters)
{
  Ordinal i;
  for (i = 0; i < nr_letters;i++)
  {
    Glyph g = glyph(i);
    if (!g)
      break;
    if (!*g)
      return false;  // in this case we are seeing an AT_Letter alphabet
  }

  bool retcode = true;
  int j = 1;
  nr_similar_letters += i;
  if (nr_similar_letters > nr_letters)
    nr_similar_letters = nr_letters;
  String_Buffer sb;
  for (;i < nr_similar_letters;i++,j++)
  {
    sb.format("%s%d",prefix.string(),j);
    if (!set_next_letter(sb.get()))
      retcode = false;
  }
  return retcode;
}

/**/

/* Methods for parsing words.
   Because we need to be able to invert words, it would be much more
   natural for these methods to belong in the Presentation class - where
   in fact they used to be. But when reading word-difference FSAs we
   need to be able to parse words without the full presentation being
   available. In this case we won't be able to understand words that use
   negative powers, except for the case where a generator has ^-1 as
   part of its name.
*/

static bool parse_inner(Parse_Error_Handler & error_handler,
                        const Presentation * presentation,Ordinal_Word *word,
                        const Letter * &s,bool operator_optional)
{
  Word_Length length = word->length();
  int prev_length = -1;
  bool operand_expected = true;
  bool operand_allowed = true;
  bool operator_allowed = false;
  const Letter * start = s;

  while (*s && *s != ')')
  {
    if (*s == '(')
    {
      prev_length = length;
      s++;
      if (!parse_inner(error_handler,presentation,word,s,operator_optional))
        return false;
      if (*s != ')')
      {
        error_handler.input_error("Expected ) Found %c\n"
                                  "(Parsing %s)\n"
                                  "(at      %*s)\n",*s,start,
                                  int(s - start+1),"^");
        return false;
      }
      s++;
      length = word->length();
      operand_expected = false;
      operand_allowed = operator_optional;
      operator_allowed = true;
    }
    else if (*s == '^')
    {
      if (prev_length < 0)
      {
        error_handler.input_error("^ encountered unexpectedly\n"
                                  "(Parsing %s)\n"
                                  "(at      %*s)\n",start,
                                  int(s - start+1),"^");

        return false;
      }
      int sign = 1;
      int answer = 0;
      bool found = false;
      s++;
      while (*s == ' ')
        s++;
      if (*s == '-')
      {
        sign = -1;
        s++;
      }
      while (*s >= '0' && *s <= '9')
      {
        answer = answer*10+*s++-'0';
        found = true;
      }
      if (!found)
      {
        /* This is different from KBMAG. KBMAG accepts a word such as
           a^*X, as though it were a^0*X, MAF rejects this as a syntax
           error as it is highly unlikely this would have been done
           intentionally  */
        error_handler.input_error("Integer expected following ^\n"
                                  "(Parsing %s)\n"
                                  "(at      %*s)\n",start,
                                  int(s - start+1),"^");
        return false;
      }
      if (prev_length + (length - prev_length)*answer > MAX_WORD)
      {
        error_handler.input_error("Word too long\n");
        return false;
      }
      Ordinal_Word subword(Subword(*word,Word_Length(prev_length),length));
      if (sign == -1)
        if (!presentation || !presentation->invert(&subword,subword))
        {
          bool ok = false;
          if (subword.length()==1)
          {
            /* We are seeing input of the form x^-n outside a group
               presentation . Before rejecting it
               see if x^-1 is an identifier, and if so treat the input
               as x^-1^n
            */
            String_Buffer sb;
            const Alphabet &alphabet = word->alphabet();
            sb.set(alphabet.glyph(subword.value(0)));
            sb.append("^-1");
            const Letter * s = sb.get();
            Ordinal v = alphabet.parse_identifier(s,true);
            if (v != INVALID_SYMBOL)
            {
              ok = true;
              subword.set_code(0,v);
            }
          }
          if (!ok)
          {
            error_handler.input_error("Negative power applied to non-invertible term\n");
            return false;
          }
        }
      // We already checked that the word is not too long yet
      word->allocate(length = prev_length + (length-prev_length)*answer,true);
      word->set_length(Word_Length(prev_length));
      for (int i = 0; i < answer;i++)
        *word += subword;
    }
    else if (*s == ' ')
      s++;
    else if (*s == '*')
    {
      if (operand_expected)
      {
        error_handler.input_error("Expected a generator. Found *.\n"
                                  "(Parsing %s)\n"
                                  "(at      %*s)\n",start,
                                  int(s - start+1),"^");
        return false;
      }
      operand_expected = operand_allowed = true;
      operator_allowed = false;
      s++;
    }
    else
    {
      if (!operand_allowed)
      {
        error_handler.input_error("Unexpected generator."
                                  " Separate terms with * operator."
                                  " Did you miss a ','?\n"
                                  "(Parsing %s)\n"
                                  "(at      %*s)\n",start,
                                  int(s - start+1),"^");
        return false;
      }
      operand_expected = false;
      operand_allowed =  operator_optional;
      operator_allowed = true;
      const Letter * save = s;
      Ordinal v = word->alphabet().parse_identifier(s,presentation == 0);
      if (length && v == IdWord)
      {
        error_handler.input_error("IdWord not allowed here\n"
                                  "(Parsing %s)\n"
                                  "(at      %*s)\n",start,
                                  int(s - start+1),"^");
        return false;
      }
      if (v >= 0)
      {
        prev_length = length;
        word->set_length(length+1);
        word->set_code(length++,v);
      }
      else if (v == INVALID_SYMBOL)
      {
        error_handler.input_error("Invalid symbol %.*s encountered\n"
                                  "(Parsing %s)\n"
                                  "(at      %*s)\n",int(s-save),save,start,
                                  int(s - start+1),"^");
        return false;
      }
    }
  }
  return true;
}

/**/

bool Alphabet::parse(Ordinal_Word *word,String string,String_Length length,
                     const Presentation * presentation,
                     Parse_Error_Handler &error_handler) const
{
  Letter * copy = 0;
  if (length == WHOLE_STRING)
  {
    length = 0;
    while (string[length])
      length++;
  }

  const Letter * s = string;
  if (string[length])
  {
    string = copy = new Letter[length];
    memcpy(copy,string,length*sizeof(Letter));
    copy[length] = 0;
    s = copy;
  }
  word->set_length(0);
  bool retcode = parse_inner(error_handler,presentation,word,s,!need_operator_symbol);
  if (*s)
    error_handler.input_error("Expected end of expression, found \"%s\"\n"
                              "(Parsing %s)\n"
                              "(at      %*s)\n",s,string.string(),
                              int(s - string+1),"^");
  if (copy)
    delete [] copy;
  return retcode;
}

/**/

Ordinal_Word * Alphabet::parse(String string,String_Length length,
                               const Presentation * presentation,
                               Parse_Error_Handler & error_handler) const
{
  Ordinal_Word *word = new Ordinal_Word(*this);
  if (parse(word,string,length,presentation,error_handler))
    return word;
  delete word;
  return 0;
}

/**/

void Alphabet::print(Container &container,Output_Stream * stream,Alphabet_Print_Format format) const
{
  if (format == APF_GAP_Product)
  {
    container.output(stream,"  alphabet := rec\n"
                            "  (\n"
                            "    type := \"product\",\n"
                            "    size := %d,\n"
                            "    arity := 2,\n"
                            "    padding := _,\n",product_alphabet_size());
    container.output(stream,"    base := rec\n"
                            "    (\n"
                            "      type := \"identifiers\",\n"
                            "      size := %d,\n"
                            "      format := \"dense\",\n"
                            "      names  := [",nr_letters);
  }
  else if (format == APF_GAP_Normal)
    container.output(stream,"  alphabet := rec\n"
                            "  (\n"
                            "    type := \"identifiers\",\n"
                            "    size := %d,\n"
                            "    format := \"dense\",\n"
                            "    names  := [",nr_letters);
  if (nr_letters)
  {
    container.output(stream,"%s",glyph(0).string());
    for (Ordinal i = 1; i < nr_letters;i++)
      container.output(stream,",%s",glyph(i).string());
  }

  if (format == APF_GAP_Product)
    container.output(stream,"]\n"
                     "    )\n"
                     "  ),\n");
  else if (format == APF_GAP_Normal)
    container.output(stream,"]\n"
                     "  ),\n");
}

/**/

String const * Alphabet::word_orderings()
{
  /* these must match Word_Ordering enumeration in mafbase.h */

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4640) /* static structures not thread-safe
                                   This is, because it is const */
#endif

  static String const orderings[] =
  {
    "shortlex",
    "wtlex",
    "wtshortlex",
    "short_accentedlex",
    "short_multiaccentedlex",
    "recursive",
    "rt_recursive",
    "wreathprod",
    "rt_wreathprod",
    "short_recursive",
    "short_rt_recursive",
    "short_wtlex",
    "short_rtlex",
    "short_fptp",
    "grouped",
    "ranked",
    "nestedrank",
    "coset",
     0
  };
#ifdef _MSC_VER
#pragma warning(pop)
#endif
  return orderings;
}

void Alphabet::print_ordering(Container &container,Output_Stream * stream) const
{
  container.output(stream,"  ordering := \"%s\",\n",word_orderings()[word_ordering].string());
  if (weights)
  {
    container.output(stream,"  weight := [");
    if (nr_letters)
    {
      container.output(stream,"%u",weights[0]);
      for (Ordinal i = 1; i < nr_letters;i++)
        container.output(stream,",%u",weights[i]);
      container.output(stream,"],\n");
    }
  }
  if (levels)
  {
    container.output(stream,"  level := [");
    if (nr_letters)
    {
      container.output(stream,"%u",levels[0]);
      for (Ordinal i = 1; i < nr_letters;i++)
        container.output(stream,",%u",levels[i]);
      container.output(stream,"],\n");
    }
  }
}

/**/

size_t Alphabet::format(String_Buffer * sb,const Word & word) const
{
  size_t allocated;
  Letter * temp = sb->reserve(0,&allocated);
  size_t needed = format_raw(temp,allocated,word);
  if (needed >= allocated)
  {
    temp = sb->reserve(needed,&allocated);
    needed = format_raw(temp,allocated,word);
  }
  return needed;
}

/**/

/* In KBMAG the ordering of words is a property of the presentation, not of
   the underlying alphabet. But in MAF we may well want to compare words in
   contexts where the presentation information is not readily available. So
   it makes sense to set ordering at the Alphabet level. Unless we do this we
   should have to ensure that whenever we want to compare words that a pointer
   to a Presentation record is available, and this would mean adding extra
   parameters to many methods.
   It would make sense to have a base Ordering class with a virtual
   compare() method and to establish the ordering by setting a pointer to
   an instance of it into the alphabet, but I have not done this for now.
*/

/**/

void Alphabet::set_word_ordering(Word_Ordering word_ordering_)
{
  remove_order();
  word_ordering = word_ordering_;
  switch (word_ordering)
  {
    case WO_Short_FPTP:
    case WO_Shortlex:
    case WO_Right_Shortlex:
      break; // nothing to do for these orders:
    case WO_Right_Recursive:
    case WO_Recursive:
    case WO_Short_Recursive:
    case WO_Short_Right_Recursive:
      compute_ranks();
      break;
    case WO_Short_Weighted_Lex:
    case WO_Weighted_Shortlex:
    case WO_Weighted_Lex:
      {
        weights = new unsigned[nr_letters];
        for (Ordinal i = 0; i < nr_letters;i++)
          weights[i] = 1; /* Make it shortlex until explicit weights specified */
      }
      break;
    case WO_Grouped:
    case WO_Ranked:
    case WO_NestedRank:
      {
        ranks = new Ordinal[nr_letters];
        for (Ordinal i = 0; i < nr_letters;i++)
          ranks[i] = 0; /* Make it shortlex until explicit levels specified */
      }
      /* fall through */
    case WO_Right_Wreath_Product:
    case WO_Wreath_Product:
    case WO_Coset:
    case WO_Accented_Shortlex:
    case WO_Multi_Accented_Shortlex:
      {
        levels = new unsigned[nr_letters];
        for (Ordinal i = 0; i < nr_letters;i++)
          levels[i] = 1; /* Make it shortlex until explicit levels specified */
        compute_ranks();
      }
      break;
  }
}

/**/

void Alphabet::set_word_ordering_from(const Alphabet & other)
{
  // This function is used to ensure that the truncated alphabet used
  // by some of the FSAs in a coset system is order-compatible with
  // the coset system alphabet. Conversely when a new alphabet is
  // created for Schreier generators the g part of is made order
  // compatible with the truncated alphabet, and the rest is all set
  // to level 1
  Word_Ordering wo = other.order_type();
  set_word_ordering(wo);
  coset_symbol = other.coset_symbol;
  Ordinal common = min(nr_letters,other.letter_count());
  Ordinal g;
  if (order_properties[wo].has_levels())
  {
    for (g = 0; g < common; g++)
      set_level(g,other.level(g),false);
    for (g = common; g < nr_letters; g++)
      set_level(g,1,false);
  }
  if (order_properties[wo].has_weights())
    for (g = 0; g < common; g++)
      weights[g] = other.weights[g];
  compute_ranks();
}

/**/

bool Alphabet::set_coset_order(Ordinal coset_symbol)
{
  /* KBMAG allows coset systems to be based on an RWS using an
     ordering other than shortlex. It should not do so because it
     is unable to cope with them because an equation of the form
     a*b=b*a will cause problems when b has a higher level than a if
     a is a subgroup generator.
     For in this case we have _H*a*b=x*_H*b=_H*b*a.
     In the Wreath ordering x*_H*b > _H*b*a, whereas we require it to
     be less.
     MAF works around this by silently changing the order of coset systems
     from WO_Wreath_Product to WO_Coset if necessary.
  */
  if (coset_symbol < nr_letters)
  {
    this->coset_symbol = coset_symbol;
    if (word_ordering == WO_Wreath_Product)
    {
      bool level = true;
      for (Ordinal g = 1;g < coset_symbol;g++)
        if (levels[g] != levels[0])
          level = false;
      if (!level)
        word_ordering = WO_Coset;
    }
    if (word_ordering == WO_Wreath_Product || word_ordering == WO_Coset)
      return true;
  }
  return false;
}

/**/

bool Alphabet::operator==(const Alphabet & other) const
{
  /* we ignore the ordering method when comparing alphabets
     as we only ever do this for FSA alphabets, for which word ordering is
     irrelevant, or when associating an FSA with a MAF instance, in which
     case we want to be able to recognise the alphabet as being the same
     as the MAF alphabet, which is ordered.
  */
  if (this == &other)
    return true;
  if (nr_letters != other.nr_letters)
    return false;
  for (Ordinal g = 0; g < nr_letters; g++)
    if (strcmp(glyph(g),other.glyph(g)))
      return false;
  return true;
}

int Alphabet::compare(const Word &lhs,const Word &rhs) const
{
  /* All compare functions should return 0 if lhs === rhs
     1 if lhs > rhs
     -1 if lhs < rhs
     Some of the methods are shamelessly adapted from KBMAG.
  */
  switch (word_ordering)
  {
    case WO_Shortlex:
      return shortlex_compare(lhs,rhs);
    case WO_Right_Shortlex:
      return right_shortlex_compare(lhs,rhs);
    case WO_Weighted_Lex:
      return wtlex_compare(lhs,rhs);
    case WO_Weighted_Shortlex:
      return wtshortlex_compare(lhs,rhs);
    case WO_Accented_Shortlex:
      return accented_compare(lhs,rhs,false);
    case WO_Multi_Accented_Shortlex:
      return accented_compare(lhs,rhs,true);
    case WO_Wreath_Product:
      return wreath_compare(lhs,rhs);
    case WO_Right_Wreath_Product:
      return right_wreath_compare(lhs,rhs);
    case WO_Recursive:
      return recursive_compare(lhs,rhs);
    case WO_Right_Recursive:
      return right_recursive_compare(lhs,rhs);
    case WO_Short_Recursive:
      return short_recursive_compare(lhs,rhs);
    case WO_Short_Right_Recursive:
      return short_right_recursive_compare(lhs,rhs);
    case WO_Short_Weighted_Lex:
      return short_wtlex_compare(lhs,rhs);
    case WO_Short_FPTP:
      return short_fptp_compare(lhs,rhs);
    case WO_Ranked:
      return ranked_compare(lhs,rhs);
    case WO_NestedRank:
      return nestedrank_compare(lhs,rhs);
    case WO_Grouped:
      return grouped_compare(lhs,rhs);
    case WO_Coset:
      return coset_compare(lhs,rhs);
  }
  return 0;
}

/**/

int Alphabet::shortlex_compare(const Word &lhs,const Word &rhs) const
{
  /* Compare words lhs and rhs to see which is later according to the
     ordering. The ordering used here is longer words are later, and
     amongst equal length words, lexicographical ordering according to the
     order of the generators.
 */
  Word_Length l = lhs.length();
  Word_Length r = rhs.length();
  if (l != r)
    return l > r ? 1 : -1;
  const Ordinal * lbuffer = lhs.buffer();
  const Ordinal * rbuffer = rhs.buffer();
  for (Word_Length i = 0; i <l;i++)
    if (lbuffer[i] != rbuffer[i])
      return lbuffer[i] > rbuffer[i] ? 1 : -1;
  return 0;
}

/**/

int Alphabet::accented_compare(const Word &lhs,const Word &rhs,bool multilevel) const
{
  /* Compare words lhs and rhs to see which is later according to the
     ordering. The ordering used here is longer words are later, and
     amongst equal length words, lexicographical ordering according to the
     level of the generators. If generators are at the same level, the
     later generator wins.
  */
  Word_Length l = lhs.length();
  Word_Length r = rhs.length();
  if (l != r)
    return l > r ? 1 : -1;
  unsigned deciding_level = 0;
  int answer = 0;
  const Ordinal * lbuffer = lhs.buffer();
  const Ordinal * rbuffer = rhs.buffer();
  for (Word_Length i = 0; i <l;i++)
    if (lbuffer[i] != rbuffer[i])
    {
      unsigned left_level = levels[lbuffer[i]];
      if (left_level != levels[rbuffer[i]])
        return left_level > levels[rbuffer[i]] ? 1 : -1;
      else if (multilevel ? left_level > deciding_level : !answer)
      {
        deciding_level = left_level;
        answer = lbuffer[i] > rbuffer[i] ? 1 : -1;
      }
    }
  return answer;
}

/**/

int Alphabet::short_fptp_compare(const Word &lhs,const Word &rhs) const
{
  /* Compare words lhs and rhs to see which is later according to the
     ordering. The ordering used here is longer words are later, and
     amongst equal length words, the word which first reaches the highest
     generator so far is greater (ignoring places where generators are equal)
 */
  Word_Length l = lhs.length();
  Word_Length r = rhs.length();
  if (l != r)
    return l > r ? 1 : -1;
  const Ordinal * lbuffer = lhs.buffer();
  const Ordinal * rbuffer = rhs.buffer();
  Ordinal max_level = PADDING_SYMBOL;
  int answer = 0;
  for (Word_Length i = 0; i <l;i++)
    if (lbuffer[i] != rbuffer[i] && (lbuffer[i] > max_level || rbuffer[i] > max_level))
      if (lbuffer[i] > rbuffer[i])
      {
        max_level = lbuffer[i];
        answer = 1;
      }
      else
      {
        max_level = rbuffer[i];
        answer = -1;
      }
  return answer;
}

/**/

int Alphabet::right_shortlex_compare(const Word &lhs,const Word &rhs) const
{
  /* Compare words lhs and rhs to see which is later according to the
     ordering. The ordering used here is longer words are later, and
     amongst equal length words, lexicographical ordering of the reverse
     of the words according to the order of the generators.
 */
  Word_Length l = lhs.length();
  Word_Length r = rhs.length();
  if (l != r)
    return l > r ? 1 : -1;
  const Ordinal * lbuffer = lhs.buffer();
  const Ordinal * rbuffer = rhs.buffer();
  for (Word_Length i = l; i-- > 0;)
    if (lbuffer[i] != rbuffer[i])
      return lbuffer[i] > rbuffer[i] ? 1 : -1;
  return 0;
}

/**/

int Alphabet::wtlex_compare(const Word &lhs,const Word &rhs) const
{
  /* Compare words lhs and rhs to see which is later according to the
     ordering. The ordering used here is heavier words are later, where weight
     is computed by adding up the weights of the generators in the words, and
     amongst equal weight words, lexicographical ordering according to the
     order of the generators.
  */

  unsigned l_weight = 0, r_weight = 0;
  Word_Length l_length = lhs.length();
  Word_Length r_length = rhs.length();
  const Ordinal * lbuffer = lhs.buffer();
  const Ordinal * rbuffer = rhs.buffer();
  Word_Length i;

  for (i = 0; i < l_length; i++)
    l_weight += weights[lbuffer[i]];
  for (i = 0; i < r_length; i++)
    r_weight += weights[rbuffer[i]];
  if (l_weight != r_weight)
    return l_weight > r_weight ? 1 : -1;
  /* If the two words are of equal weight then the words are either equal
     or if we scan the word from the left we are bound to find a different
     letter before the end of the lhs, since if we did not the rhs would
     be bound to be heavier (as <= 0 weights are not allowed) */
  for (Word_Length i = 0; i < l_length;i++)
    if (lbuffer[i] != rbuffer[i])
      return lbuffer[i] > rbuffer[i] ? 1 : -1;
  return 0;
}

/**/

int Alphabet::wtshortlex_compare(const Word &lhs,const Word &rhs) const
{
  /* Compare words lhs and rhs to see which is later according to the
     ordering. The ordering used here is heavier words are later, where weight
     is computed by adding up the weights of the generators in the words, and
     amongst equal weight words, shortlex order.
  */

  unsigned l_weight = 0, r_weight = 0;
  Word_Length l_length = lhs.length();
  Word_Length r_length = rhs.length();
  const Ordinal * lbuffer = lhs.buffer();
  const Ordinal * rbuffer = rhs.buffer();
  Word_Length i;

  for (i = 0; i < l_length; i++)
    l_weight += weights[lbuffer[i]];
  for (i = 0; i < r_length; i++)
    r_weight += weights[rbuffer[i]];
  if (l_weight != r_weight)
    return l_weight > r_weight ? 1 : -1;
  if (l_length != r_length)
    return l_length > r_length ? 1 : -1;
  /* If the two words are of equal weight then the words are either equal
     or if we scan the word from the left we are bound to find a different
     letter before the end of the lhs, since if we did not the rhs would
     be bound to be heavier (as <= 0 weights are not allowed) */
  for (Word_Length i = 0; i < l_length;i++)
    if (lbuffer[i] != rbuffer[i])
      return lbuffer[i] > rbuffer[i] ? 1 : -1;
  return 0;
}

/**/

int Alphabet::wreath_compare(const Word & word1,const Word & word2) const
{
  /* Compare word1 and word2 to see which comes later in the wreath-product
     ordering (as defined in Sims' book), using the previously set levels.

     The following explanation has been taken from orders.gi in gap4r4,
     and cleaned up slightly.

     u < v if u' < v' where u==xu'y and v==xv'y

     So, if u and v have no common prefix, u is less than v wrt this ordering
     if:

       (a) u_max < v_max in the shortlex ordering, where u_max, v_max are
           the words obtained from u, v by removing all letters that do not

           belong to the highest level, or
      (b) u_max == v_max and
          if u == u1 * u_m1 * u2 * u_m2 ... uj * u_mj
             v == v1 * v_m1 * v2 * v_m2 ... vk * v_mk
          where u_mi, v_mi are the maximal subwords of u, v containing
          only the letters of maximal weight
          (so u_max == u_m1 * u_m2 * ... * u_mj == v_m1 * v_m2 * ... * v_mk),
           and u1 < v1 in the wreath product ordering. Note that it is
           not possible that u1==v1 in this case since the words differ
           in the first symbol.

      Clearly we could invent four different "left/right" variants of this
      type of order depending on whether:
      1) we use left or right shortlex to decide the maximal subwords
      2) we look to the left or the right of the maximal subwords to
         decide draws.
     This order is the 1L2L variant. For coset systems the 1L2R version
     would be better. Right_Recursive ordering is the special case of
     either 1L/R2R in which all generators are at a different level.

     In fact we can obviously generalise further by using any other method
     of ordering the maximal subwords that takes our fancy, such as wtlex.

     The implementation below is based on the GAP code, as that seems to be
     a little faster than the KBMAG implementation, and is certainly a little
     easier to understand. However it would certainly be possible to use
     a left to right scanning algorithm instead, and to generalise this
     so that it worked for all four possibilities.
  */
  const Ordinal * w1 = word1.buffer();
  const Ordinal * w2 = word2.buffer();
  int l1 = word1.length();
  int l2 = word2.length();
  // Ignore common prefix
  while (l1 && l2 && *w1 == *w2)
  {
    w1++;
    w2++;
    l1--;
    l2--;
  }
  if (!l1)
    return l2 ? -1 : 0;
  if (!l2)
    return 1;

  /*
    We now start scanning from right to left.
    level_used denotes the level of the block of generators
    which is currently distinguishing between word1 & word2
    answer = -1 or 1 if word1 or word2 is smaller, respectively, in this block.
    Initially level_used = answer = 0. This can also occur later if either
     (i) we read two equal generators in u,v at a higher level
         than level_used. Then everything to the right of these
         equal generators becomes irrelevant.
    (ii) we read a generator in word1 or word2 at a higher level than
         level_used that is not matched by a generator at the same
         level in the other word. We keep scanning backwards
         along the other word until we find a generator of the
         corresponding level or higher, but keep level_used = answer = 0
         while we are doing this.
  */
  int answer = 0;
  unsigned level_used = 0;
  Ordinal g1;
  Ordinal g2;
  unsigned level1;
  unsigned level2;
  do
  {
    g1 = w1[l1-1];
    level1 = levels[g1];
    g2 = w2[l2-1];
    level2 = levels[g2];
    if (level1 == level2)
    {
      if (level_used <= level1)
      {
        //w1 and w2 are both at the right level, so use shortlex
        if (g1 < g2)
        {
          answer = -1;
          level_used = level1;
        }
        else if (g2 < g1)
        {
          answer = 1;
          level_used = level1;
        }
        else if (level_used < level1)
        {
          //w1 and w2 are equal at this higher level.
          //everything to the right of u,v is now
          //irrelevant we are in situation (i) above.
          answer = 0;
          level_used = 0;
        }
        /* otherwise we already decided at this level */
      }
      /* otherwise both are at a lower level than level_used, and can be
         ignored */

      /* in any case we have finished with both characters */
      l1--;
      l2--;
    }
    else if (level2 < level1)
    {
      l2--;
      if (level_used < level1)
      {
        // word1 is now at a higher level than word2
        //we are in situation (ii) (see above)
        level_used = 0;
        answer = 0;
      }
      else if (level_used > level1)
        l1--;
    }
    else /*g2 is greater */
    {
      l1--;
      if (level_used < level2)
      {
        //word2 is now at a higher level than word1
        //we are in situation (ii) (see above)
        level_used = 0;
        answer = 0;
      }
      else if (level_used > level2)
        l2--;
    }
  }
  while (l1 && l2);

  while (l1)
  {
    /* in this case we have finished with w2, so w1 will be bigger
       unless w2 was already bigger and we do not see a character
       at the deciding level or above */
    if (answer != -1 || levels[w1[--l1]] >= level_used)
      return 1;
  }

  while (l2)
  {
    /* the reverse case */
    if (answer != 1 || levels[w2[--l2]] >= level_used)
      return -1;
  }
  return answer;
}

/**/

int Alphabet::right_wreath_compare(const Word & word1,const Word & word2) const
{
  /* Compare word1 and word2 to see which comes later in the
     right-wreath-product
     This is 1L2R Wreath Order
  */

  const Ordinal * w1 = word1.buffer();
  const Ordinal * w2 = word2.buffer();
  Total_Length l1 = word1.length();
  Total_Length l2 = word2.length();
  // Ignore common prefix
  while (l1 && l2 && *w1 == *w2)
  {
    w1++;
    w2++;
    l1--;
    l2--;
  }

  // Ignore common suffix
  while (l1 && l2 && w1[l1-1] == w2[l2-1])
  {
    l1--;
    l2--;
  }

  if (!l1)
    return l2 ? -1 : 0;
  if (!l2)
    return 1;

  /*
    We now start scanning from left to right.
    level_used denotes the level of the block of generators
    which is currently distinguishing between word1 & word2
    answer = -1 or 1 if word1 or word2 is smaller so far.
    Initially level_used = answer = 0.
    We never forget the answer completely, but level_used goes back to 0
    if we find two equal generators at a higher level than level used.
    When we find a higher level generator in one word than the other
    then we set answer to the "wrong" value, because this is the value
    that will be correct if and when the other word matches that generator.
    We won't return the wrong answer if we fail to match the generator
    because we will see that we should change our mind.
  */
  int answer = 0;
  unsigned level_used = 0;
  Ordinal g1;
  Ordinal g2;
  unsigned level1;
  unsigned level2;
  Word_Length i1 = 0;
  Word_Length i2 = 0;
  do
  {
    g1 = w1[i1];
    level1 = levels[g1];
    g2 = w2[i2];
    level2 = levels[g2];
    if (level1 == level2)
    {
      if (level_used < level1)
      {
        //w1 and w2 are both at the right level, so use shortlex
        if (g1 < g2)
        {
          answer = -1;
          level_used = level1;
        }
        else if (g2 < g1)
        {
          answer = 1;
          level_used = level1;
        }
        else
        {
          //w1 and w2 are equal at this higher level.
          //everything to the left of u,v is now irrelevant
          level_used = 0;
        }
      }
      /* otherwise we already decided at this level or both are at a lower
         level than level_used, and can be ignored */

      /* in any case we have finished with both characters */
      i1++;
      i2++;
    }
    else if (level2 < level1)
    {
      i2++;
      if (level_used < level1)
      {
        // word1 is now at a higher level than word2
        if (level_used <= level2)
        {
          answer = -1; // but word2 will be bigger if we match the generator
          level_used = level2;
        }
      }
      else if (level_used > level1)
        i1++;
    }
    else /*g2 is at greater level */
    {
      i1++;
      if (level_used < level2)
      {
        //word2 is now at a higher level than word1
        if (level_used <= level1)
        {
          answer = 1; // but word1 will be bigger if we match the generator
          level_used = level1;
        }
      }
      else if (level_used > level2)
        i2++;
    }
  }
  while (i1 < l1 && i2 < l2);

  while (i1 < l1)
  {
    /* in this case we have finished with w2, so w1 will be bigger
       unless w2 was already bigger and we do not see a character
       at the deciding level or above */
    if (answer != -1 || levels[w1[i1++]] >= level_used)
      return 1;
  }

  while (i2 < l2)
  {
    /* the reverse case */
    if (answer != 1 || levels[w2[i2++]] >= level_used)
      return -1;
  }
  return answer;
}

/**/

int Alphabet::recursive_compare(const Word &lhs,const Word &rhs) const
{
  /* Compare words lhs and rhs to see which is 'later' according to the
     ordering. The ordering used here is recursive path ordering (based on
     that described in the book "Confluent String Rewriting" by Matthias
     Jantzen, Defn 1.2.14, page 24).
     ----------------------------------------------------------------------
     The ordering is as follows:

     let u, v be elements of X*
     u >= v iff one of the following conditions is fulfilled;

     1) u == v      OR
     u == u'a, v == v'b for some a,b elements of X, u',v' elements of X*
     and then:
     2) a == b and u' >= v'    OR
     3) a > b and u > v'    OR
     4) b > a and u' > v
     ----------------------------------------------------------------------
  */

  int last_moved = 0;
  const Ordinal *lbuff = lhs.buffer();
  const Ordinal *rbuff = rhs.buffer();
  int l = lhs.length()-1; // we have to use int not Word_Length
  int r = rhs.length()-1; // here because l,r go below 0

  for (;;)
  {
    if (l < 0)
    {
      if (r < 0)
        return last_moved;
      return -1;
    }
    if (r < 0)
      return 1;
    if (lbuff[l] < rbuff[r])
    {
      l--;
      last_moved = 1;
    }
    else if (lbuff[l] > rbuff[r])
    {
      r--;
      last_moved = -1;
    }
    else
    {
      l--;
      r--;
    }
  }
}

/**/

int Alphabet::right_recursive_compare(const Word &lhs,const Word &rhs) const
{
  /* This ordering is similar to recursive ordering, but the words are
     read from left to right instead of right to left.
     ----------------------------------------------------------------------
     The ordering is as follows:

     let u, v be elements of X*
     u >= v iff one of the following conditions is fulfilled;

     1) u == v      OR
        u == au', v == bv' for some a,b elements of X, u',v' elements of X*
        and then:
     2) a == b and u' >= v'  OR
     3) a > b and u > v'    OR
     4) b > a and u' > v
     ----------------------------------------------------------------------
  */

  int last_moved = 0;
  const Ordinal *lbuff = lhs.buffer();
  const Ordinal *rbuff = rhs.buffer();
  Word_Length l_length = lhs.length(), r_length = rhs.length();
  Word_Length l = 0,r = 0;

  for (;;)
  {
    if (l >= l_length)
    {
      if (r >= r_length)
        return last_moved;
      return -1;
    }
    if (r >= r_length)
      return 1;
    if (lbuff[l] < rbuff[r])
    {
      l++;
      last_moved = 1;
    }
    else if (lbuff[l] > rbuff[r])
    {
      r++;
      last_moved = -1;
    }
    else
    {
      l++;
      r++;
    }
  }
}
/**/

int Alphabet::short_recursive_compare(const Word &lhs,const Word &rhs) const
{
  // use length, then recursive to compare words. Does not seem useful
  Word_Length l_length = lhs.length(), r_length = rhs.length();

  if (l_length != r_length)
    return l_length > r_length ? 1 : -1;
  return recursive_compare(lhs,rhs);
}

/**/

int Alphabet::short_right_recursive_compare(const Word &lhs,const Word &rhs) const
{
  // use length, then right_recursive to compare words. Does not seem useful
  Word_Length l_length = lhs.length(), r_length = rhs.length();

  if (l_length != r_length)
    return l_length > r_length ? 1 : -1;
  return right_recursive_compare(lhs,rhs);
}

/**/

int Alphabet::short_wtlex_compare(const Word &lhs,const Word &rhs) const
{
  // use length, then wtlex to compare words.
  // This gives us a way of minimising the number of higher level
  // generators in a word without losing the "short" property.
  Word_Length l_length = lhs.length(), r_length = rhs.length();

  if (l_length != r_length)
    return l_length > r_length ? 1 : -1;
  return wtlex_compare(lhs,rhs);
}

/**/

int Alphabet::ranked_compare(const Word &word1,const Word &word2) const
{
  /*
    Notionally we extract word1'and word2' from word1 and word2, containing
    all the characters of the highest rank. We then shortlex compare these
    words. if word1' != word2' then we  have decided on the order. Otherwise
    we extract word1'' and word2'' at the next highest rank and compare these.
    We carry on this process until we find the words different or until
    we have exhausted the words.
    Finally if the words are still equal we do a lexcmp().
  */
  if (nr_levels == nr_letters) /* in this case there is only one generator at
                                  each level so group_compare() is the same
                                  and quicker */
    return grouped_compare(word1,word2);

  Word_Length l1 = word1.length();
  Word_Length l2 = word2.length();
  const Ordinal * w1 = word1.buffer();
  const Ordinal * w2 = word2.buffer();

  while (l1 && l2 && *w1 == *w2)
  {
    w1++;
    w2++;
    l1--;
    l2--;
  }

  while (l1 && l2 && w1[l1-1] == w2[l2-1])
  {
    l1--;
    l2--;
  }

  if (!l1)
    return l2 ? -1 : 0;
  if (!l2)
    return 1;

  int r;
  Word_Length i;
  // Count the number of characters at each level
  memset(rwa,0,sizeof(Rank_Work_Area)*nr_levels);
  for (i = 0; i < l1;i++)
  {
    r = ranks[w1[i]];
    rwa[r].l1++;
  }

  for (i = 0; i < l2;i++)
  {
    r = ranks[w2[i]];
    rwa[r].l2++;
  }

  Ordinal_Word sw1(*this,l1);
  Ordinal_Word sw2(*this,l2);
  bool started = false;
  Word_Length start1 = 0;
  Word_Length start2 = 0;
  int best_level = 0;
  int nr_levels_used = 0;
  for (r = 0; r < nr_levels;r++)
  {
    if (rwa[r].l1 || rwa[r].l2)
    {
      if (!started && rwa[r].l1 != rwa[r].l2)
      {
        // In this case the words at the worst level of generator are
        // different lengths so we can decide immediately which comes first
        return rwa[r].l1 > rwa[r].l2 ? 1 : -1;
      }
      started = true;
      rwa[r].start1 = rwa[r].p1 = start1;
      start1 += rwa[r].l1;
      rwa[r].start2 = rwa[r].p2 = start2;
      start2 += rwa[r].l2;
      best_level = r;
      nr_levels_used++;
    }
  }

  if (nr_levels_used == 1) /* in this case the words are equal lengths
                              and only use generators from the same level,
                              so we can do a lexcmp */
    return word1.lexcmp(word2);

  /* we need to read the words to decide which comes first */
  Ordinal * nw1 = sw1.buffer();
  Ordinal * nw2 = sw2.buffer();
  for (i = 0; i < l1;i++)
  {
    r = ranks[w1[i]];
    nw1[rwa[r].p1++] = w1[i];
  }
  for (i = 0; i < l2;i++)
  {
    r = ranks[w2[i]];
    nw2[rwa[r].p2++] = w2[i];
  }

  /* now we can compare the subwords at each rank */
  int answer = 0;
  for (r = 0;r <= best_level;r++)
  {
    answer = shortlex_compare(Subword(sw1,rwa[r].start1,rwa[r].start1+rwa[r].l1),
                              Subword(sw2,rwa[r].start2,rwa[r].start2+rwa[r].l2));
    if (answer != 0)
      return answer;
  }
  /* finally if the words are still equal we do a lexcmp */
  return word1.lexcmp(word2);
}

int Alphabet::nestedrank_compare(const Word &word1,const Word &word2) const
{
  /*
    Notionally we extract word1'and word2' from word1 and word2, containing
    all the characters of the highest rank. We then shortlex compare these
    words. if word1' != word2' then we  have decided on the order. Otherwise
    we extract word1'' and word2'' at the highest and next highest rank and
    compare these.
    We carry on this process until we find the words different or until
    we have exhausted the words.
  */

  Word_Length l1 = word1.length();
  Word_Length l2 = word2.length();
  const Ordinal * w1 = word1.buffer();
  const Ordinal * w2 = word2.buffer();

  while (l1 && l2 && *w1 == *w2)
  {
    w1++;
    w2++;
    l1--;
    l2--;
  }

  while (l1 && l2 && w1[l1-1] == w2[l2-1])
  {
    l1--;
    l2--;
  }

  if (!l1)
    return l2 ? -1 : 0;
  if (!l2)
    return 1;

  Ordinal_Word sw1(*this,l1);
  Ordinal_Word sw2(*this,l2);
  Ordinal * nw1 = sw1.buffer();
  Ordinal * nw2 = sw2.buffer();
  int answer = 0;
  for (int r = 0; r < nr_levels;r++)
  {
    Word_Length nl1 =  0;
    for (Word_Length i = 0; i < l1;i++)
      if (ranks[w1[i]] <= r)
        nw1[nl1++] = w1[i];

    Word_Length nl2 = 0;
    for (Word_Length i = 0; i < l2;i++)
      if (ranks[w2[i]] <= r)
        nw2[nl2++] = w2[i];
    if (nl1 != nl2)
    {
      answer = nl1 < nl2 ? -1 : 1;
      break;
    }
    else if (!nl1)
      continue;
    sw1.set_length(nl1);
    sw2.set_length(nl2);
    answer = sw1.lexcmp(sw2);
    if (answer)
      break;
  }
  return answer;
}

/**/

int Alphabet::grouped_compare(const Word &word1,const Word &word2) const
{
  /*
    Notionally we extract word1'and word2' from word1 and word2, containing
    all the characters of the highest rank. We then look at the length of
    these words. if word1' is longer or shorter than word2' then we have
    decided on the order. Otherwise  we extract word1'' and word2'' at the
    next highest rank and compare their lengths.
    We carry on this process until we find the words different or until
    we have exhausted the words.
    Finally if the words are still equal we do a lexcmp() (since the words
    are inevitably the same length)
    This ordering can be regarded as the limiting case of wtlex in which
    the difference in weights is such that a word containing just one
    generator at a higher level than any of the generators in the other
    word is certain to exceed it, no matter how long the other word is.
  */

  Word_Length l1 = word1.length();
  Word_Length l2 = word2.length();
  const Ordinal * w1 = word1.buffer();
  const Ordinal * w2 = word2.buffer();

  while (l1 && l2 && *w1 == *w2)
  {
    w1++;
    w2++;
    l1--;
    l2--;
  }

  while (l1 && l2 && w1[l1-1] == w2[l2-1])
  {
    l1--;
    l2--;
  }

  if (!l1)
    return l2 ? -1 : 0;
  if (!l2)
    return 1;

  Ordinal r;
  Word_Length i;
  // Count the number of characters at each level
  memset(rwa,0,sizeof(Rank_Work_Area)*nr_levels);
  for (i = 0; i < l1;i++)
  {
    r = ranks[w1[i]];
    rwa[r].l1++;
  }

  for (i = 0; i < l2;i++)
  {
    r = ranks[w2[i]];
    rwa[r].l2++;
  }

  for (r = 0; r < nr_levels;r++)
    if (rwa[r].l1 != rwa[r].l2)
      return rwa[r].l1 > rwa[r].l2 ? 1 : -1;

  return word1.lexcmp(word2);
}

/**/

int Alphabet::coset_compare(const Word & word1,const Word &word2) const
{
  Word_Length l1 = word1.length();
  Word_Length l2 = word2.length();
  const Ordinal * w1 = word1.buffer();
  const Ordinal * w2 = word2.buffer();
  Word_Length i1,i2;
  for (i1 = 0;i1 < l1;i1++)
    if (w1[i1] < coset_symbol)
      break;
  for (i2 = 0;i2 < l2;i2++)
    if (w2[i2] < coset_symbol)
      break;
  int answer = 0;
  if (i1 != l1)
    answer = i2 != l2 ? wreath_compare(Subword((Word &)word1,i1,l1),Subword((Word &)word2,i2,l2)) : 1;
  else
    answer = i2 != l2 ? -1 : 0;
  if (answer == 0)
    answer = wreath_compare(Subword((Word &) word1,0,i1),Subword((Word &)word2,0,i2));
  return answer;
}

/**/

bool Alphabet::order_is_effectively_shortlex() const
{
  switch (word_ordering)
  {
    case WO_Shortlex:
      return true;
    case WO_Short_FPTP:
      return nr_letters <= 2;
    case WO_Right_Shortlex:
      return false;
    case WO_Short_Weighted_Lex:
    case WO_Weighted_Lex:
    case WO_Weighted_Shortlex:
      for (Ordinal g = 1; g < nr_letters;g++)
        if (weights[g] != weights[0])
          return false;
      return true;
    case WO_Wreath_Product:
    case WO_Right_Wreath_Product:
    case WO_Ranked:
    case WO_NestedRank:
    case WO_Grouped:
    case WO_Coset:
    case WO_Accented_Shortlex:
    case WO_Multi_Accented_Shortlex:
      for (Ordinal g = 1; g < nr_letters;g++)
        if (levels[g] != levels[0])
          return false;
      return true;
    case WO_Recursive:
    case WO_Right_Recursive:
    case WO_Short_Recursive:
    case WO_Short_Right_Recursive:
      return false;
    default:
      MAF_INTERNAL_ERROR(container,
                         ("Alphabet::is_effectively_shortlex() has not been"
                          "implemented for ordering %d\n",word_ordering));
      return false;
  }
}

/**/

struct Wreath_Compare_State
{
  signed char answer;
  signed char lex;
  Ordinal level1;
  Ordinal level2;
  Ordinal decided_level;
  Word_Length pending;
  Word_Length lead;
};

const void * Alphabet::gt_initial_state(size_t *size) const
{
  static const signed char istate[sizeof(Wreath_Compare_State)] = {0};
  switch (word_ordering)
  {
    case WO_Shortlex:
    case WO_Right_Shortlex:
      *size = 1;
      return istate;
    case WO_Accented_Shortlex:
    case WO_Multi_Accented_Shortlex:
    case WO_Short_FPTP:
      *size = gt_key_size();
      return istate;
    case WO_Short_Weighted_Lex:
    case WO_Weighted_Shortlex:
    case WO_Weighted_Lex:
      *size = 1+sizeof(int);
      return istate;
    case WO_Right_Wreath_Product:
    case WO_Short_Recursive:
    case WO_Short_Right_Recursive:
    case WO_Right_Recursive:
    case WO_Recursive:
    case WO_Wreath_Product:
      *size = sizeof(Wreath_Compare_State);
      return istate;
    case WO_Grouped:
      {
        *size = gt_key_size();
        void * answer = gt_state.reserve(*size);
        memset(answer,0,*size);
        return answer;
      }
    case WO_Coset:
    case WO_NestedRank:
    case WO_Ranked:
    default:
      MAF_INTERNAL_ERROR(container,
                         ("Greater than automaton not supported for ordering"
                           " %s\n",word_orderings()[word_ordering].string()));
      *size = 0;
      return 0;
  }
}

/**/

const void * Alphabet::gt_failure_state(size_t *size) const
{
  static const signed char istate[sizeof(Wreath_Compare_State)] = {-2};
  switch (word_ordering)
  {
    case WO_Shortlex:
    case WO_Right_Shortlex:
      *size = 1;
      return istate;
    case WO_Accented_Shortlex:
    case WO_Multi_Accented_Shortlex:
    case WO_Short_FPTP:
      *size = gt_key_size();
      return istate;
    case WO_Short_Weighted_Lex:
    case WO_Weighted_Lex:
    case WO_Weighted_Shortlex:
      *size = 1+sizeof(int);
      return istate;
    case WO_Right_Recursive:
    case WO_Right_Wreath_Product:
    case WO_Recursive:
    case WO_Wreath_Product:
    case WO_Short_Right_Recursive:
    case WO_Short_Recursive:
      *size = sizeof(Wreath_Compare_State);
      return istate;
    case WO_Grouped:
      {
        *size = gt_key_size();
        void * answer = gt_state.reserve(*size);
        memset(answer,0,*size);
        *  (int *) answer = -1;
        return answer;
      }
    case WO_Coset:
    case WO_NestedRank:
    case WO_Ranked:
    default:
      MAF_INTERNAL_ERROR(container,
                         ("Greater than automaton not supported for ordering"
                           " %s\n",word_orderings()[word_ordering].string()));
      *size = 0;
      return 0;
  }
}

/**/

int Alphabet::gt_order(const void * state_key) const
{
  const Byte *old_key = (const Byte *) state_key;
  switch (word_ordering)
  {
    case WO_Short_Right_Recursive:
    case WO_Short_Recursive:
    case WO_Right_Wreath_Product:
    case WO_Right_Recursive:
    case WO_Recursive:
    case WO_Wreath_Product:
    case WO_Shortlex:
    case WO_Right_Shortlex:
    case WO_Accented_Shortlex:
    case WO_Multi_Accented_Shortlex:
    case WO_Short_FPTP:
      return (signed char) old_key[0];
    case WO_Short_Weighted_Lex:
      if (old_key[0] == 2) /* Word1 is longer */
        return 1;
      /* words must be same length so far - code is now same as WO_Weighted_Lex */
    case WO_Weighted_Shortlex:
    case WO_Weighted_Lex:
      {
        int w;
        memcpy(&w,old_key+1,sizeof(int));
        if (w != 0)
          return w;
        return (signed char) old_key[0];
      }
    case WO_Grouped:
      return * (int *) old_key;
    case WO_NestedRank:
    case WO_Ranked:
    case WO_Coset:
      MAF_INTERNAL_ERROR(container,
                         ("Greater than automaton not supported for ordering"
                           " %s\n",word_orderings()[word_ordering].string()));
  }
  return 0;
}

/**/

const void * Alphabet::gt_new_state(size_t * size,
                                    const void * state_key,Transition_ID ti) const
{
  Ordinal g1,g2;
  product_generators(&g1,&g2,ti);
  const Byte *old_key = (const Byte *) state_key;
  Byte *new_key;

  switch (word_ordering)
  {
    case WO_Shortlex:
      new_key = gt_state.reserve(*size = 1);
      if (g1 == PADDING_SYMBOL)
      {
        *size = 0;
        return 0;
      }
      if (g2 == PADDING_SYMBOL)
        new_key[0] = 2;
      else if (!old_key[0] && g1 != g2)
        new_key[0] = g1 > g2 ? 1: -1;
      else
        new_key[0] = old_key[0];
      return new_key;

    case WO_Short_FPTP:
      new_key = gt_state.reserve(*size = 1+sizeof(Ordinal));
      if (g1 == PADDING_SYMBOL)
      {
        *size = 0;
        return 0;
      }
      if (g2 == PADDING_SYMBOL || old_key[0]==2)
      {
        new_key[0] = 2;
        memset(new_key+1,0,sizeof(Ordinal));
      }
      else
      {
        Ordinal max_level;
        new_key[0] = old_key[0];
        memcpy(&max_level,old_key+1,sizeof(Ordinal));
        if (g1 != g2 && (g1 > max_level || g2 > max_level))
          if (g1 > g2)
          {
            new_key[0] = 1;
            max_level = g1;
          }
          else
          {
            new_key[0] = (Byte) -1;
            max_level = g2;
          }
        memcpy(new_key+1,&max_level,sizeof(Ordinal));
      }
      return new_key;

    case WO_Accented_Shortlex:
    case WO_Multi_Accented_Shortlex:
      new_key = gt_state.reserve(*size = 1+sizeof(Ordinal));
      if (g1 == PADDING_SYMBOL)
      {
        *size = 0;
        return 0;
      }
      if (g2 == PADDING_SYMBOL || old_key[0]==2)
      {
        new_key[0] = 2;
        memset(new_key+1,0,sizeof(Ordinal));
      }
      else
      {
        Ordinal max_level;
        new_key[0] = old_key[0];
        memcpy(&max_level,old_key+1,sizeof(Ordinal));
        if (levels[g1] != levels[g2])
        {
          if (max_level != -1)
          {
            new_key[0] = levels[g1] > levels[g2] ? 1 : -1;
            max_level = -1;
          }
        }
        else if (word_ordering == WO_Accented_Shortlex ?
                 new_key[0] == 0 && g1 != g2 :
                 max_level != -1 && g1 != g2 && levels[g1] > unsigned(max_level))
        {
          max_level = word_ordering == WO_Accented_Shortlex ? 0 : levels[g1];
          if (g1 > g2)
            new_key[0] = 1;
          else
            new_key[0] = (Byte) -1;
        }
        memcpy(new_key+1,&max_level,sizeof(Ordinal));
      }
      return new_key;

    case WO_Right_Shortlex:
      if (g1 == PADDING_SYMBOL)
      {
        *size = 0;
        return 0;
      }
      new_key = gt_state.reserve(*size = 1);
      if (old_key[0]==2 || g2 == PADDING_SYMBOL)
        new_key[0] = 2;
      else if (g1 != g2)
        new_key[0] = g1 > g2 ? 1: -1;
      else
        new_key[0] = old_key[0];
      return new_key;

    case WO_Weighted_Lex:
      if (old_key[0] == 2 && g2 != PADDING_SYMBOL)
      {
        /* This is the difficult case where input so far looks like:
           abc $$$, and we now get a non-pad on the RHS. We cannot
           decide this, and so enter the failure state.
           Actually we only need to enter the failure state if the weight
           part cannot decide, but it is best to do so immediately as
           otherwise Diff_Reduce::reduce() would have to make a more
           difficult decision about which of ab $c and ab c$ was better.
        */
        *size = 0;
        return 0;
      }
      /* fall through - otherwise key is generally the same for
         WO_Short_Weighted_Lex case as well */
    case WO_Short_Weighted_Lex:
      {
        new_key = gt_state.reserve(*size = 1+sizeof(int));
        if (!old_key[0] && g1 != g2)
        {
          new_key[0] = g1 > g2 ? 1 : -1;
          /* If we read a padding_symbol on the right at the point where we
             appear to be leaving the equality state, then we cannot really
             tell the lexical order, and could not do so without remembering
             g1. So we set the lex flag to 2 in this one case.
             For WO_Short_Weighted_Lex success is now inevitable, but
             in the WO_Weighted_Lex case g2 is not allowed not to be padding,
             in future and we get a failure due to a "syntax error" if it is.
          */
          if (g2 == PADDING_SYMBOL)
            new_key[0] = 2;
        }
        else
          new_key[0] = old_key[0];

        if (g1 != g2)
        {
          int w;
          memcpy(&w,old_key+1,sizeof(int));
          if (g1 != PADDING_SYMBOL)
            w += weights[g1];
          if (g2 != PADDING_SYMBOL)
            w -= weights[g2];
          else if (word_ordering == WO_Short_Weighted_Lex)
            new_key[0] = 2;
          if (g1 == PADDING_SYMBOL && (w <= 0 || word_ordering == WO_Short_Weighted_Lex))
          {
            *size = 0;
            return 0;
          }
          memcpy(new_key+1,&w,sizeof(int));
        }
        else
          memcpy(new_key+1,old_key+1,sizeof(int));
        return new_key;
      }

    case WO_Weighted_Shortlex:
      if (old_key[0] == 2 && g1 == PADDING_SYMBOL)
      {
        /* If the RHS has been padded we won't allow padding on the left.
           This makes it easier to decide whether one word is now longer
           or shorter than the other.
        */
        *size = 0;
        return 0;
      }
      else
      {
        new_key = gt_state.reserve(*size = 1+sizeof(int));
        if (!old_key[0] && g1 != g2)
        {
          new_key[0] = g1 > g2 ? 1 : -1;
          /* If we read a padding_symbol on the right at the point where we
             appear to be leaving the equality state, then we cannot really
             tell the lexical order, and could not do so without remembering
             g1. However, for this order "lex" success is now inevitable as
             we are talking about "shortlex", and we don't allow padding on
             both sides.
          */
          if (g2 == PADDING_SYMBOL)
            new_key[0] = 2;
        }
        else
          new_key[0] = old_key[0];

        if (g1 != g2)
        {
          int w;
          memcpy(&w,old_key+1,sizeof(int));
          if (g1 != PADDING_SYMBOL)
            w += weights[g1];
          else
            new_key[0] = (Byte) -1; /* lex failure is now inevitable. In
                                       theory we should use some value such as
                                       -2 to disallow padding on the right here.
                                       However, we will only see pads on left
                                       from now on. */
          if (g2 != PADDING_SYMBOL)
            w -= weights[g2];
          else
            new_key[0] = 2;
          if (g1 == PADDING_SYMBOL && w <= 0)
          {
            *size = 0;
            return 0;
          }
          memcpy(new_key+1,&w,sizeof(int));
        }
        else
          memcpy(new_key+1,old_key+1,sizeof(int));
        return new_key;
      }

    case WO_Short_Recursive:
    case WO_Recursive:
    case WO_Wreath_Product:
      {
        /* The code for these cases is complex.
           The definition of wreath product order makes it fairly clear
           that once the words disagree then for each word, only a generator
           at the same or a higher level than any generator we have yet seen
           in that word since leaving identity matters. So in principle we
           store state information consisting of the generators that we see
           that satisfy this condition, something like aaaAbBBcCCC, aaBBBbCC.
           While one word is at a higher level than the other we unfortunately
           need to store all generators that are at a higher level than
           any generator seen in the other word (and which were part of the
           increasing sequence of generators for this word).
           However, if we have managed to read a symbol at a particular level
           in both words then we can compare them. Either the symbols are
           equal, and can now be thrown away, or they are not equal, and
           establish the lexical order of the words at that level.
           Once we can decide the lexical order at a certain level then
           we can throw away all the information we may have remembered at
           any lower level. And from that point on we only need to count
           the generators, to see which word is longer.
           This means that in fact we only ever need to store generators,
           for one word - the one which happens to be greater at the moment.
        */

        Wreath_Compare_State state;
        memcpy(&state,state_key,sizeof(Wreath_Compare_State));
        if (state.answer == 0)
        {
          if (g1 == g2)
          {
            *size = sizeof(state);
            return state_key;
          }
        }
        if (word_ordering == WO_Short_Recursive)
        {
          if (g1 == PADDING_SYMBOL)
          {
            *size = 0;
            return 0;
          }
          if (state.answer == 2 || g2 == PADDING_SYMBOL)
          {
            state.answer = 2;
            state.lex = 0;
            state.level1 = 0;
            state.level2 = 0;
            state.pending = 0;
            state.lead = 0;
            new_key = gt_state.reserve(sizeof(state));
            memcpy(new_key,&state,sizeof(state));
            return new_key;
          }
        }
        else if (g1 == PADDING_SYMBOL && state.answer != 1 ||
                 g2 == PADDING_SYMBOL && state.answer == 2)
        {
          *size = 0;
          return 0;
        }

        int changed = 0;
        if (g1 != PADDING_SYMBOL && ilevels[g1] >= state.level1)
          changed |= 1;
        if (g2 != PADDING_SYMBOL && ilevels[g2] >= state.level2)
          changed |= 2;
        if (!changed)
        {
          *size = sizeof(state)+(state.pending*code_size);
          return state_key;
        }

        if (state.lead &&
            (!(changed & 1) || ilevels[g1] <= state.level1) &&
            (!(changed & 2) || ilevels[g2] <= state.level1))
        {
          /* In this case we don't need to go to the bother of unpacking
             the words and just need to count */
          int lead = state.lead;
          if (state.answer != 1)
            lead = -lead;
          if (changed & 1)
            lead++;
          if (changed & 2)
            lead--;
          if (lead > 0)
          {
            state.answer = 1;
            state.lead = Word_Length(lead);
          }
          else if (lead < 0)
          {
            state.answer = -1;
            state.lead = Word_Length(-lead);
          }
          else
          {
            state.answer = state.lex;
            state.lead = 0;
          }
          if (g1 == PADDING_SYMBOL && state.answer != 1)
          {
            /* first word is now smaller and cannot increase */
            *size = 0;
            return 0;
          }
          *size = sizeof(state);
          new_key = gt_state.reserve(*size);
          memcpy(new_key,&state,sizeof(state));
          return new_key;
        }

        /* If we get here we are in a difficult case,
           and need to examine our previous state more fully.
           First get any word information stored in the old state */
        Word_Length pending1 = state.answer == 1 ? state.pending : 0;
        Word_Length pending2 = state.answer == -1 ? state.pending : 0;
        Ordinal_Word w1(*this,pending1+state.lead+1);
        Ordinal_Word w2(*this,pending2+state.lead+1);
        /* the unpacks can read from the same location because only one
           word is present */

        unpack(w1.buffer(),old_key+sizeof(state),pending1);
        unpack(w2.buffer(),old_key+sizeof(state),pending2);
        w1.set_length(pending1);
        w2.set_length(pending2);

        if (state.lead)
        {
          if (state.answer == 1)
          {
            pending1 += state.lead;
            while (state.lead--)
              w1.append(representatives[state.level1]);
          }
          else
          {
            pending2 += state.lead;
            while (state.lead--)
              w2.append(representatives[state.level2]);
          }
        }

        /* Add the new generators */
        if (changed & 1)
        {
          w1.append(g1);
          state.level1 = ilevels[g1];
          pending1++;
        }
        if (changed & 2)
        {
          w2.append(g2);
          state.level2 = ilevels[g2];
          pending2++;
        }

        /* Now forget everything we no longer need to make a decision */
        Ordinal * lvalues = w1.buffer();
        Ordinal * rvalues = w2.buffer();
        if (changed)
        {
          if (pending1 && pending2)
          {
            Ordinal test_level = state.level1 < state.level2 ? state.level1 : state.level2;
            Word_Length l = pending1,r = pending2;

            /* Find the beginning of the subwords at the level to be tested */
            while (l && ilevels[lvalues[l-1]] >= test_level)
              l--;
            while (r && ilevels[rvalues[r-1]] >= test_level)
              r--;

            if (l < pending1 && r < pending2 &&
                ilevels[lvalues[l]] == test_level &&
                ilevels[rvalues[r]] == test_level)
            {
              if (state.decided_level < test_level)
              {
                if (lvalues[l] == rvalues[r])
                {
                  /* We cannot yet decide this level, but any lower levels
                     for which a decision was pending are now finally
                     decided */
                  if (l != 0)
                  {
                    Ordinal level = ilevels[lvalues[l-1]];
                    state.decided_level = level;
                    state.lex = 1;
                  }
                  else if (r !=0)
                  {
                    Ordinal level = ilevels[rvalues[r-1]];
                    state.decided_level = level;
                    state.lex = -1;
                  }
                }
                else
                {
                  state.lex = lvalues[l] > rvalues[r] ? 1 : -1;
                  state.decided_level = test_level;
                }
                /* everything to the left is now irrelevant */
                lvalues += l+1;
                pending1 -= l+1;
                rvalues += r+1;
                pending2 -= r+1;
              }
              else
              {
                /* we have characters to throw away from a level that is
                   already decided lexically */
                lvalues++;
                pending1--;
                rvalues++;
                pending2--;
              }
            }
          }
          if (pending1 && pending2)
          {
            /* we read a character in a level that can only been seen in the
               lesser word, and which cannot occur in the greater word because
               it is already at a higher level */
            if (state.level1 > state.level2)
            {
              state.lex = -1;
              state.decided_level = state.level2;
              pending2 = 0;
              while (pending1 && ilevels[lvalues[0]] < state.decided_level)
              {
                lvalues++;
                pending1--;
              }
            }
            else
            {
              state.lex = 1;
              state.decided_level = state.level1;
              pending1 = 0;
              while (pending2 && ilevels[rvalues[0]] < state.decided_level)
              {
                rvalues++;
                pending2--;
              }
            }
          }
        }

        if (pending1)
        {
          if (state.answer == 0 && g2 == PADDING_SYMBOL)
            state.answer = 2; /* if the words were equal then we have to go
                                 into a state which disallows non-pads if
                                 we leave equality with a pad on the right,
                                 because in this case the input might look
                                 like abcad $abcd, and these would compare
                                 equal even though the first word is bigger.
                              */
          else
            state.answer = 1;
          state.pending = pending1;
        }
        else if (pending2)
        {
          state.answer = -1;
          state.pending = pending2;
        }
        else
        {
          state.answer = state.lex;
          state.pending = 0;
        }

        if (state.level1 == state.level2 && state.decided_level == state.level1)
        {
          /* we have already decided the lex order, and now only need to count
             generators */
          state.lead = state.pending;
          state.pending = 0;
        }
        else
          state.lead = 0;
        if (g1 == PADDING_SYMBOL && state.answer != 1)
        {
          /* first word is now smaller and cannot increase */
          *size = 0;
          return 0;
        }

        *size = sizeof(state) + state.pending*code_size;
        new_key = gt_state.reserve(*size+code_size);
        memcpy(new_key,&state,sizeof(state));
        if (state.answer == 1)
          pack(new_key+sizeof(state),lvalues,state.pending);
        else
          pack(new_key+sizeof(state),rvalues,state.pending);
        return new_key;
      }

    case WO_Short_Right_Recursive:
    case WO_Right_Recursive:
    case WO_Right_Wreath_Product:
      {
        /* The code for these cases is somewhat similar to the previous
           three cases, but even more difficult, since as soon as one word
           gets to a higher level we have to keep all symbols from that
           word until the other reaches the same level, since if when
           the other gets to that level the symbols match we have to
           decide based on what is to the right. And while this is happening
           the other word can still overtake what had been previously
           decided on the left.
           Decisions made at anything other than the highest level are
           always provisional, since the occurrence of a generator at a
           higher level which is matched in the other word allows us to
           to start considering lower level generators again.
        */

        Wreath_Compare_State state;
        memcpy(&state,state_key,sizeof(Wreath_Compare_State));
        if (state.answer == 0)
        {
          if (g1 == g2)
          {
            *size = sizeof(state);
            return state_key;
          }
        }
        if (word_ordering == WO_Short_Right_Recursive)
        {
          if (g1 == PADDING_SYMBOL)
          {
            *size = 0;
            return 0;
          }
          if (state.answer == 2 || g2 == PADDING_SYMBOL)
          {
            state.answer = 2;
            state.lex = 0;
            state.level1 = 0;
            state.level2 = 0;
            state.pending = 0;
            state.lead = 0;
            new_key = gt_state.reserve(sizeof(state));
            memcpy(new_key,&state,sizeof(state));
            return new_key;
          }
        }
        else if (g1 == PADDING_SYMBOL && state.answer != 1 ||
                 g2 == PADDING_SYMBOL && state.answer == 2)
        {
          *size = 0;
          return 0;
        }

        int changed = 0;
        if (g1 != PADDING_SYMBOL && ilevels[g1] >= state.level1)
          changed |= 1;
        if (g2 != PADDING_SYMBOL && ilevels[g2] >= state.level2)
          changed |= 2;
        if (!changed)
        {
          *size = sizeof(state)+(state.pending*code_size);
          return state_key;
        }

        if (state.lead &&
            (!(changed & 1) || ilevels[g1] <= state.level1) &&
            (!(changed & 2) || ilevels[g2] <= state.level1) /* since level1==level2*/)
        {
          /* In this case we don't need to go to the bother of unpacking
             the words and just need to count */
          int lead = state.lead;
          if (state.answer != 1)
            lead = -lead;
          if (changed & 1)
            lead++;
          if (changed & 2)
            lead--;
          if (lead > 0)
          {
            state.answer = 1;
            state.lead = Word_Length(lead);
          }
          else if (lead < 0)
          {
            state.answer = -1;
            state.lead = Word_Length(-lead);
          }
          else
          {
            state.answer = state.lex;
            state.lead = 0;
          }
          if (g1 == PADDING_SYMBOL && state.answer != 1)
          {
            /* first word is now smaller and cannot increase */
            *size = 0;
            return 0;
          }
          *size = sizeof(state);
          new_key = gt_state.reserve(*size);
          memcpy(new_key,&state,sizeof(state));
          return new_key;
        }

        /* If we get here we are in a difficult case,
           and need to examine our previous state more fully.
           First get any word information stored in the old state.
           It seemed simpler to consider the relation between the
           previously bigger and smaller words rather than between
           word1 and word2 explicitly. We just need to remember to
           change the sign of the answer consistently */
        Word_Length bigger_length = state.pending+state.lead;
        Word_Length smaller_length = 0;
        Ordinal_Word bigger_word(*this,state.pending+state.lead+1);
        Ordinal_Word smaller_word(*this,1);
        int answer = state.answer >= 0 ? 1 : -1;

        unpack(bigger_word.buffer(),old_key+sizeof(state),state.pending);
        bigger_word.set_length(state.pending);
        smaller_word.set_length(0);

        while (state.lead)
        {
          bigger_word.append(representatives[state.level1]);
          state.lead--;
        }

        /* Add the new generators */
        if (changed & 1)
        {
          if (state.answer >= 0)
          {
            bigger_length++;
            bigger_word.append(g1);
          }
          else
          {
            smaller_length++;
            smaller_word.append(g1);
          }
        }

        if (changed & 2)
        {
          if (state.answer < 0)
          {
            bigger_length++;
            bigger_word.append(g2);
          }
          else
          {
            smaller_length++;
            smaller_word.append(g2);
          }
        }

        /* Now forget everything we no longer need to make a decision */
        Ordinal * lvalues = bigger_word.buffer();
        Ordinal * rvalues = smaller_word.buffer();
        if (changed)
        {
          if (bigger_length && smaller_length)
          {
            Ordinal lev1 = 0; // to shut up stupid compilers
            Ordinal lev2 = ilevels[rvalues[0]];
            /* first throw away any symbols that the bigger word had
               in reserve that are at a lower level than the smaller word's
               new symbol */
            while (bigger_length &&
                   (lev1 = ilevels[lvalues[0]]) < lev2)
            {
              if (state.decided_level <= lev1)
              {
                state.decided_level = lev1;
                state.lex = (signed char) answer;
              }
              lvalues++;
              bigger_length--;
            }

            if (bigger_length && lev2 < lev1)
            {
              if (state.decided_level <= lev2)
              {
                // input here might be something like c b.
                // the lesser word is greater lexically at the level of
                // the unmatched generator, so that if next input is $ c
                // the smaller word overtakes the other even because there
                // is nothing to the right yet.
                state.decided_level = lev2;
                state.lex = -answer;
              }
              smaller_length = 0;
            }

            if (bigger_length && smaller_length && lev2 && lev1)
            {
              /* lev1 must equal lev2 otherwise we would not get here */
              if (rvalues[0] != lvalues[0])
              {
                state.decided_level = lev1;
                state.lex = lvalues[0] > rvalues[0] ? answer : -answer;
              }
              else
              {
                /* any decision made so far is only relevant if there is no
                   later decision on the right */
                state.decided_level = 0;
              }
              lvalues++;
              bigger_length--;
              smaller_length = 0;
              if (state.decided_level)
              {
                Word_Length new_bigger_length = 0;
                Ordinal * new_lvalues = bigger_word.buffer();
                while (bigger_length)
                {
                  lev1 = ilevels[*lvalues];
                  if (lev1 == state.decided_level)
                    new_lvalues[new_bigger_length++] = representatives[lev1];
                  else if (lev1 > state.decided_level)
                    break;
                  bigger_length--;
                  lvalues++;
                }
                while (bigger_length--)
                  new_lvalues[new_bigger_length++] = *lvalues++;
                lvalues = new_lvalues;
                bigger_length = new_bigger_length;
              }
            }
          }
        }

        if (bigger_length)
        {
          if (state.answer == 0 && g2 == PADDING_SYMBOL)
            state.answer = 2;
          else
            state.answer = (signed char) answer;
        }
        else if (smaller_length)
        {
          /* the "smaller" word is actually now bigger */
          state.answer = -answer;
          lvalues = rvalues;
          bigger_length = smaller_length;
        }
        else
          state.answer = state.lex;

        if (bigger_length)
        {
          Word_Length pending = 0;
          for (pending = 0;pending < bigger_length;pending++)
            if (ilevels[lvalues[pending]] > state.decided_level)
              break;
          if (pending == bigger_length)
          {
            state.pending = 0;
            state.lead = bigger_length;
          }
          else
          {
            state.pending = pending;
            state.lead = 0;
          }
        }

        if (state.lead || !bigger_length)
          state.level1 = state.level2 = state.decided_level;
        else
        {
          state.level1 = state.level2 = 0;
          if (state.answer > 0)
            state.level2 = state.decided_level;
          else
            state.level1 = state.decided_level;
        }

        if (g1 == PADDING_SYMBOL && state.answer != 1)
        {
          /* first word is now smaller and cannot increase */
          *size = 0;
          return 0;
        }

        *size = sizeof(state) + state.pending*code_size;
        new_key = gt_state.reserve(*size+code_size);
        memcpy(new_key,&state,sizeof(state));
        if (state.answer == 1)
          pack(new_key+sizeof(state),lvalues,state.pending);
        else
          pack(new_key+sizeof(state),rvalues,state.pending);
        return new_key;
      }


    case WO_Grouped:
      {
        /* we store our answer in new_key[0], the lexical state in new_key[1]
           and count the generators at each level in the remaining keys */
        new_key = gt_state.reserve(*size = (nr_levels+2)*sizeof(int));
        int * group_key = (int *) new_key;
        memcpy(group_key,old_key,*size);
        if (g1 != PADDING_SYMBOL)
          group_key[1+ilevels[g1]]++;
        if (g2 != PADDING_SYMBOL)
          group_key[1+ilevels[g2]]--;
        if (g1 == PADDING_SYMBOL)
          group_key[1] = -1;
        else if (g2 == PADDING_SYMBOL)
          group_key[1] = 1;
        else if (!group_key[1] && g1 != g2)
          group_key[1] = g1 > g2 ? 1 : -1;
        group_key[0] = 0;
        for (size_t i = nr_levels+2; i-- > 1;)
          if (group_key[i])
          {
            group_key[0] = group_key[i];
            break;
          }
        if (g1 == PADDING_SYMBOL && group_key[0] != 1)
        {
          *size = 0;
          return 0;
        }
        return group_key;
      }
    case WO_Ranked:
    case WO_NestedRank:
    case WO_Coset:
      MAF_INTERNAL_ERROR(container,
                         ("Greater than automaton not supported for ordering"
                           " %s\n",word_orderings()[word_ordering].string()));
  }
  *size = 0;
  return 0;
}

/**/

int Alphabet::gt_compare(const void * state1,const void * state2) const
{
  /* See header file */
  const Byte * key1 = (const Byte *)state1;
  const Byte * key2 = (const Byte *)state2;
  switch (word_ordering)
  {
    case WO_Weighted_Shortlex:
      {
        int w1;
        int w2;
        memcpy(&w1,key1+1,sizeof(int));
        memcpy(&w2,key2+1,sizeof(int));
        if (w1 != w2)
        {
          if (w1 > w2)
          {
            /* In this case weight is telling us that w1 is better,
               but w1 cannot accept $,g transitions while key2 can.
               So we cannot say one is better than the other here */
            if (key1[0]==2 && key2[0]!=2)
              return -1;
            return 1;
          }
          if (key2[0]==2 && key1[0]!=2)  // as above
            return -1;
          return 2;
          /* answer cannot be actually 2 on both sides, at least not unless we
             are building a coset acceptor.
             Because in that case we would be doing something like comparing
             u1u2 $$ and u1u2 u1$. If these have arrived at the same difference
             then they already were at the same difference, and we would have
             found a reduction already.
             In the coset case we might be comparing states that came from
             different initial states of the difference machine, so it
             is conceivable we could somehow get into this situation */
        }
        if (key1[0] == key2[0])
          return 0;
        return (signed char) key1[0] > (signed char) key2[0] ? 1 : 2;
      }
    case WO_Short_Weighted_Lex:
      if ((key1[0]==2) != (key2[0]==2))
        return key1[0] == 2 ? 1 : -1;
      /* otherwise same as WO_Weighted_Lex */
    case WO_Weighted_Lex:
      {
        int w1;
        int w2;
        memcpy(&w1,key1+1,sizeof(int));
        memcpy(&w2,key2+1,sizeof(int));
        if (w1 != w2)
        {
          if (w1 > w2)
          {
            if (key1[0]==2 && key2[0]!=2)
              return -1;
            return 1;
          }
          if (key2[0]==2 && key1[0]!=2)
            return -1;
          return 2;
          /* answer cannot be actually 2 on both sides, at least not unless we
             are building a coset acceptor.
             Because in that case we would be doing something like comparing
             u1u2 $$ and u1u2 u1$. If these have arrived at the same difference
             then they already were at the same difference, and we would have
             found a reduction already.
             In the coset case we might be comparing states that came from
             different initial states of the difference machine, so it
             is conceivable we could somehow get into this situation */
        }
      }
      /* otherwise same as shortlex case */
    case WO_Shortlex:
    case WO_Right_Shortlex:
      if (key1[0] == key2[0])
        return 0;
      return (signed char) key1[0] > (signed char) key2[0] ? 1 : 2;
    case WO_Short_FPTP:
      if (key1[0] != key2[0])
        return (signed char) key1[0] > (signed char) key2[0] ? 1 : 2;
      if (key1[0] == 0 || key1[0] == 2)
        return 0;
      /* Otherwise both states have u>v or both u < v.
         If u > v we hope we have already reached a high level.
         If u < v we hope we have not */
      {
        Ordinal p1,p2;
        memcpy(&p1,key1+1,sizeof(Ordinal));
        memcpy(&p2,key2+1,sizeof(Ordinal));
        if (p1 == p2)
          return 0;
        if (key1[0] == 1)
          return p1 > p2 ?  1 : 2;
        return p1 > p2 ? 2 : 1;
      }
    case WO_Multi_Accented_Shortlex:
    case WO_Accented_Shortlex:
      if (key1[0] != key2[0])
        return (signed char) key1[0] > (signed char) key2[0] ? 1 : 2;
      if (key1[0] == 0 || key1[0] == 2)
        return 0;
      /* Otherwise both states have u>v or both u < v.
         If u > v we hope we have already reached a high level.
         If u < v we hope we have not */
      {
        Ordinal p1,p2;
        memcpy(&p1,key1+1,sizeof(Ordinal));
        memcpy(&p2,key2+1,sizeof(Ordinal));
        int np1 = p1 == -1 ? INT_MAX : p1;
        int np2 = p2 == -1 ? INT_MAX : p2;
        if (p1 == p2)
          return 0;
        if (key1[0] == 1)
          return np1 > np2 ?  1 : 2;
        return np1 > np2 ? 2 : 1;
      }
    case WO_Short_Recursive:
    case WO_Short_Right_Recursive:
      if (key1[0] != key2[0])
        return (signed char) key1[0] > (signed char) key2[0] ? 1 : 2;
      if (key1[0] == 2)
        return 0;
      /* otherwise fall through */
    case WO_Right_Recursive:
    case WO_Right_Wreath_Product:
    case WO_Recursive:
    case WO_Wreath_Product:
      {
        int better1 = key1[0] != 2 || key2[0]==2 ? 1 : -1;
        int better2 = key2[0] != 2 || key1[0]==2 ? 2 : -1;
        int answer1 = key1[0] == 2 ? 1 : (signed char) key1[0];
        int answer2 = key2[0] == 2 ? 1 : (signed char) key2[0];
        if (answer1 != answer2)
          return answer1 > answer2 ? better1 : better2;
        Wreath_Compare_State state1;
        Wreath_Compare_State state2;
        memcpy(&state1,key1,sizeof(state1));
        memcpy(&state2,key2,sizeof(state2));
        /* Now get any word information stored in the old state */
        Ordinal_Word w1(*this,state1.pending);
        Ordinal_Word w2(*this,state2.pending);
        unpack(w1.buffer(),key1+sizeof(state1),state1.pending);
        unpack(w2.buffer(),key2+sizeof(state2),state2.pending);

        if (state1.lead)
        {
          state1.pending += state1.lead;
          while (state1.lead--)
            w1.append(representatives[state1.answer > 0 ? state1.level1 : state1.level2]);
        }
        if (state2.lead)
        {
          state2.pending += state2.lead;
          while (state2.lead--)
            w2.append(representatives[state2.answer > 0 ? state2.level1 : state2.level2]);
        }
        if (state1.answer > 0)
        {
          int cmp = w1.compare(w2);
          if (cmp)
            return cmp == 1 ? better1 : better2;
        }
        if (state1.answer == -1)
        {
          int cmp = w1.compare(w2);
          if (cmp)
            return cmp == -1 ? 1 : 2;
        }
        /* In this case the two v words are both ahead or behind u by symbols
           at the same levels, and therefore the most possible difference
           between the states is that one or more levels are lexically undecided
           in one case but not the other. Since the words have had irrelevant
           symbols removed or set equal the lex state will decide , so a
           better lex state must be better. If this does not decide we then
           look at the level which was decided so far.
           A decision made at a higher level is better or worse depending on
           whether it decided for or against success.
        */
        if (state1.lex != state2.lex)
          return state1.lex > state2.lex ? better1 : better2;
        if (state1.decided_level != state2.decided_level)
        {
          /* If we reached a lex decision at all we cannot be comparing padded
             states */
          if (state1.lex==1)
            return state1.decided_level > state2.decided_level ? 1 : 2;
          if (state1.lex==-1)
            return state1.decided_level > state2.decided_level ? 2 : 1;
        }
        return 0;
      }
    case WO_Grouped:
      {
        const int * ikey1 = (const int *)state1;
        const int * ikey2 = (const int *)state2;
        for (int i = nr_levels+2; i-- > 1;)
        {
          if (ikey1[i] != ikey2[i])
            return ikey1[i] > ikey2[i] ? 1 : 2;
        }
        return 0;
      }
    case WO_Ranked:
    case WO_NestedRank:
    case WO_Coset:
      MAF_INTERNAL_ERROR(container,
                         ("Greater than automaton not supported for ordering"
                           " %s\n",word_orderings()[word_ordering].string()));
  }
  return -1;
}

/**/

size_t Alphabet::gt_key_size() const
{
  /* returns a fixed size if the key does not vary in size and 0 otherwise */

  if (order_properties[word_ordering].gt_key_size)
    return order_properties[word_ordering].gt_key_size;

  if (word_ordering == WO_Grouped)
    return (nr_levels+2)*sizeof(int);
  return 0;
}

/**/

size_t Alphabet::gt_size(const void * state_key) const
{
  if (!state_key)
    return 0;
  size_t answer = gt_key_size();
  if (!answer)
  {
    switch (Partial_Switch(order_type()))
    {
      case WO_Short_Right_Recursive:
      case WO_Short_Recursive:
      case WO_Right_Wreath_Product:
      case WO_Right_Recursive:
      case WO_Wreath_Product:
      case WO_Recursive:
        {
          Wreath_Compare_State state;
          memcpy(&state,state_key,sizeof(Wreath_Compare_State));
          answer = sizeof(state) + state.pending*code_size;
        }
        break;
    }
  }
  return answer;
}
