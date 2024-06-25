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


// $Log: fsapad.cpp $
// Revision 1.5  2010/06/10 13:57:05Z  Alun
// All tabs removed again
// Revision 1.4  2010/05/11 07:47:38Z  Alun
// help changed
// Revision 1.3  2009/10/07 16:55:32Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.2  2009/09/12 18:47:24Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.1  2008/10/08 12:37:32Z  Alun_Williams
// New file.

/* Program to calculate the FSA which accepts the padded language
for a given alphabet. All the real work (there is very little of it) is done
by FSA_Factory::pad_language() in fsa.cpp */
#include <string.h>
#include <stdlib.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"
#include "mafctype.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                   bool use_stdout,unsigned fsa_format_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  String input_file = 0;
  String output_file = 0;
  Container & container = * Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_STDIN|SO_STDOUT);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (input_file == 0)
      input_file = argv[i++];
    else if (output_file == 0)
      output_file = argv[i++];
    else
      bad_usage = true;
  }

  int exit_code = 1;
  if (!bad_usage && ((input_file !=0) ^ so.use_stdin))
    exit_code = inner(&container,input_file,output_file,so.use_stdout,so.fsa_format_flags);
  else
  {
    cprintf("Usage:\nfsapad [loglevel] [format] -i | input_file [-o | output_file]\n"
            "where stdin (if you specify -i), or input_file (otherwise),"
            " contains a GASP\nformat FSA.\n"
            "This program creates the FSA which accepts all correctly padded"
            " word pairs\n");
    so.usage(".pad");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename1,String filename2,
                 bool use_stdout,unsigned fsa_format_flags)
{
  if (use_stdout)
    container->set_gap_stdout(true);
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,container);
  FSA_Simple * fsa2 = 0;
  if (fsa1)
  {
    fsa2 = FSA_Factory::pad_language(*container,fsa1->base_alphabet);

    if (fsa2)
    {
      String_Buffer sb;
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,
                                        ".pad",
                                        String::MFSF_Replace);

      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    delete fsa1;
  }
  return fsa2 ? 0 : 1;
}

