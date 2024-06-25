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


// $Log: initterm.cpp $
// Revision 1.5  2010/06/10 13:57:23Z  Alun
// All tabs removed again
// Revision 1.4  2009/11/10 20:58:55Z  Alun
// includes awcc.h to disable compiler warnings from LTCG
// Revision 1.3  2009/09/13 09:22:02Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// use of windows.h wrapped by awwin.h so we can clean up warnings and packing
//
//
// Revision 1.2  2007/10/24 21:15:37Z  Alun
//

#include "awwin.h"
#include <stdio.h>
#include "initterm.h"
#include "awcc.h"

extern "C" void __cdecl _initterm (_Pvfv * pfbegin,_Pvfv * pfend)
{
   /* walk the table of function pointers from the bottom up, until
      the end is encountered.  Do not skip the first entry.  The initial
      value of pfbegin points to the first valid entry.  Do not try to
      execute what pfend points to.  Only entries before pfend are valid.
   */

  while ( pfbegin < pfend )
  {
    // if current table entry is non-NULL, call it.
    if (*pfbegin)
      (**pfbegin)();
    ++pfbegin;
  }
}

/**/

struct Atexit_Item
{
  _Pvfv func;
  Atexit_Item * next;
};

static Atexit_Item * atexit_list = 0;

extern "C" int atexit (_Pvfv func)
{
  Atexit_Item * item = (Atexit_Item *) HeapAlloc(GetProcessHeap(),0,
                                                 sizeof(Atexit_Item));
  if (item)
  {
    item->next = atexit_list;
    item->func = func;
    atexit_list = item;
    return 0;
  }
  return -1;
}

/**/

extern "C" void _do_exit( void )
{
  while (atexit_list)
  {
    Atexit_Item * item = atexit_list;
    atexit_list = item->next;
    (*item->func)();
    HeapFree(GetProcessHeap(),0,item);
  }
}
