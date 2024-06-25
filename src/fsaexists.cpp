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


// $Log: fsaexists.cpp $
// Revision 1.9  2010/06/10 13:57:01Z  Alun
// All tabs removed again
// Revision 1.8  2010/05/11 08:01:43Z  Alun
// help changed
// Revision 1.7  2009/10/07 16:22:16Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.6  2009/09/12 18:47:20Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.5  2008/10/10 06:53:28Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.5  2008/10/10 07:53:28Z  Alun
// Minor changes that facilitate using cprintf() macro for diagnostics
// Revision 1.4  2007/12/20 23:25:43Z  Alun
//

/* Program to calculate the "There exists" FSA of a product FSA. All the real
   work is done by FSA_Factory::exists() in fsa.cpp */
#include <string.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container & container,String filename1,String filename2,
                   bool sticky,bool swapped,bool use_stdout,
                   unsigned fsa_format_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * input_file = 0;
  char * output_file = 0;
  bool sticky = false;
  bool swapped = false;
  Container & container = * Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|SO_STDIN);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-sticky"))
      {
        sticky = true;
        i++;
      }
      else if (arg.is_equal("-swapped"))
      {
        swapped = true;
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
  if (!bad_usage && ((input_file !=0) ^ so.use_stdin) &&
      !(output_file && so.use_stdout))
    exit_code = inner(container,input_file,output_file,sticky,swapped,
                      so.use_stdout,so.fsa_format_flags);
  else
  {
    cprintf("Usage:\n"
            "fsaexists [loglevel] [format] [-sticky] [-swapped] input_file"
            " [-o | output_file]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\n2-variable GASP FSA.\n"
            "The \"There exists\" FSA is computed, in which a word w1 is"
            " accepted if and\nonly if there is a word w2 such that (w1,w2) is"
            " accepted by the original FSA.\nIf the -sticky option is specified"
            " the output FSA also accepts any longer word\nwhich has such a"
            " word as a prefix. This may be the same FSA and may be much\n"
            "easier to calculate. If -swapped is specified then the output FSA"
            " accepts w1\nif the original FSA accepts (w2,w1) for some w2.\n");
    so.usage(".exists or .swapped.exists");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container & container,String filename1,String filename2,
                 bool sticky,bool swapped,bool use_stdout,unsigned fsa_format_flags)
{
  if (use_stdout)
    container.set_gap_stdout(true);
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,&container);
  FSA_Simple * fsa2 = 0;
  if (fsa1)
  {
    fsa2 = FSA_Factory::exists(*fsa1,sticky,swapped);
    if (fsa2)
    {
      String_Buffer sb;
      String var_name = sb.make_filename("",fsa1->get_name(),"_exists",false,
                                         String::MFSF_Replace_No_Dot);
      fsa2->set_name(var_name);
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,
                                        swapped ? ".swapped.exists" : ".exists");

      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    else if (!fsa1->is_product_fsa())
      container.error_output("fsaexists requires a product FSA as the source FSA\n");
    delete fsa1;
  }
  return fsa2 ? 0 : 1;
}

