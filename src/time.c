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
$Log: time.c $
Revision 1.2  2009/09/14 10:32:05Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
use of windows.h wrapped by awwin.h so we can clean up warnings and packing


*/


#include "awwin.h"
#include <time.h>

/* Number of 100 nanosecond units from 1/1/1601 to 1/1/1970 */
#define EPOCH_BIAS  116444736000000000i64

/* Union to facilitate converting from FILETIME to unsigned __int64 */
typedef union
{
  unsigned __int64 ft_scalar;
  FILETIME ft_struct;
} FT;

time_t time (time_t *timeptr)
{
  time_t tim;
  FT nt_time;

  GetSystemTimeAsFileTime( &(nt_time.ft_struct) );

  tim = (time_t)((nt_time.ft_scalar - EPOCH_BIAS) / 10000000i64);

  if (timeptr)
    *timeptr = tim;         /* store time if requested */

  return tim;
}

