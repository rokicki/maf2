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
$Log: mafqueue.h $
Revision 1.3  2009/12/11 11:41:42Z  Alun
Changes required by new style Node_Manager interface
Revision 1.2  2008/09/30 08:00:16Z  Alun
"Early Sep 2008 snapshot"
*/
#pragma once
#ifndef MAFQUEUE_INCLUDED
#define MAFQUEUE_INCLUDED 1

#ifndef MAF_INCLUDED
#include "maf.h"
#endif

// Classes defined elsewhere
#ifndef NODEBASE_INCLUDED
#include "nodebase.h"
#endif


class Equation_Queue
{
  private:
    Node_Manager &nm;
    Node_Reference head;
    Node_Reference tail;
    Node_Count count;
  public:
    Equation_Queue(Node_Manager &nm_);
    ~Equation_Queue();
    bool use(Node_Reference *e);
    void add(Node_Handle e);
    void remove(Node_Handle e);
    Node_Count length() const { return count;};
};

#endif
