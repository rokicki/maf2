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


// $Log: gpsubwa.cpp $
// Revision 1.5  2010/05/16 19:39:36Z  Alun
// Help and other messages changed. Factory method now has a Word_Reducer parameter
// so we don not have to use difference machine to perform coset reduction
// Revision 1.4  2009/09/12 18:47:36Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Language_Size data type introduced and used where appropriate
// Revision 1.3  2008/11/02 20:55:50Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.3  2008/11/02 21:55:49Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.2  2008/06/08 10:48:12Z  Alun
// "Aug 2008 snapshot"
// Revision 1.1  2007/10/24 21:08:56Z  Alun
// New file.
//

#include <string.h>
#include <limits.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"
#include "equation.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,unsigned fsa_format_flags);

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * sub_suffix = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|
                                SO_REDUCTION_METHOD|SO_PROVISIONAL);
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
    MAF & maf = * MAF::create_from_input(true,group_filename,sub_suffix,
                                         &container,0);
    if (!maf.load_reduction_method(so.reduction_method))
    {
      cprintf("Unable to reduce words using selected mechanism\n");
      delete &maf;
      exit_code = 1;
    }
    else
      exit_code = inner(maf,so.fsa_format_flags);
  }
  else
  {
    cprintf("Usage:\n"
            "gpsubwa [loglevel] [format] [reduction_method] groupname [subsuffix]\n"
            "where groupname is a GASP rewriting system, and"
            " groupname.subsuffix (or\ngroupname.sub, if subsuffix is not"
            " specified), is a substructure file.\ngroupname.gm and a method"
            " of performing coset word reduction must have been\npreviously"
            " computed\n"
            "This program attempts to compute and check the subgroup word-acceptor\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,unsigned fsa_format_flags)
{
  Container & container = maf.container;

  const FSA * dm2 = maf.load_fsas(GA_DIFF2);
  if (!dm2 && maf.properties().is_normal_coset_system)
  {
    maf.container.error_output("Cannot create subgroup word-acceptor for a"
                               " normal closure coset system\nwithout an"
                               " automatic structure.\n");
    maf.container.error_output("Could not load coset difference machine for %s!\n",
                               maf.properties().filename.string().string());
    delete &maf;
    return 1;
  }

  Word_Reducer * wr = maf.take_word_reducer();
  FSA_Simple * subwa = maf.subgroup_word_acceptor(*wr,dm2);
  if (subwa)
  {
    Language_Size subwa_count = subwa->language_size(true);
    if (subwa_count == LS_INFINITE)
      container.progress(1,"The subgroup is infinite\n");
    else if (subwa_count == LS_HUGE)
      container.progress(1,"The subgroup is very large but finite\n");
    else
      container.progress(1,"The subgroup contains " FMT_LS " elements\n",subwa_count);
    container.progress(1,"The subgroup word-acceptor has " FMT_ID " states.\n",
                       subwa->state_count());

    delete subwa;
    maf.save_fsa(subwa,".subwa",fsa_format_flags);
  }
  else
    container.error_output("The subgroup word-acceptor could not be created\n");
  delete wr;
  delete &maf;
  return 0;
}

