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


// $Log: x_to_str.cpp $
// Revision 1.3  2010/06/10 13:57:51Z  Alun
// All tabs removed again
// Revision 1.2  2009/09/13 11:21:50Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.1  2008/11/04 23:02:46Z  Alun_Williams
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
//

#include <stdlib.h>
#include <limits.h>
#include "awcc.h"
#include "awcrt.h"
#include "x_to_str.h"

/* Implementations of functions for converting numbers to strings*/

static void x_to_str(char *buffer,unsigned long value,unsigned radix)
{
  char *p = buffer;
  char *first = p;

  do
  {
    unsigned digit = (unsigned) (value % radix);
    value /= radix;

    if (digit > 9)
      *p++ = (char) (digit - 10 + 'a');
    else
      *p++ = (char) (digit + '0');
  }
  while (value > 0);

  /* We have the digits of the number in the buffer, but in reverse order.*/

  *p-- = '\0';
  while (first < p)
  {
    char temp = *p;
    *p-- = *first;
    *first++ = temp;   /* swap *p and *first */
  }
}

char * i_to_str(char *buffer,int value,int radix)
{
  if (radix == 10 && value < 0)
  {
    buffer[0] = '-';
    x_to_str(buffer+1, -value, radix);
  }
  else
    x_to_str(buffer, value, radix);
  return buffer;
}

char * l_to_str(char *buffer,long value,int radix)
{
  if (radix == 10 && value < 0)
  {
    buffer[0] = '-';
    x_to_str(buffer+1, -value, radix);
  }
  else
    x_to_str(buffer, value, radix);
  return buffer;
}

char * ul_to_str(char *buffer,unsigned long value,int radix)
{
  x_to_str(buffer, value, radix);
  return buffer;
}

static void x64_to_str(char *buffer,u64 value,unsigned radix)
{
  char *p = buffer;
  char *first = p;

  do
  {
    unsigned digit = (unsigned) (value % radix);
    value /= radix;

    if (digit > 9)
      *p++ = (char) (digit - 10 + 'a');
    else
      *p++ = (char) (digit + '0');
  }
  while (value > 0);

  /* We have the digits of the number in the buffer, but in reverse order */

  *p-- = '\0';

  while (first < p)
  {
    char temp = *p;
    *p-- = *first;
    *first++ = temp;   /* swap *p and *first */
  }
}

/**/

char * i64_to_str(char *buffer,i64 value,int radix)
{
  if (radix == 10 && value < 0)
  {
    buffer[0] = '-';
    x64_to_str(buffer+1, -value, radix);
  }
  else
    x64_to_str(buffer, value, radix);
  return buffer;
}

/**/

char * u64_to_str(char *buffer,u64 value,int radix)
{
  x64_to_str(buffer, value, radix);
  return buffer;
}
