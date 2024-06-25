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


// $Log: kbprogcos.cpp $
// Revision 1.5  2010/05/11 19:31:29Z  Alun
// EXCLUDE changed so we only build same FSAs as KBMAG kbprogcos.
// Revision 1.4  2009/10/13 21:46:36Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSAs KBMAG would not (if any)
// Revision 1.3  2009/09/12 18:47:41Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2007/10/24 21:15:31Z  Alun
//

#define PROGRAM_NAME "kbprogcos"
#define COSETS true
#define NO_WD  true
#define GA_FLAGS 0
#define EMULATE true
#define KBMAG_FINITE_INDEX
#define EXCLUDE (GA_L1_ACCEPTOR | GA_MAXRWS | GA_DIFF1C | GA_DIFF2C | \
                 GA_GM | GA_MINKB | GA_MAXKB | GA_WA | GA_DGM | GA_RR | \
                 GA_SUBPRES | GA_SUBWA | GA_COSETS)
#define IS_KBPROG 1
#include "automata.cpp"
