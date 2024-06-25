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


// $Log: gpgenmult2.cpp $
// Revision 1.5  2010/05/16 23:54:39Z  Alun
// Help changed, many other minor changes
// Revision 1.4  2009/09/12 18:47:30Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2008/11/02 20:41:34Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.3  2008/11/02 21:41:33Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.2  2007/10/24 21:15:31Z  Alun
//

#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"
#include "mafword.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,bool midfa,unsigned fsa_format_flags);

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * subgroup_suffix = 0;
  bool cosets = false;
  bool midfa = false;
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
    else if (group_filename == 0)
    {
      group_filename = argv[i];
      i++;
    }
    else if (cosets && subgroup_suffix == 0)
      subgroup_suffix = argv[i];
    else
      bad_usage = true;
  }
  int exit_code;
  if (group_filename && !bad_usage)
  {
    MAF * maf = MAF::create_from_input(cosets,group_filename,subgroup_suffix,
                                 &container,0);
    exit_code = inner(*maf,midfa,so.fsa_format_flags);
    delete maf;
  }
  else
  {
    cprintf("Usage: gpgenmult2 [loglevel] [format] [-midfa] groupname"
            " [-cos [cossuffix | subsuffix]]\n"
            "where groupname contains a GASP rewriting system and, if the"
            " -cos option is\nused, groupname.cossuffix contains a coset"
            " system for a subgroup of groupname.\n"
            "groupname.gm or groupname.cossuffix.gm must have been previously"
            " computed.\n"
            "The program computes the general multiplier for words of length"
            " 2 and outputs it to groupname.gm2, or groupname.cossuffix.gm2\n"
            "If the -cos option is used then -midfa causes gpgenmult2 to"
            " use\ngroupname.cossuffix.migm, and to create"
            " groupname.cossuffix.migm2 instead\n"
            "of groupname.cossuffix.gm2\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,bool midfa,unsigned fsa_format_flags)
{
  String_Buffer sb;
  Word_List wl(maf.group_alphabet());
  Ordinal nr_generators = maf.group_alphabet().letter_count();
  Ordinal_Word word(maf.group_alphabet(),2);
  String filename = maf.properties().filename;
  if (!maf.properties().is_coset_system)
    midfa = false;
  maf.load_group_fsas(GA_GM);
  General_Multiplier * gm = (General_Multiplier *) maf.load_fsas(midfa ? GA_GM : GA_DGM);

  if (!gm || !maf.group_fsas().gm)
  {
    maf.container.error_output("Unable to load multiplier for %s\n",filename.string());
    return 1;
  }

  for (Ordinal g1 = PADDING_SYMBOL;g1 < nr_generators;g1++)
  {
    Word_Length i = 0;
    if (g1 != PADDING_SYMBOL)
      word.set_code(i++,g1);
    for (Ordinal g2 = PADDING_SYMBOL;g2 < nr_generators;g2++)
    {
      Word_Length j = i;
      if (g2 != PADDING_SYMBOL)
        word.set_code(j++,g2);
      word.set_length(j);
      wl.add(word);
    }
  }
  FSA_Simple * fsa = gm->composite(maf,wl);
  maf.save_fsa(fsa,midfa ? ".migm2" : ".gm2",fsa_format_flags);
  delete fsa;
  return fsa==0;
}
