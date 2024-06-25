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


// $Log: dllcrt0.cpp $
// Revision 1.4  2010/06/10 13:56:54Z  Alun
// All tabs removed again
// Revision 1.3  2009/11/10 20:59:06Z  Alun
// include awcc.h to silence unwanted link time code generation warnings
// Revision 1.2  2009/09/14 10:32:06Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// use of windows.h wrapped by awwin.h so we can clean up warnings and packing
//
//
//

#include "awwin.h"
#include "initterm.h"
#include "awcc.h"

#pragma data_seg(".CRT$XCA")
_Pvfv __xc_a[] = { NULL };
#pragma data_seg(".CRT$XCZ")
_Pvfv __xc_z[] = { NULL };

//#pragma data_seg(".CRT$XPA")
//_Pvfv __xp_a[] = { NULL };
//#pragma data_seg(".CRT$XPZ")
//_Pvfv __xp_z[] = { NULL };
extern _Pvfv __xp_a[],__xp_z[],__xt_a[],__xt_z[];
//#pragma data_seg(".CRT$XTA")
//_Pvfv __xt_a[] = { NULL };
//#pragma data_seg(".CRT$XTZ")
//_Pvfv __xt_z[] = { NULL };
#pragma data_seg()  /* reset */
#pragma comment(linker, "/merge:.CRT=.data")

// User routine DllMain is called on all notifications

extern BOOL WINAPI DllMain(HANDLE  hDllHandle,DWORD   dwReason,
                           LPVOID  lpreserved);


extern "C" BOOL WINAPI _DllMainCRTStartup(HANDLE hDllHandle,DWORD dwReason,
                                          LPVOID lpreserved)
{
  if ( dwReason == DLL_PROCESS_ATTACH )
  {
    // Call C++ constructors
    _initterm( __xc_a, __xc_z );
  }

  BOOL retcode = DllMain(hDllHandle, dwReason, lpreserved);

  if ( dwReason == DLL_PROCESS_DETACH )
  {
    _do_exit();
    _initterm(__xp_a, __xp_z);
    _initterm(__xt_a, __xt_z);
  }
  return retcode;
}
