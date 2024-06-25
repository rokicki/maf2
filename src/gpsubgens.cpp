/*
  Copyright 2008,2009 Alun Williams
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


// $Log: gpsubgens.cpp $
// Revision 1.4  2009/09/12 19:46:43Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// 
// 
// Revision 1.3  2009/03/03 14:28:04Z  Alun
// Typo corrected
// Revision 1.2  2008/11/02 21:45:54Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.1  2008/07/29 22:42:08Z  Alun
// New file.
// Revision 1.1  2007/11/15 22:57:50Z  Alun
// New file.
//

#include <string.h>
#include <stdlib.h>
#include "maf.h"
#include "container.h"
#include "mafauto.h"
#include "maf_so.h"
#include "mafctype.h"
#include "maf_sub.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,String sub_suffix,int n);

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * sub_suffix = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,0);
  bool bad_usage = false;
#define cprintf container.error_output
  MAF::Options options;
  options.no_differences = true;
  int n = 2;

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
    else if (is_digit(*argv[i]))
    {
      n = atoi(argv[i++]);
      cprintf("%d\n",n);
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
    MAF & maf = * MAF::create_from_input(false,group_filename,0,&container,
                                         CFI_DEFAULT,&options);
    exit_code = inner(maf,sub_suffix,n);
  }
  else
  {
    cprintf("Usage:\n"
            "gpsubgens [loglevel] groupname [subsuffix] [n]\n"
            "where groupname is the filename of a rewriting system and n>=2 .\n"
            "groupname.wa must have been previously computed\n"
            "This program computes generators for the g^n subgroup\n"
            " The subgroup generators are normally output to a"
            " substructure file called\ngroupname.subsuffix.");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,String sub_suffix,int n)
{
  int exit_code = 0;
  Subalgebra_Descriptor sub(maf);
  Container & container = maf.container;
  MAF * coset_system = sub.create_coset_system(false,false);
  if (!sub_suffix)
    sub_suffix = ".sub_power";
  coset_system->realise_rm();
  const FSA * wa = maf.load_fsas(GA_WA);
  int time_out = 0;

  FSA::Word_Iterator wi(*wa);
  Ordinal_Word lhs(coset_system->alphabet);
  Ordinal_Word rhs(coset_system->alphabet);
  rhs.append(coset_system->properties().coset_symbol);
  int max_length = 24;
  String_Buffer sb;

  for (Word_Length stop_length = 1; stop_length < max_length ;stop_length++)
    for (State_ID si = wi.first();
         si;si = wi.next(wi.word.length() < stop_length))
      if (wa->is_accepting(si) && wi.word.length() == stop_length)
      {
        if (wi.word.value(0) <= maf.inverse(wi.word.value(stop_length-1)))
        {
          if (container.status_needed(1))
          {
            wi.word.format(&sb);
            container.status(1,1,"Checking word %s. Generators now " FMT_ID
                                 ".Timeout %d\n",
                             sb.get().string(),sub.generator_count(),
                             20-time_out++);
          }

          lhs = rhs;
          for (int i = 0; i < n;i++)
            lhs += wi.word;
          if (coset_system->add_axiom(lhs,rhs,AA_ADD_TO_RM|AA_DEDUCE|AA_POLISH))
          {
            sub.add_generator(Subword(lhs,1));
            time_out = 0;
          }
          if (time_out > 20)
            break;
        }
      }
  sub.save_as(maf.properties().filename,sub_suffix);
  delete coset_system;
  delete &maf;
  return exit_code;
}

