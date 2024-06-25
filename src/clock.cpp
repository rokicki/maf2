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


// $Log: clock.cpp $
// Revision 1.3  2010/06/10 13:56:52Z  Alun
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
#include <time.h>
#include "awcc.h"
#include "awcrt.h"

typedef union
{
  u64 value;
  FILETIME filetime;
} FT;

/**/

static u64 init_time()
{
  FT now;
  GetSystemTimeAsFileTime(&now.filetime);
  return now.value;
}

static u64 start = init_time();

/**/

extern "C" clock_t clock()
{
  FT now;

  GetSystemTimeAsFileTime(&now.filetime);
  return (clock_t) ((now.value - start)/10000);
}
