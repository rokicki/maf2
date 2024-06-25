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
$Log: maf_ssi.h $
Revision 1.3  2009/09/12 18:48:40Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.2  2008/10/07 17:47:10Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.2  2008/10/07 18:47:09Z  Alun
Changes to make Special_Subset for flexible
Revision 1.1  2007/10/24 21:09:00Z  Alun
New file.
*/
/* This header file defines various classes that are associated with the
Special_Subset type used to manage initial and accepting states, but not
Special_Subset itself, which does not need to be exposed so widely. */

#ifndef MAF_SSI_INCLUDED
#pragma once
#define MAF_SSI_INCLUDED 1

#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

/* classes referred to but defined elsewhere */
class Container;
class Special_Subset;

/* classes defined in this header file */
class Special_Subset_Iterator;
class Special_Subset_Owner;

enum Special_Subset_Format
{
  SSF_Empty,
  SSF_List,     //
  SSF_Flagged,  // These three are generic, the other can only hold
  SSF_Both,     // the special subsets of the obvious types.
  SSF_Singleton,
  SSF_Range,
  SSF_All
};

class Special_Subset_Iterator
{
  private:
    Element_Count position;
  public:
    Special_Subset_Iterator() :
      position(0)
    {}
    Element_ID item(Special_Subset & special_subset,bool first);
    Element_ID first(Special_Subset & ss)
    {
      return item(ss,true);
    }
    Element_ID next(Special_Subset & ss)
    {
      return item(ss,false);
    }
};

class Special_Subset_Owner
{
  public:
    Container & container;
    virtual Element_Count element_count() const = 0;
    virtual Element_ID first_valid_element() const
    {
      return 1;
    }
    virtual bool is_valid_element(Element_ID element) const
    {
      return element >= first_valid_element() && element < element_count();
    }
    Special_Subset_Owner(Container & container_) :
      container(container_)
    {}
    virtual ~Special_Subset_Owner() {};
};

class Simple_Finite_Set : public Special_Subset_Owner
{
  Element_Count nr_elements;
  public:
    virtual Element_Count element_count() const
    {
      return nr_elements;
    }
    Simple_Finite_Set(Container & container_,Element_Count nr_elements_) :
      Special_Subset_Owner(container_),
      nr_elements(nr_elements_)
    {}
};

#endif
