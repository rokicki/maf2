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


// $Log: midfadeterminize.cpp $
// Revision 1.7  2010/05/11 07:38:26Z  Alun
// Argument parsing changed not to need strcmp(). Help changed
// Revision 1.6  2009/09/12 18:47:56Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.5  2008/08/16 09:44:20Z  Alun
// Added option to make all states of the input FSA initial.
// This is of interest for coset systems
// Revision 1.4  2007/12/20 23:25:45Z  Alun
//

/* Program to calculate the FSA which accepts the same language as a starting
   MIDFA FSA, but which has one initial state. All the real work is done by
   FSA_Factory::determinise() in fsa.cpp */
#include <string.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                   FSA_Factory::Determinise_Flag partial,
                   bool merge_labels,bool use_stdout,
                   unsigned fsa_format_flags,bool all_initial);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * input_file = 0;
  char * output_file = 0;
  bool merge_labels = true;
  FSA_Factory::Determinise_Flag partial = FSA_Factory::DF_All;
  Container & container = *Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|
                                SO_STDIN|SO_STDOUT);
  bool bad_usage = false;
  bool all_initial = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (so.present(arg,"-no_labels"))
      {
        merge_labels = false;
        i++;
      }
      else if (arg.is_equal("-identical"))
      {
        partial = FSA_Factory::DF_Identical;
        i++;
      }
      else if (arg.is_equal("-equal"))
      {
        partial = FSA_Factory::DF_Equal;
        i++;
      }
      else if (arg.is_equal("-all"))
      {
        all_initial = true;
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
    exit_code = inner(&container,input_file,output_file,partial,merge_labels,
                      so.use_stdout,so.fsa_format_flags,all_initial);
  else
  {
    cprintf("Usage:\nmidfadeterminize [loglevel] [format] [-equal | -identical] [-no_labels] [-all]\n"
            " -i | input_file [-o | output_file]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\nGASP FSA that is a"
            " MIDFA.\nThe output is a determinised FSA that accepts the same"
            " language\nIf -identical is specified, then the output may still"
            " be a MIDFA, but initial\nstates with the same label have been"
            " merged. -equal is similar, but in this\ncase states are merged"
            " if they have a label in common. This is useful with\ncomposite"
            " coset multipliers.\nUnless -no_labels is specified the output"
            " FSA is labelled, with labels that\nare a list of all the"
            " words labelling corresponding states of the original FSA.\n"
            "If -all is specified all states of the initial FSA are made"
            " initial states.\n");
    so.usage(".midfadeterminize");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename1,String filename2,
                 FSA_Factory::Determinise_Flag partial,bool merge_labels,
                 bool use_stdout,unsigned fsa_format_flags,bool all_initial)
{
  if (use_stdout)
    container->set_gap_stdout(true);
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,container);
  FSA_Simple * fsa2 = 0;
  if (fsa1)
  {
    if (all_initial)
      fsa1->set_initial_all();
    fsa2 = FSA_Factory::determinise(*fsa1,partial,merge_labels);
    if (fsa2)
    {
      String_Buffer sb;
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,".midfadeterminize");
      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    delete fsa1;
  }
  return fsa2 ? 0 : 1;
}

