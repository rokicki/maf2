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


// $Log: maf_cfp.cpp $
// Revision 1.5  2010/03/19 22:09:22Z  Alun
// unsigned char changed to "Byte" everywhere
// Revision 1.4  2009/09/12 20:12:00Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2008/11/02 15:48:12Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.4  2008/11/02 16:48:12Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2008/10/08 00:33:01Z  Alun
// Added code to unpack to State_Pair_List
// Revision 1.2  2007/10/24 21:15:29Z  Alun
//

/* See maf_cfp.h for general comments about the classes in this module.
   Although there are a lot of similarities between the various classes
   we want best possible performance from them, so it would be a bad idea
   to try to implement the same functionality using more generic code.
*/
#include <limits.h>
#include <string.h>
#include "awcc.h"
#include "maf_cfp.h"
#include "maf_el.h"
#include "maf_spl.h"

inline size_t aligned_size(size_t size)
{
  return (size+15) & ~15;
}

/**/

size_t Subset_Packer::pack(const Byte * key,State_Count last_element)
{
  State_Count i = first_element;/* Skip initial segment of elements that are ignored */
  Byte * p = search;
  State_Count end_elements = last_element==0 ? nr_elements : last_element+1;

  for (;;)
  {
    /* Skip over missing states */
    State_Count gap = 0;
    while (i < end_elements && !key[i])
    {
      i++;
      gap++;
    }
    if (i < end_elements)
    {
      /* If too many missing states then put in zero length runs */
      while (gap > UCHAR_MAX)
      {
        *p++ = UCHAR_MAX;
        *p++ = 0;
        gap -= UCHAR_MAX;
      }
      /* Now we have got to the start of a run of states */
      *p++ = Byte(gap);
      Byte * start = p++;
      *start = 0;
      while (*start < UCHAR_MAX)
      {
        int shift = 0;
        Byte current = 0;
        while (i < end_elements && shift < CHAR_BIT)
          current += key[i++] << shift++;
        *p++ = current;
        *start += 1;
        State_Count remaining = end_elements-i;
        if (remaining > CHAR_BIT*2)
          remaining = CHAR_BIT*2;
        State_Count j;
        for (j = 0; j < remaining;j++)
          if (key[i+j])
            break;
        if (j == remaining) /* This run of elements has ended */
          break;
      }
    }
    if (i >= end_elements)
    {
      /* Indicate end of packed subset */
      *p++ = 0;
      *p++ = 0;
      break;
    }
  }
#ifdef DEBUG_PACKER
  unpack(test);
  if (memcmp(test,key,nr_elements))
  {
    int i;
    for (i = 0; i < nr_elements;i++)
      if (test[i] != key[i])
        printf("%d %d %d\n",i,test[i],key[i]);
    * (char *) 0 = 0;
  }
#endif
  return p - search;
}

/**/

void Subset_Packer::unpack(Byte * key) const
{
  State_Count i = first_element;
  const Byte * p = search;
  memset(key,0,sizeof(key[0])*first_element);
  for (;;)
  {
    size_t gap = *p++;
    memset(key+i,0,gap);
    i += gap;
    if (!gap && !*p)
      break;
    size_t run_length = *p++;
    while (run_length--)
    {
      int shift = 0;
      while (i < nr_elements && shift < CHAR_BIT)
        key[i++] = (*p >> shift++) & 1;
      p++;
    }
  }
  memset(key+i,0,nr_elements-i);
}

/**/

void Subset_Packer::unpack(Element_List * sl) const
{
  State_ID i = first_element;
  const Byte * p = search;
  sl->empty();
  for (;;)
  {
    size_t gap = *p++;
    i += gap;
    if (!gap && !*p)
      break;
    size_t run_length = *p++;
    while (run_length--)
    {
      int shift = 0;
      while (i < nr_elements && shift < CHAR_BIT)
      {
        if ((*p >> shift++) & 1)
          sl->append_one(i);
        i++;
      }
      p++;
    }
  }
}

/**/

Subset_Packer::Subset_Packer(unsigned nr_elements_,unsigned first_element_) :
  nr_elements(nr_elements_) ,
  first_element(first_element_)
{
  int keys_per_byte = CHAR_BIT;
  /* Calculate maximum key size */
  /* First we get the number of bytes needed for packing all the keys*/
  bytes_needed = (nr_elements-first_element+keys_per_byte-1)/keys_per_byte;
  /* We need two extra bytes for each run of UCHAR_MAX bytes +
     2 more for termining 0 0 run + 1 for initial format selector*/
  bytes_needed += 1+ (bytes_needed+UCHAR_MAX-1)/UCHAR_MAX * 2 + 2;
  search = new Byte[aligned_size(bytes_needed)];
#ifdef DEBUG_PACKER
  test = new Byte[nr_elements];
#endif
}

/**/

size_t Compared_Subset_Packer::pack3(const char * key)
{
  State_Count i = first_element;  /* Skip initial segment */
  Byte * p = search;
  State_Count gap = 0;

  for (;;)
  {
    /* Skip over missing states */
    while (i < nr_elements && !key[i])
    {
      i++;
      gap++;
    }

    if (i < nr_elements)
    {
      /* If too many missing states then put in zero length runs */
      while (gap > UCHAR_MAX)
      {
        *p++ = UCHAR_MAX;
        *p++ = 0;
        gap -= UCHAR_MAX;
      }

      /* Now we have got to the start of a run of states */
      *p++ = Byte(gap);
      Byte * start = p++;
      *start = 0;
      gap = 0;
      while (*start < UCHAR_MAX)
      {
#if UCHAR_MAX==255

        Byte current = key[i++];
#if 1
        static const Byte mul3[] = {0,1,2,0,3,6,0,9,18,0,27,54,0,81,162};
        if (i < nr_elements)
          current += mul3[3+key[i++]];
        if (i < nr_elements)
          current += mul3[6+key[i++]];
        if (i < nr_elements)
          current += mul3[9+key[i++]];
        if (i < nr_elements)
          current += mul3[12+key[i++]];
#else
        if (i < nr_elements)
          current += key[i++]*3;
        if (i < nr_elements)
          current += key[i++]*9;
        if (i < nr_elements)
          current += key[i++]*27;
        if (i < nr_elements)
          current += key[i++]*81;
#endif
#else
        int multiplier = 1;
        Byte current = 0;
        while (i < nr_elements && multiplier*3 <= UCHAR_MAX+1)
        {
          current += key[i++]*multiplier;
          multiplier *= 3;
        }
#endif
        *p++ = current;
        *start += 1;

        State_Count remaining = nr_elements-i;
#if UCHAR_MAX==255
        if (remaining > 10)
          remaining = 10;
#else
        if (remaining > 2*keys_per_byte)
          remaining = 2*keys_per_byte;
#endif
        State_Count j;
        for (j = 0; j < remaining;j++)
          if (key[i+j])
            break;

        if (j == remaining) /* This run of non-zero states has ended */
        {
          gap = remaining;
          i += remaining;
          break;
        }
      }
    }

    if (i == nr_elements)
    {
      /* Indicate end of packed key */
      *p++ = 0;
      *p++ = 0;
      break;
    }
  }
#ifdef DEBUG_PACKER
  unpack3(test);
  if (memcmp(test,key,nr_elements))
  {
    int i;
    for (i = 0; i < nr_elements;i++)
      if (test[i] != key[i])
        printf("%d %d %d\n",i,test[i],key[i]);
    * (char *) 0 = 0;
  }
#endif
  return p - search;
}

static struct
{
  char q;
  char r;
} div3[243];

/**/

void Compared_Subset_Packer::unpack3(char * key) const
{
  State_Count i = first_element;
  const Byte * p = search;
  memset(key,0,first_element);

  for (;;)
  {
    size_t gap = *p++;
    memset(key+i,0,gap);
    i += gap;
    if (!gap && !*p)
      break;
    size_t run_length = *p++;
    while (run_length--)
    {
#if UCHAR_MAX==255
      Byte c = *p;
      key[i++] = div3[c].r;
      if ( i < nr_elements)
        c = div3[c].q,key[i++] = div3[c].r;
      if ( i < nr_elements)
        c = div3[c].q,key[i++] = div3[c].r;
      if ( i < nr_elements)
        c = div3[c].q,key[i++] = div3[c].r;
      if ( i < nr_elements)
        c = div3[c].q,key[i++] = div3[c].r;
#else
      int multiplier = 1;
      while (i < nr_elements && multiplier*3 <= UCHAR_MAX+1)
      {
        key[i++] = *p/multiplier % 3;
        multiplier *= 3;
      }
#endif
      p++;
    }
  }
  memset(key+i,0,nr_elements-i);
}

/**/

void Compared_Subset_Packer::unpack3(State_Pair_List * spl) const
{
  State_Count i = first_element;
  const Byte * p = search;

  for (;;)
  {
    size_t gap = *p++;
    i += gap;
    if (!gap && !*p)
      break;
    size_t run_length = *p++;
    while (run_length--)
    {
#if UCHAR_MAX==255
      Byte c = *p;
      if (div3[c].r)
        spl->append(i,div3[c].r);
      i++;
      if (i < nr_elements)
      {
        c = div3[c].q;
        if (div3[c].r)
          spl->append(i,div3[c].r);
        i++;
      }
      if (i < nr_elements)
      {
        c = div3[c].q;
        if (div3[c].r)
          spl->append(i,div3[c].r);
        i++;
      }
      if (i < nr_elements)
      {
        c = div3[c].q;
        if (div3[c].r)
          spl->append(i,div3[c].r);
        i++;
      }
      if (i < nr_elements)
      {
        c = div3[c].q;
        if (div3[c].r)
          spl->append(i,div3[c].r);
        i++;
      }
#else
      int multiplier = 1;
      while (i < nr_elements && multiplier*3 <= UCHAR_MAX+1)
      {
        int s2 = *p/multiplier % 3;
        if (s2)
          spl->append(i,s2);
        i++;
        multiplier *= 3;
      }
#endif
      p++;
    }
  }
}

/**/

size_t Compared_Subset_Packer::pack4(const char * key)
{
  State_Count i = first_element;  /* Skip initial segment */
  Byte * p = search;

  for (;;)
  {
    /* Skip over missing states */
    State_Count gap = 0;
    while (i < nr_elements && !key[i])
    {
      i++;
      gap++;
    }

    if (i < nr_elements)
    {
      /* If too many missing states then put in zero length runs */
      while (gap > UCHAR_MAX)
      {
        *p++ = UCHAR_MAX;
        *p++ = 0;
        gap -= UCHAR_MAX;
      }

      /* Now we have got to the start of a run of states */
      *p++ = Byte(gap);
      Byte * start = p++;
      *start = 0;
      while (*start < UCHAR_MAX)
      {
#if UCHAR_MAX==255
        Byte current = key[i++];
        if (i < nr_elements)
          current += key[i++]*4;
        if (i < nr_elements)
          current += key[i++]*16;
        if (i < nr_elements)
          current += key[i++]*64;
#else
        int multiplier = 1;
        Byte current = 0;
        while (i < nr_elements && multiplier*4 <= UCHAR_MAX+1)
        {
          current += key[i++]*multiplier;
          multiplier *= 4;
        }
#endif
        *p++ = current;
        *start += 1;

        State_Count remaining = nr_elements-i;
#if UCHAR_MAX==255
        if (remaining > 8)
          remaining = 8;
#else
        if (remaining > 2*keys_per_byte)
          remaining = 2*keys_per_byte;
#endif
        State_Count j;
        for (j = 0; j < remaining;j++)
          if (key[i+j])
            break;

        if (j == remaining) /* This run of non-zero states has ended */
          break;
      }
    }

    if (i == nr_elements)
    {
      /* Indicate end of packed key */
      *p++ = 0;
      *p++ = 0;
      break;
    }
  }
#ifdef DEBUG_PACKER
  unpack4(test);
  if (memcmp(test,key,nr_elements))
  {
    int i;
    for (i = 0; i < nr_elements;i++)
      if (test[i] != key[i])
        printf("%d %d %d\n",i,test[i],key[i]);
    * (char *) 0 = 0;
  }
#endif
  return p - search;
}

/**/

void Compared_Subset_Packer::unpack4(char * key) const
{
  State_Count i = first_element;
  const Byte * p = search;
  memset(key,0,first_element);

  for (;;)
  {
    size_t gap = *p++;
    memset(key+i,0,gap);
    i += gap;
    if (!gap && !*p)
      break;
    size_t run_length = *p++;
    while (run_length--)
    {
#if UCHAR_MAX==255
      key[i++] = *p % 4;
      if ( i < nr_elements)
        key[i++] = *p/4 % 4;
      if ( i < nr_elements)
        key[i++] = *p/16 % 4;
      if ( i < nr_elements)
        key[i++] = *p/64 % 4;
#else
      int multiplier = 1;
      while (i < nr_elements && multiplier*4 <= UCHAR_MAX+1)
      {
        key[i++] = *p/multiplier % 4;
        multiplier *= 4;
      }
#endif
      p++;
    }
  }
  memset(key+i,0,nr_elements-i);
}

/**/

void Compared_Subset_Packer::unpack4(State_Pair_List * spl) const
{
  State_Count i = first_element;
  const Byte * p = search;
  State_ID s2;

  for (;;)
  {
    size_t gap = *p++;
    i += gap;
    if (!gap && !*p)
      break;
    size_t run_length = *p++;
    while (run_length--)
    {
#if UCHAR_MAX==255
      s2 = *p % 4;
      if (s2)
        spl->append(i,s2);
      i++;
      if (i < nr_elements)
      {
        s2 = *p/4 % 4;
        if (s2)
          spl->append(i,s2);
        i++;
      }
      if (i < nr_elements)
      {
        s2 = *p/16 % 4;
        if (s2)
          spl->append(i,s2);
        i++;
      }
      if (i < nr_elements)
      {
        s2 = *p/64 % 4;
        if (s2)
          spl->append(i,s2);
        i++;
      }
#else
      int multiplier = 1;
      while (i < nr_elements && multiplier*4 <= UCHAR_MAX+1)
      {
        s2 = *p/multiplier % 4;
        if (s2)
          spl->append(i,s2);
        i++;
        multiplier *= 4;
      }
#endif
      p++;
    }
  }
}

/**/

Compared_Subset_Packer::Compared_Subset_Packer(unsigned nr_elements_,
                                               unsigned first_element_,
                                               bool need_equal_state_) :
  nr_elements(nr_elements_),
  first_element(first_element_),
  need_equal_state(need_equal_state_)
{
  int i = 1;
  int keys_per_byte = 0;
  unsigned factor = need_equal_state ? 4 : 3;
  while ((i *= factor) <= UCHAR_MAX+1)
    keys_per_byte++;
  /* Calculate maximum key size */
  /* First we get the number of bytes needed for packing all the keys*/
  bytes_needed = (nr_elements-first_element+keys_per_byte-1)/keys_per_byte;
  /* We need two extra bytes for each run of UCHAR_MAX bytes +
     2 more for termining 0 0 run */
  bytes_needed += (bytes_needed+UCHAR_MAX-1)/UCHAR_MAX * 2 + 2;
  search = new Byte[aligned_size(bytes_needed)];
#ifdef DEBUG_PACKER
  test = new char[nr_elements];
#endif
  if (!div3[1].r)
    for (Byte i = 0; i < 243;i++)
    {
      div3[i].q = i / 3;
      div3[i].r = i % 3;
    }
}

/**/

void Compared_Subset_Packer::unpack(State_Pair_List * spl) const
{
  spl->empty();
  spl->reserve(nr_elements,false);
  if (need_equal_state)
    unpack4(spl);
  else
    unpack3(spl);
}

/**/

static int bits_needed(unsigned long set_size)
{
  /* Find how many numbers are needed to represent numbers from 0 to set_size-1
  */

  unsigned long i = 1;
  int bits_per_element = 0;

  while (i && i < set_size)
  {
    bits_per_element++;
    i *= 2;
  }
  return bits_per_element;
}

static unsigned long bit_mask(size_t bit_size)
{
  /* This returns the all ones bit mask needed to correctly
     extra a number of bit_size bits from the low order bit_size bits
     of an unsigned long.
     In theory we want just to return (1 << bit_size) - 1 but that might just
     go wrong if bit_size is 32 (or whatever the sizeof(long) in bits
     happens to be). The code here should be safe for any processor. */
  return bit_size ? (((1 << (bit_size-1)) - 1) << 1) | 1 : 0;
}

/**/

Base_Tuple_Packer::Base_Tuple_Packer() :
  bits_per_long(sizeof(unsigned long)*CHAR_BIT)
{
  union
  {
    unsigned long key[1];
    Byte data[1];
  } packed_key;
  /* check our packing will not be fooled by endianness of processor */
  packed_key.key[0] = 0x12345678;
  little_endian = packed_key.data[0] == 0x78 &&
                  packed_key.data[1] == 0x56 &&
                  packed_key.data[2] == 0x34 &&
                  packed_key.data[3] == 0x12;
  big_endian = packed_key.data[3] == 0x78 &&
               packed_key.data[2] == 0x56 &&
               packed_key.data[1] == 0x34 &&
               packed_key.data[0] == 0x12;
}

/**/

void Base_Tuple_Packer::adjust_size()
{
  correction = 0;
  /* check our packing will not be fooled by endianness of processor */
  size_t remainder = size & (sizeof(unsigned long) -1);
  if (remainder)
  {
    /* historically some CPUs used mixed endianness!. In that
       case we just round up to the next long boundary */
    if (!little_endian && !big_endian)
      size = (size + sizeof(unsigned long) -1) & ~(sizeof(unsigned long) -1);
    /* For a big_endian processor we must shift the number left to
       make sure we don't leave a hole and lose data off the end */
    if (big_endian)
      correction = bits_per_long - remainder*CHAR_BIT;
  }
}

/**/

Triple_Packer::Triple_Packer(State_ID key[3])
{
  sizes[0] = bits_needed(key[0]);
  masks[0] = bit_mask(sizes[0]);
  factor[0] = key[0];
  sizes[1] = bits_needed(key[1]);
  masks[1] = bit_mask(sizes[1]);
  factor[1] = key[1];
  sizes[2] = bits_needed(key[2]);
  masks[2] = bit_mask(sizes[2]);
  factor[2] = key[2];
  unsigned long max_value = ULONG_MAX;
  max_value /= factor[2];
  max_value /= factor[1];
  max_value /= factor[0];
  use_arithmetic_code = max_value != 0;
  if (use_arithmetic_code)
    size = (bits_needed(factor[0]*factor[1]*factor[2]) + CHAR_BIT-1)/CHAR_BIT;
  else
    size = (sizes[0] + sizes[1] + sizes[2] + CHAR_BIT-1)/CHAR_BIT;
  adjust_size();
}

/**/

void * Triple_Packer::pack_key(const State_ID * key)
{
  unsigned long * p = packed_key.key;
  if (use_arithmetic_code)
    packed_key.key[0] = key[0] + (key[1]+key[2]*factor[1])*factor[0];
  else
  {
    *p = key[0];
    unsigned long v = key[1];
    unsigned long bits_available = bits_per_long - sizes[0];
    if (bits_available)
      *p |= v << sizes[0];
    if (bits_available < sizes[1])
    {
      *(p = packed_key.key+1) = v >> bits_available;
      bits_available += bits_per_long;
    }
    bits_available -= sizes[1];
    v = key[2];
    if (bits_available)
      *p |= v << (bits_per_long-bits_available);
    if (bits_available < sizes[2])
      *++p = v >> bits_available;
  }
  *p <<= correction;
#ifdef DEBUG_PACKER
  unpack_key(test);
  if (memcmp(test,key,3*sizeof(State_ID)))
  {
    int i;
    for (i = 0; i < 3;i++)
      if (test[i] != key[i])
        printf("%d %d %d\n",i,test[i],key[i]);
    * (char *) 0 = 0;
  }
#endif
  return packed_key.data;
}

void Triple_Packer::unpack_key(State_ID * key) const
{
  const unsigned long * p = packed_key.key;
  unsigned long v = packed_key.key[0];
  if (use_arithmetic_code)
  {
    if (correction)
      v >>= correction;
    key[0] = v % factor[0];
    v /= factor[0];
    key[1] = v % factor[1];
    key[2] = v / factor[1];
  }
  else
  {
    key[0] = v;
    unsigned long bits_available = bits_per_long - sizes[0];
    if (bits_available)
    {
      key[0] &= masks[0];
      key[1] = v >> sizes[0];
    }
    else
      key[1] = 0;
    if (bits_available < sizes[1])
    {
      v = *++p;
      if (size < 2*sizeof(unsigned long))
        v >>= correction;
      key[1] |= v << bits_available;
      bits_available += bits_per_long;
    }
    bits_available -= sizes[1];
    if (bits_available)
    {
      key[1] &= masks[1];
      key[2] = v >> (bits_per_long - bits_available);
    }
    else
      key[2] = 0;

    if (bits_available < sizes[2])
    {
      v = *++p >> correction;
      key[2] |= v << bits_available;
    }
    key[2] &= masks[2];
  }
}

/**/

Pair_Packer::Pair_Packer(State_ID key[2])
{
  sizes[0] = bits_needed(key[0]);
  masks[0] = bit_mask(sizes[0]);
  factor[0] = key[0];
  sizes[1] = bits_needed(key[1]);
  masks[1] = bit_mask(sizes[1]);
  factor[1] = key[1];
  unsigned long max_value = ULONG_MAX;
  max_value /= factor[1];
  max_value /= factor[0];
  use_arithmetic_code = max_value != 0;
  if (use_arithmetic_code)
    size = (bits_needed(factor[0]*factor[1]) + CHAR_BIT-1)/CHAR_BIT;
  else
    size = (sizes[0] + sizes[1] + CHAR_BIT-1)/CHAR_BIT;
  adjust_size();
}

/**/

void * Pair_Packer::pack_key(const State_ID * key)
{
  unsigned long * p = packed_key.key;
  if (use_arithmetic_code)
    packed_key.key[0] = key[0] + key[1]*factor[0];
  else
  {
    *p = key[0];
    unsigned long v = key[1];
    unsigned long bits_available = bits_per_long - sizes[0];
    if (bits_available)
      *p |= v << sizes[0];
    if (bits_available < sizes[1])
      *(p = packed_key.key+1) = v >> bits_available;
  }
  *p <<= correction;
#ifdef DEBUG_PACKER
  unpack_key(test);
  if (memcmp(test,key,2*sizeof(State_ID)))
  {
    int i;
    for (i = 0; i < 2;i++)
      if (test[i] != key[i])
        printf("%d %d %d\n",i,test[i],key[i]);
    * (char *) 0 = 0;
  }
#endif
  return packed_key.data;
}

/**/

void Pair_Packer::unpack_key(State_ID * key) const
{
  const unsigned long * p = packed_key.key;
  unsigned long v = packed_key.key[0];
  if (use_arithmetic_code)
  {
    if (correction)
      v >>= correction;
    key[0] = v % factor[0];
    key[1] = v / factor[0];
  }
  else
  {
    key[0] = v;
    unsigned long bits_available = bits_per_long - sizes[0];
    if (bits_available)
    {
      key[0] &= masks[0];
      key[1] = v >> sizes[0];
    }
    else
      key[1] = 0;
    if (bits_available < sizes[1])
    {
      v = *++p;
      v >>= correction;
      key[1] |= v << bits_available;
    }
  }
}

/**/

size_t Sparse_Function_Packer::pack(const State_ID * key)
{
  State_ID i = first_element;  /* ignore initial segment*/
  Byte * p = search;

  /* WARNING. Group_Automata::validate_difference_machine() assumes
     knowledge of the implementation of this method. If the
     compression scheme is changed the code of that method may need
     to be revised */
  for (;;)
  {
    /* Skip over missing states */
    State_Count gap = 0;
    while (i < nr_elements && !key[i])
    {
      i++;
      gap++;
    }

    if (i < nr_elements)
    {
      /* If too many missing states then put in zero length runs */
      while (gap > UCHAR_MAX)
      {
        *p++ = UCHAR_MAX;
        *p++ = 0;
        gap -= UCHAR_MAX;
      }
      /* Now we have got to the start of a run of states */
      *p++ = Byte(gap);
      Byte * start = p++;
      *start = 0;
      unsigned long current = 0;
      int available = CHAR_BIT*sizeof(unsigned long);
      int bit_size = available;
      for (;;)
      {
        bool complete = true;
        if (available >= bits_per_state)
          current |= key[i] << (available -= bits_per_state);
        else
        {
          current |= key[i] >> (bits_per_state - available);
          available -= bits_per_state;
          complete = false;
        }
        while (available + CHAR_BIT <= bit_size)
        {
          *p++ = current >> (CHAR_BIT*(sizeof(unsigned long)-1));
          current <<= CHAR_BIT;
          available += CHAR_BIT;
        }
        if (!complete)
          current |= key[i] << available;
        *start += 1;
        if (++i == nr_elements || *start == UCHAR_MAX)
          break;
        if (!key[i])
        {
          int keep_going = (2*CHAR_BIT + available % CHAR_BIT)/bits_per_state;
          bool found = false;
          for (int j = 1; j <= keep_going && !found;j++)
          {
            if (*start + j >= UCHAR_MAX || i+j >= nr_elements)
              break;
            if (key[i+j])
              found = true;
          }
          if (!found)
            break;
        }
      }
      if (available < bit_size)
        *p++ = current >> (CHAR_BIT*(sizeof(unsigned long)-1));
    }

    if (i == nr_elements)
    {
      /* Indicate end of packed key */
      *p++ = 0;
      *p++ = 0;
      break;
    }
  }
#ifdef DEBUG_PACKER
  if (p - search > bytes_needed)
     * (char *) 0 = 0;
  unpack(test);
  if (memcmp(test,key,nr_elements*sizeof(State_ID)))
  {
    int i;
    for (i = 0; i < nr_elements;i++)
      if (test[i] != key[i])
        printf("%d %d %d\n",i,test[i],key[i]);
    * (char *) 0 = 0;
  }
#endif
  return p - search;
}

/**/

void Sparse_Function_Packer::unpack(State_ID * key) const
{
  State_Count i = first_element;
  const Byte * p = search;
  memset(key,0,first_element*sizeof(State_ID));

  for (;;)
  {
    size_t gap = *p++;
    memset(key+i,0,gap*sizeof(State_ID));
    i += gap;
    if (!gap && !*p)
      break;
    size_t run_length = *p++;
    unsigned long current = 0;
    int bit_size = CHAR_BIT*sizeof(unsigned long);
    int read = 0;
    while (run_length--)
    {
      State_ID value = current &= ((1 << read)-1);
      while (read < bits_per_state)
      {
        current = (current << CHAR_BIT) | *p++;
        read += CHAR_BIT;
      }
      if (read > bit_size)
      {
        key[i] = (value << (bits_per_state - (read-bit_size)));
        key[i++] |= (current >> (read -= bits_per_state));
      }
      else
        key[i++] = current >> (read -= bits_per_state);
    }
  }
  memset(key+i,0,(nr_elements-i)*sizeof(State_ID));
}

/**/

Sparse_Function_Packer::Sparse_Function_Packer(State_Count nr_elements_,
                                               State_Count nr_range_elements,
                                               State_ID first_element_) :
  nr_elements(nr_elements_),
  first_element(first_element_)
{
  State_ID i = 1;
  bits_per_state = 0;
  while (i < nr_range_elements)
  {
    bits_per_state++;
    i *= 2;
  }
  bytes_needed = (nr_elements-first_element)*bits_per_state/CHAR_BIT;
  bytes_needed += nr_elements*3/2;
  bytes_needed += 2;
  /* Each run of states needs two extra bytes for the gap and
     run length and possibly another byte to hold remainder bits.
     Obviously the maximum number of runs occurs when we see
     0 and non zero states alternating. We also need 2 extra bytes
     for the final 0 0.
  */
  search = new Byte[aligned_size(bytes_needed)];
}

/**/

Quad_Packer::Quad_Packer(State_ID key[4])
{
  sizes[0] = bits_needed(key[0]);
  masks[0] = bit_mask(sizes[0]);
  sizes[1] = bits_needed(key[1]);
  masks[1] = bit_mask(sizes[1]);
  sizes[2] = bits_needed(key[2]);
  masks[2] = bit_mask(sizes[2]);
  sizes[3] = bits_needed(key[3]);
  masks[3] = bit_mask(sizes[3]);
  size = (sizes[0] + sizes[1] + sizes[2] + sizes[3] + CHAR_BIT-1)/CHAR_BIT;
  adjust_size();
}

/**/

void * Quad_Packer::pack_key(const State_ID * key)
{
  unsigned long * p = packed_key.key;
  *p = key[0];

  unsigned long v = key[1];
  unsigned long bits_available = bits_per_long - sizes[0];
  if (bits_available)
    *p |= v << sizes[0];
  if (bits_available < sizes[1])
  {
    *(p = packed_key.key+1) = v >> bits_available;
    bits_available += bits_per_long;
  }
  bits_available -= sizes[1];
  v = key[2];
  if (bits_available)
    *p |= v << (bits_per_long - bits_available);
  if (bits_available < sizes[2])
  {
    *++p = v >> bits_available;
    bits_available += bits_per_long;
  }
  bits_available -= sizes[2];

  v = key[3];
  if (bits_available)
    *p |= v << (bits_per_long-bits_available);
  if (bits_available < sizes[3])
    *++p = v >> bits_available;

  *p <<= correction;
#ifdef DEBUG_PACKER
  unpack_key(test);
  if (memcmp(test,key,4*sizeof(State_ID)))
  {
    int i;
    for (i = 0; i < 4;i++)
      if (test[i] != key[i])
        printf("%d %d %d\n",i,test[i],key[i]);
    * (char *) 0 = 0;
  }
#endif
  return packed_key.data;
}

void Quad_Packer::unpack_key(State_ID * key) const
{
  const unsigned long * p = packed_key.key;
  unsigned long v = packed_key.key[0];
  size_t read = sizeof(unsigned long);

  key[0] = v;
  unsigned long bits_available = bits_per_long - sizes[0];
  if (bits_available)
  {
    key[0] &= masks[0];
    key[1] = v >> sizes[0];
  }
  else
    key[1] = 0;
  if (bits_available < sizes[1])
  {
    v = *++p;
    read += sizeof(unsigned long);
    if (size < read)
      v >>= correction;
    key[1] |= v << bits_available;
    bits_available += bits_per_long;
  }
  bits_available -= sizes[1];

  if (bits_available)
  {
    key[1] &= masks[1];
    key[2] = v >> (bits_per_long - bits_available);
  }
  else
    key[2] = 0;
  if (bits_available < sizes[2])
  {
    v = *++p;
    read += sizeof(unsigned long);
    if (size < read)
      v >>= correction;
    key[2] |= v << bits_available;
    bits_available += bits_per_long;
  }
  bits_available -= sizes[2];

  if (bits_available)
  {
    key[2] &= masks[2];
    key[3] = v >> (bits_per_long - bits_available);
  }
  else
    key[3] = 0;

  if (bits_available < sizes[3])
  {
    v = *++p >> correction;
    key[3] |= v << bits_available;
  }
  key[3] &= masks[3];
}

