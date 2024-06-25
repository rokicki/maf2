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


// $Log: makecos.cpp $
// Revision 1.4  2010/05/11 17:07:43Z  Alun
// Argument parsing changed not to need strcmp(). Help changed.
// Revision 1.3  2009/09/12 18:47:55Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2007/10/24 21:15:32Z  Alun
//

#include "maf.h"
#include "fsa.h"
#include "container.h"
#include "maf_sub.h"

/**/

static int inner(Container * container,String group_filename,String subgroup_suffix,
                 bool add_generators,bool no_inverse_generators)
{
  unsigned flags = add_generators ? CFI_NAMED_H_GENERATORS : 0;
  if (no_inverse_generators)
    flags |= CFI_NO_INVERSES;
  MAF * maf = MAF::create_from_substructure(group_filename,subgroup_suffix,
                                            container,flags|CFI_ALLOW_CREATE);
  delete maf;
  delete container;
  return 0;
}

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  bool bad_usage = false;
  bool sub_generators = false;
  bool no_inverse_sub_generators = false;
  char * group_filename = 0;
  char * subgroup_filename = 0;
  Container * container = MAF::create_container();
#define cprintf container->error_output

  while (i < argc && !bad_usage)
  {
    String arg = argv[i];
    if (arg.is_equal("-sg"))
    {
      sub_generators = true;
      i++;
    }
    else if (arg.is_equal("-ni"))
    {
      no_inverse_sub_generators = true;
      i++;
    }
    else if (*argv[i] == '-')
      bad_usage = true;
    else if (group_filename == 0)
    {
      group_filename = argv[i];
      i++;
    }
    else if (subgroup_filename == 0)
    {
      subgroup_filename = argv[i];
      i++;
    }
  }

  if (!bad_usage && group_filename)
    return inner(container,group_filename,subgroup_filename,sub_generators,
                 no_inverse_sub_generators);
  else
  {
    cprintf("Usage: makecosfile [-sg] [-ni] rwsname [subsuffix]\n"
            "where rwsname is a GASP rewriting system for a group or monoid"
            " and\nrwsname.subsuffix is a substructure file describing a"
            " subgroup.\n"
            "This program generates the input file for the corresponding"
            " coset system.\n"
            "The -sg option causes the subgroup generators to be used as "
            "additional\ngenerators. If used inverses for these additional "
            "generators are normally also\nadded. The -ni option stops this,"
            " so only the specified subgroup generator\nnames are added.\n");
    delete container;
    return 1;
  }
}
