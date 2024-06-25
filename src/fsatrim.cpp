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


// $Log: fsatrim.cpp $
// Revision 1.3  2010/06/10 13:57:09Z  Alun
// All tabs removed again
// Revision 1.2  2010/05/11 07:38:46Z  Alun
// help changed
// Revision 1.1  2009/10/07 16:32:50Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
//

/* Program to remove failing and inaccessible state from an FSA . All the real
   work is done by FSA_Factory::trim() in fsa.cpp */
#include <string.h>
#include <stdlib.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                   bool use_stdout,
                   unsigned fsa_format_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * input_file = 0;
  char * output_file = 0;
  Container & container = *Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|
                                SO_STDIN|SO_STDOUT);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (!so.use_stdin && input_file == 0)
      input_file = argv[i++];
    else if (output_file == 0)
      output_file = argv[i++];
    else
      bad_usage = true;
  }

  int exit_code = 1;
  if (!bad_usage && ((input_file !=0) ^ so.use_stdin) &&
      !(output_file && so.use_stdout))
    exit_code = inner(&container,input_file,output_file,
                      so.use_stdout,so.fsa_format_flags);
  else
  {
    cprintf("Usage:\n"
            "fsatrim [loglevel] [format] -i | input_file [-o | output_file]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\nGASP SA.\n"
            "The output FSA is an states that accepts the same language, but"
            " from which\nfailing and inaccessible states have been removed.\n");
    so.usage(".trim");
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
  if (fsa1)
  {
    FSA_Simple * fsa2 = FSA_Factory::trim(*fsa1);
    if (fsa2)
    {
      String_Buffer sb;
      String var_name = sb.make_filename("",fsa1->get_name(),"_trim",false,
                                         String::MFSF_Replace_No_Dot);
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,".trim");
      fsa2->set_name(var_name);
      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    delete fsa1;
  }
  return fsa1 ? 0 : 1;
}
