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


// $Log: maf_el.cpp $
// Revision 1.6  2010/06/10 13:57:36Z  Alun
// All tabs removed again
// Revision 1.5  2010/03/29 09:33:46Z  Alun
//
// size() method added for Packed_Element_List
// Revision 1.4  2009/09/12 18:47:49Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.3  2008/10/10 07:46:50Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.3  2008/10/10 08:46:49Z  Alun_Williams
// No longer uses binary search when inserting into a small list
// Revision 1.2  2007/11/15 22:58:08Z  Alun
//

/* See maf_el.h for explanation */
#include <string.h>
#include "awcc.h"
#include "maf_el.h"

void Element_List::allocate(Element_Count new_allocation,bool keep)
{
  if (!keep)
    nr_used = 0;
  else if (new_allocation < nr_used)
    new_allocation = nr_used;
  if (new_allocation != nr_allocated)
  {
    nr_allocated = new_allocation;
    Element_ID * new_elements = new Element_ID[nr_allocated];
    for (Element_Count i = 0; i < nr_used;i++)
      new_elements[i] = elements[i];
    if (elements)
      delete [] elements;
    elements = new_elements;
  }
}

/**/

void Element_List::reserve(Element_Count total_needed,bool keep)
{
  if (total_needed > nr_allocated)
  {
    total_needed = (total_needed + 31) & ~31;
    if (!elements)
    {
      elements = new Element_ID[total_needed];
      nr_allocated = total_needed;
    }
    else
      allocate(total_needed,keep);
  }
}

/**/

void Validated_Element_List::append_range(Element_ID low,Element_ID high)
{
  if (validate_low && low < valid_low)
    low = valid_low;
  if (validate_high && high > valid_high)
    high = valid_high;
  if (low <= high)
  {
    Element_Count needed = high - low + 1;
    if (needed != max_count)
    {
      reserve(nr_used+needed,true);
      Element_ID s;
      for (s = low;s <= high;s++)
        elements[nr_used++] = s;
    }
    else
      nr_used = max_count;
  }
}

/**/

bool Element_List::insert(Element_ID element)
{
  bool retcode = true;
  if (nr_used == nr_allocated)
    reserve(nr_used + 1,true);

  Element_Count low = 0;
  if (nr_used > 20)
  {
    Element_Count high = nr_used;
    while (low < high)
    {
      unsigned mid = low + (high-low)/2;
      if (element > elements[mid])
        low = mid+1;
      else
        high = mid;
    }
  }
  else
  {
    for (;low < nr_used;low++)
      if (elements[low] >= element)
        break;
  }

  if (low < nr_used)
  {
    if (element != elements[low])
    {
      // I expermented with inline code here, but memmove() is quicker for big
      // lists.
      memmove(elements+low+1,elements+low,(nr_used++-low)*sizeof(Element_ID));
      elements[low] = element;
    }
    else
      retcode = false;
  }
  else
    elements[nr_used++] = element;
  return retcode;
}

/**/

bool Element_List::remove(Element_ID element)
{
  bool retcode = false;

  Element_Count low = 0;
  Element_Count high = nr_used;
  while (low < high)
  {
    unsigned mid = low + (high-low)/2;
    if (element > elements[mid])
      low = mid+1;
    else
      high = mid;
  }

  if (low < nr_used && element == elements[low])
  {
    memmove(elements+low,elements+low+1,
            (nr_used-low-1)*sizeof(elements[0]));
    nr_used--;
    retcode = true;
  }
  return retcode;
}

/**/

bool Element_List::find(Element_ID element,Element_Count * position) const
{
  Element_Count low = 0;
  Element_Count high = nr_used;

  while (low < high)
  {
    unsigned mid = low + (high-low)/2;
    if (element > elements[mid])
      low = mid+1;
    else
      high = mid;
  }

  if (low < nr_used && element == elements[low])
  {
    if (position)
      *position = low;
    return true;
  }
  return false;
}

/**/

void Element_List::merge(const Element_List &other,Element_Count max_count)
{
  Element_List temp;
  Element_Count new_count = count() + other.count();
  if (max_count && new_count > max_count)
    new_count = max_count;
  temp.reserve(new_count,false);

  Element_List::Iterator eli1(*this);
  Element_List::Iterator eli2(other);
  Element_ID e1 = eli1.first();
  Element_ID e2 = eli2.first();

  while (e1 && e2)
  {
    if (e1 < e2)
    {
      temp.append_one(e1);
      e1 = eli1.next();
    }
    else if (e2 < e1)
    {
      temp.append_one(e2);
      e2 = eli2.next();
    }
    else
    {
      temp.append_one(e1);
      e1 = eli1.next();
      e2 = eli2.next();
    }
  }
  while (e1)
  {
    temp.append_one(e1);
    e1 = eli1.next();
  }
  while (e2)
  {
    temp.append_one(e2);
    e2 = eli2.next();
  }
  take(temp);
}

size_t Packed_Element_List::size(const void * buffer)
{
  return buffer ? (* (Element_ID *) buffer + 1) * sizeof(Element_ID) : 0;
}
