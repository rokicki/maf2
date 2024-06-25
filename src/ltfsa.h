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
$Log: ltfsa.h $
Revision 1.5  2010/04/10 19:05:25Z  Alun
Code now less generic since it now needs to know labels are Node_ID
Revision 1.4  2008/10/16 08:39:06Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.4  2008/10/16 09:39:06Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.3  2008/09/30 10:15:40Z  Alun
Switch to using Hash+Collection_Manager as replacement for Indexer.
Currently this is about 10% slower, but this is a more flexible
architecture.
*/
#ifndef LTFSA_INCLUDED
#pragma once
#define LTFSA_INCLUDED 1

/* LTFSA stands for labelled transition FSA. Each transition can have
a label associated with it. The primary purpose of this class is to
provide a means of keeping track of the equation with the least LHS that
contains a transition between two states of a word-difference machine */

#ifndef KEYEDFSA_INCLUDED
#include "keyedfsa.h"
#endif
#ifndef NODEBASE_INCLUDED
#include "nodebase.h"
#endif

class Label_Compressor;

class LTFSA : public Keyed_FSA
{
  private:
    Label_Compressor * compressor;
    Node_ID * current_labels;
    Array_Of_Data transition_labels;
    mutable State_ID label_state;
  public:
    LTFSA(Container & container,const Alphabet & alphabet,
              Transition_ID nr_symbols_,int hash_size,size_t key_size,
              bool compressed = true,size_t cache_size = 0);
    virtual ~LTFSA();

    Node_ID get_transition_label(State_ID state,Transition_ID transition) const;
    bool set_transition_label(State_ID state,Transition_ID transition,
                              Node_ID label);
    void remove_state(State_ID bad_state)
    {
      if (label_state == bad_state)
        label_state = -1;
      Keyed_FSA::remove_state(bad_state);
    }
};

#endif
