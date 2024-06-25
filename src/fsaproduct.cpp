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


// $Log: fsaproduct.cpp $
// Revision 1.4  2010/05/11 07:53:29Z  Alun
// help changed
// Revision 1.3  2007/12/20 23:25:44Z  Alun
//

/* Program to combine two 1-variable and a 2-variable FSA using
   done by FSA_Factory::product_intersection() in fsa.cpp */
#include <string.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                    String filename3,String filename4,unsigned fsa_format_flags);
/**/

int main(int argc,char ** argv)
{
  int exit_code = 1;
  char *file1 = 0;
  char *file2 = 0;
  char *file3 = 0;
  char *file4 = 0;
  int i = 1;
  Container & container = * Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|
                                SO_STDOUT);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (file1 == 0)
      file1 = argv[i++];
    else if (file2 == 0)
      file2 = argv[i++];
    else if (file3 == 0)
      file3 = argv[i++];
    else if (file4 == 0)
      file4 = argv[i++];
    else
      bad_usage = true;
  }
  if (!bad_usage && file3 && !(file4 && so.use_stdout))
  {
    exit_code = inner(&container,file1,file2,file3,file4,so.fsa_format_flags);
    if (exit_code != 0)
      cprintf("Specified FSA are not compatible\n");
  }
  else
  {
    cprintf("Usage: fsaproduct [log_level] [format] file1 file2 file3 [file4]\n"
            "where file1 contains a 2-variable GASP FSA and file2, file3 contain"
            " 1-variable\nGASP FSA with the same alphabet.\n"
            "The output FSA accepts a word pair (u,v) if (u,v) is accepted by"
            " file1,\nu by file2, and v by file3\n"
            "Output is to file4, if specified, or to stdout otherwise.\n");
    so.usage();
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename1,String filename2,
                 String filename3,String filename4,unsigned fsa_format_flags)
{
  if (!filename3)
    container->set_gap_stdout(true);
  FSA * fsa1 = FSA_Factory::create(filename1,container);
  FSA * fsa2 = FSA_Factory::create(filename2,container);
  FSA * fsa3 = FSA_Factory::create(filename3,container);
  FSA * answer = FSA_Factory::product_intersection(*fsa1,*fsa2,*fsa3);
  if (answer)
  {
    answer->save(filename4,fsa_format_flags);
    delete answer;
  }
  delete fsa1;
  delete fsa2;
  delete fsa3;
  return answer ? 0 : 1;
}

