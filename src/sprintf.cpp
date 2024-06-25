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


// $Log: sprintf.cpp $
// Revision 1.2  2008/11/04 22:03:46Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.2  2008/11/04 23:03:45Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
//

#include <stdio.h>
#include <stdarg.h>
#include "awcc.h"
#include "variadic.h"
#include "sstream.h"

extern "C" int sprintf(char *buffer, const char * control, ...)
{
  DECLARE_VA(va,control);
  String_Stream stream(buffer);
  int retcode = stream.formatv(control, va);
  return retcode;
}
