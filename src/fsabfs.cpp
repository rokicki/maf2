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


// $Log: fsabfs.cpp $
// Revision 1.6  2010/05/11 07:36:15Z  Alun
// help changed
// Revision 1.5  2009/10/07 16:47:22Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.4  2009/09/12 18:47:16Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2007/12/20 23:25:43Z  Alun
//

/* Program to arrange states of an FSA in BFS order. All the real work is
   done by FSA_Simple::sort_bfs() in fsa.cpp */
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                   bool use_stdout,unsigned fsa_format_flags);

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
    else if (input_file == 0 && !so.use_stdin)
      input_file = argv[i++];
    else if (output_file == 0)
      output_file = argv[i++];
    else
      bad_usage = true;
  }

  int exit_code = 1;
  if (!bad_usage && ((input_file!=0) ^ so.use_stdin) &&
      !(output_file && so.use_stdout))
    exit_code = inner(&container,input_file,output_file,so.use_stdout,
                      so.fsa_format_flags);
  else
  {
    cprintf("Usage: fsabfs [loglevel] [format] -i | input_file [-o | output_file]\n"
            "where input_file contains a GASP FSA.\n"
            "The output FSA is the same as the original except that the"
            " states are\nbreadth-first-search ordered.\n");
    so.usage(".bfs");
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
    String_Buffer sb;
    String var_name = sb.make_filename("",fsa1->get_name(),"_bfs",false,
                                       String::MFSF_Replace_No_Dot);
    fsa1->sort_bfs();
    fsa1->set_name(var_name);
    filename2 = sb.make_destination(filename2,filename1,".bfs");
    fsa1->save(filename2,fsa_format_flags);
    delete fsa1;
  }
  return fsa1 ? 0 : 1;
}
