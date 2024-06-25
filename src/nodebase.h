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
$Log: nodebase.h $
Revision 1.2  2010/06/10 13:58:15Z  Alun
All tabs removed again
Revision 1.1  2010/01/25 15:08:50Z  Alun
New file.
*/
#pragma once
#ifndef NODEBASE_INCLUDED
#define NODEBASE_INCLUDED 1
#include <limits.h>

// defined elsewhere
class Node;
class Node_Manager;

class Nodes
{
  friend class Node;
  friend class Block_Manager;
  protected:
    Node **block_table;
};

typedef const Node *State;
typedef Node Equation_Node;

#ifndef MAF_USE_LOOKUP
#ifdef _WIN64
#define MAF_USE_LOOKUP 1
#endif
#endif

#ifndef MAF_USE_LOOKUP
/* I'd like to have #if sizeof(void *) > sizeof(Element_ID below, but
   since we can't so that, and nor is there a SIZE_T_MAX we use LONG_MAX.
   That won't work on the horrible WIN64 memory model, but we already dealt
   with that above */
#if LONG_MAX > INT_MAX
#define MAF_USE_LOOKUP 1
#else
#define MAF_USE_LOOKUP 0
#endif
#endif

#if MAF_USE_LOOKUP
typedef State_ID Node_ID;
#else
typedef State Node_ID;
#endif

class Node_Reference
{
  friend class Node;
  private:
    State s;
#if MAF_USE_LOOKUP
    Node_ID nid;
#endif
  public:
    State operator->() const
    {
      return s;
    }
    operator const State &() const
    {
      return s;
    }
    bool operator==(State other) const
    {
      return s == other;
    }
    bool operator!=(State other) const
    {
      return s != other;
    }
#if MAF_USE_LOOKUP
    operator const Node_ID &() const
    {
      return nid;
    }
    explicit Node_Reference(State s_ = 0,Node_ID nid_ = 0) :
      s(s_),
      nid(nid_)
    {}
    inline Node_Reference(const Node_Manager &nm,Node_ID nid_);
    bool operator==(const Node_Reference & other) const
    {
      return nid == other.nid;
    }
    bool operator!=(const Node_Reference & other) const
    {
      return nid != other.nid;
    }
    bool is_null() const
    {
      return nid==0;
    }
#else
    explicit Node_Reference(State s_ = 0,Node_ID = 0) :
      s(s_)
    {}
    inline Node_Reference(const Node_Manager &,Node_ID nid_) :
      s(nid_)
    {}
    bool is_null() const
    {
      return s==0;
    }
#endif
};

/* If MAF_USE_LOOKUP is true then Node_Reference is big enough that we
   would rather pass a reference to one than duplicating it on the stack.
   If not, then it is only the same size as a pointer, so we may as well
   pass it by value */
#if MAF_USE_LOOKUP
typedef const Node_Reference &Node_Handle;
#else
typedef const Node_Reference Node_Handle;
#endif

typedef Node_Handle Equation_Handle;

enum Overlap_Search_Type
{
  OST_Aborted_Pass,
  OST_Minimal,
  OST_Quick,
  OST_Moderate,
  OST_Full
};

#endif
