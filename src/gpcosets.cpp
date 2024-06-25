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


// $Log: gpcosets.cpp $
// Revision 1.3  2010/05/16 20:09:16Z  Alun
// construction of coset table can now use any word reducer, and does not need
// multiplier.
// help changed
// Revision 1.2  2009/09/12 18:47:28Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.1  2008/09/18 20:55:04Z  Alun
// New file.
// Revision 1.1  2007/11/15 22:57:50Z  Alun
// New file.
//

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
  char * group_filename = 0;
  char * sub_suffix = 0;
  bool cosets = false;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_REDUCTION_METHOD|SO_FSA_FORMAT);
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
  int exit_code = 1;
  if (group_filename && !bad_usage)
  {
    MAF & maf = * MAF::create_from_input(cosets,group_filename,sub_suffix,&container,
                                         CFI_DEFAULT);
    if (!maf.load_fsas(GA_WA))
    {
      cprintf("Unable to load word-acceptor\n");
      delete &maf;
    }
    else if (!maf.load_reduction_method(so.reduction_method))
    {
      cprintf("Unable to reduce words using selected mechanism\n");
      delete &maf;
    }
    else
      exit_code = inner(maf,so.fsa_format_flags);
  }
  else
  {
    cprintf("Usage:\n"
            "gpcosets [loglevel] [format] [reduction_method] groupname"
            " [-cos [subsuffix]]\n"
            "where groupname is a GASP rewriting system describing a group.\n"
            "If the -cos option is specified then the coset table of the"
            " subgroup in\ngroupname.subsuffix is computed (or groupname.sub"
            " if no explicit subsuffix is\nspecified). If -cos is not"
            " specified then the coset table for the trivial\nsubgroup is"
            " created.\n"
            "A word-acceptor and an automaton which can reduce words to"
            " coset\nrepresentatives must have been previously computed\n");
    so.usage();
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,unsigned fsa_format_flags)
{
  int exit_code = 1;
  FSA_Simple * coset_table = 0;
  Word_Reducer * wr = maf.take_word_reducer();
  if ((coset_table = maf.coset_table(*wr,*maf.fsas.wa))!=0)
  {
    maf.save_fsa(coset_table,GAT_Coset_Table,fsa_format_flags);
    delete coset_table;
    exit_code = 0;
  }
  delete wr;
  delete &maf;
  return exit_code;
}

