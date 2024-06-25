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


// $Log: gpminkb.cpp $
// Revision 1.7  2010/05/16 21:04:07Z  Alun
// Help changed
// Revision 1.6  2009/10/13 22:43:04Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.5  2009/09/12 18:47:33Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2008/06/08 10:45:20Z  Alun
// "Aug 2008 snapshot"
// Revision 1.3  2007/12/20 23:25:44Z  Alun
//

#include "maf.h"
#include "container.h"
#include "mafauto.h"
#include "maf_so.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf);

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * sub_suffix = 0;
  bool cosets = false;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_KBMAG_COMPATIBILITY);
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
      else if (!so.recognised(argv,i))
        bad_usage = true;
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
  int exit_code;
  if (group_filename && !bad_usage)
  {
    MAF & maf = * MAF::create_from_input(cosets,group_filename,sub_suffix,
                                         &container);
    exit_code = inner(maf);
  }
  else
  {
    cprintf("Usage:\n"
            "gpminkb [loglevel] [] groupname [-cos [subsuffix | cossuffix]]\n"
            "where groupname is the filename of a GASP rewriting system\n."
            "groupname.diff2, groupname.wa, and groupname.gm must have"
            " been computed (or, if the -cos option is used,"
            " groupname.cossuffix.midiff2,\n groupname.cossuffix.wa"
            " and groupname.cossuffix.migm)\n"
            "This program computes various automata:\n"
            "groupname.minred : The FSA which accepts the minimally reducible"
            "words\n"
            "groupname.minkb : The FSA which accepts the primary equations\n"
            "groupname.maxkb : The FSA which accepts the primary and secondary"
            " equations\n"
            "groupname.diff1c: The correct primary difference machine\n"
            "groupname.diff2c: The correct difference machine\n"
            "For coset systems the last two FSA have suffixes midiff1c and"
            " midiff2 and otuput is to groupname.cossuffix.minred etc.\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf)
{
  Group_Automata automata;
  int retcode = 1;
  if (automata.load_vital(maf))
  {
    automata.grow_automata(&maf.rewriter_machine(),0,GA_MINRED|GA_MINKB|GA_DIFF1C|GA_DIFF2C,0);
    retcode = 0;
  }
  delete &maf;
  return retcode;
}

