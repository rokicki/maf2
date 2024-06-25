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


// $Log: gpmakefsa.cpp $
// Revision 1.2  2010/05/11 19:32:42Z  Alun
// EXCLUDE changed to ensure we don't construct FSA that KBMAG would not
//

#define PROGNAME_NAME "gpmakefsa"
#define NO_KB  1
#define GPMAKEFSA 1
#define EXCLUDE (GA_MAXRWS | GA_MAXKB | GA_RR | GA_L1_ACCEPTOR | \
                 GA_DIFF2C | GA_DIFF1C | GA_MINKB | GA_RWS | GA_SUBPRES | \
                 GA_SUBWA | GA_COSETS)
#include "automata.cpp"
