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


// $Log: gpmult2.cpp $
// Revision 1.4  2010/05/16 23:29:13Z  Alun
// Help changed. Arguments of General_Multiplier::composite() have changed.
// Revision 1.3  2009/09/14 09:57:51Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2007/10/24 21:15:32Z  Alun
//

#include <stdlib.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"
#include "mafword.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,bool midfa,Ordinal g1,Ordinal g2,
                   unsigned fsa_format_flags);

int main(int argc,char ** argv)
{
  int i = 1;
  int g1 = -1,g2 = -1;
  bool cosets = false;
  bool midfa = false;
  char * group_filename = 0;
  char * sub_suffix = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];

      if (arg.is_equal("-cos"))
      {
        cosets = true;
        i++;
      }
      else if (arg.is_equal("-midfa") ||
               arg.is_equal("-MIDFA") ||
               arg.is_equal("-mi"))
      {
        midfa = true;
        i++;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (g1 == -1)
    {
      g1 = Ordinal(atoi(argv[i]));
      i++;
    }
    else if (g2 == -1)
    {
      g2 = Ordinal(atoi(argv[i]));
      i++;
    }
    else if (group_filename == 0)
    {
      group_filename = argv[i];
      i++;
    }
    else if (cosets && sub_suffix == 0)
    {
      sub_suffix = argv[i];
      i++;
    }
    else
      bad_usage = true;
  }
  int exit_code = 1;
  if (!bad_usage && group_filename && g1 > 0 && g2 > 0)
  {
    MAF * maf = MAF::create_from_input(cosets,group_filename,sub_suffix,
                                         &container,0);
    Ordinal max_g = cosets ? maf->properties().coset_symbol :
                             maf->properties().nr_generators;
    if (g1 <= max_g && g2 <= max_g)
      exit_code = inner(*maf,midfa,Ordinal(g1),Ordinal(g2),
                        so.fsa_format_flags);
    else
      container.usage_error("Invalid generator numbers."
                            " Generators must be between 1 and %d\n",
                            max_g);
    delete maf;
  }
  else
  {
    cprintf("Usage: gpmult2 [loglevel] [format] [-midfa] g1 g2"
            " groupname [-cos [cossuffix | subsuffix]]\n"
            "where groupname is a GASP rewriting system and, if the -cos"
            " option is specified, groupname.cossuffix is a coset system.\n"
            "groupname.gm2 or groupname.coxsuffix.gm2 must previously have"
            " been computed.\n"
            "g1 and g2 must each be a number from 1..n representing a"
            " generator.\nThe multiplier for g1*g2 is output to groupname.m<g1>_<g2>.\n"
            "If the -cos option is used then -midfa causes gpmult2 to use\n"
            "groupname.cossuffix.migm2 instead of groupname.cossuffix.gm2 and"
            " to output to\ngroupname.cossuffix.mi<g1>_<g2>\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,bool midfa,Ordinal g1,Ordinal g2,unsigned fsa_format_flags)
{
  String_Buffer sb;
  Word_List wl(maf.alphabet);
  Ordinal_Word word(maf.alphabet,2);

  if (!maf.properties().is_coset_system)
    midfa = false;
  General_Multiplier * gm = (General_Multiplier *) maf.load_fsas(midfa ? GA_GM2 : GA_DGM2);
  if (!gm)
  {
    maf.container.error_output("Could not load required multiplier!\n");
    return 1;
  }

  String output_filename;
  Word_Length i = 0;
  if (g1 > 0)
    word.set_code(i++,g1-1);
  if (g2 > 0)
    word.set_code(i++,g2-1);
  word.set_length(i);
  wl.add(word);
  FSA_Simple * fsa = gm->composite(maf,wl);
  if (fsa)
  {
    maf.save_fsa(fsa,maf.multiplier_name(&sb,word,midfa),fsa_format_flags);
    delete fsa;
  }
  return 0;
}

