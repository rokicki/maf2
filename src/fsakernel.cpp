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


// $Log: fsakernel.cpp $
// Revision 1.8  2010/05/11 07:50:30Z  Alun
// help changed
// Revision 1.7  2009/11/08 20:39:45Z  Alun
// Command line option handling changed.
// Revision 1.6  2009/10/07 16:40:54Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.5  2009/09/12 18:47:20Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2007/12/20 23:25:44Z  Alun
//

/* Program to calculate the not of an FSA. All the real
   work is done by FSA_Factory::kernel() in fsa.cpp */
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                   bool use_stdout,unsigned fsa_format_flags,unsigned kf_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * input_file = 0;
  char * output_file = 0;
  unsigned kf_flags = 0;
  Container & container = * Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|
                                SO_STDIN|SO_STDOUT);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (so.present(arg,"-no_minimise") || so.present(arg,"no_minimize"))
      {
        kf_flags |= KF_NO_MINIMISE;
        i++;
      }
      else if (so.present(arg,"-accept_all"))
      {
        kf_flags |= KF_ACCEPT_ALL;
        i++;
      }
      else if (arg.is_equal("-infinite"))
      {
        kf_flags |= KF_INCLUDE_ALL_INFINITE;
        i++;
      }
      else if (arg.is_equal("-midfa"))
      {
        kf_flags |= KF_MIDFA;
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
    exit_code = inner(&container,input_file,output_file,so.use_stdout,so.fsa_format_flags,
                      kf_flags);
  else
  {
    cprintf("Usage:\n"
            "fsakernel [loglevel] [format] [-no_minimise] [-accept_all] [-infinite] [-midfa] -i | input_file"
            " [-o | output_file]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\nGASP FSA.\n"
            "The output FSA accepts the same language as the original, except"
            " that words\nwhich correspond to states that cannot recur are"
            " rejected. If you specify\n-infinite then instead the output FSA"
            " rejects words if they correspond\nto states that could only"
            " occur finitely often. The -accept_all option makes\nall the"
            " words that are not rejected by these rules accepted, but this"
            " is only\nusually needed if you do not specify -infinite.\n"
            "The -midfa option causes the rejected states to be removed, and"
            " a MIDFA is\ncreated from the remaining states.\n If"
            "-no_minimise is specified the final FSA is not minimised.");
    so.usage(".kernel");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename1,String filename2,
                 bool use_stdout,unsigned fsa_format_flags,unsigned kf_flags)
{
  if (use_stdout)
    container->set_gap_stdout(true);
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,container);
  FSA_Simple * fsa2 = 0;
  if (fsa1)
  {
    fsa2 = FSA_Factory::kernel(*fsa1,kf_flags);
    if (fsa2)
    {
      String_Buffer sb;
      String var_name = sb.make_filename("",fsa1->get_name(),"_kernel",false,
                                         String::MFSF_Replace_No_Dot);
      fsa2->set_name(var_name);
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,".kernel");
      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    delete fsa1;
  }
  return fsa2 ? 0 : 1;
}

