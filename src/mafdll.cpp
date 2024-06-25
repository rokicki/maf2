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


// $Log: mafdll.cpp $
// Revision 1.4  2010/06/10 13:57:31Z  Alun
// All tabs removed again
// Revision 1.3  2009/09/13 09:17:10Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// use of windows.h wrapped by awwin.h so we can clean up warnings and packing
//
//
// Revision 1.2  2007/12/20 23:25:45Z  Alun
//

#include "awwin.h"
#include "heap.h"

#pragma warning(disable:4100)

extern "C" BOOL WINAPI DllMain(HANDLE  hDllHandle,DWORD dwReason,LPVOID lpreserved)
{
  /* the global heap regiters an atexit() function to check for memory leaks.
     If a program terminates normally this should be called. But if a program
     excepts, or terminates abnormally it is not reasonable to expect all
     allocations to have been freed.
     If a program has terminated normally, then it will already have done the
     atexit() processing when we get here. If not then it won't have. */
  if (dwReason == DLL_PROCESS_DETACH)
    Heap::prevent_leak_dump();
  return TRUE;
}
