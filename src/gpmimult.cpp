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


// $Log: gpmimult.cpp $
// Revision 1.5  2010/06/10 13:57:14Z  Alun
// All tabs removed again
// Revision 1.4  2010/05/16 23:21:57Z  Alun
// help changed
// Revision 1.3  2009/09/12 18:47:32Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.2  2008/11/02 18:00:00Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.2  2008/11/02 18:59:59Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.1  2007/10/24 21:08:54Z  Alun
// New file.
//

#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_sub.h"
#include "maf_so.h"
#include "equation.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,String prefix,unsigned fsa_format_flags);
    static void do_multiplier(MAF & maf,const Word & word,
                              String output_suffix,
                              String gen_prefix,
                              unsigned fsa_format_flags);

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * sub_suffix = 0;
  String prefix = "_x";
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-pref") || arg.is_equal("-prefix"))
      {
        prefix = argv[i+1];
        i += 2;
        if (i > argc)
          bad_usage = true;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (group_filename == 0)
    {
      group_filename = argv[i];
      i++;
    }
    else if (sub_suffix == 0)
    {
      sub_suffix = argv[i];
      i++;
    }
    else
      bad_usage = true;
  }
  int exit_code;
  if (group_filename && !bad_usage)
  {
    MAF & maf = * MAF::create_from_input(true,group_filename,sub_suffix,
                                         &container,0);
    exit_code = inner(maf,prefix,so.fsa_format_flags);
  }
  else
  {
    cprintf("Usage:\n"
            "gpmimult [loglevel] [format] [-pref str] \ngroupname [cossuffix]\n"
            "where groupname is a GASP rewriting system for a group and"
            " groupname.cossuffix\nis a coset system. groupname.cosssuffix.migm must"
            " have been computed.\n"
            "This program computes the individual multipliers for"
            " each generator in a format compatible with KBMAG.\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,String gen_prefix,unsigned fsa_format_flags)
{
  String_Buffer sb;
  Word_List wl(maf.group_alphabet());
  Ordinal nr_generators = maf.group_alphabet().letter_count();
  Ordinal_Word word(maf.group_alphabet(),2);
  String filename = maf.properties().filename;
  bool is_coset_system = maf.properties().is_coset_system;
  maf.load_fsas(GA_GM);

  if (!maf.fsas.gm)
  {
    maf.container.error_output("Could not load general multiplier for %s!\n",
                               filename.string());
    return 1;
  }

  String output_filename;

  for (Ordinal g1 = PADDING_SYMBOL;g1 < nr_generators;g1++)
  {
    Word_Length i = 0;
    if (g1 != PADDING_SYMBOL)
      word.set_code(i++,g1);
    word.set_length(i);
    String_Buffer sb2;
    do_multiplier(maf,word,maf.multiplier_name(&sb2,word,is_coset_system),
                  gen_prefix,fsa_format_flags);
  }
  delete &maf;
  return 0;
}

/**/

static void do_multiplier(MAF & maf,const Word & word,
                          String output_suffix,
                          String gen_prefix,
                          unsigned fsa_format_flags)
{
  FSA_Simple * fsa = maf.fsas.gm->kbmag_multiplier(maf,word,gen_prefix);
  maf.save_fsa(fsa,output_suffix,fsa_format_flags);
  delete fsa;
}
