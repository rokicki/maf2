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


// $Log: nodelist.cpp $
// Revision 1.6  2009/12/13 20:57:56Z  Alun
// Many changes required by new style Node_Manager
// Revision 1.5  2009/09/13 20:38:05Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2008/08/29 08:33:22Z  Alun
// "Early Sep 2008 snapshot"
// Revision 1.3  2007/09/07 20:20:17Z  Alun_Williams
//

#include "nodelist.h"
#include "mafnode.h"

/**/

/* Methods for managing lists of stuff to do.
   Most of the code for managing the creation of the reduction machine is
   recursive, and what is more, potentially capable of destroying its own
   context. So, to avoid problems we do much of the work by building simple
   linked lists of nodes to manipulate. We ensure that the nodes themselves
   remain valid either by making sure we have not called update_machine(),
   so that the worst that can have happened is that the node has been removed
   from the tree and gone into limbo, or where this might not be enough,
   by calls to Node::attach() or Node::detach(). The latter is usually
   preferable.
*/

struct Node_List::Node_Item
{
  Node_Item * next;
  Node_ID nid;
  int parameter;
};

/**/

void Node_List::merge(Node_List *other)
{
  /* Stick the second list on the end of the first list, and empty second */
  if (other->head)
  {
    if (head)
      tail->next = other->head;
    else
      head = other->head;
    tail = other->tail;
    node_count += other->node_count;
  }
  other->node_count = 0;
  other->head = other->tail = 0;
}

/**/

bool Node_List::use(Node_Reference *nr,const Node_Manager &nm,int * parameter)
{
  /* Find out what is next on our list of things to do */
  if (head)
  {
    Node_Item *save = head;
    *nr = Node_Reference(nm,head->nid);
    if (parameter)
      *parameter = head->parameter;
    head = head->next;
#if 1
    if (free_list.node_count < 128)
    {
      if (free_list.head)
        free_list.tail->next = save;
      else
        free_list.head = save;
      free_list.tail = save;
      free_list.tail->next = 0;
      free_list.node_count++;
    }
    else
#endif
      delete save;
    node_count--;
    return true;
  }
  else
  {
    *nr = Node_Reference(0,0);
    return false;
  }
}

Node_List Node_List::free_list;

/**/

void Node_List::add_state(Node_Handle nh,int parameter)
{
#if 1
  if (!free_list.head)
  {
    free_list.head = free_list.tail = new Node_Item;
    for (int i = 0;i < 128;i++)
    {
      free_list.tail->next = new Node_Item;
      free_list.tail = free_list.tail->next;
    }
    free_list.tail->next = 0;
    free_list.node_count = 128;
  }
  Node_Item * list = free_list.head;
  free_list.head = list->next;
  free_list.node_count--;
#else
  Node_Item * list = new Node_Item;
#endif
  list->next = 0;
  if (head)
    tail->next = list;
  else
    head = list;
  tail = list;
  node_count++;
  list->nid = nh;
  list->parameter = parameter;
}

void Node_List::empty()
{
  if (this != &free_list)
    free_list.merge(this);
  else
  {
    while (head)
    {
      Node_Item *save = head;
      head = head->next;
      delete save;
      node_count--;
    }
  }
}

/**/

void Sortable_Node_List::add_state(Node_Handle nh,int parameter)
{
  unsigned l = nh->length(nm);
  if (l > height)
    height = l;
  Node_List::add_state(nh,parameter);
}

/**/

void Sortable_Node_List::sort()
{
  // have to use old style cast below as otherwise compiler gets confused
  Multi_Node_List mnl(nm,(Word_Length)height);
  Node_Reference nr;
  int para;
  while (use(&nr,&para))
    mnl.add_state(nr,para);
  mnl.collapse(this);
}

/**/

Word_Length Multi_Node_List::node_length(State node) const
{
  Word_Length answer = node->length(nm);
  if (answer > height)
    answer = height;
  return answer;
}

