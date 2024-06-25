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


// $Log: keyedfsa.cpp $
// Revision 1.10  2010/06/10 13:57:26Z  Alun
// All tabs removed again
// Revision 1.9  2010/03/05 14:57:27Z  Alun
// removed divide by zero if FSA with empty alphabet created
// Revision 1.8  2009/09/16 07:20:15Z  Alun
// Additional source code changes needed to get clean compilation on latest GNU
//
// Revision 1.7  2009/09/12 18:47:41Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.6  2008/10/13 20:39:02Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.7  2008/10/13 21:39:01Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.6  2008/09/30 22:06:50Z  Alun
// Changes to improve inlining of code
// Revision 1.5  2008/09/30 05:20:03Z  Alun
// Switch to using Hash+Collection_Manager as replacement for Indexer.
// Currently this is about 10% slower, but this is a more flexible
// architecture.
// Revision 1.3  2007/11/15 22:58:09Z  Alun
//

#include <limits.h>
#include <memory.h>
#include "awcc.h"
#include "keyedfsa.h"

Keyed_FSA::Keyed_FSA(Container & container,const Alphabet & alphabet,
                     Transition_ID nr_symbols_,int hash_size,size_t key_size,
                     bool compressed,size_t cache_size) :
  FSA_Common(container,alphabet),
  nr_symbols(nr_symbols_),
  ind(hash_size,key_size),
  cache(0),
  nr_cache_rows(0),
  current_state(0)
{
  current_transition = new State_ID[nr_symbols];
  if (compressed)
    allocate_compressor();
  change_flags(GFF_BFS|GFF_ACCESSIBLE,0);
  if (cache_size && compressed)
  {
    if (nr_symbols)
      nr_cache_rows = (cache_size+sizeof(State_ID)*nr_symbols-1)/(sizeof(State_ID)*nr_symbols);
    else
      nr_cache_rows = 1;
    cache = new Transition_Realiser(*this,0,nr_cache_rows);
  }
  ind.manage(transitions);
}

/**/

Keyed_FSA::~Keyed_FSA()
{
  delete [] current_transition;
  if (cache)
    delete cache;
}

/**/

void Keyed_FSA::get_transitions(State_ID * buffer,State_ID state) const
{
  bool read_it = true;

  if (cache)
  {
    State_ID row = state % nr_cache_rows;
    State_ID cached_si = cache->state_at(row);
    if (cached_si < 0)
    {
      /* call to cache->realise_row below will make a recursive call
         to get_transitions() here, but won't get in here because the
         the cache line will appear to be correct.
         The recursive call will then get the cache_line, and see
         that it is the same as the buffer and take evasive action. */
      cache->realise_row(cached_si = state,row);
    }
    if (cached_si == state)
    {
      State_ID * cbuffer = cache->cache_line(row);
      if (cbuffer != buffer)
      {
        /* any recursive call finished and the cache line is correct */
        for (Transition_ID i = 0; i < nr_symbols;i++)
          buffer[i] = cbuffer[i];
        read_it = false;
      }
      else
      {
        ;/* call from cache->realise_row() - fill in the buffer as normal */
      }
    }
  }

  if (read_it)
  {
    if (compressor)
      compressor->decompress(buffer,(const unsigned char *) transitions.get(state));
    else if (!transitions.get_data(buffer,sizeof(State_ID)*nr_symbols,state))
      for (Transition_ID i = 0; i < nr_symbols;i++)
        buffer[i] = 0;
  }
}

/**/

State_ID Keyed_FSA::new_state(State_ID initial_state,Transition_ID ti,bool buffer) const
{
  if (cache)
  {
    State_ID row = initial_state % nr_cache_rows;
    State_ID cached_si = cache->state_at(row);
    if (cached_si < 0 ||
        buffer && cached_si != initial_state)
      cache->realise_row(cached_si = initial_state,row);
    if (cached_si == initial_state)
    {
      State_ID * cbuffer = cache->cache_line(row);
      return cbuffer[ti];
    }
  }

  if (compressor)
  {
    if (buffer)
    {
      if (current_state != initial_state)
        get_transitions(current_transition,current_state = initial_state);
      return current_transition[ti];
    }
    else
      return compressor->new_state((const unsigned char *)
                                   transitions.get(initial_state),ti);

  }
  else
  {
    State_ID new_state = 0;
    transitions.get_data(&new_state,sizeof(State_ID),initial_state,ti*sizeof(State_ID));
    return new_state;
  }
}

/**/

bool Keyed_FSA::set_transitions(State_ID state,const State_ID * buffer)
{
  if (cache)
  {
    State_ID row = state % nr_cache_rows;
    State_ID cached_si = cache->state_at(row);
    if (cached_si == state)
    {
      State_ID * cbuffer = cache->cache_line(row);
      memcpy(cbuffer,buffer,nr_symbols*sizeof(State_ID));
    }
  }

  if (compressor)
  {
    if (state == current_state)
      memcpy(current_transition,buffer,nr_symbols*sizeof(State_ID));
    size_t size = compressor->compress(buffer);
    return transitions.set_data(state,compressor->cdata,size);
  }
  return transitions.set_data(state,buffer,nr_symbols*sizeof(State_ID));
}

