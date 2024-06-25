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
$Log: maf_ss.h $
Revision 1.6  2010/06/10 13:58:12Z  Alun
All tabs removed again
Revision 1.5  2010/03/19 10:53:24Z  Alun
Added expect_big_bitset() method as a hint  about the representation to be
used
Revision 1.4  2009/09/14 09:29:52Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.3  2008/10/07 15:29:14Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.3  2008/10/07 16:29:14Z  Alun
Packer class added. Changes to improve performance
Revision 1.2  2008/08/11 19:09:12Z  Alun
Added include/exclude methods
Revision 1.1  2007/10/24 21:09:02Z  Alun
New file.
*/
#pragma once
#ifndef MAF_SS_INCLUDED
#define MAF_SS_INCLUDED 1
#ifndef BITARRAY_INCLUDED
#include "bitarray.h"
#endif
#ifndef MAF_EL_INCLUDED
#include "maf_el.h"
#endif
#ifndef MAF_SSI_INCLUDED
#include "maf_ssi.h"
#endif

/*
  class Special_Subset is used for managing sets of accepting and
  initial states, or other interesting subsets of the set of states or set of
  labels of an FSA. It aims to allow this information to be stored in a
  format that is both space and time efficient.

  The format can be changed dynamically without altering the contents of
  the set.

  The "universal set" for Special_Subset is determined by the owner specified
  in the constructor, but is always a range of non-negative integers. The owner
  provides three methods which determine what this set is:

    element_count() should return a number 1 greater than the number of the last
    valid element. This allows Special_Subset to use the usual C style for
    array references/iterators. So arguably this method should be called
    end() or something similar.

    first_valid_element() returns the first number that is permitted to be
    in the set. This defaults to 1, not 0. In this case 0 can never be
    included in the set.

    The is_valid_element() function could in theory be used to create other
    "holes" in the range. However, this is not properly supported when
    setting a range set.

    is_valid_element() is only checked when adding or removing individual
    elements to the subset.

    the SSF_All set is the subrange first_valid..owner.element_count()-1.
*/

class Special_Subset
{
  friend class Special_Subset_Iterator;
  public:
    class Packer;
    friend class Packer;
    const Special_Subset_Owner & owner;
  private:
    Special_Subset_Format preferred_format;
    Special_Subset_Format format;
    Bit_Array member_flags;
    Element_List member_list;
    Element_ID first_member;
    mutable Element_ID last_member;// This is mutable because it gets updated
                                 // sometimes when format is SSF_All
    Element_Count nr_members;
    bool big_bitset_expected;
  public:
    /* On construction Special_Subsets are empty unless you pass SSF_All or
       SSF_Singleton as the format. In the latter case the set initially
       contains 1 as its only element */
    Special_Subset(const Special_Subset_Owner & owner_,Special_Subset_Format format_ = SSF_Empty);
    bool assign_membership(Element_ID element,bool is_member);
    void exclude(Element_ID element)
    {
      assign_membership(element,false);
    }
    void empty()
    {
      exclude_all();
    }
    void expect_big_bitset(bool flag = true)
    {
      /* called from FSA_Common::clear_accepting()/clear_initial()
         to indeicate whether the member_flags bitset is expected to
         be full length */
      big_bitset_expected = flag;
    }
    void exclude_all();
    void include(Element_ID element)
    {
      assign_membership(element,true);
    }
    void include(const Special_Subset & other);
    void include(const Element_List & other);
    void include_all();

    Element_Count count() const
    {
      return format == SSF_All ? owner.element_count()-owner.first_valid_element() : nr_members;
    }

    void renumber(const Element_ID * new_element_numbers);
    void set_fast_random_access(bool update_preferred_format = true);
    void set_fast_sequential_access(bool update_preferred_format = true);
    bool set_range_set(Element_ID first,Element_ID last);
    bool set_singleton_set(Element_ID ei)
    {
      return set_range_set(ei,ei);
    }
    bool shrink(bool check_bit_array);
    // take copies other to this and empties other with minimum of copying.
    // both sets must have the same owner or the copy fails.
    bool take(Special_Subset & other);
    // unpack() must only be called with a key previously created by a Packer.
    void unpack(const Byte * buffer,size_t buffer_size);

    bool contains(Element_ID element) const
    {
      switch (format)
      {
        case SSF_Empty:
          return false;
        case SSF_All:
          return owner.is_valid_element(element);
        case SSF_Singleton:
          return element == first_member;
        case SSF_Range:
          return element >= first_member && element <= last_member;
        case SSF_Both:
        case SSF_Flagged:
          return member_flags.safe_get(element);
        case SSF_List:
          if (element < first_member || element > last_member)
            return false;
          return member_list.find(element);
        default:
          NOT_REACHED(return false;)
      }
    }

    Element_ID first() const
    {
      return first_member;
    }

    bool is_empty() const
    {
      return nr_members == 0;
    }

    Element_ID last() const
    {
      return format != SSF_All ? last_member : owner.element_count()-1;
    }

    Special_Subset_Format get_format() const
    {
      return format;
    }
    void print(Output_Stream * stream) const;
  private:
    size_t pack(Byte * buffer);
    Byte * pack_element(Byte *buffer,Element_ID element,size_t size) const;
    Element_ID unpack_element(const Byte *buffer,size_t size) const;
    size_t bytes_needed(Element_ID element) const;

    bool has_list() const
    {
      return format == SSF_List || format == SSF_Both;
    }
    bool has_flags() const
    {
      return format == SSF_Flagged || format == SSF_Both;
    }
    Special_Subset_Format recommended_format() const
    {
      // This does not consider whether any of the special formats
      // are possible, but selects whichever will be smaller of
      // the two formats that can hold any subset.
      return nr_members *bytes_needed(last_member) >=
            (size_t) (last_member+CHAR_BIT)/CHAR_BIT ? SSF_Flagged : SSF_List;
    }
    void leave_range_format();
  public:
    class Packer
    {
      private:
        Byte * buffer;
      public:
        Packer(Special_Subset & ss)
        {
          size_t required = Bit_Array::byte_length(0,ss.owner.element_count())+1;
          buffer = new Byte[required];
        }
        ~Packer()
        {
          delete [] buffer;
        }
        size_t pack(Special_Subset & ss)
        {
          size_t retcode = ss.pack(buffer);
#if 0
          Special_Subset_Iterator ssi1,ssi2;
          Special_Subset ss2(ss.owner);
          ss2.unpack(buffer,retcode);
          bool first = true;
          for (;;)
          {
            State_ID si1 = ssi1.item(ss,first);
            State_ID si2 = ssi2.item(ss2,first);
            if (si1 != si2)
              * (char *) 0 = 0;
            else if (!si1 && !si2)
               break;
            first = false;
          }
#endif
          return retcode;
        }
        void * get_buffer()
        {
          return buffer;
        }
        const void * get_buffer() const
        {
          return buffer;
        }
     };
};

#endif

