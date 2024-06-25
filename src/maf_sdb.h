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
$Log: maf_sdb.h $
Revision 1.2  2009/09/12 18:48:38Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.1  2009/08/23 16:45:08Z  Alun
New file.
*/
#pragma once
#ifndef MAF_SDB_INCLUDED
#define MAF_SDB_INCLUDED 1

#ifndef HASH_INCLUDED
#include "hash.h"
#endif
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

/**/

class String_DB : public Hash
{
  public:
    String_DB(Element_Count hash_size) :
      Hash(hash_size,0)
    {
    }
    void empty()
    {
      Hash::empty();
    }
    bool insert(String s,size_t length, Element_ID * id=0)
    {
      return Hash::insert(s,length,id)==1;
    }
    bool find(String s,size_t length, Element_ID * string_nr = 0) const
    {
      Element_ID id;
      bool retcode = Hash::find(s,length,&id);
      if (string_nr)
        *string_nr = id;
      return retcode;
    }
    Element_Count count() const
    {
      return Hash::count();
    }
};

#endif

