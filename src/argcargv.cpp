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


// $Log: argcargv.cpp $
// Revision 1.3  2010/06/10 13:56:49Z  Alun
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
#include "argcargv.h"

extern "C" int _setupargs(char *** argv_ptr)
{

  // First get a pointer to the system's version of the command line
  LPSTR sys_command_line = GetCommandLine();
  size_t length = lstrlen(sys_command_line);

  // Allocate memory to store a copy of the command line.  We'll modify
  // this copy, rather than the original command line.  Yes, this memory
  // currently doesn't explicitly get freed, but it goes away when the
  // process terminates.
  LPSTR command_line = (LPSTR) HeapAlloc( GetProcessHeap(), 0, length+1);
  lstrcpy( command_line, sys_command_line);
  int argc;
  char **argv = 0;

  for (;;)
  {
    char * s;
    for (argc = 0,s = command_line; *s;)
    {
      // Skip whitespace
      while (*s && (*s == ' ' || *s == '\t'))
        s++;

      if (*s == '"')   // Argument starting with a quote???
      {
        s++;   // Advance past quote character
        if (argv)
          argv[argc] = s;
        // Scan to end quote, or NULL terminator
        while ( *s && *s != '"')
          s++;
      }
      else                        // Non-quoted argument
      {
        if (argv)
          argv[argc] = s;
        while (*s && *s != ' ' && *s != '\t')
          s++;
      }
      argc++;
      if (*s)
      {
        if (argv)
          *s = 0;
        s++;
      }
    }
    if (argv)
    {
      argv[argc] = 0;
      *argv_ptr = argv;
      return argc;
    }
    argv = (char **) HeapAlloc(GetProcessHeap(),0,(argc+1)*sizeof(char *));
  }
}
