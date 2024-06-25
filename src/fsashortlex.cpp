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


// $Log: fsashortlex.cpp $
// Revision 1.6  2010/05/12 06:19:38Z  Alun
// help changed
// Revision 1.5  2009/09/12 18:47:26Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2008/10/02 18:09:46Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.4  2008/10/02 19:09:45Z  Alun
// Added -lt command line option and changed suffix
// Revision 1.3  2007/12/20 23:25:44Z  Alun
//

/* Program to calculate the shortlex FSA for a given alphabet. All the real
   work (there is very little of it) is done by FSA_Factory::shortlex() in
   fsa.cpp */
#include <string.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                   bool less_than,bool strict,bool use_stdout,unsigned fsa_format_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * input_file = 0;
  char * output_file = 0;
  bool strict = false;
  Container & container = * Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_STDIN|SO_STDOUT);
  bool bad_usage = false;
  bool less_than = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-strict"))
      {
        i++;
        strict = true;
      }
      else if (arg.is_equal("-lt"))
      {
        i++;
        less_than = true;
      }
      else if (!so.recognised(argv,i))
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
    exit_code = inner(&container,input_file,output_file,less_than,strict,so.use_stdout,so.fsa_format_flags);
  else
  {
    cprintf("Usage:\nfsashortlex [loglevel] [format] [-strict] [-lt]"
            " -i | input_file [-o | output_file]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\nGASP FSA."
            "This program calculates the \"greater than\" automaton for"
            " the alphabet of\nthe input FSA. If -strict is not specified"
            " the automaton permits interior\npadding symbols in the word on"
            " the right hand side. This may be useful\nwhen building a"
            " word-acceptor from a provisional word-difference machine.\n"
            "If -lt is specified the \"less than\" automaton is calculated"
            " instead\n");
    so.usage(".shortlex.gt or .shortlex.lt");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename1,String filename2,
                 bool less_than,bool strict,bool use_stdout,unsigned fsa_format_flags)
{
  if (use_stdout)
    container->set_gap_stdout(true);
  FSA_Simple * fsa1 = FSA_Factory::create(filename1,container);
  FSA_Simple * fsa2 = 0;
  if (fsa1)
  {
    FSA_Simple * fsa2 = FSA_Factory::shortlex(*container,fsa1->base_alphabet,!strict);
    if (less_than)
    {
      FSA_Simple *temp = FSA_Factory::transpose(*fsa2);
      delete fsa2;
      fsa2 = temp;
    }
    if (fsa2)
    {
      String_Buffer sb;
      if (!use_stdout)
        filename2 = sb.make_destination(filename2,filename1,
                                        less_than ? ".shortlex.lt" : ".shortlex.gt",
                                        String::MFSF_Replace);

      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    delete fsa1;
  }
  return fsa2 ? 0 : 1;
}

