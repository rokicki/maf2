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


// $Log: fsaand.cpp $
// Revision 1.6  2010/06/10 09:00:47Z  Alun_Williams
// Help changed
// Revision 1.5  2009/10/07 16:20:50Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.4  2008/10/02 18:27:00Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.4  2008/10/02 19:26:59Z  Alun
// -first option added
// Revision 1.3  2007/12/20 23:25:43Z  Alun
//

/* Program to combine FSA using and. All the real work is
   done by FSA_Factory::binop() in fsa.cpp */
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename1,String filename2,
                    String filename3,bool first,unsigned fsa_format_flags);
/**/

int main(int argc,char ** argv)
{
  int exit_code = 1;
  char *file1 = 0;
  char *file2 = 0;
  char *file3 = 0;
  int i = 1;
  Container & container = * Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY);
  bool bad_usage = false;
  bool first = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-first"))
      {
        first = true;
        i++;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (file1 == 0)
      file1 = argv[i++];
    else if (file2 == 0)
      file2 = argv[i++];
    else if (file3 == 0)
      file3 = argv[i++];
    else
      bad_usage = true;
  }
  if (!bad_usage && file2)
  {
    exit_code = inner(&container,file1,file2,file3,first,so.fsa_format_flags);
    if (exit_code != 0)
      cprintf("Specified FSA are not compatible\n");
  }
  else
  {
    cprintf("Usage: fsaand [log_level] [format] [-first] file1 file2 [file3]\n"
            "where file1 and file2 contain GASP FSA with the"
            " same alphabet.\n"
            "The output FSA accepts a word if and only if both the input FSA do\n"
            "Output is to file3, if specified, or to stdout otherwise\n"
            "If -first is specified then the output FSA accepts a word if and"
            " only if both\nthe input FSA do, and no prefix of the word was"
            " accepted by both FSA.\n");
    so.usage();
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename1,String filename2,
                 String filename3,bool first,unsigned fsa_format_flags)
{
  if (!filename3)
    container->set_gap_stdout(true);
  FSA * fsa1 = FSA_Factory::create(filename1,container);
  FSA * fsa2 = FSA_Factory::create(filename2,container);
  FSA * answer = first ? FSA_Factory::fsa_and_first(*fsa1,*fsa2) :
                         FSA_Factory::fsa_and(*fsa1,*fsa2);
  String_Buffer sb;
  String var_name = sb.make_filename("",fsa1->get_name(),"_and",false,
                                     String::MFSF_Replace_No_Dot);
  if (answer)
  {
    answer->set_name(var_name);
    answer->save(filename3,fsa_format_flags);
    delete answer;
  }
  delete fsa1;
  delete fsa2;
  return answer ? 0 : 1;
}

