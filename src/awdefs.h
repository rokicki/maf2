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
$Log: awdefs.h $
Revision 1.3  2009/09/13 20:01:46Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.2  2008/11/04 22:03:46Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.2  2008/11/04 23:03:46Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
*/
#ifndef AWDEFS_INCLUDED
#pragma once
#define AWDEFS_INCLUDED 1

#ifndef TRUE
#define TRUE '\1'
#define FALSE '\0'
#endif

#ifndef LOWORD
#define LOWORD(l)           ((unsigned short)(long)(l))
#define HIWORD(l)           ((unsigned short)((((long)(l)) >> 16) & 0xFFFF))
#endif

#ifdef __cplusplus
#undef min
#undef max

#define DECLARE_MIN_MAX(type) \
  type inline min(const type & v1,const type & v2) \
  { \
    return v1 < v2 ? v1 : v2; \
  } \
  type inline max(const type & v1,const type & v2) \
  { \
    return v1 > v2 ? v1 : v2; \
  }

DECLARE_MIN_MAX(int)
DECLARE_MIN_MAX(long)
DECLARE_MIN_MAX(unsigned long)
DECLARE_MIN_MAX(unsigned int)
DECLARE_MIN_MAX(unsigned short)
DECLARE_MIN_MAX(short)
DECLARE_MIN_MAX(double)
#undef DECLARE_MIN_MAX
#endif

/* Macros to make it easy to prevent attempts to assign or copy class objects
   that should not be moved or copied */
#define NO_ASSIGNMENT(classname) \
  private: classname & operator=(const classname & other);
#define NO_COPY(classname) private: classname(const classname & other);
#define BLOCKED(classname) NO_ASSIGNMENT(classname) NO_COPY(classname)
#endif

