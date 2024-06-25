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


// $Log: fsareverse.cpp $
// Revision 1.6  2010/05/11 07:40:16Z  Alun
// help changed. No longer uses strcmp()
// Revision 1.5  2009/10/07 16:21:00Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.4  2009/09/12 18:47:26Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2007/12/20 23:25:44Z  Alun
//

/* Program to calculate the FSA which accepts the same language as a starting
   FSA, but read backwards. All the real work is done by
   FSA_Factory::reverse() in fsa.cpp */
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

/* functions in this module */
int main(int argc,char ** argv);
  int inner(Container & container,String filename1,String filename2,
            bool create_midfa,bool label,bool use_stdout,unsigned fsa_format_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  bool create_midfa = false;
  bool label = false;
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
      String arg = argv[i];
      if (arg.is_equal("-s") || arg.is_equal("-labelled") ||
          arg.is_equal("-labeled"))
      {
        label = true;
        i++;
      }
      else if (arg.is_equal("-midfa"))
      {
        create_midfa = true;
        i++;
      }
      else if (!so.recognised(argv,i))
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
    exit_code = inner(container,input_file,output_file,create_midfa,label,
                      so.use_stdout,so.fsa_format_flags);
  else
  {
    cprintf("Usage:\n"
            "fsareverse [loglevel] [format] [-midfa] [-labelled] -i | input_file [-o | output_file]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\nGASP FSA.\n"
            "The reverse FSA is computed, in which a word w1 is accepted if"
            " and only if\nthe original FSA accepts the same word when read in"
            " the opposite direction.\nThe -midfa option creates an FSA with"
            " multiple initial states, one for each\naccept state of the"
            " original FSA.\n"
            "The -labelled option (or -s) causes the states to be labelled"
            " with the subset\nof states of the original FSA to which each"
            " corresponds.\n");
    so.usage(".reverse or .mireverse");
  }
  delete &container;
  return exit_code;
}

/**/

int inner(Container & container,String filename1,String filename2,
          bool create_midfa,bool label,bool use_stdout,unsigned fsa_format_flags)
{
  if (use_stdout)
    container.set_gap_stdout(true);
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,&container);
  FSA_Simple * fsa2 = 0;
  if (fsa1)
  {
    fsa2 = FSA_Factory::reverse(*fsa1,create_midfa,label);
    if (fsa2)
    {
      String_Buffer sb;
      String var_name = sb.make_filename("",fsa1->get_name(),"_reverse",false,
                                         String::MFSF_Replace_No_Dot);
      fsa2->set_name(var_name);
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,create_midfa ? ".mireverse" : ".reverse");
      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    delete fsa1;
  }
  return fsa2 ? 0 : 1;
}

