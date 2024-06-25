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


// $Log: mafqueue.cpp $
// Revision 1.5  2010/01/05 11:43:52Z  Alun
// Changes required by new style Node_Manager interface
// Revision 1.4  2009/09/12 18:47:45Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2008/10/01 00:24:36Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.3  2008/10/01 01:24:35Z  Alun
// stdio.h not needed
// Revision 1.2  2008/09/30 09:25:12Z  Alun
// Final version built using Indexer.
//

/* An Equation_Queue is used in a similar way to a Node_List, but there are
some important differences

1) Items can be removed from an equation queue without being processed
   because they are linked both forwards and backwards.
2) Equation_Nodes can be on at most one equation_queue. They must be
   removed from any queue they are on before being put in another.
3) Only equations can go in an equation_queue. This is because the storage
   for the links is in the "reduction" member of Node.

The main advantage of using an Equation_Queue to manage a task within MAF
is that it reduces activity in the heap manager. For a big reduction machine
this may improve performance quite a lot.
*/
#include "mafqueue.h"
#include "mafnode.h"
#include "maf_nm.h"
#include "container.h"

/**/

Equation_Queue::Equation_Queue(Node_Manager &nm_) :
  head(0),
  count(0),
  tail(0),
  nm(nm_)
{}

/**/

Equation_Queue::~Equation_Queue()
{
  Node_Reference e;
  while (use(&e))
    e->detach(nm,e);
}

/**/

void Equation_Queue::add(Node_Handle nh)
{
  MAF_ASSERT(!nh.is_null() && !nh->flagged(EQ_QUEUED),
             nm.maf.container,("Bad attempt to queue equation\n"));
  Equation_Node & e = *nh->attach(nm);
  if (head.is_null())
  {
    head = nh;
    tail = nh;
    e.reduction.queue_prev = 0;
    e.reduction.queue_next = 0;
  }
  else
  {
    e.reduction.queue_prev = tail;
    e.reduction.queue_next = 0;
    tail->node(nm).reduction.queue_next = nh;
    tail = nh;
  }
  e.set_flags(EQ_QUEUED);
  count++;
}

/**/

void Equation_Queue::remove(Node_Handle nh)
{
  MAF_ASSERT(nh->flagged(EQ_QUEUED),nm.maf.container,("Attempting to remove unqueued equation from queue\n"));
  Equation_Node & e = nh->node(nm);

  if (e.reduction.queue_prev)
    Node::fast_find_node(nm,e.reduction.queue_prev)->node(nm).reduction.queue_next = e.reduction.queue_next;
  else
  {
    MAF_ASSERT(head==&e,nm.maf.container,("Equation_Queue corruption\n"));
    head = Node_Reference(nm,e.reduction.queue_next);
  }

  if (e.reduction.queue_next)
    Node::fast_find_node(nm,e.reduction.queue_next)->node(nm).reduction.queue_prev = e.reduction.queue_prev;
  else
  {
    MAF_ASSERT(tail==&e,nm.maf.container,("Equation_Queue corruption\n"));
    tail = Node_Reference(nm,e.reduction.queue_prev);
  }
  e.reduction.queue_next = 0;
  e.reduction.queue_prev = 0;
  e.clear_flags(EQ_QUEUED);
  e.detach(nm,nh);
  count--;
}

/**/

bool Equation_Queue::use(Node_Reference * nr)
{
  *nr = head;
  if (head.is_null())
    return false;

  Equation_Node & e = head->node(nm);
  count--;
  head = Node_Reference(nm,e.reduction.queue_next);
  e.reduction.queue_next = 0;
  e.reduction.queue_prev = 0;
  if (!head.is_null())
    head->node(nm).reduction.queue_prev = 0;
  else
    tail = Node_Reference(0,0); // not really necessary, just cleaner
  e.clear_flags(EQ_QUEUED);
  return true;
}

