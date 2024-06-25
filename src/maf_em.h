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
$Log: maf_em.h $
Revision 1.4  2010/02/12 18:51:52Z  Alun
Jun 2010 version.
Revision 1.3  2009/11/10 08:15:38Z  Alun
member name changed
Revision 1.2  2009/09/12 18:48:35Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.1  2008/09/30 08:28:48Z  Alun
New file.
*/

/* Class Equation_Manager is used to manage the processing of new
   equations that have been discovered.

   When an equation has been discovered we need to decide where, if anywhere,
   to insert it (i.e. tree, pool or discard).

   When we have decided to insert an equation into the tree it may need to
   be adjusted, to minimise any adverse consequences.

   Even it we decide not to insert it into the tree it may be that we can
   derive some other more useful equation from it.

   So insertion of even one equation is sufficiently complex to merit a
   class of its own to handle it.
*/
#pragma once
#ifndef MAF_EM_INCLUDED
#define MAF_EM_INCLUDED 1
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

/* Classes defined elsewhere */
class Container;
class Derivation;
struct Equation_Properties;
class MAF;
class Rewriter_Machine;
class Node;
class Node_Manager;
class Presentation_Data;
class Word;
typedef Node Equation_Node;

/* This class is intended purely as a base for Equation_Manager.
   I did things like this to make the "we" and "flags" members directly
   available to Equation_Manager */

class Managed_Equation
{
  friend class Equation_Manager;
  private:
    Managed_Equation * next;
    class Working_Equation * we;
    unsigned flags;
    bool owner;
  private:
    Managed_Equation(Working_Equation * we_ = 0,unsigned flags_ = 0,bool owner_ = false) :
      next(0),
      we(we_),
      flags(flags_),
      owner(owner_)
    {}
    Managed_Equation(Node_Manager &nm,const Word & lhs,const Word & rhs,
                     const Derivation & d,unsigned flags);
    ~Managed_Equation();
    Managed_Equation & operator=(Managed_Equation & other);
};

class Equation_Manager : private Managed_Equation
{
  private:
    Rewriter_Machine &rm;
    MAF &maf;
    const Presentation_Data & pd;
    Node_Manager &nm;
    Rewriter_Machine::Status &stats;
    Container &container;
    Managed_Equation * head;
    Managed_Equation ** tail;
    Managed_Equation &current;
    Total_Length was_total;
    unsigned ae_flags_or;
    int action_needed;
    bool some_discarded;
  public:
    Equation_Manager(Rewriter_Machine & rm_);
    ~Equation_Manager();
    int learn(Working_Equation * we,unsigned flags,bool * discarded);
    void queue(const Word & lhs,const Word & rhs,const Derivation &d,unsigned flags)
    {
      Managed_Equation * me = new Managed_Equation(nm,lhs,rhs,d,flags);
      *tail = me;
      tail = &me->next;
    }
  private:
    int process();
    void queue(Managed_Equation * me)
    {
      *tail = me;
      tail = &me->next;
    }
    bool use();
    void choose_action(const Equation_Properties &ep);
    int is_insertable(const Equation_Properties &ep);
    void moderate(Equation_Properties * ep);
};

#endif
