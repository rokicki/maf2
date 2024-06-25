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


// $Log: gpxlatwa.cpp $
// Revision 1.2  2011/06/14 01:19:47Z  Alun
// Removed declarations of code moved elsewhere
// Revision 1.1  2011/06/11 15:49:50Z  Alun
// New file.
//

/* Utility to translate the word acceptor for a group into a new set of
   generators. */
#include <signal.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * sub_suffix = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,0);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-cos"))
        i++;
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
  int exit_code = 1;
  if (group_filename && !bad_usage)
  {
    MAF & maf = * MAF::create_from_input(true,group_filename,sub_suffix,&container,
                                         CFI_DEFAULT);
    FSA_Simple * xlat_wa = maf.translate_acceptor();
    if (xlat_wa)
    {
      xlat_wa->save_as(maf.properties().subgroup_filename.string(),
                       "_RWS", ".xwa");
      delete xlat_wa;
      exit_code = 0;
    }
    delete &maf;
  }
  else
  {
    cprintf("Usage:\n"
            "gpxlatwa [loglevel] [-rs | -rws] [-schreier]"
            " groupname [subsuffix | cossuffix]\n"
            "where groupname is a GASP rewriting system for a group, which"
            " has been\nsuccesfully processed, and the coset system described"
            " by the substructure file\ngroupname.subssuffix has also been"
            " successfully processed, has named subgroup\ngenerators and has"
            " index 1.\n"
            "This program computes a word acceptor for the group using the"
            " named subgroup\ngenerators and outputs it to"
            " groupname.subsuffix.xwa\n");
    so.usage();
  }
  delete &container;
  return exit_code;
}
