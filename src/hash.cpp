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


// $Log: hash.cpp $
// Revision 1.5  2010/06/10 13:57:22Z  Alun
// All tabs removed again
// Revision 1.4  2010/03/19 08:20:52Z  Alun
// clean() method added to stop reinitialisation in destructor
// Revision 1.3  2009/09/12 18:47:38Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.2  2008/11/02 20:04:00Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.3  2008/11/02 21:04:00Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.2  2008/10/10 07:47:56Z  Alun
// quantisation of sizes added to improve CPU cache utilisation
// Revision 1.1  2008/09/30 10:16:54Z  Alun
// New file.
//

#include <memory.h>
#include <limits.h>
#include "awdefs.h"
#include "hash.h"
#include "mafbase.h"

#ifdef _MSC_VER
#pragma warning(disable:4514) // removal of unused inline function
#endif

static unsigned long better_hash_key(const unsigned char * d,size_t length)
{
  const unsigned long POLYNOMIAL = 0x04c11db7L; // This is the AUTODIN-II/Ethernet polynomial */
  unsigned long answer = ~0UL;
  static unsigned long table[256];
  static bool done = false;

  if (!done)
  {
    size_t i,j;
    for (i = 0;i < 256;i++)
    {
      unsigned long v = i << 24;
      for (j = 0; j < 8;j++)
        if (v & 0x80000000)
          v = (v << 1) ^ POLYNOMIAL;
        else
          v <<= 1;
      table[i] = v;
    }
    done = 1;
  }

  while (length--)
  {
    unsigned long uch = *d++;

#if (ULONG_MAX > 0xffffffffL)
    uch ^= (answer >> 24) & 255;
#else
    uch ^= (answer >> 24);
#endif
    answer = (answer << 8) ^ table[uch];
  }
#if (ULONG_MAX > 0xfffffffful)
  return answer & 0xfffffffful;
#else
  return answer;
#endif
}

static unsigned long hash_key(const unsigned char * data,size_t ksize)
{
#if UCHAR_MAX==255
  if (ksize < 2*sizeof(long))
    return better_hash_key(data,ksize);
#endif
  unsigned long answer = 0;
  if (((long) data & (sizeof(long) - 1))==0)
  {
    unsigned long * d = (unsigned long *) data;
    while (ksize >= sizeof(unsigned long))
    {
      answer = answer*259 + *d++;
      ksize -= sizeof(unsigned long);
    }
    if (ksize)
    {
      unsigned long l = 0;
      memcpy(&l,d,ksize);
      answer = answer*259 + l;
    }
  }
  else
  {
    unsigned long l;
    while (ksize >= sizeof(unsigned long))
    {
      memcpy(&l,data,sizeof(unsigned long));
      answer = answer*259 + l;
      ksize -= sizeof(unsigned long);
      data += sizeof(long);
    }
    if (ksize)
    {
      l = 0;
      memcpy(&l,data,ksize);
      answer = answer*259 + l;
    }
  }
  return answer;
}

Hash::Hash(Element_Count hash_size_,size_t key_size,Element_Count maximum_size_) :
  fixed_key_size(key_size),
  hash_size(hash_size_),
  maximum_size(maximum_size_)
{
  direct_keys = key_size && key_size <= sizeof(unsigned char *);
  arrays.add(key,false);
  arrays.add(hash_next,false);
  if (!fixed_key_size)
    arrays.add(this->key_size,false);
  initialise(hash_size);
}

/**/

void Hash::initialise(Element_Count new_hash_size)
{
  /* "quantise" the hash size, so that repeated passes of minimise() will
     tend to get allocated the same areas of memory */
  if (maximum_size && new_hash_size > maximum_size)
    new_hash_size = maximum_size;
  if (new_hash_size > 0)
    hash_size = unsigned(new_hash_size + 1024) & ~1023u;
  if (!(hash_size & 3))
    hash_size += 3;
  if (hash_size < 2039)
    hash_size = 2039;

  hash_first.resize(hash_size,false,INVALID_ID);
  nr_allocated = 0;
  nr_entries = 0;
  arrays.set_capacity(0);
}

/**/

void Hash::empty(Element_Count new_hash_size)
{
  if (!direct_keys && key.capacity())
    for (Element_ID i = 0;i < nr_entries;i++)
      if (key[i])
        delete [] key[i];
  initialise(new_hash_size);
}

/**/

void Hash::clean()
{
//  printf("Total_key size was %zu\n",total_key_size());
  if (!direct_keys && key.capacity())
    for (Element_ID i = 0;i < nr_entries;i++)
      if (key[i])
        delete [] key[i];
  key.empty();
  hash_next.empty();
  key_size.empty();
  hash_first.empty();
}

/**/

Hash::~Hash()
{
  clean();
  arrays.erase();
}

void Hash::grow()
{
  /* Called when we need to increase the maximum number of records in the
     database */
  if (!nr_allocated)
    nr_allocated = hash_size;
  else if (nr_allocated < 1024*1024)
  {
    if (nr_allocated <= 1024*(512-64))
      nr_allocated *= 2;
    else
      nr_allocated = 1024*1024;
  }
  else
    nr_allocated += 65536;

  if (maximum_size > 0 && nr_allocated > maximum_size)
  {
    nr_allocated = maximum_size;
    if (nr_allocated <= nr_entries)
    {
      nr_allocated = (nr_entries + 256) & ~255;
      maximum_size = 0;
    }
  }

  arrays.set_capacity(nr_allocated);
}

/**/

void Hash::rehash()
{
  /* Hash table is getting full, so increase its size */
  hash_size = min(Element_Count(2*nr_allocated+1),Element_Count(3*nr_entries));
  hash_first.resize(hash_size,false,INVALID_ID);
  if (!direct_keys)
  {
    for (Element_ID v = 0; v < nr_entries;v++)
      if (this->key[v])
      {
        unsigned h = hash_key(this->key[v],fixed_key_size ?
                              fixed_key_size : this->key_size[v]) % hash_size;
        hash_next[v] = hash_first[h];
        hash_first[h] = v;
      }
  }
  else
  {
    for (Element_ID v = 0; v < nr_entries;v++)
    {
      unsigned h = hash_key((const unsigned char *) &this->key[v],fixed_key_size) % hash_size;
      hash_next[v] = hash_first[h];
      hash_first[h] = v;
    }
  }
}

/**/

bool Hash::find(const void * key,size_t key_size,Element_ID * pid) const
{
  bool retcode = false;

  if (valid_key(&key_size))
  {
    Element_ID id;
    unsigned hash;
    retcode = lookup(&id,&hash,key,key_size);
    if (pid)
      *pid = id;
  }
  return retcode;
}

/**/

int Hash::insert_common(const void * key,size_t key_size,Element_ID * pid,bool insert_new,bool take)
{
  int retcode = 0;

  if (valid_key(&key_size))
  {
    Element_ID id;
    unsigned hash;
    if (!lookup(&id,&hash,key,key_size) && insert_new)
    {
      if (nr_entries >= nr_allocated)
        grow();
      if ((Element_Count) hash_size < nr_entries)
      {
        rehash();
        hash = hash_key((const unsigned char *) key,key_size) % hash_size;
      }
      id = nr_entries++;
      insert_key(id,key,key_size,hash,take);
      retcode = 1;
    }
    if (pid)
      *pid = id;
  }
  else
    retcode = -1;
  return retcode;
}

/**/

bool Hash::lookup(Element_ID * id,unsigned * hash,
                  const void *key,size_t key_size) const
{
  unsigned h = hash_key((const unsigned char *) key,key_size) % hash_size;
  Element_ID v = hash_first[h];
  bool found = false;

  if (direct_keys)
    while (v < nr_entries)
    {
      if (memcmp(key,&this->key[v],key_size)==0)
      {
        found = true;
        break;
      }
      v = hash_next[v];
    }
  else
    while (v < nr_entries)
    {
      if ((fixed_key_size || this->key_size[v]==key_size) &&
          memcmp(key,this->key[v],key_size)==0)
      {
        found = true;
        break;
      }
      v = hash_next[v];
    }
  *hash = h;
  *id = found ? v : INVALID_ID;
  return found;
}

/**/

void Hash::insert_key(Element_ID id,const void * key,size_t key_size,
                      unsigned hash, bool take)
{
  hash_next[id] = hash_first[hash];

  /* Note that here and elsewhere special handling is required for a key of
     size 0. In this case unpack routines will expect to be passed a null
     pointer, and we may well have been passed one too.
     But Hash needs to allocate a non-null key pointer,
     as this is the only way to be sure the entry exists.
     If we allow a null key into the database, remove_key() goes wrong, and
     so does rehashing.
  */

  if (direct_keys)
    memcpy(&this->key[id],key,key_size);
  else if (!take || !key)
  {
    this->key[id] = new unsigned char[key_size];
    memcpy(this->key[id],key,key_size);
  }
  else
    this->key[id] = (unsigned char *) key;

  if (!fixed_key_size)
    this->key_size[id] = key_size;
  hash_first[hash] = id;
}

/**/

bool Hash::remove_key(Element_ID sequence)
{
  if (sequence < nr_entries && key.capacity())
  {
    if (direct_keys)
    {
      /* If entries are being deleted we cannot tell whether a zero entry is
         a removed entry or a zero key. So if user deletes entries we stop
         using direct mode and waste some memory */
      direct_keys = false;
      for (Element_ID i = 0; i < nr_entries;i++)
      {
        unsigned char * save = key[i];
        key[i] = new unsigned char[fixed_key_size];
        memcpy(key[i],&save,fixed_key_size);
      }
    }
    if (key[sequence])
    {
      size_t length = fixed_key_size ? fixed_key_size : key_size[sequence];
      unsigned h = hash_key(key[sequence],length) % hash_size;
      Element_ID v = hash_first[h];
      if (v == sequence)
        hash_first[h] = hash_next[sequence];
      else
      {
        while (hash_next[v] != sequence)
          v = hash_next[v];
        hash_next[v] = hash_next[sequence];
      }
      hash_next[sequence] = INVALID_ID;
      delete [] key[sequence];
      key[sequence] = 0;
      return true;
    }
  }
  return false;
}

/**/

void Hash::remove_entry(Element_ID sequence,bool reclaim)
{
  if (remove_key(sequence))
  {
    arrays.delete_item(sequence);
    hash_next[sequence] = INVALID_ID; // this gets set to 0 by arrays.delete_item
    if (reclaim && sequence+1 == nr_entries)
      nr_entries--;
  }
}

/**/

bool Hash::get_key(void * indicator,Element_ID sequence) const
{
  if (sequence < nr_entries && sequence < key.capacity())
    if (direct_keys)
    {
      memcpy(indicator,&key[sequence],fixed_key_size);
      return true;
    }
    else if (key[sequence])
    {
      memcpy(indicator,key[sequence],fixed_key_size ? fixed_key_size : key_size[sequence]);
      return true;
    }
  return false;
}

/**/

bool Hash::bb_set_key(Element_ID sequence,Byte_Buffer & bb,size_t key_size)
{
  bool retcode = change_key(sequence,(const void *) bb,key_size,true);
  if (retcode)
    bb.take();
  return retcode;
}

/**/

bool Hash::change_key(Element_ID sequence,const void * new_key,size_t key_size,bool take)
{
  bool retcode = false;
  Element_ID id;
  unsigned hash;
  if (sequence < nr_entries && valid_key(&key_size) &&
      !lookup(&id,&hash,new_key,key_size))
  {
    remove_key(sequence);
    insert_key(sequence,new_key,key_size,hash,take);
    retcode = true;
  }
  return retcode;
}

/**/

void Hash::take(Hash & other)
{
  if (this != &other)
  {
    empty();
    hash_first.take(other.hash_first);
    hash_next.take(other.hash_next);
    key.take(other.key);
    key_size.take(other.key_size);
    nr_entries = other.nr_entries;
    nr_allocated = other.nr_allocated;
    fixed_key_size = other.fixed_key_size;
    hash_size = other.hash_size;
    other.nr_entries = 0;
    other.nr_allocated = 0;
    other.initialise(0);
  }
}
