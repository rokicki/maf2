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


// $Log: fsalequal.cpp $
// Revision 1.5  2010/05/11 07:55:53Z  Alun
// help changed
// Revision 1.4  2009/05/06 15:08:53Z  Alun
// Now relies on FSA_Simple::compare() for most of the work
// Revision 1.3  2007/12/20 23:25:44Z  Alun
//

/* Program to compare the language of two FSA */
#include <string.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"
#include "alphabet.h"

int main(int argc,char ** argv);
  static int inner(Container & container,String filename1,String filename2);
/**/

int main(int argc,char ** argv)
{
  int exit_code = 1;
  char *file1 = 0;
  char *file2 = 0;
  int i = 1;
  Container & container = * Container::create();
  Standard_Options so(container,0);
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
    else
      bad_usage = true;
  }
  if (!bad_usage && file2)
    exit_code = inner(container,file1,file2);
  else
  {
    cprintf("Usage: fsalequal [log_level] file1 file2\n"
            "where file1 and file2 contain GASP FSA with the"
            " same alphabet.\n"
            "The exit code is 0 if the FSA accept the same language and 4 or more if not.\n");
    so.usage();
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container & container,String filename1,String filename2)
{
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,&container);
  FSA_Simple * fsa2 = FSA_Factory::create(filename2,&container);
  if (fsa1->alphabet_size() != fsa2->alphabet_size())
  {
    cprintf("The alphabets are different sizes\n");
    delete fsa1;
    delete fsa2;
    return 4;
  }
  if (fsa1->base_alphabet != fsa2->base_alphabet)
  {
    cprintf("The alphabets are different\n");
    delete fsa1;
    delete fsa2;
    return 5;
  }
  fsa1->remove_rewrites();
  if (fsa1->has_multiple_initial_states())
  {
    FSA_Simple * nfsa1 = FSA_Factory::determinise(*fsa1);
    delete fsa1;
    fsa1 = nfsa1;
  }
  else
  {
    fsa1->set_label_type(LT_Unlabelled);
    FSA_Simple * nfsa1 = FSA_Factory::minimise(*fsa1);
    delete fsa1;
    fsa1 = nfsa1;
    fsa1->change_flags(0,GFF_BFS);
  }

  fsa2->remove_rewrites();
  if (fsa2->has_multiple_initial_states())
  {
    FSA_Simple * nfsa2 = FSA_Factory::determinise(*fsa2);
    delete fsa2;
    fsa2 = nfsa2;
  }
  else
  {
    fsa2->set_label_type(LT_Unlabelled);
    FSA_Simple * nfsa2 = FSA_Factory::minimise(*fsa2);
    delete fsa2;
    fsa2 = nfsa2;
    fsa2->change_flags(0,GFF_BFS);
  }

  if (fsa1->state_count() != fsa2->state_count())
  {
    cprintf("The canonical forms of these FSA have a different number of states\n");
    delete fsa1;
    delete fsa2;
    return 6;
  }
  if (!(fsa1->get_flags() & GFF_BFS))
    fsa1->sort_bfs();
  if (!(fsa2->get_flags() & GFF_BFS))
    fsa2->sort_bfs();
  if (fsa1->compare(*fsa2)!=0)
  {
    cprintf("The FSA do not accept the same language\n");
    delete fsa1;
    delete fsa2;
    return 7;
  }

  delete fsa1;
  delete fsa2;
  cprintf("The FSA accept the same language\n");
  return 0;
}

