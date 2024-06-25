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


// $Log: maf_spl.cpp $
// Revision 1.6  2010/03/29 18:03:02Z  Alun
// Necessary casts added
// Revision 1.5  2009/09/12 18:47:52Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2008/11/03 17:52:00Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.5  2008/11/03 18:52:00Z  Alun
// Horrible bug fixed in unpack
// Revision 1.4  2008/10/07 20:21:30Z  Alun
// Check before calling reserve()
// Revision 1.3  2007/11/15 22:58:08Z  Alun
//

/* See maf_spl.h for explanation */
#include <string.h>
#include <limits.h>
#include "awcc.h"
#include "maf_spl.h"

void State_Pair_List::allocate(State_Count new_allocation,bool keep)
{
  if (!keep)
    nr_used = 0;
  else if (new_allocation < nr_used)
    new_allocation = nr_used;
  if (new_allocation != nr_allocated)
  {
    nr_allocated = new_allocation;
    State_ID * new_states = new State_ID[nr_allocated*2];
    for (State_Count i = 0; i < nr_used*2;i++)
      new_states[i] = states[i];
    if (states)
      delete [] states;
    states = new_states;
  }
}

/**/

void State_Pair_List::reserve(State_Count total_needed,bool keep)
{
  if (total_needed > nr_allocated)
  {
    total_needed = (total_needed + 31) & ~31;
    if (!states)
    {
      states = new State_ID[total_needed*2];
      nr_allocated = total_needed;
    }
    else
      allocate(total_needed,keep);
  }
}

/**/

bool State_Pair_List::insert(State_ID state1,State_ID state2,bool all_dense)
{
  bool retcode = true;
  if (nr_used == nr_allocated)
    reserve(nr_used + 1,true);

  State_Count low = 0;
  State_Count high = nr_used;

  while (low < high)
  {
    unsigned mid = low + (high-low)/2;
    if (state1 > states[mid*2] ||
        state1 == states[mid*2] && state2 > states[mid*2+1])
      low = mid+1;
    else
      high = mid;
  }

  if (low < nr_used)
  {
    if (state1 != states[low*2] || state2 != states[low*2+1])
    {
      memmove(states+low*2+2,states+low*2,
              (nr_used-low)*sizeof(states[0])*2);
      states[low*2] = state1;
      states[low*2+1] = state2;
      nr_used++;
    }
    else
      retcode = false;
  }
  else
  {
    states[nr_used*2] = state1;
    states[nr_used*2+1] = state2;
    nr_used++;
  }
  if (!all_dense && dense)
    dense = state1 <= USHRT_MAX && state2 <= USHRT_MAX;
  return retcode;
}

/**/

bool State_Pair_List::remove(State_ID state1,State_ID state2)
{
  bool retcode = false;

  State_Count low = 0;
  State_Count high = nr_used;
  while (low < high)
  {
    unsigned mid = low + (high-low)/2;
    if (state1 > states[mid*2] ||
        state1 == states[mid*2] && state2 > states[mid*2+1])
      low = mid+1;
    else
      high = mid;
  }

  if (low < nr_used && state1 == states[low*2] && state2 == states[low*2+1])
  {
    memmove(states+low*2,states+low*2+2,
              (nr_used-low-1)*sizeof(states[0])*2);
    nr_used--;
    retcode = true;
  }
  return retcode;
}

/**/

PACKED_DATA State_Pair_List::packed_data(size_t * size) const
{
  if (!nr_used)
  {
    /* using null for the empty list makes a truly astonishing difference
       to performance of composition, due to the fact that it crops up
       very frequently. By using null pointer we save heap churn */
    if (size)
      *size = 0;
    return 0;
  }
  if (dense)
  {
    int offset = (sizeof(State_ID)+sizeof(unsigned short)-1)/sizeof(unsigned short);
    unsigned short * buffer = new unsigned short[nr_used*2+offset];
    * (State_ID *) buffer = nr_used*8 + 4 + pad_state;
    unsigned short * packed_data = buffer + offset;
    for (State_Count i = 0; i < nr_used; i++)
    {
      *packed_data++ = (unsigned short) states[i*2];
      *packed_data++ = (unsigned short) states[i*2+1];
    }
    if (size)
      *size = (nr_used*2+offset)*sizeof(unsigned short);
    return buffer;
  }
  else
  {
    State_ID * buffer = new State_ID[nr_used*2+1];
    buffer[0] = nr_used*8 + pad_state;
    memcpy(buffer+1,states,nr_used*sizeof(State_ID)*2);
    if (size)
      *size = (nr_used*2+1)*sizeof(State_ID);
    return buffer;
  }
}

void State_Pair_List::unpack(const Byte * packed_data)
{
  State_ID * buffer = (State_ID *) packed_data;
  empty();
  if (buffer)
  {
    reserve(*buffer/8,false);
    nr_used = *buffer/8;
    pad_state = *buffer & 3;
    if (*buffer & 4)
    {
      int offset = (sizeof(State_ID)+sizeof(unsigned short)-1)/sizeof(unsigned short);
      unsigned short * packed_data = (unsigned short *) buffer+offset;
      for (State_Count i = 0; i < nr_used;i++)
      {
        states[2*i] = *packed_data++;
        states[2*i+1] = *packed_data++;
      }
    }
    else
    {
      memcpy(states,buffer+1,nr_used*sizeof(State_ID)*2);
      dense = false;
    }
  }
}

bool State_Pair_List::all_dense(State_Count count1,State_Count count2)
{
  return count1-1 <= USHRT_MAX && count2-1 <= USHRT_MAX;
}
