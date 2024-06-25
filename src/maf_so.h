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
$Log: maf_so.h $
Revision 1.9  2010/06/10 16:44:49Z  Alun
Fixes to ensure clean compilation with g++
Revision 1.8  2010/06/10 13:58:11Z  Alun
All tabs removed again
Revision 1.7  2010/03/27 19:05:28Z  Alun
Lots of options for parsing numeric options added
,
Revision 1.6  2009/11/08 19:59:56Z  Alun
present() method added
Revision 1.5  2009/10/12 21:56:06Z  Alun
Compatibility of FSA utilities with KBMAG improved.
Compatibility programs produce fewer FSAs KBMAG would not (if any)
Revision 1.4  2009/09/12 18:48:39Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files


Revision 1.3  2007/11/15 22:58:12Z  Alun
*/
#pragma once
#ifndef MAF_SO_INCLUDED
#define MAF_SO_INCLUDED 1

#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

// flags for selecting which standard options are relevant
const unsigned SO_FSA_FORMAT = 1;
const unsigned SO_REDUCTION_METHOD = 2;
const unsigned SO_STDIN = 4;
const unsigned SO_PROVISIONAL = 8;
const unsigned SO_FSA_KBMAG_COMPATIBILITY = 16;
const unsigned SO_STDOUT = 32;
const unsigned SO_GPUTIL = 64;   // Flag to change the name used in the
const unsigned SO_WORDUTIL = 128; // "output is to" message

// clases referred to and defined elsewhere
class Container;

class Standard_Options
{
  public:
    unsigned fsa_format_flags;
    Group_Automaton_Type reduction_method;
    const unsigned relevant;
    unsigned log_level;
    bool verbose;
    bool use_stdin;
    bool use_stdout;
    Container & container;
    Standard_Options(Container & container_,unsigned relevant_);
    bool recognised(char ** argv,int & i);
    void usage(String default_suffix = 0) const;
    /* present() can be used to check for options whose names consist of
       several parts.
       present("-option_name",option) will return true if the option is any of
       "-option_name" , "-option-name", "-optionName" or "-optionname"
    */
    static bool present(String arg,String option);
    bool parse_natural(Unsigned_Long_Long * answer,
                      String arg,
                      Unsigned_Long_Long max_value,String option_name);
    bool parse_natural(Byte * answer,String arg,Byte max_value,String option_name)
    {
      Unsigned_Long_Long local = *answer;
      if (!max_value)
        max_value = UCHAR_MAX;
      bool retcode = parse_natural(&local,arg,max_value,option_name);
      *answer = Byte(local);
      return retcode;
    }

    bool parse_natural(unsigned * answer,String arg,unsigned max_value,String option_name)
    {
      Unsigned_Long_Long local = *answer;
      if (!max_value)
        max_value = UINT_MAX;
      bool retcode = parse_natural(&local,arg,max_value,option_name);
      *answer = (unsigned long) local;
      return retcode;
    }

    bool parse_natural(Element_Count * answer,String arg,Element_Count max_value,String option_name)
    {
      Unsigned_Long_Long local = *answer;
      if (!max_value)
        max_value = MAX_STATES;
      bool retcode = parse_natural(&local,arg,max_value,option_name);
      *answer = Element_Count(local);
      return retcode;
    }
    bool parse_natural(Word_Length * answer,String arg,Word_Length max_value,String option_name)
    {
      Unsigned_Long_Long local = *answer;
      if (!max_value)
        max_value = MAX_WORD;
      bool retcode = parse_natural(&local,arg,max_value,option_name);
      *answer = Word_Length(local);
      return retcode;
    }

    bool parse_total_length(Total_Length * answer,String arg,Total_Length max_value,String option_name)
    {
      Unsigned_Long_Long local = *answer;
      if (!max_value)
        max_value = UNLIMITED;
      bool retcode = parse_natural(&local,arg,max_value,option_name);
      *answer = Total_Length(local);
      return retcode;
    }

};

#endif
