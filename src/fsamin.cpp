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


// $Log: fsamin.cpp $
// Revision 1.10  2010/05/11 07:49:16Z  Alun
// help changed
// Revision 1.9  2009/11/08 20:41:35Z  Alun
// Command line option handling changed.
// Revision 1.8  2009/10/07 16:33:00Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.7  2009/09/12 18:47:22Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.6  2008/08/22 18:05:26Z  Alun
// "Early Sep 2008 snapshot"

/* Program to minimise an FSA . All the real work is
   done by FSA_Factory::minimise() in fsa.cpp */
#include <string.h>
#include <stdlib.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                   bool no_labels,bool no_rws,
                   FSA_Factory::Merge_Label_Flag merge_labels,
                   bool use_stdout,
                   unsigned fsa_format_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * input_file = 0;
  char * output_file = 0;
  bool no_labels = false;
  bool no_rws = false;
  FSA_Factory::Merge_Label_Flag merge_labels = FSA_Factory::MLF_None;
  Container & container = *Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|
                                SO_STDIN|SO_STDOUT);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      if (so.present(argv[i],"-no_labels"))
      {
        i++;
        no_labels = true;
      }
      else if (so.present(argv[i],"-merge_labels"))
      {
        i++;
        merge_labels = FSA_Factory::MLF_All;
      }
      else if (so.present(argv[i],"-merge_initial_labels"))
      {
        i++;
        merge_labels = FSA_Factory::MLF_Non_Accepting;
      }
      else if (so.present(argv[i],"-no_rws"))
      {
        i++;
        no_rws = true;
      }
      else if (!so.recognised(argv,i))
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
      !(merge_labels && no_labels) && !(output_file && so.use_stdout))
    exit_code = inner(&container,input_file,output_file,no_labels,no_rws,
                      merge_labels,so.use_stdout,so.fsa_format_flags);
  else
  {
    cprintf("Usage:\n"
            "fsamin [loglevel] [format] [-no_labels | -merge_labels | -merge_initial_labels] [-no_rws] -i |"
            " input_file [-o | output_file]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\nGASP FSA.\n"
            "The output FSA is the FSA with the fewest states that accepts the"
            " same language\nwith the same state labels.\n"
            "-nolabels removes state labels from the FSA prior to minimisation.\n"
            "Using -nolabels may reduce the number of states in the FSA.\n"
            "-norws removes rewrites from the FSA prior to minimisation.\n"
            "This converts an index automaton into a word-acceptor.\n"
            "-merge_labels allows states with different labels to be merged."
            " The new states\nare given labels that are a list of all the old"
            " labels.-merge_initial_labels is\nsimilar but only applies to"
            " labels that are on non-accepting states.\n");
    so.usage(".min");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename1,String filename2,
                 bool no_labels,bool no_rws,
                 FSA_Factory::Merge_Label_Flag merge_labels,bool use_stdout,
                 unsigned fsa_format_flags)
{
  if (use_stdout)
    container->set_gap_stdout(true);
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,container);
  if (fsa1)
  {
    if (no_rws)
      fsa1->remove_rewrites();
    if (no_labels)
      fsa1->set_nr_labels(0);
    FSA_Simple * fsa2 = FSA_Factory::minimise(*fsa1,TSF_Default,merge_labels);
    if (fsa2)
    {
      String_Buffer sb;
      String var_name = sb.make_filename("",fsa1->get_name(),"_min",false,
                                         String::MFSF_Replace_No_Dot);
      fsa2->set_name(var_name);
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,".min");
      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    delete fsa1;
  }
  return fsa1 ? 0 : 1;
}
