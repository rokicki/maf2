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


//    DP4 version 4.0000                                0000000000800000000000
// $Log: crt0twin.cpp $
// Revision 1.3  2010/06/10 13:56:53Z  Alun
// All tabs removed again
// Revision 1.2  2009/09/13 11:09:11Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// use of windows.h wrapped by awwin.h so we can clean up warnings and packing
//
//
// Revision 1.1  2007/06/13 08:21:02Z  Alun
// New file.
// Revision 1.1  2002/09/17 09:13:54Z  Alun
// New file.
//

#include <stdlib.h>
#include "awwin.h"
#include "initterm.h"

#pragma data_seg(".CRT$XCA")
_Pvfv __xc_a[] = { NULL };
#pragma data_seg(".CRT$XCZ")
_Pvfv __xc_z[] = { NULL };

#pragma data_seg()  /* reset */
#pragma comment(linker, "/merge:.CRT=.data")

// Force the linker to include KERNEL32.LIB
#pragma comment(linker, "/defaultlib:kernel32.lib")

extern "C" void __cdecl WinMainCRTStartup( void )
{
  char *lpszCommandLine = GetCommandLine();
  STARTUPINFO StartupInfo;

  // Skip past program name (first token in command line).

  if ( *lpszCommandLine == '"' )  // Check for and handle quoted program name
  {
    // Scan, and skip over, subsequent characters until  another
    // double-quote or a null is encountered

    while( *lpszCommandLine && (*lpszCommandLine != '"') )
      lpszCommandLine++;

    // If we stopped on a double-quote (usual case), skip over it.

   if ( *lpszCommandLine == '"' )
     lpszCommandLine++;
  }
  else    // First token wasn't a quote
    while ( *lpszCommandLine > ' ' )
      lpszCommandLine++;

  // Skip past any white space preceeding the second token.

  while ( *lpszCommandLine && (*lpszCommandLine <= ' ') )
    lpszCommandLine++;

  StartupInfo.dwFlags = 0;
  GetStartupInfo( &StartupInfo );

  // Call C++ constructors
  _initterm( __xc_a, __xc_z );

  exit(WinMain(GetModuleHandle(NULL), NULL, lpszCommandLine,
                    StartupInfo.dwFlags & STARTF_USESHOWWINDOW
                            ? StartupInfo.wShowWindow : SW_SHOWDEFAULT ));
}
