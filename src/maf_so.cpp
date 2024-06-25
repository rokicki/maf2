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


// $Log: maf_so.cpp $
// Revision 1.10  2010/06/10 13:57:40Z  Alun
// All tabs removed again
// Revision 1.9  2010/05/02 10:43:10Z  Alun
// method for parsing numeric arguments added with support for ACE style
// multiplier suffixes.
// More automata supported by SO_REDUCTION_METHOD
// Revision 1.8  2009/11/08 21:06:20Z  Alun
// Finally added function for dealing with variations permitted for options with_
// Revision 1.7  2009/10/12 22:02:42Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSAs KBMAG would not (if any)
// Revision 1.6  2009/09/12 18:47:52Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.5  2008/11/02 18:00:00Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.6  2008/11/02 18:59:59Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.5  2008/10/08 17:10:10Z  Alun
// More formatting options added for FSAs
// Revision 1.4  2008/08/22 07:41:28Z  Alun
// "Early Sep 2008 snapshot"
// Revision 1.3  2007/11/15 22:58:09Z  Alun
//

#include "awcc.h"
#include "container.h"
#include "maf_so.h"
#include "fsa.h"
#include "mafctype.h"

Standard_Options::Standard_Options(Container & container_,unsigned relevant_) :
  container(container_),
  relevant(relevant_),
  fsa_format_flags(0),
  log_level(2),
  verbose(false),
  use_stdin(false),
  use_stdout(false),
  reduction_method(GAT_Auto_Select)
{
}

/**/

bool Standard_Options::present(String arg,String option)
{
  if (arg.is_equal(option))
    return true;
  String_Buffer sb;
  sb.set(option);
  size_t l = sb.get().length();
  size_t i = 0;
  Letter * buffer = sb.reserve(l,0,true);
  for (i = 0; i < l;i++)
    if (option[i] == '_')
      buffer[i] = '-';
  if (arg.is_equal(buffer))
    return true;
  Letter * to = buffer;
  for (i = 0; i < l;i++)
    if (option[i] != '_')
      *to++ = buffer[i];
  *to++ = 0;
  if (arg.is_equal(buffer))
    return true;
  to = buffer;
  bool convert = false;
  for (i = 0; i < l;i++)
    if (option[i] != '_')
    {
      if (convert)
      {
        *to++ = char_classification.to_upper_case(option[i]);
        convert = false;
      }
      else
        *to++ = buffer[i];
    }
    else
      convert = true;
  *to++ = 0;
  if (arg.is_equal(option))
    return true;
  return arg.is_equal(option,true);
}

/**/

bool Standard_Options::parse_natural(Unsigned_Long_Long * answer,String arg,
                                   Unsigned_Long_Long max_value, String option)
{
  Unsigned_Long_Long v = 0;
  const Letter * s = arg;

  if (s == 0)
  {
    container.usage_error("Option %s requires a numeric parameter value\n",
                          option.string());
    return false;
  }

  for (;*s;s++)
  {
    if (!is_digit(*s))
      break;
    v = v*10 + *s -'0';
    if (max_value && v > max_value)
    {
      container.usage_error("Expected value between 0 and %llu for option %s\n",
                            max_value,option.string());
      return false;
    }
  }
  if (*s)
  {
    Unsigned_Long_Long multiplier = 0;
    switch (*s)
    {
      case 'k':
        multiplier = 1000;
        break;
      case 'K':
        multiplier = 1024;
        break;
      case 'm':
        multiplier = 1000000;
        break;
      case 'M':
        multiplier = 1024*1024;
        break;
      case 'g':
        multiplier = 1000000000;
        break;
      case 'G':
        multiplier = 1024*1024*1024;
        break;
    }
    if (multiplier)
    {
      s++;
      if (max_value && v > max_value/multiplier)
      {
        container.usage_error("Expected value between 0 and %llu for option %s\n",
                              max_value,option.string());
        return false;
      }
      v *= multiplier;
    }
    if (*s)
    {
      container.usage_error("Expected number, not \"%s\", as value for option %s\n",
                             arg.string(),option.string());
      return false;
    }
  }
  *answer = v;
  return true;
}

/**/

bool Standard_Options::recognised(char ** argv,int &i)
{
  String arg = argv[i];

  if (relevant & SO_FSA_FORMAT)
  {
    if (arg.is_equal("-op"))
    {
      arg = argv[i+1];
      if (arg.is_equal("s"))
      {
        fsa_format_flags |= FF_SPARSE;
        i += 2;
        return true;
      }
      if (arg.is_equal("d"))
      {
        fsa_format_flags |= FF_DENSE;
        i += 2;
        return true;
      }
      return false;
    }
    if (arg.is_equal("-csn") || arg.is_equal("-comment"))
    {
      fsa_format_flags |= FF_COMMENT;
      i++;
      return true;
    }
    if (present(arg,"-annotate_transitions"))
    {
      fsa_format_flags |= FF_ANNOTATE_TRANSITIONS;
      i++;
      return true;
    }
    if (arg.is_equal("-annotate"))
    {
      fsa_format_flags |= FF_ANNOTATE;
      i++;
      return true;
    }
    if (arg.is_equal("-dense"))
    {
      fsa_format_flags |= FF_DENSE;
      i++;
      return true;
    }
    if (arg.is_equal("-sparse"))
    {
      fsa_format_flags |= FF_SPARSE;
      i++;
      return true;
    }
  }

  if (relevant & SO_STDOUT)
  {
    if (arg.is_equal("-o"))
    {
      use_stdout = true;
      i++;
      return true;
    }
  }

  if (relevant & SO_FSA_KBMAG_COMPATIBILITY)
  {
    if (arg.is_equal("-ip"))
    {
      arg = argv[i+1];
      if (arg.is_equal("s"))
      {
        i += 2;
        return true;
      }
      if (arg.is_equal("d"))
      {
        i += 2;
        return true;
      }
      return false;
    }
    if (arg.is_equal("-l"))
    {
      i++;
      return true;
    }
    if (arg.is_equal("-h"))
    {
      i++;
      return true;
    }
  }


  if (arg.is_equal("-silent") || arg.is_equal("-s"))
  {
    container.set_log_level(log_level = 0);
    i++;
    return true;
  }

  if (arg.is_equal("-quiet"))
  {
    container.set_log_level(log_level = 1);
    i++;
    return true;
  }

  if (arg.is_equal("-v") ||
      arg.is_equal("-vv") ||
      arg.is_equal("-verbose") || present(arg,"-very_verbose"))
  {
    verbose = true;
    container.set_log_level(log_level = 2);
    i++;
    return true;
  }

  if (relevant & SO_REDUCTION_METHOD)
  {
    if (arg.is_equal("-cosets"))
    {
      reduction_method = GAT_Coset_Table;
      i++;
      return true;
    }

    if (arg.is_equal("-diff1c"))
    {
      reduction_method = GAT_Primary_Difference_Machine;
      i++;
      return true;
    }

    if (arg.is_equal("-diff2"))
    {
      reduction_method = GAT_Full_Difference_Machine;
      i++;
      return true;
    }

    if (arg.is_equal("-diff2c"))
    {
      reduction_method = GAT_Correct_Difference_Machine;
      i++;
      return true;
    }

    if (arg.is_equal("-gm"))
    {
      reduction_method = GAT_General_Multiplier;
      i++;
      return true;
    }

    if (arg.is_equal("-dgm"))
    {
      reduction_method = GAT_Deterministic_General_Multiplier;
      i++;
      return true;
    }

    if (present(arg,"-fast_kbprog"))
    {
      reduction_method = GAT_Fast_RWS;
      i++;
      return true;
    }

    if (arg.is_equal("-kbprog"))
    {
      reduction_method = GAT_Minimal_RWS;
      i++;
      return true;
    }

    if (relevant & SO_PROVISIONAL)
    {
      if (arg.is_equal("-pkbprog"))
      {
        reduction_method = GAT_Provisional_RWS;
        i++;
        return true;
      }

      if (arg.is_equal("-pdiff1"))
      {
        reduction_method = GAT_Provisional_DM1;
        i++;
        return true;
      }

      if (arg.is_equal("-pdiff2"))
      {
        reduction_method = GAT_Provisional_DM2;
        i++;
        return true;
      }
    }
  }
  if (relevant & SO_STDIN)
  {
    if (arg.is_equal("-i"))
    {
      use_stdin = true;
      i++;
      return true;
    }
  }
  return false;
}

/**/

void Standard_Options::usage(String suffix) const
{
#define cprintf container.error_output
  if (suffix)
  {
    const char *fake_file;
    if (relevant & SO_GPUTIL)
      fake_file = "rws";
    else if (relevant & SO_WORDUTIL)
      fake_file = "words";
    else
      fake_file = "fsa";
    cprintf("The default filename is input_file%s . So output is to:\n"
            "  %s2 if output_file was specified as %s2,\n"
            "  %s1%s if output_file is not specified and input_file is %s1,\n"
            "  stdout if -o is specified instead of output_file\n",
            suffix.string(),fake_file,fake_file,fake_file,suffix.string(),
            fake_file);
  }
  cprintf("[loglevel] can be one of the following:\n"
          "  -verbose or -v : regular progress reports are made\n"
          "  -quiet         : progress reports are made at significant points\n"
          "  -silent        : there are no progress reports at all\n");

  if (relevant & SO_FSA_FORMAT)
    cprintf("[format] options:\n"
            "  -dense or -op d selects dense format, -sparse or -op s selects"
            " sparse format.\n"
            "  format defaults to dense for one variable FSAs, sparse for"
            " two variable FSAs.\n"
            "  -csn or -comment causes the transitions to be commented with"
            " state numbers\n"
            "  -annotate adds state definition information to the transitions\n"
            "  -annotate_transitions adds comments to sparse transitions"
            " of product FSAs\n");

  if (relevant & SO_REDUCTION_METHOD)
  {
    cprintf("[reduction method] can be any one of the following:\n"
            "  -cosets,-fastkbprog,-kbprog,-dgm,-gm,-diff2c,-diff2,-diff1c");

    if (relevant & SO_PROVISIONAL)
      cprintf(",-pdiff2,-pdiff1,-pkbprog.\n");
    else
      cprintf("\n");
    cprintf("If unspecified (recommended) MAF will use the best available"
            " method.\n");
  }
}
