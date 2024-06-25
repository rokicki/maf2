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
$Log: keyedfsa.h $
Revision 1.7  2010/05/17 07:12:32Z  Alun
Added SUGGESTED_HASH_SIZE.  Implementation of find_state() has had to
change since INVALID_ID is not negative
Revision 1.6  2009/09/12 18:48:28Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.5  2008/10/16 08:41:14Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.6  2008/10/16 09:41:13Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.5  2008/09/30 22:21:31Z  Alun
more inlining
Revision 1.4  2008/09/29 23:20:38Z  Alun
Switch to using Hash+Collection_Manager as replacement for Indexer.
Currently this is about 10% slower, but this is a more flexible
architecture.
Revision 1.2  2007/10/24 21:15:36Z  Alun
*/

#ifndef KEYEDFSA_INCLUDED
#pragma once
#define KEYEDFSA_INCLUDED 1
#ifndef FSA_INCLUDED
#include "fsa.h"
#endif
#ifndef HASH_INCLUDED
#include "hash.h"
#endif

// Classes referred to and defined elsewhere
class Container;

/* The Keyed_FSA class is the class usually used to build FSAs since it
   allows for states to be accessed via a descriptive key of some kind
   rather than by number alone, and since it does not require you to know
   the number of states in advance.

   When using the class it usually best to first create all the states, and
   only then set accept states,initial states, and labels. At one time
   the FSA_Common implementation of this functionality did not allow at all
   for the number of states to change. This is not true any more, at least
   for initial states and accepting states. However, for labels you cannot
   change the number of labels explicitly without deleting all the existing
   labels, though you can simply add new labels and allow them to be managed
   automatically. But, usually it will still be best to do all this work at
   the end since it will reduce the amount of data being accessed at any
   one time, and reduce the amount of reallocation of large arrays.

   The BFS flag is set in the constructor, since it is assumed that FSAs
   built with this class will be built in BFS order. So you should either
   clear this flag, or call sort_bfs() on the final FSA if the states are
   not in fact arranged in BFS order.

   If you require additional arrays of per-state data then simply declare
   an Array_Of<x> or an Array_Of_Data object, and ask Keyed_FSA to manage
   it. It will be resized automatically as the FSA grows. When doing this
   it is best not to request management of the array until you are actually
   going to use it.
*/

const Element_Count SUGGESTED_HASH_SIZE = 1024*1024+7;

class Keyed_FSA : public FSA_Common
{
  private:
    Hash ind;
    Transition_ID nr_symbols;
    mutable State_ID current_state;
    State_ID * current_transition;
    Array_Of_Data state_definitions;
    Array_Of_Data transitions;
    State_Count nr_allocated;
    Transition_Realiser *cache;
    State_Count nr_cache_rows;
  public:
    Keyed_FSA(Container & container,const Alphabet & alphabet,
              Transition_ID nr_symbols_,int hash_size,size_t key_size,
              bool compressed = true,size_t cache_size = 0);
    virtual ~Keyed_FSA();
    // Virtual methods for FSA
    State_Count state_count() const
    {
      return ind.count();
    }
    Transition_ID alphabet_size() const
    {
      return nr_symbols;
    }
    virtual void get_transitions(State_ID * buffer,State_ID state) const;
    State_ID new_state(State_ID initial_state,Transition_ID symbol_nr,bool buffer = true) const;
    bool set_transitions(State_ID state,const State_ID * buffer);

    /* our methods */
    void manage(Unknown_Collection & cb)
    {
      ind.manage(cb);
    }
    State_ID find_state(const void *key,size_t key_size = 0,bool insert=true)
    {
      Byte_Buffer b((void *) key);
      State_ID state = find_state(b,key_size,insert,false);
      b.take(); // prevent destruction of the memory pointed to
      return state;
    }
    State_ID find_state(Byte_Buffer & key,size_t key_size = 0,bool insert=true,bool take = true)
    {
      State_ID state = ind.bb_find_entry(key,key_size,insert,take);
      if (state == INVALID_ID)
        state = 0;
      return state;
    }
    bool get_state_key(void * key,State_ID state) const
    {
      return ind.get_key(key,state);
    }
    const void *get_state_key(State_ID state) const
    {
      return ind.get_key(state);
    }
    bool set_state_key(State_ID state,const void * new_key,size_t new_key_size)
    {
      return ind.set_key(state,new_key,new_key_size);
    }
    size_t get_key_size(State_ID state) const
    {
      return ind.get_key_size(state);
    }
    // redirect_state can be called when you discover that two states that you
    // thought should be different are the same. The key of the old_state is
    // removed, and all transitions to it, and subsequent attempts to read it
    // will return information pertaining to the new state.
    bool redirect_state(State_ID old_state,State_ID new_state);
    void remove_keys()
    {
      ind.clean();
    }
    void remove_state(State_ID bad_state)
    {
      /* This completely removes the state from the FSA. Do not call this
         unless you are sure that the state is inaccessible. This method is
         primarily intended for situations where it is convenient to create
         a state provisionally, to perform some checks against it, and then
         to delete it before creating the transition to it. */
      if (cache)
      {
        State_ID row = bad_state % nr_cache_rows;
        State_ID cached_si = cache->state_at(row);
        if (cached_si == bad_state)
          cache->invalidate_line(row);
      }
      if (current_state == bad_state)
        current_state = -1;
      ind.remove_entry(bad_state,true);
    }
    /* Optional state definition information */
    bool get_definition(State_Definition * definition,State_ID sequence) const
    {
      if (state_definitions.capacity())
        return state_definitions.get_data(definition,sizeof(State_Definition),sequence);
      return false;
    }
    bool set_definition(State_ID state,const State_Definition & definition)
    {
      if (!state_definitions.capacity())
        ind.manage(state_definitions);
      return state_definitions.set_data(state,&definition,sizeof(State_Definition));
    }
};

#endif
