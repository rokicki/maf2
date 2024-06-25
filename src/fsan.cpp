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


// $Log: fsan.cpp $
// Revision 1.4  2010/05/11 07:48:31Z  Alun
// help changed. command line parsing improved
// Revision 1.3  2009/10/07 16:54:44Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.2  2009/09/12 18:47:22Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files

/* Program to calculate the FSA which accepts all words of length at most n
for a given alphabet. All the real work (there is very little of it) is done
by FSA_Factory::all() in fsa.cpp */
#include <stdlib.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"
#include "mafctype.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                   int length,bool exact,bool use_stdout,unsigned fsa_format_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * input_file = 0;
  char * output_file = 0;
  Container & container = * Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_STDIN|SO_STDOUT);
  int length = -1;
  bool bad_usage = false;
  bool all = false;
  bool exact = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-all"))
      {
        all = true;
        i++;
      }
      else if (arg.is_equal("-exact"))
      {
        exact = true;
        i++;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (length == -1 && !all)
    {
      if (is_digit(argv[i][0]))
        length = atoi(argv[i++]);
      else
        bad_usage = true;
    }
    else if (input_file == 0 && !so.use_stdin)
      input_file = argv[i++];
    else if (output_file == 0 && !so.use_stdout)
      output_file = argv[i++];
    else
      bad_usage = true;
  }

  int exit_code = 1;
  if (!bad_usage && ((input_file !=0) ^ so.use_stdin) && ((length!=-1) ^ all) &&
      !(all && exact))
    exit_code = inner(&container,input_file,output_file,length,exact,so.use_stdout,so.fsa_format_flags);
  else
  {
    cprintf("Usage:\nfsan [loglevel] [format] -all | [-exact] n -i | input_file [-o | output_file]\n"
            "where n is a number between 0 and 65533 and stdin (if you specify"
            " -i), or\ninput_file (otherwise), contains a GASP FSA.\n"
            "This program creates the FSA which accepts words of length n or"
            " less in the alphabet of the specified FSA.\n"
            "If -all is specified then instead the FSA that accepts all words is created.\n");
    so.usage(".n");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename1,String filename2,
                 int length,bool exact,bool use_stdout,unsigned fsa_format_flags)
{
  if (use_stdout)
    container->set_gap_stdout(true);
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,container);
  FSA_Simple * fsa2 = 0;
  if (fsa1)
  {
    if (length < 0)
      fsa2 = FSA_Factory::universal(*container,fsa1->base_alphabet);
    else
    {
      fsa2 = FSA_Factory::all_words(*container,fsa1->base_alphabet,length);
      if (exact)
        fsa2->set_single_accepting(length+1);
    }

    if (fsa2)
    {
      String_Buffer sb;
      String_Buffer sb1;
      sb1.format(".%d",length);
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,
                                        sb1.get(),
                                        String::MFSF_Replace);

      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    delete fsa1;
  }
  return fsa2 ? 0 : 1;
}

