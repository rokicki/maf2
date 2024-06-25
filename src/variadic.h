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
$Log: variadic.h $
Revision 1.3  2009/09/12 18:48:45Z  Alun
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
#pragma once
#ifndef VARIADIC_INCLUDED
#define VARIADIC_INCLUDED 1

#include <stdarg.h>

/* This header is attempting to wrap stdarg.h. This is probably fairly
   pointless!
*/

#if defined(__GNUG__) && !defined(MAF_NO_WRAP_STDARG)
#define MAF_NO_WRAP_STDARG 1
#endif

#ifndef MAF_NO_WRAP_STDARG

// The LIKE_GNU code is what I used to do with g++, before abandoning
// the Variadic_Arguments class altogether on that platform. So on another
// platform where Variadic_Arguments causes a problem we can either do the
// same as we now do for g++, or that

class Variadic_Arguments
{
  public:
    va_list args;
#ifdef LIKE_GNU
    Variadic_Arguments(const va_list & args_) :
      args(args_)
    {}
#else
   Variadic_Arguments(const char * &control)
    {
      va_start(args,control);
    }
#endif
    ~Variadic_Arguments()
    {
      va_end(args);
    }
    operator va_list & ()
    {
      return args;
    }
};

#ifdef LIKE_GNU
#define DECLARE_VA(va,control) \
  va_list args; \
  va_start(args,control); \
  Variadic_Arguments va(args);
#else
#define DECLARE_VA(va,control) \
  Variadic_Arguments va(control);
#endif

/* Unfortunately we can't get at arguments without a macro
   (unless we use templates) */
#define next_argument(va_object,type) \
   va_arg(va_object.args,type)

#else

#define DECLARE_VA(va,control) \
  va_list va; \
  va_start(va,control);
#define next_argument(va_object,type) \
   va_arg(va_object,type)

#endif

#endif

