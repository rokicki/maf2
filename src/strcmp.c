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


#include <string.h>

#pragma function(strcmp)

int strcmp(const char *lhs,const char * rhs)
{
  int ret;
  while ( (ret = *lhs - *rhs)==0 && *lhs)
    lhs++,rhs++;
  if (ret < 0)
    return -1;
  if (ret > 0)
    return 1;
  return 0;
}
