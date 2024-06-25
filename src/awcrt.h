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
$Log: awcrt.h $
Revision 1.2  2008/11/04 22:03:46Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.2  2008/11/04 23:03:46Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
*/
#pragma once
#ifndef AWCRT_INCLUDED
#define AWCRT_INCLUDED 1

#ifdef _MSC_VER
typedef unsigned __int64 u64;
typedef __int64 i64;
#else
typedef unsigned long long u64;
typedef long long i64;
#endif

#endif
