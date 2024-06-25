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
$Log: awcc.h $
Revision 1.9  2010/06/10 13:57:53Z  Alun
All tabs removed again
Revision 1.8  2010/03/25 09:48:46Z  Alun
Added Signed_Long_Long and Unsigned_Long_Long so nowhere else needs to
worry about lack of long long support in MS
Revision 1.7  2009/09/24 07:41:12Z  Alun
Incorporated support for dealing with buggy compilers as required by Spirofractal
Revision 1.6  2009/09/14 09:58:24Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.5  2008/11/02 17:07:44Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.5  2008/11/02 18:07:43Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.4  2007/12/20 23:25:45Z  Alun
*/
#pragma once
#ifndef AWCC_INCLUDED
#define AWCC_INCLUDED 1

/* Sometimes a compiler may generate incorrect code. It is quite often the case
   that inserting a call to a dummy function will make the problem go away
   (especially if the problem is caused by an invalid assumption made by an
    optimising compiler). The function below can be used for that purpose.
*/

extern void _cc_bug();

#ifdef _MSC_VER
#define imported __declspec(dllimport)
#pragma warning(disable : 4214) // Use of bitfield other than int
#pragma warning(disable : 4244) /* Possible data loss on conversion. Produced on
                                   any non-trivial expression assigned to short
                                   types. */
#pragma warning(disable : 4355) // use of this in member/base initialiser list
#pragma warning(disable : 4511) // Copy constructor could not be generated
#pragma warning(disable : 4512) // Assignment operator could not be generated
#pragma warning(disable : 4625) // Copy constructor could not be generated because of base class
#pragma warning(disable : 4626) // Assignment operator could not be generated because of base class

#pragma warning(disable : 4514) // Unreferenced inline function
#pragma warning(disable : 4710) // Function not inlined
#pragma warning(disable : 4711) // Function auto-inlined
#pragma warning(disable : 4820) // Padding added
#pragma warning(disable : 4725) // Instruction may be inaccurate on some Pentiums
#pragma warning(disable : 4201) // Use of nameless struct/union
                                // We usually compile with -Za so this is an
                                // error but when using -Ze to allow windows.h
                                // then we don't want this warning at all
                                // since MS headers use this construct.
#pragma warning(error : 4002 4020 4099 4150 4172 4189 4700 4715)

#pragma inline_depth(8)
#pragma inline_recursion(on)
#ifndef __cplusplus
#define inline __inline
#endif

#ifndef pascal
#define pascal __stdcall
#endif

/* If you put code after an __assume(0) you get a warning about unreachable
code. However, with VS6 SP6 compiler you will get a compiler error if the
offending branch drops out of a non-void function. (The "Processor pack"
version of C2.DLL is OK)
*/
#if _MSC_VER >= 1300
#define NOT_REACHED(stmt) {__assume(0);}
#else
#ifdef DEBUG
#define NOT_REACHED(stmt) {__assume(0);stmt}
#else
#define NOT_REACHED(stmt) {__assume(0);}
#endif
#endif

/* Even though they are different compilers it seems to be the case that these
   two compilers are prone to making mistakes in the same places.
   They are both definitely capable of producing incorrect code that later
   compilers compile correctly. So far there are no known places in MAF where
   either compiler goes wrong. But the author has very occasionally had problems
   in some of this other programs, usually in code that is doing a lot of
   floating point arithmetic.
*/
#if _MSC_VER == 1200 || defined(__ICL) && __ICL==450
#define c12_bug() _cc_bug();
#else
#define c12_bug()
#endif
#endif

/* restrict is a keyword in C99, but not in C++ 98.
  The Microsoft C12 and C13 compilers don't support it even in C.
  Intel compilers support it in both C and C++.
  GNU compilers support it in C if C99 is requested but not in C++, but
  allow __restrict__.
  MAF source does not currently use restrict anywhere, (though it possibly
  would benefit from it in some places). The author has used restrict in
  his other programs
*/

#ifdef __GNUG__
#define restrict __restrict__
#endif

#ifndef  restrict
#ifndef __ICL
#define restrict
#endif
#endif

#ifndef imported
#define imported
#endif

#ifndef NOT_REACHED
#define NOT_REACHED(stmt) stmt
#endif

#ifndef __cplusplus
#ifndef inline
#define inline
#endif
#endif

#ifdef __GNUG__
/* In Microsoft C++, size_t is a primitive type - at least it can be used in
   without declaring it (even with -Za compiler flag and no include files at
   all). I believe this is correct.
   But in g++ it is not built in
*/
#include <stddef.h>
#endif

#ifndef __GNUG__
#define __attribute__(x)
#endif

#ifdef _MSC_VER
/* I use MS_ATTRIBUTE to wrap MS specific __declspec() construct. */
#define MS_ATTRIBUTE(a) a
typedef unsigned __int64 Unsigned_Long_Long;
typedef __int64 Signed_Long_Long;
#else
#define MS_ATTRIBUTE(a)
typedef unsigned long long Unsigned_Long_Long;
typedef long long Signed_Long_Long;
#endif

/* The Microsoft compiler can generate warnings when a switch on an expression
   with an enum type does not handle all possible values of the enum explicitly.
   This is potentially very useful as it will show up places which may need to
   be changed when a new value is added to the enum. But in some places we may
   only want to handle a small subset of the possible cases. If the expression
   is cast to int the warnings go away. */
typedef int Partial_Switch;

#endif

