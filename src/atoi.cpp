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


// $Log: atoi.cpp $
// Revision 1.2  2008/09/30 06:57:31Z  Alun
// "Aug 2008 snapshot"
//

#include <stdlib.h>
#include "mafctype.h"

extern "C" int atoi(const char * str)
{
  while (is_white(*str))
    str++;
  bool negative = *str == '-';
  if (negative)
    str++;
  else if (*str == '+')
    str++;
  int answer = 0;
  while (is_digit(*str))
    answer = answer*10 + *str++-'0';
  return negative ? -answer : answer;
}

extern "C" long atol(const char * str)
{
  return atoi(str);
}
