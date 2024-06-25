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


// $Log: bitarray.cpp $
// Revision 1.2  2008/10/10 07:01:14Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.2  2008/10/10 08:01:13Z  Alun
// Changes to improve CPU cache utilisation
//

#include <string.h>
#include "awcc.h"
#include "bitarray.h"

void Bit_Array::take(Bit_Array & other)
{
  if (bits)
    delete [] bits;
  bits = other.bits;
  nr_valid_bits = other.nr_valid_bits;
  nr_allocated_bits = other.nr_allocated_bits;
  other.nr_valid_bits = 0;
  other.nr_allocated_bits = 0;
  other.bits = 0;
}

/**/

void Bit_Array::change_length(unsigned long new_nr_bits,
                              Bit_Array_Initial_Value initial_value,
                              bool no_shorten)
{
  size_t size = (new_nr_bits+CHAR_BIT-1)/CHAR_BIT;
  size_t old_size = nr_allocated_bits/CHAR_BIT;
  size_t old_valid = (nr_valid_bits+CHAR_BIT-1)/CHAR_BIT;
  size_t common = size < old_valid ? size : old_valid;

  if (no_shorten && size < old_size)
    size = old_size;

  if (size != old_size)
  {
    Byte * new_bits = size ? new Byte[size] : 0;
    memcpy(new_bits,bits,common);
    if (bits)
      delete [] bits;
    bits = new_bits;
  }

  if (initial_value != BAIV_Uninitialised && size > common)
    memset(bits+common,initial_value ? ~0 : 0,size-common);

  if (initial_value != BAIV_Uninitialised)
  {
    while (nr_valid_bits < common*CHAR_BIT && nr_valid_bits < new_nr_bits)
      assign(nr_valid_bits++,initial_value != BAIV_Zero);
    nr_valid_bits = new_nr_bits;
  }

  int i = new_nr_bits % CHAR_BIT;
  if (i != 0 && new_nr_bits && (size < old_valid || initial_value==BAIV_One))
  {
    size_t last_byte = (new_nr_bits-1)/CHAR_BIT;
    while (i < CHAR_BIT)
      bits[last_byte] &= ~(1 << i++);
  }

  nr_allocated_bits = size*CHAR_BIT;
}

