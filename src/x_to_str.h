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
$Log: x_to_str.h $
Revision 1.1  2008/11/04 23:02:50Z  Alun_Williams
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
*/
#pragma once
#ifndef X_to_str_INCLUDED
#define X_to_str_INCLUDED  1
#ifndef AWCRT_INCLUDED
#include "awcrt.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern char * i_to_str (char *buffer,int value,int radix);
extern char * l_to_str (char *buffer,long value,int radix);
extern char * ul_to_str (char *buffer,unsigned long value,int radix);
extern char * i64_to_str (char *buffer,i64 value,int radix);
extern char * u64_to_str (char *buffer,u64 value,int radix);

#ifdef __cplusplus
}
#endif

#endif
