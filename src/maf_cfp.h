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
$Log: maf_cfp.h $
Revision 1.6  2010/03/19 22:09:56Z  Alun
Changed unsigned char to Byte everywhere
Revision 1.5  2009/09/12 18:48:33Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.4  2008/10/07 19:19:20Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.4  2008/10/07 20:19:19Z  Alun
new methods added to improve performance.
Revision 1.3  2007/11/15 22:58:12Z  Alun
*/
#pragma once
#ifndef MAF_CFP_INCLUDED
#define MAF_CFP_INCLUDED 1

/* This header file defines various classes that are useful for
   packing characteristic functions, or other similar discrete functions,
   and also n-tuples of numbers into smaller amounts of memory than are
   needed for the most convenient representation.

   They are primarily intended for use in situations where potentially very
   large numbers of such objects need to be stored, but only a few are in
   use at any one time, and where most are sparse. Typically they are used
   during the construction of FSAs, but there is no reason why the code
   should not be reused for other purposes.

   Subset_Packer packs or unpacks a set of bool values that represent which
   elements of some larger set are present in a particular subset. In fact
   this class is no longer used. It only works well when either the universal
   set is quite small, or the subsets are rather large, since otherwise the
   key that is being packed needs to be tested in a for loop and zeroed for
   each new state, and the packed key is much larger than need be. It is
   usually better to use a key of Special_Subset type, and to use
   Special_Subset::Packer to pack it.

   Compared_Subset_Packer packs or unpacks a set of char values that
   represent which elements of some larger set are present in a
   particular subset, and also repesent whether each element is greater or
   less than some other unknown quantity (unknown to the packer, obviously
   not to what created the key). If the appropriate parameter is passed
   the packer can also remember equality. This increases the length of the
   packed key by 20%, so it is definitely worth catering for this case
   separately. Actually this class can be used to pack any array of
   char values that are all 0-2, or all 0-3 if the need_equal_state parameter
   is true. But the use described is the expected one.

   Sparse_Function_Packer packs or unpacks a set of State_ID values
   that represent a mapping from one discrete set to another. It can
   therefore also be used to represent a set of ordered pairs (e1,e2) where
   e1 belongs to one set and e2 to a second, provided we have at most one
   pair (e1,x) for any particular e1. Both Subset_Packer and
   Compared_Subset_Packer could be replaced by Sparse_Function_Packer.
   However they are important special cases, and the code for them is
   slightly simpler, and, presumably, faster.

   Both Compared_Subset_Packer and Sparse_Function_Packer suffer from the
   same defects as Subset_Packer.

   Pair_Packer packs an ordered pair of natural numbers, Triple_Packer,
   an ordered triple,Quad_Packer, an ordered quadruple. For larger tuples
   Sparse_Function_Packer can be used, though it may not be very effective.
*/

#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

// Classes referred to but defined elsewhere
class Element_List;
class State_Pair_List;

class Subset_Packer
{
  private:
    State_Count nr_elements;
    unsigned first_element;
    int keys_per_byte;
    size_t bytes_needed;
    Byte * search;
#ifdef DEBUG_PACKER
    Byte * test;
#endif
  public:
    Subset_Packer(unsigned nr_keys_,unsigned first_element = 0);
    ~Subset_Packer()
    {
      delete [] search;
#ifdef DEBUG_PACKER
      delete [] test;
#endif
    }
    void * get_buffer()
    {
      return search;
    }
    size_t pack(const Byte * key,State_Count last_element = 0);
    void unpack(Byte * key) const;
    void unpack(Element_List * sl) const;
};

class Compared_Subset_Packer
{
  private:
    State_Count nr_elements;
    unsigned first_element;
    int keys_per_byte;
    size_t bytes_needed;
    Byte * search;
    bool need_equal_state;
#ifdef DEBUG_PACKER
    char * test;
#endif
  public:
    size_t pack(const char * key)
    {
      return need_equal_state ? pack4(key) : pack3(key);
    }
    void unpack(char * key) const
    {
      if (need_equal_state)
        unpack4(key);
      else
        unpack3(key);
    }
    void unpack(State_Pair_List *spl) const;
    Compared_Subset_Packer(unsigned nr_keys_,unsigned first_element_ = 0,
                           bool need_equal_state_ = true);
    ~Compared_Subset_Packer()
    {
      delete [] search;
#ifdef DEBUG_PACKER
      delete [] test;
#endif
    }
    void * get_buffer()
    {
      return search;
    }
  private:
    size_t pack3(const char * key);
    size_t pack4(const char * key);
    void unpack3(char * key) const;
    void unpack4(char * key) const;
    void unpack3(State_Pair_List * spl) const;
    void unpack4(State_Pair_List * spl) const;
};

class Sparse_Function_Packer
{
  private:
    State_Count nr_elements;
    State_ID first_element;
    int bits_per_state;
    size_t bytes_needed;
    Byte * search;
#ifdef DEBUG_PACKER
    State_ID * test;
#endif
  public:
    Sparse_Function_Packer(State_Count domain_size,State_Count range_size,
                           State_ID first_element = 0);
    ~Sparse_Function_Packer()
    {
      delete [] search;
#ifdef DEBUG_PACKER
      delete [] test;
#endif
    }
    size_t pack(const State_ID * key);
    void unpack(State_ID * key) const;
    void * get_buffer()
    {
      return search;
    }
};

class Base_Tuple_Packer
{
  protected:
    bool big_endian;
    bool little_endian;
    const int bits_per_long;
    int correction;
    size_t size;
  public:
    Base_Tuple_Packer();
    size_t key_size() const
    {
      return size;
    }
    void adjust_size();
};

class Pair_Packer : public Base_Tuple_Packer
{
  private:

    union
    {
      unsigned long key[2];
      Byte data[1];
    } packed_key;
    size_t sizes[2];
    unsigned long masks[2];
    unsigned long factor[2];
    bool use_arithmetic_code;
#ifdef DEBUG_PACKER
    State_ID test[2];
#endif
  public:
    Pair_Packer(State_ID key[2]);
    void * pack_key(const State_ID * key);
    void * get_buffer()
    {
      return packed_key.data;
    }
    void unpack_key(State_ID * key) const;
};

class Triple_Packer : public Base_Tuple_Packer
{
  private:
    union
    {
      unsigned long key[3];
      Byte data[1];
    } packed_key;
    size_t sizes[3];
    unsigned long masks[3];
    unsigned long factor[3];
    bool use_arithmetic_code;
#ifdef DEBUG_PACKER
    State_ID test[3];
#endif
  public:
    Triple_Packer(State_ID key[3]);
    void * pack_key(const State_ID * key);
    void * get_buffer()
    {
      return packed_key.data;
    }
    void unpack_key(State_ID * key) const;
};

class Quad_Packer : public Base_Tuple_Packer
{
  private:
    union
    {
      unsigned long key[4];
      Byte data[1];
    } packed_key;
    size_t sizes[4];
    unsigned long masks[4];
    unsigned long factor[4];
#ifdef DEBUG_PACKER
    State_ID test[4];
#endif
  public:
    Quad_Packer(State_ID key[4]);
    void * pack_key(const State_ID * key);
    void * get_buffer()
    {
      return packed_key.data;
    }
    void unpack_key(State_ID * key) const;
};

#endif
