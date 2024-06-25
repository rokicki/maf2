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


// $Log: gpaxioms.cpp $
// Revision 1.5  2010/06/10 13:57:10Z  Alun
// All tabs removed again
// Revision 1.4  2010/05/16 21:00:48Z  Alun
// argument processing and help changed
// Revision 1.3  2009/09/12 18:47:28Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.2  2007/10/24 21:15:31Z  Alun
//

#include <time.h>
#include "maf.h"
#include "container.h"
#include "maf_so.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,bool midfa,Compositor_Algorithm algorithm);

int main(int argc,char ** argv)
{
  int i = 1;
  bool cosets = false;
  bool midfa = false;
  char * group_filename = 0;
  char * sub_suffix = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_KBMAG_COMPATIBILITY);
  bool bad_usage = false;
  Compositor_Algorithm algorithm = CA_Hybrid;
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
      else if (arg.is_equal("-serial") || arg.is_equal("-n"))
      {
        algorithm = CA_Serial;
        i++;
      }
      else if (arg.is_equal("-hybrid"))
      {
        algorithm = CA_Hybrid;
        i++;
      }
      else if (arg.is_equal("-parallel"))
      {
        algorithm = CA_Parallel;
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
    MAF * maf = MAF::create_from_input(cosets,group_filename,sub_suffix,&container,0);
    time_t now = time(0);
    exit_code = inner(*maf,midfa,algorithm);
    container.progress(1,"Elapsed time %ld\n",long(time(0) - now));
    delete maf;
  }
  else
  {
    cprintf("Usage:\n"
            "gpaxioms [loglevel] [-midfa] [-serial | -hybrid | -parallel]"
            " groupname\n[-cos [cossuffix | subsuffix]]\n\n"
            "where groupname contains a GASP rewriting system for a group,"
            " and, if the -cos\noption is used, groupname.cossuffix"
            " is a coset system.\n"
            "This program checks the relevant automatic structure is correct\n"
            "If -midfa is specified (only relevant with coset systems) the"
            " checks are\nperformed using the MIDFA multipliers.\n"
            "If -serial is specified all necessary multipliers are built from"
            " individual\nmultipliers. If -parallel is specified one"
            " multiplier containing all the\nrequired words is built. With"
            " the default option, -hybrid, a multiplier is\nbuilt for all"
            " words of length 2 that may be needed, but for longer words\n"
            "individual multipliers are constructed. It is not possible to"
            " know which\nof these algorithms will be quickest.\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

static int inner(MAF & maf,bool midfa,Compositor_Algorithm algorithm)
{
  General_Multiplier * gm = (General_Multiplier *) maf.load_fsas(midfa ? GA_GM : GA_DGM);
  if (!maf.check_axioms(*gm,algorithm))
  {
    maf.container.error_output("Axiom check failed\n");
    return 1;
  }
  maf.container.progress(1,"Axiom check succeeded.\n");
  return 0;
}

