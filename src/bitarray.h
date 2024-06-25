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
$Log: bitarray.h $
Revision 1.5  2010/03/19 10:34:17Z  Alun
Added opional keep_size parameter to empty() method
Revision 1.4  2009/09/12 18:48:24Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.3  2008/10/10 06:44:08Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.3  2008/10/10 07:44:08Z  Alun
Changes to improve performance
Revision 1.2  2008/09/30 08:27:59Z  Alun
"Mid Sep 2008 snapshot"
*/
#pragma once
#ifndef BITARRAY_INCLUDED
#define BITARRAY_INCLUDED 1
#include <limits.h>
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif
#ifndef AWDEFS_INCLUDED
#include "awdefs.h"
#endif

enum Bit_Array_Initial_Value
{
  BAIV_Zero,
  BAIV_One,
  BAIV_Uninitialised
};

class Bit_Array
{
  BLOCKED(Bit_Array)
  private:
    Byte * bits;
    unsigned long nr_valid_bits;
    unsigned long nr_allocated_bits;
  public:
    Bit_Array(unsigned long nr_bits_ = 0,
              Bit_Array_Initial_Value initial_value = BAIV_Zero) :
      nr_valid_bits(0),
      nr_allocated_bits(0),
      bits(0)
    {
      if (nr_bits_)
        change_length(nr_bits_,initial_value);
    }
    ~Bit_Array()
    {
      if (bits)
        delete [] bits;
    }
    void assign(unsigned long bit_nr,bool value)
    {
      if (value)
        bits[bit_nr/CHAR_BIT] |= (1 << (bit_nr % CHAR_BIT));
      else
        bits[bit_nr/CHAR_BIT] &= ~(1 << (bit_nr % CHAR_BIT));
    }
    /* The new initial_value is applied only to bits beyond the
       last previously valid bit. */
    void change_length(unsigned long new_nr_bits,
                      Bit_Array_Initial_Value initial_value = BAIV_Zero,
                      bool no_shorten = false);
    void empty()
    {
      change_length(0,BAIV_Zero);
    }
    void safe_assign(unsigned long bit_nr,bool value)
    {
      if (bit_nr >= nr_valid_bits)
      {
        change_length(bit_nr+1,BAIV_Zero,true);
        if (!value)
          return;
      }
      assign(bit_nr,value);
    }
    // Copies the other bit array to this one and then empties the other
    void take(Bit_Array & other);
    void zap(unsigned long bit_nr)
    {
      // clear all bits in the byte containing this bit
      bits[bit_nr/CHAR_BIT] = 0;
    }

    // const methods
    const Byte * bit_string(unsigned long bit_first) const
    {
      return bits+bit_first/CHAR_BIT;
    }
    bool contains(unsigned long bit_nr) const
    {
      /* synonym for get() more appropriate when BitArray is being used as
         a characterstic set */
      return safe_get(bit_nr);
    }
    bool get(unsigned long bit_nr) const
    {
      return bits[bit_nr/CHAR_BIT] & (1 << (bit_nr % CHAR_BIT)) ? true : false;
    }
    bool safe_get(unsigned long bit_nr) const
    {
      if (bit_nr < nr_valid_bits)
        return get(bit_nr);
      return false;
    }
    unsigned long length() const
    {
      return nr_valid_bits;
    }
    static size_t byte_length(unsigned long first,unsigned long last)
    {
      if (first <= last)
      {
        first /= CHAR_BIT;
        last /= CHAR_BIT;
        return last-first+1;
      }
      return 0;
    }
};

#endif
