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
$Log: hash.h $
Revision 1.4  2010/05/17 07:04:53Z  Alun
INVALID_ID added. take_ownership() parameter added to manage() method
Revision 1.3  2009/09/12 18:48:27Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.2  2008/10/05 21:41:34Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.2  2008/10/05 22:41:33Z  Alun
more inlining
Revision 1.1  2008/09/30 10:16:58Z  Alun
New file.
*/
#ifndef HASH_INCLUDED
#pragma once
#define HASH_INCLUDED 1

#ifndef ARRAYBOX_INCLUDED
#include "arraybox.h"
#endif

/*
Hash is a class for creating an injective function with an arbitrary domain
and codomain a subset of the whole numbers. In other words it creates a unique
hash from arbitrary byte patters to 0..n for some n.

Effectively it allows you to create arrays indexed by any type whatsoever.
On its own the class is not always that useful. However, you can create
any number of auxiliary arrays that implement Unknown_Collection and ask a Hash
instance to manage them. This makes it easy to manage big collections of
related data items.

Formerly MAF used to have a class Indexer which did something similar.
However, that was very inflexible in the types of data it could manage. Using
the combination of Hash+Unknown_Collection arrays you can effectively change
data types dynamically by adding and removing arrays of extra information
at will.
*/

class Byte_Buffer;

// id will be set to this for an unsuccessful lookup
const Element_ID INVALID_ID = MAX_STATES;

class Hash
{
  private:
    Array_Of<Element_ID> hash_first; /* size hash_size */
    Collection_Manager arrays; /* arrays manages at least the next
                                         three arrays */
    Array_Of<Element_ID> hash_next;
    Array_Of<size_t> key_size;
    Array_Of<unsigned char *> key;
    Element_Count nr_entries;
    Element_Count nr_allocated;
    size_t fixed_key_size;
    unsigned hash_size;
    Element_Count maximum_size;
    bool direct_keys;
  public:
    Hash(Element_Count hash_size_,size_t key_size,Element_Count maximum_size_ = 0);
    virtual ~Hash();

    Element_Count count() const
    {
      return nr_entries;
    }
    void clean();
    void empty(Element_Count new_hash_size = 0);
    void manage(Unknown_Collection &collection,bool take_ownership = false)
    {
      arrays.add(collection,take_ownership);
    }
    void take(Hash & other);

    /* Item related functions */
    bool find(const void * key,size_t key_size,Element_ID * sequence=0) const;

    /* insert functions return 1 if item inserted,
        0 if already present, -1 on error
       (which means parameters are invalid) */
    int insert(const void * key,size_t key_size,Element_ID * sequence=0)
    {
      return insert_common(key,key_size,sequence,true,false);
    }
    int bb_insert(Byte_Buffer &key,size_t key_size,bool take = true,
                  Element_ID * sequence = 0)
    {
      int retcode = insert_common(key.look(),key_size,sequence,true,take);
      if (take && retcode == 1)
        key.take();
      return retcode;
    }

    // methods for compatibility with Indexer
    Element_ID find_entry(const void *key,size_t key_size,
                           bool insert_new = true)
    {
      Element_ID answer;
      insert_common(key,key_size,&answer,insert_new,false);
      return answer;
    }

    State_ID bb_find_entry(Byte_Buffer &key,size_t key_size,
                           bool insert_new = true,
                           bool take = true)
    {
      Element_ID id;
      int retcode = insert_common(key.look(),key_size,&id,insert_new,take);
      if (take && retcode == 1)
        key.take();
      return id;
    }

    bool get_key(void * key,Element_ID sequence) const;
    // The next function must not be called in between a clean() and an empty()
    // and must not be called with an invalid sequence number
    const void * get_key(Element_ID sequence) const
    {
      if (!direct_keys)
        return key[sequence];
      return &key[sequence];
    }
    // The next function must only be called if you specified 0 as the key size
    //  in the constructor, and is not valid between calls to clean() and empty()
    size_t get_key_size(Element_ID sequence) const
    {
      return key_size[sequence];
    }

    void remove_entry(Element_ID sequence,bool reclaim = true);
    bool set_key(Element_ID sequence,const void * new_key,size_t key_size)
    {
      return change_key(sequence,new_key,key_size,false);
    }
    bool bb_set_key(Element_ID sequence,Byte_Buffer & new_key,size_t key_size);
    size_t total_key_size() const
    {
      size_t answer = 0;
      if (fixed_key_size)
        return count() * fixed_key_size;
      for (Element_ID i = 0;i < nr_entries;i++)
        answer += key_size[i];
      return answer;
    }
  private:
    void initialise(Element_Count new_hash_size);
    void grow();
    void rehash();

    int insert_common(const void * key,size_t key_size,Element_ID * id,
                      bool insert_new,bool take);
    bool valid_key(size_t *key_size) const
    {
      if (fixed_key_size)
      {
        if (*key_size == 0)
          *key_size = fixed_key_size;
        else if (*key_size != fixed_key_size)
          return false;
      }
      return true;
    }
    inline bool lookup(Element_ID * id,unsigned * hash,const void *key,size_t key_size) const;
    inline void insert_key(Element_ID sequence,const void * new_key,size_t key_size,
                            unsigned hash,bool take);

    bool change_key(Element_ID sequence,const void * new_key,size_t key_size,bool take);

    bool remove_key(Element_ID sequence);
};

#endif
