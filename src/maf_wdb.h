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
$Log: maf_wdb.h $
Revision 1.7  2009/09/12 18:48:41Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.6  2008/11/21 15:40:52Z  Alun
Should not included stdio.h
Revision 1.5  2008/10/22 08:52:09Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.4  2008/09/29 20:31:44Z  Alun
Switch to using Hash+Collection_Manager as replacement for Indexer.
Currently this is about 10% slower, but this is a more flexible
architecture.
Revision 1.2  2007/10/24 21:15:37Z  Alun
*/
#pragma once
#ifndef MAF_WDB_INCLUDED
#define MAF_WDB_INCLUDED 1

#ifndef HASH_INCLUDED
#include "hash.h"
#endif
#ifndef MAFWORD_INCLUDED
#include "mafword.h"
#endif

/**/

class Word_DB : public Word_Collection,public Hash
{
  public:
    mutable Ordinal_Word xlat;
    Word_DB(const Alphabet & alphabet_,Element_Count hash_size) :
      Word_Collection(alphabet_),
      xlat(alphabet_),
      Hash(hash_size,0)
    {
    }
    void add(const Word & word)
    {
      insert(word);
    }
    void empty()
    {
      Hash::empty();
    }
    Element_ID enter(const Word & word)
    {
      size_t size;
      Extra_Packed_Word pw(proper_word(word),&size);
      return bb_find_entry(pw,size);
    }
    bool insert(const Word & word,Element_ID * id=0)
    {
      size_t size;
      Extra_Packed_Word pw(proper_word(word),&size);
      if (id)
        *id = bb_find_entry(pw,size);
      else
        bb_find_entry(pw,size);
      return pw==0;
    }
    bool find(const Word & word,Element_ID * word_nr) const
    {
      size_t size;
      Extra_Packed_Word pw(proper_word(word),&size);
      Element_ID id;
      bool retcode = Hash::find(pw,size,&id);
      if (word_nr)
        *word_nr = id;
      return retcode;
    }
    bool get(Word * word,Element_ID word_nr) const
    {
      const void * p = get_key(word_nr);
      if (word->alphabet() == alphabet)
        word->extra_unpack((const Byte *) p);
      else
      {
        xlat.extra_unpack((const Byte *) p);
        *word = xlat;
      }
      return p!=0;
    }
    Element_Count count() const
    {
      return Hash::count();
    }
  private:
    const Word & proper_word(const Word & word) const
    {
      if (word.alphabet() == alphabet)
        return word;
      xlat = word;
      return xlat;
    }
};

class Equation_DB : public Word_DB
{
  private:
    Array_Of_Data packed_rhses;
  public:
    Equation_DB(const Alphabet & alphabet_,Element_Count hash_size) :
      Word_DB(alphabet_,hash_size)
    {
      manage(packed_rhses);
    }
    bool get_lhs(Word * word,Element_ID eqn_nr) const
    {
      return Word_DB::get(word,eqn_nr);
    }
    bool get_rhs(Word * word,Element_ID eqn_nr) const
    {
      const void * p = packed_rhses.get(eqn_nr);
      word->extra_unpack((const Byte *) p);
      return p!=0;
    }
    void update_rhs(Element_ID eqn_nr,const Word & word)
    {
      Extra_Packed_Word pw(word);
      packed_rhses[eqn_nr] = pw;
    }
};

/**/

class Word_List_DB : public Hash
{
  public:
    Word_List_DB(Element_Count hash_size,bool insert_null = true) :
      Hash(hash_size,0)
    {
      if (insert_null)
        Hash::find_entry(0,0);
    }
    bool get(Word_List * wl,Element_ID id) const
    {
      const void * p = get_key(id);
      wl->unpack((const Byte *) p);
      return p!=0;
    }
    Element_ID find_entry(const Word_List & wl,bool insert = true)
    {
      size_t size;
      Packed_Word_List pwl(wl,&size);
      return Hash::bb_find_entry(pwl,size,insert);
    }
    bool insert(Element_ID * id,const Word_List & wl)
    {
      size_t size;
      Packed_Word_List pwl(wl,&size);
      return bb_insert(pwl,size,true,id) > 0;
    }

    bool insert(const Word_List & wl)
    {
      Element_ID id;
      return insert(&id,wl);
    }

    bool contains(const Word_List & wl)
    {
      size_t size;
      Packed_Word_List pwl(wl,&size);
      return Hash::find(pwl,size);
    }

    bool update(Element_ID id,const Word_List & wl)
    {
      size_t size;
      Packed_Word_List pwl(wl,&size);
      return bb_set_key(id,pwl,size);
    }
};

#endif

