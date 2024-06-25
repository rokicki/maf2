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


// $Log: mafctype.cpp $
// Revision 1.4  2010/06/10 13:57:30Z  Alun
// All tabs removed again
// Revision 1.3  2009/09/13 21:17:01Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.2  2008/09/30 06:57:31Z  Alun
// "Aug 2008 snapshot"
//

#include "mafbase.h"
#include "mafctype.h"

static String lower = "abcdefghijklmnopqrstuvwxyz";
static String upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static String digits = "0123456789";
static String hex_digits = "0123456789abcdefABCDEF";
static String operators = "+-*/<=>|&^[]~!()?:%";

Char_Classification::Char_Classification()
{
  int i;
  String s;
  for (i = 0; i <= UCHAR_MAX;i++)
  {
    ctype[i] = 0;
    to_upper[i] = to_lower[i] = (unsigned char) i;
  }
  for (i = 0; i < 26;i++)
  {
    to_upper[lower[i]] = upper[i];
    to_lower[upper[i]] = lower[i];
  }

  for (s = lower;*s;s++)
    ctype[*s] = CC_Lower|CC_Initial|CC_Subsequent;
  for (s = upper;*s;s++)
    ctype[*s] = CC_Upper|CC_Initial|CC_Subsequent;
  for (s = digits;*s;s++)
    ctype[*s] = CC_Digit|CC_Subsequent;
  for (s = hex_digits;*s;s++)
    ctype[*s] |= CC_Hex_Digit;
  for (s = operators;*s;s++)
    ctype[*s] |= CC_Operator;
  ctype['_'] |= CC_Initial|CC_Subsequent;
  ctype[' '] |= ctype['\t'] = ctype['\r'] = ctype['\n'] = CC_White;
  ctype['.'] |= CC_Dot;
  ctype['/'] |= CC_Path_Separator;
  ctype['\\'] |= CC_Path_Separator;
  ctype[':'] |= CC_Path_Separator;
}

static const Char_Classification cdata;

const Char_Classification & Char_Classification::data()
{
  return cdata;
}
