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


// $Log: gpmigmdet.cpp $
// Revision 1.4  2010/05/16 21:15:10Z  Alun
// help changed. determinise_multiplier() is now an FSA_Factory method.
// Revision 1.3  2009/09/12 18:47:32Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2008/11/04 22:03:46Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.2  2008/11/04 23:03:45Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
//

/* Program to calculate the determininistic general multiplier for a
   coset system given the MIDFA multiplier. */
#include <string.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,unsigned fsa_format_flags);

int main(int argc,char ** argv)
{
  int i = 1;
  bool cosets = true;
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
      if (!so.recognised(argv,i))
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
    MAF & maf = * MAF::create_from_input(cosets,group_filename,sub_suffix,
                                         &container,0);
    exit_code = inner(maf,so.fsa_format_flags);
  }
  else
  {
    cprintf("Usage:\n"
            "gpmigmdet [loglevel] [format] groupname [subsuffix | cossuffix]\n"
            "where groupname is a GASP rewriting system for a group and"
            " groupname.subsuffix\nis a substructure file describing a"
            " subgroup.\n"
            " groupname.cossuffix.migm must have been computed\n"
            "This program computes the determinised version of the multiplier"
            " and outputs it\nto groupname.cossuffix.gm.\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,unsigned fsa_format_flags)
{
  maf.load_fsas(GA_GM);

  if (!maf.fsas.gm)
  {
    maf.container.error_output("Could not load MIDFA general multiplier for %s!\n",
                               maf.properties().filename.string().string());
    return 1;
  }

  FSA_Simple * fsa = FSA_Factory::determinise_multiplier(*maf.fsas.gm);
  maf.save_fsa(fsa,GAT_Deterministic_General_Multiplier,fsa_format_flags);
  delete fsa;
  delete &maf;
  return 0;
}

