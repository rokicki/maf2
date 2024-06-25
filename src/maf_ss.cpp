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


// $Log: maf_ss.cpp $
// Revision 1.6  2010/03/19 11:28:41Z  Alun
// Changes to reduce memory churning when dealing with big bitsets
// Revision 1.5  2009/09/14 09:58:10Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2009/07/04 13:36:26Z  Alun
// Should prefer bit strings even on draw with other formats as otherwise we can
// corrupt memory through buffer being slightly too small
// Revision 1.4  2008/10/13 22:04:20Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2008/10/10 07:48:33Z  Alun
// Changes to allow this class to be used as key for FSAs. Usually smaller and
// faster than old Subset_Packer type keys
// Revision 1.2  2008/08/18 08:07:00Z  Alun
// Does not switch to SSF_All when there is actually only one accept state
// unless this is specially requested, since this causes unpleasant surprises
// when new states are added later on.
// Revision 1.1  2007/10/24 21:08:30Z  Alun
// New file.
//
#include <string.h>
#include "awcc.h"
#include "maf_ss.h"
#include "container.h"

static const int ENDIAN_DETECTOR = 0x00ffff01;
static const bool little_endian = * (bool *) &ENDIAN_DETECTOR;

Special_Subset::Special_Subset(const Special_Subset_Owner & owner_,
                               Special_Subset_Format format_) :
  owner(owner_),
  preferred_format(SSF_Empty),
  format(format_),
  first_member(0),
  last_member(0),
  nr_members(0),
  big_bitset_expected(false)
{
  if (format == SSF_All)
  {
    nr_members = -1;
    first_member = owner.first_valid_element();
  }
  else if (format == SSF_Singleton)
  {
    nr_members = 1;
    first_member = last_member = owner.first_valid_element();
  }
}

bool Special_Subset::assign_membership(Element_ID element,bool is_member)
{
  /* set the membership status of the specified element.
     This is surprisingly complex, owing to the variety
     of formats we use for storing this information */
  if (!owner.is_valid_element(element))
    return !is_member;

  /* First deal with the special formats that cannot store
     general subsets of the element set */
  switch (format)
  {
    case SSF_Empty:
      if (!is_member)
        return true;
      format = SSF_Singleton;
      first_member = last_member = element;
      nr_members = 1;
      return true;

    case SSF_All:
      {
        if (is_member)
          return true;
        Element_Count nr_elements = owner.element_count();
        if (element == first_member)
          return set_range_set(first_member+1,nr_elements-1);
        if (element == nr_elements-1)
          return set_range_set(first_member,nr_elements-2);
        format = SSF_Flagged;
        first_member = owner.first_valid_element();
        last_member = nr_elements-1;
        member_flags.change_length(nr_elements,BAIV_One,true);
        member_flags.assign(element,false);
        return true;
      }
    case SSF_Singleton:
      if (!is_member)
      {
        if (element == first_member)
          exclude_all();
        return true;
      }
      if (element == first_member)
        return true;
      if (element == first_member - 1)
      {
        first_member = element;
        nr_members = 2;
        format = SSF_Range;
        return true;
      }
      if (element == last_member + 1)
      {
        format = SSF_Range;
        nr_members = 2;
        last_member = element;
        return true;
      }
      leave_range_format();
      break;
    case SSF_Range:
      if (!is_member)
      {
        if (element < first_member || element > last_member)
          return true;
        if (element == first_member)
        {
          first_member++;
          nr_members--;
          if (nr_members == 1)
            format = SSF_Singleton;
          return true;
        }
        if (element == last_member)
        {
          last_member--;
          nr_members--;
          if (nr_members == 1)
            format = SSF_Singleton;
          return true;
        }
        leave_range_format();
        break;
      }
      else
      {
        if (element == last_member + 1)
        {
          nr_members++;
          last_member = element;
          return true;
        }
        if (element == first_member - 1)
        {
          nr_members++;
          first_member = element;
          return true;
        }
        leave_range_format();
        break;
      }
    case SSF_Both:    // These are the generic formats - we don't need to do
    case SSF_Flagged: // anything here
    case SSF_List:
      break;
  }

  int delta = 0;
  if (has_flags())
  {
    if (is_member == contains(element))
      return true;
    delta = is_member ? 1 : -1;
  }

  if (is_member)
  {
    if (element < first_member)
      first_member = element;
    if (has_list())
    {
      if (last_member < element)
      {
        last_member = element;
        member_list.append_one(element);
        delta = 1;
      }
      else if (member_list.insert(element))
        delta = 1;
    }
    else if (last_member < element)
      last_member = element;
  }
  else if (has_list() && member_list.remove(element))
    delta = -1;

  nr_members += delta;
  if (!nr_members)
  {
    exclude_all();
    return true;
  }
  if (nr_members == last_member-first_member+1)
  {
    set_range_set(first_member,last_member);
    return true;
  }

  if (has_flags())
    member_flags.safe_assign(element,is_member);
  if (format == SSF_List &&
      size_t(nr_members*sizeof(State_ID)) > size_t(owner.element_count()/CHAR_BIT))
  {
    if (preferred_format == SSF_Empty)
      preferred_format = SSF_Flagged;
    shrink(false);
  }

  if (!is_member && (element == first_member || element==last_member))
    shrink(true); /* We always want to know the first and last elements in the set */

  return true;
}

/**/

void Special_Subset::exclude_all()
{
  if (format == SSF_Both && nr_members*2 < last_member/CHAR_BIT)
  {
    Element_List::Iterator eli(member_list);
    for (Element_ID si = eli.first();si;si = eli.next())
      member_flags.zap(si);
  }
  else
    member_flags.empty();
  member_list.empty();
  first_member = last_member = 0;
  nr_members = 0;
  format = SSF_Empty;
}

/**/

void Special_Subset::include_all()
{
  member_flags.empty();
  member_list.allocate(0,false);
  first_member = owner.first_valid_element();
  last_member = owner.element_count()-1;
  nr_members = last_member-first_member+1;
  format = SSF_All;
}

void Special_Subset::renumber(const Element_ID * new_element_numbers)
{
  switch (format)
  {
    case SSF_Empty:
    case SSF_All:
      return;
    case SSF_Singleton:
      first_member = last_member = new_element_numbers[first_member];
      return;
    case SSF_Range:
      // there is obviously no guarantee members are still contiguous
      leave_range_format();
      break;
    case SSF_Both:    // These formats all need special handling
    case SSF_Flagged:
    case SSF_List:
    default:
      break;
  }
  if (has_list())
  {
    Element_List save;
    save.take(member_list);
    exclude_all();
    Element_List::Iterator eli(save);
    for (Element_ID si = eli.first();si;si = eli.next())
      assign_membership(new_element_numbers[si],true);
  }
  else
  {
    Bit_Array save;
    save.take(member_flags);
    exclude_all();
    Element_Count count = save.length();
    for (Element_ID element = 1; element < count;element++)
      if (save.safe_get(element))
        assign_membership(new_element_numbers[element],true);
  }
  shrink(true);
}

/**/

void Special_Subset::set_fast_random_access(bool update_preferred)
{
  /* Ensure that we do not have to check membership using
     binary search on a list */
  if (format == SSF_List)
  {
    if (update_preferred)
    {
      if (preferred_format == SSF_Empty)
        preferred_format = SSF_Flagged;
      else if (preferred_format == SSF_List)
        preferred_format = SSF_Both;
    }

    Element_List::Iterator eli(member_list);
    member_flags.change_length(big_bitset_expected ? owner.element_count() : eli.last()+1,
                               BAIV_Zero,true);
    for (Element_ID element = eli.first();element;element = eli.next())
      member_flags.assign(element,true);
    format = SSF_Both;
  }
}

/**/

void Special_Subset::set_fast_sequential_access(bool update_preferred_format)
{
  /* Ensure that we do not have to iterate through the members
     by checking the membership of each element */
  if (format == SSF_All)
    last_member = count();
  if (format == SSF_Flagged)
  {
    Element_Count count = member_flags.length();
    if (update_preferred_format)
    {
      if (preferred_format == SSF_Empty)
        preferred_format = SSF_List;
      else if (preferred_format == SSF_Flagged)
        preferred_format = SSF_Both;
    }

    for (Element_ID element = owner.first_valid_element(); element < count ;element++)
      if (member_flags.get(element))
        member_list.append_one(element);
    format = SSF_Both;
  }
}

/**/

bool Special_Subset::set_range_set(Element_ID first,Element_ID last)
{
  exclude_all();
  if (first <= last && owner.is_valid_element(first))
  {
    first_member = first;
    last_member = last;
    nr_members = last - first + 1;
    format = nr_members == 1 ? SSF_Singleton : SSF_Range;
    return true;
  }
  return false;
}

/**/

bool Special_Subset::shrink(bool check_bit_array)
{
  /* We change the format if we can save space.
     Unless check_bit_array() is true we don't do the more expensive
     checks that are needed to decide whether it is appropriate to
     switch away from SSF_Flagged formats. */

  if (nr_members == owner.element_count()-owner.first_valid_element() &&
      nr_members != 1 || format==SSF_All)
  {
    include_all();
    return true;
  }

  if (has_list())
  {
    Element_List::Iterator eli(member_list);
    last_member = eli.last();
    first_member = eli.first();
    if (nr_members == 1)
      return set_singleton_set(first_member);
    else if (nr_members == last_member - first_member + 1)
      return set_range_set(first_member,last_member);
    else if (recommended_format() == SSF_Flagged)
    {
      set_fast_random_access(false);
      if (preferred_format != SSF_Both)
      {
        member_list.empty();
        format = SSF_Flagged;
      }
      return true;
    }
  }

  if (has_flags() && check_bit_array)
  {
    Element_ID bit_length;
    for (bit_length = last_member+1;bit_length > 1;bit_length--)
      if (member_flags.contains(bit_length-1))
        break;
    member_flags.change_length(bit_length,BAIV_Zero,big_bitset_expected);

    last_member = bit_length-1;
    if (nr_members == 1)
      return set_singleton_set(last_member);
    while (!member_flags.get(first_member))
      first_member++;
    if (last_member - first_member + 1 == nr_members)
      return set_range_set(first_member,last_member);

    if (recommended_format() == SSF_List)
    {
      set_fast_sequential_access(false);
      member_flags.change_length(0);
      format = SSF_List;
    }
    else
    {
      member_list.empty();
      format = SSF_Flagged;
    }
    return true;
  }
  return false;
}

/**/

void Special_Subset::print(Output_Stream * stream) const
{
  Container & container = owner.container;
  switch (format)
  {
    case SSF_Empty:
      break;
    case SSF_Singleton:
      container.output(stream,FMT_ID,first_member);
      break;
    case SSF_All:
      last_member = owner.element_count()-1;
    case SSF_Range:
      container.output(stream,FMT_ID ".." FMT_ID,first_member,last_member);
      break;
    case SSF_Both:
    case SSF_List:
      {
        Element_List::Iterator eli(member_list);
        bool started = false;
        size_t written = 3;
        for (Element_ID element = eli.first();element;element = eli.next())
        {
          if (written >= 64)
          {
            container.output(stream,",\n            ");
            written = container.output(stream,FMT_ID,element);
          }
          else
            written += container.output(stream,started ? "," FMT_ID : FMT_ID,element);
          started = true;
        }
      }
      break;
    case SSF_Flagged:
      {
        bool started = false;
        int written = 3;
        for (Element_ID element = 1;element <= last_member;element++)
        {
          if (contains(element))
          {
            if (written >= 64)
            {
              container.output(stream,",\n            ");
              written = container.output(stream,FMT_ID,element);
            }
            else
              written += container.output(stream,started ? "," FMT_ID : FMT_ID,element);
            started = true;
          }
        }
      }
      break;
    default:
      NOT_REACHED(;)
  }
}

/**/

void Special_Subset::leave_range_format()
{
  if (preferred_format != SSF_Empty)
    format = preferred_format;
  else
    format = recommended_format();
  if (has_list())
    for (Element_ID element = first_member; element <= last_member;element++)
      member_list.append_one(element);
  if (has_flags())
  {
    member_flags.change_length(last_member+1,
                               first_member < last_member/2 ?
                               BAIV_One : BAIV_Zero,true);
    if (big_bitset_expected)
      member_flags.change_length(owner.element_count(),BAIV_Zero,true);
    if (first_member < last_member /2)
      for (Element_ID element = 0; element < first_member;element++)
        member_flags.assign(element,false);
      else
        for (Element_ID element = first_member; element <= last_member; element++)
          member_flags.assign(element,true);
  }
}

/**/

bool Special_Subset::take(Special_Subset &other)
{
  if (&owner == &other.owner)
  {
    first_member = other.first_member;
    last_member = other.last_member;
    nr_members = other.nr_members;
    format = other.format;
    member_list.take(other.member_list);
    member_flags.take(other.member_flags);
    other.empty();
    return true;
  }
  return false;
}

/**/

size_t Special_Subset::bytes_needed(Element_ID element) const
{
  /* Returns the size of character array that can contain this
     value in a "natural" way */
  if (element >= 0)
  {
    if (element <= UCHAR_MAX)
      return 1;
    if (element <= USHRT_MAX)
      return sizeof(short);
    if (element <= (USHRT_MAX << CHAR_BIT) + UCHAR_MAX)
      return sizeof(unsigned short) + 1;
  }
  return sizeof(Element_ID);
}

/**/

Byte * Special_Subset::pack_element(Byte *buffer,Element_ID element,size_t size) const
{
  switch (size)
  {
    case 1:
      *buffer = element;
      break;
    case sizeof(unsigned short):
      {
        unsigned short s = element;
        memcpy(buffer,&s,sizeof(unsigned short));
        break;
      }
    case sizeof(unsigned short)+1:
      if (little_endian)
        memcpy(buffer,&element,sizeof(unsigned short) + 1);
      else
        memcpy(buffer,(char *) (&element+1)-(sizeof(unsigned short)+1),sizeof(unsigned short)+1);
      break;
    case sizeof(Element_ID):
      memcpy(buffer,&element,sizeof(Element_ID));
      break;
    default:
      NOT_REACHED(* (char *) 0 = 0;)
  }
  return buffer+size;
}

/**/

Element_ID Special_Subset::unpack_element(const Byte *buffer,size_t size) const
{
  Element_ID element;

  switch (size)
  {
    case 1:
      element = *buffer;
      break;
    case sizeof(unsigned short):
      {
        unsigned short s;
        memcpy(&s,buffer,sizeof(unsigned short));
        element = s;
        break;
      }
    case sizeof(unsigned short)+1:
      element = 0;
      if (little_endian)
        memcpy(&element,buffer,sizeof(unsigned short) + 1);
      else
        memcpy((char *) (&element+1)-(sizeof(unsigned short)+1),buffer,sizeof(unsigned short)+1);
      break;
    case sizeof(Element_ID):
      memcpy(&element,buffer,sizeof(Element_ID));
      break;
    default:
      NOT_REACHED(element = 0;);
  }
  return element;
}

/**/

size_t Special_Subset::pack(Byte * buffer)
{
  /* key consists of 1 byte format selector + variable number of bytes.
     The first byte indicates how what follows is packed.
     This byte is constructed by adding together a format number, which
     is multiplied by sizeof(Element_ID) and a size indicator which is
     one of the four values:
     1, sizeof(unsigned short), sizeof(unsigned short)+1, sizeof(Element_ID)

     Formats numbers are as follows:
     0,1,2,3,4, : Set contains numbers followed by n byte bit masks indicating
                  which of the n* CHAR_BIT following elements are also present.
     5 : Set is a bit string starting from 0
     6 : Set is a bit string starting from the number packed next.
     7 : Set is a singleton
     8..61 : Set is a range of n-6 elements
     62 Set is a range of n elements with n the second packed number.
  */
  Byte * start = buffer;
  switch (format)
  {
    case SSF_Empty:
      return 0;
    case SSF_Singleton:
    {
      size_t size = bytes_needed(first_member);
      buffer = pack_element(buffer+1,first_member,size);
      *start = 7*sizeof(Element_ID) + size;
      return size+1;
    }
    case SSF_All:
      last_member = owner.element_count()-1;
      /* fall through */
    case SSF_Range:
    {
      bool small_range = last_member-first_member < 62-7;
      size_t size = small_range ? bytes_needed(first_member) : bytes_needed(last_member);
      buffer = pack_element(buffer+1,first_member,size);
      if (small_range)
      {
        *start = (last_member-first_member+7)*sizeof(Element_ID)+size;
        return size+1;
      }
      else
      {
        buffer = pack_element(buffer,last_member,size);
        *start = 62*sizeof(Element_ID)+size;
        return size*2+1;
      }
    }
    case SSF_Flagged:
    case SSF_Both:
    case SSF_List:
    {
      size_t size = bytes_needed(last_member);
      *start = 0*sizeof(Element_ID)+size;
      buffer++;
      Element_Count count = nr_members;
      size_t bit_start = size_t(first_member/CHAR_BIT) <= size ? 0 :
                                             first_member/CHAR_BIT * CHAR_BIT;
      size_t bit_length = Bit_Array::byte_length(bit_start,last_member);
      size_t size_as_bit_string = bit_start==0 ? 1 + bit_length :
                                                 1 + size + bit_length;
      int best = size_as_bit_string <= count*size ? 5 : 0;
      /* There is no reason why the bit_string format will necessarily be the
         best when it is better than a simple list. However, in the case
         where it is it usually will be the best format, and working out the
         size of the others slows things down too much. */
      if (best != 5)
      {
        if (format == SSF_Flagged)
          set_fast_sequential_access(false);
        /* fall through */
        const Element_ID * elements = member_list.buffer();
        Element_Count squashes[6];
        Element_Count sid[5];
        squashes[1] = 1+size+1;
        squashes[2] = 1+size+2;
        squashes[3] = 1+size+3;
        squashes[4] = 1+size+4;
        Element_ID id = elements[0];
        sid[1] = sid[2] = sid[3] = sid[4] = id;
        for (Element_Count position = 1;position < count;position++)
        {
          id = elements[position];
          for (int i = 1;i < 5;i++)
            if (id - sid[i] > i*CHAR_BIT)
            {
              squashes[i] += size+i;
              sid[i] = id;
            }
        }
        squashes[0] = 1 + member_list.count()*size;
        for (int i = 1;i < 5;i++)
          if (squashes[i] < squashes[best])
            best = i;

        *start = best*sizeof(Element_ID)+size;
        for (Element_Count position = 0;position < count;)
        {
          Element_ID el = elements[position++];
          buffer = pack_element(buffer,el,size);
          for (int i = 0 ; i < best;i++)
            buffer[i] = 0;
          while (position < count && elements[position] - el <= best*CHAR_BIT)
          {
            int bit = elements[position] - el - 1;
            int byte = bit/CHAR_BIT;
            bit %= CHAR_BIT;
            buffer[byte] |= 1 << bit;
            position++;
          }
          buffer += best;
        }
        return buffer-start;
      }
      else
      {
        Bit_Array bits;
        Bit_Array * use_bits;
        size_t real_bit_start = bit_start;
        if (!has_flags())
        {
          const Element_ID * elements = member_list.buffer();
          bits.change_length(last_member-bit_start+1,BAIV_Zero);
          for (Element_Count position = 0;position < count;position++)
            bits.assign(elements[position]-bit_start,true);
          use_bits = &bits;
          real_bit_start = 0;
        }
        else
          use_bits = &member_flags;

        if (bit_start)
        {
          *start = 6*sizeof(Element_ID) + size;
          buffer = pack_element(buffer,bit_start,size);
        }
        else
          *start = 5*sizeof(Element_ID) + size;
        memcpy(buffer,use_bits->bit_string(real_bit_start),bit_length);
        return size_as_bit_string;
      }
    }
    default:
    NOT_REACHED(* (char *) 0 = 0;return 0;)
    break;
  }
}

/**/

void Special_Subset::unpack(const Byte * buffer,size_t buffer_size)
{
  empty();
  if (buffer_size)
  {
    const Byte * end = buffer+buffer_size;
    size_t el_size = *buffer % sizeof(Element_ID);
    int packed_format = *buffer / sizeof(Element_ID);
    if (el_size == 0)
    {
      el_size = sizeof(Element_ID);
      packed_format--;
    }
    buffer++;
    switch (packed_format)
    {
      default:
      {
        Element_ID el = unpack_element(buffer,el_size);
        if (packed_format == 7)
          include(el);
        else
          set_range_set(el,el+packed_format-7);
        break;
      }
      case 62:
      {
        Element_ID first = unpack_element(buffer,el_size);
        Element_ID second = unpack_element(buffer+el_size,el_size);
        set_range_set(first,second);
        break;
      }
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
      {
        Element_ID el = unpack_element(buffer,el_size);
        last_member = first_member = el;
        member_list.append_one(el);
        buffer += el_size;
        do
        {
          for (int i = 0;i < packed_format;i++)
          {
            unsigned char c = *buffer++;
            if (c)
              for (int j = 0; j < CHAR_BIT;)
                if (c & (1 << j++))
                  member_list.append_one(last_member = el + j);
            el += CHAR_BIT;
          }
          if (buffer < end)
          {
            el = unpack_element(buffer,el_size);
            member_list.append_one(last_member = el);
            buffer += el_size;
          }
        }
        while (buffer < end);
        format = SSF_List;
        nr_members = member_list.count();
        break;
      }
      case 5:
      case 6:
      {
        Element_ID offset = 0;
        if (packed_format == 6)
        {
          offset = unpack_element(buffer,el_size);
          buffer += el_size;
        }
        while (buffer < end)
        {
          if (*buffer)
            for (int i = 0; i < CHAR_BIT;i++)
              if (*buffer & (1 << i))
                member_list.append_one(offset+i);
          buffer++;
          offset += CHAR_BIT;
        }
        format = SSF_List;
        nr_members = member_list.count();
        {
          Element_List::Iterator eli(member_list);
          last_member = eli.last();
          first_member = eli.first();
        }
      }
    }
  }
}

/**/

Element_ID Special_Subset_Iterator::item(Special_Subset & ss,bool first)
{
  if (first)
  {
    if (ss.format == SSF_All)
    {
      ss.first_member = ss.owner.first_valid_element();
      ss.last_member = ss.owner.element_count()-1;
      ss.nr_members = ss.last_member-ss.first_member+1;
    }
    if (ss.format == SSF_Flagged)
      ss.set_fast_sequential_access(true);
    if (ss.has_list())
      position = 0;
    else
      position = ss.first_member;
  }
  if (ss.has_list())
  {
    if (position < ss.member_list.count())
      return ss.member_list.elements[position++];
    return 0;
  }
  for (;position <= ss.last_member;position++)
    if (ss.contains(position))
      return position++;
  return 0;
}
