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
$Log: nodelist.h $
Revision 1.7  2009/12/12 09:56:28Z  Alun
Added Sortable_Node_List.
Various changes required by new style Node_Manager interface
Revision 1.6  2009/09/12 18:48:42Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.5  2008/08/29 08:19:32Z  Alun
"Early Sep 2008 snapshot"
Revision 1.4  2007/09/07 20:20:18Z  Alun_Williams
*/

#pragma once
#ifndef NODELIST_INCLUDED
#define NODELIST_INCLUDED 1
#ifndef AWCC_INCLUDED
#include "awcc.h"
#endif
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif
#ifndef NODEBASE_INCLUDED
#include "nodebase.h"
#endif

class Node_List
{
  /* Class Node_List is used for building lists of nodes that
     we want to do something with */
  struct Node_Item;
  private:
    static Node_List free_list;
    Node_Item * head;
    Node_Item * tail;
    Node_Count node_count;
  public:
    Node_List() :
      node_count(0),
      head(0),
      tail(0)
    {}
    ~Node_List()
    {
      empty();
    }
    void add_state(Node_Handle nh,int parameter = 0);
    Node_Count length() const
    {
      return node_count;
    };
    void merge(Node_List *other);
    void empty();
    bool use(Node_Reference *e,const Node_Manager &nm,int * parameter = 0);
    static void purge()
    {
      free_list.empty();
    }
};

class Multi_Node_List
{
  private:
    Node_List * lists;
    Word_Length height;
    Node_Count nr_items;
    const Node_Manager & nm;
  public:
    Multi_Node_List(const Node_Manager & nm_,Word_Length height_) :
      lists(new Node_List[(height = height_)+1]),
      nr_items(0),
      nm(nm_)
    {}
    ~Multi_Node_List()
    {
      delete [] lists;
    }
    Word_Length node_length(State node) const;
    void add_state(Node_Handle node,int parameter = 0)
    {
      nr_items++;
      lists[node_length(node)].add_state(node,parameter);
    }
    void collapse(Node_List *target_list,Word_Length limit = WHOLE_WORD)
    {
      if (limit >= height)
        limit = height;
      for (Word_Length i = 0; i <= limit;i++)
        target_list->merge(&lists[i]);
    }
    Node_Count length() const
    {
      return nr_items;
    }
};

class Sortable_Node_List : public Node_List
{
  private:
    const Node_Manager &nm;
    unsigned height;
  public:
    Sortable_Node_List(const Node_Manager &nm_) :
      nm(nm_),
      height(0)
    {}
    void add_state(Node_Handle node,int parameter = 0);
    void sort();
    bool use(Node_Reference *e,int * parameter = 0)
    {
      return Node_List::use(e,nm,parameter);
    }
};

#endif
