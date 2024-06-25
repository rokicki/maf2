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


// $Log: crt0tcon.cpp $
// Revision 1.3  2010/06/10 13:56:53Z  Alun
// All tabs removed again
// Revision 1.2  2009/09/14 10:32:08Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// use of windows.h wrapped by awwin.h so we can clean up warnings and packing
//
//
//

#include "awwin.h"
#include <stdlib.h>
#include "argcargv.h"
#include "initterm.h"

_Pvfv _FPinit;          /* floating point init. */

#pragma data_seg(".CRT$XCA")
_Pvfv __xc_a[] = { NULL };
#pragma data_seg(".CRT$XCZ")
_Pvfv __xc_z[] = { NULL };

#pragma data_seg()  /* reset */
#pragma comment(linker, "/merge:.CRT=.data")

extern "C" int __cdecl main(int, char **, char **);    // In user's code

extern "C" void __cdecl mainCRTStartup( void )
{
  char **argv;
  int argc = _setupargs(&argv);
  // Call C++ constructors
  _initterm( __xc_a, __xc_z );
  exit(main(argc, argv, 0));
}
