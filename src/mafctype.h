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
$Log: mafctype.h $
Revision 1.3  2009/09/12 18:48:31Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.2  2008/09/30 06:57:31Z  Alun
"Aug 2008 snapshot"
*/
/* I am not going to rely on ctype.h for classifying characters
   as I don't need locale stuff and don't trust implementations to
   provide things like _iscsymf(), which is the one MAF really needs */
#pragma once
#ifndef MAFCTYPE_INCLUDED
#define MACTYPE_INCLUDED 1

#include <limits.h>
#ifndef AWCC_INCLUDED
#include "awcc.h"
#endif

const unsigned CC_Lower = 1; //lower case alpha
const unsigned CC_Upper = 2; // upper case alpha
const unsigned CC_White = 4; // whitespace
const unsigned CC_Initial = 8; // allowed as initial character of identifier;
const unsigned CC_Subsequent = 16; // allowed as subsequent character of identifier;
const unsigned CC_Digit = 32;
const unsigned CC_Hex_Digit = 64;
const unsigned CC_Dot = 128;
const unsigned CC_Path_Separator = 256;
const unsigned CC_Operator = 512;

static const class Char_Classification
{
  public:
    unsigned ctype[UCHAR_MAX];
    unsigned char to_upper[UCHAR_MAX];
    unsigned char to_lower[UCHAR_MAX];

    Char_Classification();
    unsigned is_flagged(unsigned char c,unsigned flags) const
    {
      // I am not returning this as a bool in the hope of avoiding unnecessary
      // bit manipulations by the compiler
      return ctype[c] & flags;
    }
    unsigned char to_upper_case(unsigned char c) const
    {
      return to_upper[c];
    }
    unsigned char to_lower_case(unsigned char c) const
    {
      return to_lower[c];
    }
    static const Char_Classification & data();
} &char_classification = Char_Classification::data();

inline unsigned is_upper(unsigned char c)
{
  return char_classification.is_flagged(c,CC_Upper);
}

inline unsigned is_lower(unsigned char c)
{
  return char_classification.is_flagged(c,CC_Lower);
}

inline unsigned is_initial(unsigned char c)
{
  return char_classification.is_flagged(c,CC_Initial);
}

inline unsigned is_subsequent(unsigned char c)
{
  return char_classification.is_flagged(c,CC_Subsequent);
}

inline unsigned is_digit(unsigned char c)
{
  return char_classification.is_flagged(c,CC_Digit);
}

inline unsigned is_hex_digit(unsigned char c)
{
  return char_classification.is_flagged(c,CC_Hex_Digit);
}

inline unsigned is_white(unsigned char c)
{
  return char_classification.is_flagged(c,CC_White);
}

inline unsigned is_path_separator(unsigned char c)
{
  return char_classification.is_flagged(c,CC_Path_Separator);
}

inline unsigned is_flagged(unsigned char c,unsigned flag)
{
  return char_classification.is_flagged(c,flag);
}

#endif
