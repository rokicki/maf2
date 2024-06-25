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
$Log: node_status.h $
Revision 1.7  2010/01/10 23:10:07Z  Alun
Added NS_Weak_Secondary_Equation, renamed various other members and removed
unused members
Revision 1.6  2009/09/13 15:19:23Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.5  2007/11/15 22:58:12Z  Alun
*/
#pragma once
#ifndef NODE_STATUS_INCLUDED
#define NODE_STATUS_INCLUDED 1

/* I am forced to have this header file because standard C++ does not allow
   incomplete declaration of enum types. IMHO this is terrible as it forces
   header files to reveal even more than they already have to about
   implementation. Making this a class type would be wrong. It would be much
   better to mend C++ broken enums by inventing a new alternate keyword for
   them that does allow them to be incompletely declared, or for the size
   to be specified.
   The compiler does not need to know the size at the point of declaration.
   If the incomplete type is used as a member that is fair enough, but, if as is much
   more likely  they are used as a parameter it is more or less certain that
   they will have sizeof(int) and in any case the offending function cannot
   be called unless the full definition of the enum is in scope.
   Since Microsoft C++ can compile the code perfectly OK in its extended mode
   it is clear that the standard is WRONG.
*/

enum Node_Status
{
  // These are in decreasing order of merit
  NS_Irreducible,
  NS_Expanded_Equation,
  NS_Adopted_Equation,
  NS_Weak_Secondary_Equation,
  NS_Unconjugated_Equation,
  NS_Undifferenced_Equation,
  NS_Oversized_Equation,
  NS_Dubious_Unconjugated_Equation,
  NS_Dubious_Undifferenced_Equation,
  NS_Dubious_Secondary_Equation,
  NS_Last_Valid = NS_Dubious_Secondary_Equation,
  NS_Dubious_Uncorrected_Equation,
  NS_Uncorrected_Equation,
  NS_Correction_Pending_Equation,
  NS_Removed_Equation,
  NS_Removed_Node,
  NS_Invalid
};

// Since we have this header file lets put this in too since it allows us
// to include maf_rm.h without mafnode.h
typedef unsigned short Node_Flags;
#endif
