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
$Log: lowindex.h $
Revision 1.1  2010/05/18 03:49:52Z  Alun
New file.
*/
#pragma once
#ifndef LOWINDEX_INCLUDED
#define LOWINDEX 1

#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

// classes referred to but defined elsewhere
class Low_Index_Subgroup_Finder;
class Word_Collection;
class MAF;
class FSA;

class Subgroup_Iterator
{
  private:
    Low_Index_Subgroup_Finder & finder;
  public:
    Subgroup_Iterator(MAF &maf_,Element_Count start_index_,
                              Element_Count stop_index_);
    ~Subgroup_Iterator();
    const FSA_Simple * first(Word_Collection *subgroup_generators);
    const FSA_Simple * next(Word_Collection *subgroup_generators);
};

#endif
