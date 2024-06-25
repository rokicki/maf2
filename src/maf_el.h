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
$Log: maf_el.h $
Revision 1.5  2010/03/17 08:54:19Z  Alun
append_one_grow() method added. This is not actually being used anywhere
yet, but probably should be.
Revision 1.4  2009/09/12 18:48:34Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.3  2008/10/14 00:02:54Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.4  2008/10/14 01:02:53Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.3  2008/10/10 08:44:32Z  Alun_Williams
new methods added. reserve() only called when necessary
Revision 1.2  2007/11/15 22:58:12Z  Alun
*/
#pragma once
#ifndef MAF_EL_INCLUDED
#define MAF_EL_INCLUDED 1
#ifndef AWDEFS_INCLUDED
#include "awdefs.h"
#endif
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

/* Element_List is used to hold lists of Element_IDs. It can be used in various
   ways.
   In the first place the FSA_Reader class uses the derived class
   Validated_Element_List when building up the list of initial and accepting
   states. In this case states are always added with append_range() in order
   to ensure that the input is valid. This derived class also tries to avoid
   allocating any data if the set is the largest possible.
   Subset_Packer can unpack into a Element_List instance instead of a
   characteristic set, because it may well be more useful to the caller to
   have information in this format. Subset_Packer needs to add items to the
   list one at a time. In this case we will probably be hoping for good
   performance, so it uses append_one(), which does no validation.
   Finally the insert() method is provided for the case where the elements are
   not added in sequence, but we do need to make sure the list is sorted and
   has no duplicates.

   Generally speaking only one of append_one(),append_range(), or insert()
   should be used with any one instance of the class. However it is OK to
   use append_one() to apply a terminator value (usually 0) to a Element_List
   which is otherwise sorted.

   The find() and remove() methods only work on Element_Lists that are sorted.
   Therefore either the items must be added in order, or else the items
   must be added with insert()
*/

class Element_List
{
  BLOCKED(Element_List)
  public:
    class Iterator;
    friend class Iterator;
    friend class Special_Subset_Iterator;
  protected:
    Element_ID *elements;
    Element_Count nr_allocated;
    Element_Count nr_used;
  public:
    Element_List() :
      elements(0),
      nr_allocated(0),
      nr_used(0)
    {}
    virtual ~Element_List()
    {
      if (elements)
        delete [] elements;
    }
    // Sets the size of the list.
    void allocate(Element_Count new_length,bool keep = true);
    void append_one(Element_ID element)
    {
      /* Adds one new element to the end of a Element_List */
      if (nr_used == nr_allocated)
        reserve(nr_used+1,true);
      elements[nr_used++] = element;
    }
    void append_one_grow(Element_ID element)
    {
      /* Adds one new element to the end of a Element_List */
      if (nr_used == nr_allocated)
        reserve((nr_used+nr_used/8+256) & ~255,true);
      elements[nr_used++] = element;
    }
    // Get direct access to the elements buffer. The caller must
    // first ensure the buffer is large enough for the intended action
    Element_ID * buffer()
    {
      return elements;
    }
    const Element_ID * buffer() const
    {
      return elements;
    }
    void empty()
    {
      nr_used = 0;
    }
    void merge(const Element_List & other,Element_Count max_count = 0);
    void take(Element_List & other)
    {
      if (&other != this)
      {
        if (elements)
          delete [] elements;
        nr_allocated = other.nr_allocated;
        elements = other.elements;
        nr_used = other.nr_used;
        other.nr_used = other.nr_allocated = 0;
        other.elements = 0;
      }
    }
    /* reserve() ensure the buffer has room for at least total_needed
       items altogether. i.e. what is already in counts, you are not
       making room for total_needed new items */
    void reserve(Element_Count total_needed,bool keep);
    /* insert() inserts one element into a sorted element list.
       the return code is true if the element is new and false if it
       was already present. */
    bool insert(Element_ID element);
    bool pop(Element_ID * element)
    {
      if (!nr_used)
        return false;
      *element = elements[--nr_used];
      return true;
    }
    bool remove(Element_ID element);
    Element_Count capacity() const
    {
      return nr_allocated;
    }
    Element_Count count() const
    {
      return nr_used;
    }
    bool find(Element_ID element,Element_Count * position = 0) const;
    bool contains(Element_ID element) const
    {
      return find(element);
    }
    PACKED_DATA packed_data(size_t * size = 0) const
    {
      Element_ID * buffer = new Element_ID[nr_used+1];
      buffer[0] = nr_used;
      for (Element_Count i = 0; i < nr_used;i++)
        buffer[i+1] = elements[i];
      if (size)
        *size = (nr_used+1)*sizeof(Element_ID);
      return buffer;
    }
    void unpack(const Byte * packed_data)
    {
      Element_ID * buffer = (Element_ID *) packed_data;
      allocate(*buffer,false);
      for (Element_Count i = 0; i < *buffer;i++)
        elements[nr_used++] = buffer[i+1];
    }
    size_t size() const
    {
      return nr_used * sizeof(Element_ID);
    }
    class Iterator
    {
      private:
        Element_Count position;
      public:
        const Element_List &el;
        Iterator(const Element_List & el_) :
          el(el_),
          position(0)
        {}
        Element_ID first()
        {
          position = 0;
          return el.nr_used ? el.elements[0] : 0;
        }
        Element_ID last()
        {
          position = el.nr_used;
          return position > 0 ? el.elements[--position] : 0;
        }
        Element_ID current()
        {
          return position < el.nr_used ? el.elements[position] : 0;
        }
        Element_ID next()
        {
          return position+1 < el.nr_used ? el.elements[++position] : 0;
        }
        Element_ID previous()
        {
          return position > 0 ? el.elements[--position] : 0;
        }
    };
};

PACKED_CLASS(Packed_Element_List,Element_List);

class Validated_Element_List : public Element_List
{
  private:
    Element_Count max_count;
    Element_ID valid_low;
    Element_ID valid_high;
    bool validate_low;
    bool validate_high;
  public:
    Validated_Element_List() :
      max_count(0),
      validate_low(false),
      validate_high(false)
    {}
    // Add a range of elements to a list. Validation is performed
    void append_range(Element_ID low,Element_ID high);
    void set_valid_low(Element_ID valid_low_,bool validate = true)
    {
      valid_low = valid_low_;
      validate_low = validate;
    }
    void set_valid_high(Element_ID valid_high_,bool validate = true)
    {
      valid_high = valid_high_;
      validate_high = validate;
    }
    void set_valid_range(Element_ID valid_low,Element_ID valid_high)
    {
      set_valid_low(valid_low);
      set_valid_high(valid_high);
      max_count = valid_low <= valid_high ? valid_high - valid_low + 1 : 0;
    }
    bool full() const
    {
      return count() == max_count;
    }
};

typedef Element_List State_List;
typedef Validated_Element_List Validated_State_List;
typedef Packed_Element_List Packed_State_List;

#endif

