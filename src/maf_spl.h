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
$Log: maf_spl.h $
Revision 1.5  2009/09/12 18:48:39Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.4  2008/10/07 19:23:10Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.4  2008/10/07 20:23:09Z  Alun
append() method added
Revision 1.3  2007/11/15 22:58:12Z  Alun
*/
#pragma once
#ifndef MAF_SPL_INCLUDED
#define MAF_SPL_INCLUDED 1
#ifndef AWDEFS_INCLUDED
#include "awdefs.h"
#endif
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

/* State_Pair_List is used to hold sorted lists of pairs of State_IDs in
   which no two pairs are the same. It is used by Multiplier::composite() to
   construct the keys of its states, and also in the construction of
   non shortlex word-acceptors.
*/

class State_Pair_List
{
  BLOCKED(State_Pair_List)
  public:
    class Iterator;
    friend class Iterator;
  protected:
    State_ID *states;
    State_Count nr_allocated;
    State_Count nr_used;
    bool dense;
    unsigned char pad_state;
  public:
    State_Pair_List( const Byte * packed_data = 0) :
      states(0),
      nr_allocated(0),
      nr_used(0)
    {
      unpack(packed_data);
    }
    virtual ~State_Pair_List()
    {
      if (states)
        delete [] states;
    }
    static bool all_dense(State_Count count1,State_Count count2);
    // Sets the size of the list.
    void allocate(State_Count new_length,bool keep = true);
    // append is intended for unpacking functions in which the data being
    // unpacked is already sorted. The caller must ensure the required
    // space is available.
    bool append(State_ID si1,State_ID si2)
    {
      states[nr_used*2] = si1;
      states[nr_used*2+1] = si2;
      nr_used++;
      return true;
    }
    void empty()
    {
      nr_used = 0;
      dense = true;
      pad_state = 0;
    }
    void get(State_ID * pair,State_Count position) const
    {
      pair[0] = states[position*2];
      pair[1] = states[position*2+1];
    }
    void take(State_Pair_List & other)
    {
      if (&other != this)
      {
        size_t was_allocated = nr_allocated;
        State_ID * was_states = states;
        nr_allocated = other.nr_allocated;
        states = other.states;
        nr_used = other.nr_used;
        dense = other.dense;
        other.nr_used = 0;
        other.nr_allocated = was_allocated;
        other.states = was_states;
        other.dense = true;
      }
    }
    /* reserve() ensure the buffer has room for at least total_needed
       pairs of items altogether. i.e. what is already in counts, you are
       not making room for total_needed new items */
    void reserve(State_Count total_needed,bool keep);
    /* insert() inserts one state into a sorted state list.
       the return code is true if the state is new and false if it
       was already present. */

    bool insert(State_ID si1,State_ID si2,bool all_dense);
    bool insert(const State_ID *pair,bool all_dense)
    {
      return insert(pair[0],pair[1],all_dense);
    }
    bool remove(State_ID si1,State_ID si2);
    bool remove(const State_ID *pair)
    {
      return remove(pair[0],pair[1]);
    }
    void set_left_padded()
    {
      pad_state = 1;
    }
    void set_right_padded()
    {
      pad_state = 2;
    }
    State_Count count() const
    {
      return nr_used;
    }
    bool is_left_padded() const
    {
      return pad_state == 1;
    }
    bool is_right_padded() const
    {
      return pad_state == 2;
    }
    PACKED_DATA packed_data(size_t * size = 0) const;
    void unpack(const Byte * packed_data);
    size_t size() const;
    class Iterator
    {
      private:
        State_Count position;
        bool fast_current(State_ID *pair)
        {
          spl.get(pair,position);
          return true;
        }
      public:
        const State_Pair_List &spl;
        Iterator(const State_Pair_List & spl_) :
          spl(spl_),
          position(0)
        {}
        bool current(State_ID *pair)
        {
          if (position < spl.count())
            return fast_current(pair);
          return false;
        }
        bool first(State_ID *pair)
        {
          position = 0;
          return position < spl.count() ? fast_current(pair) : false;
        }
        bool last(State_ID *pair)
        {
          position = spl.count();
          return previous(pair);
        }
        bool next(State_ID * pair)
        {
          return position + 1 < spl.count() ? (++position,fast_current(pair)) : false;
        }
        bool previous(State_ID * pair)
        {
          return position > 0 ? (--position,fast_current(pair)) : false;
        }
    };
};

PACKED_CLASS(Packed_State_Pair_List,State_Pair_List);


#endif

