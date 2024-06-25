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


// $Log: fsafl.cpp $
// Revision 1.1  2010/05/11 18:09:20Z  Alun
// New file.
//

/* Program to compute an FSA with an explicitly specified finite language
   All the work is done by FSA_Factory::finite_language() in fsa.cpp */
#include <stdlib.h>
#include "maf.h"
#include "fsa.h"
#include "container.h"
#include "maf_so.h"
#include "mafword.h"

int main(int argc,char ** argv);
  static int inner(MAF & maf,String words_filename,
                   String output_filename,bool labelled,
                   bool use_stdout,unsigned fsa_format_flags);

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  char * rws_filename = 0;
  char * words_file = 0;
  char * output_file = 0;
  Container & container = * Container::create();
  Standard_Options so(container,SO_FSA_FORMAT|SO_STDIN|SO_STDOUT);
  bool bad_usage = false;
  bool labelled = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-labelled") || arg.is_equal("-labeled"))
      {
        labelled = true;
        i++;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (rws_filename == 0)
      rws_filename = argv[i++];
    else if (words_file == 0 && !so.use_stdin)
      words_file = argv[i++];
    else if (output_file == 0)
      output_file = argv[i++];
    else
      bad_usage = true;
  }

  int exit_code = 1;
  if (!bad_usage && rws_filename !=0 && ((words_file!=0) ^ so.use_stdin))
  {
    MAF * maf = 0;
    if (so.use_stdin || so.use_stdout)
      container.set_gap_stdout(true);
    maf = MAF::create_from_input(false,rws_filename,0,&container);
    exit_code = inner(*maf,words_file,output_file,labelled,
                      so.use_stdout,so.fsa_format_flags);
  }
  else
  {
    cprintf("Usage:\nfsafl [loglevel] [format] input_file  -i | words_file [-o | output_file]\n"
            "where input_file is a GASP rewriting system using the desired alphabet,"
            " and stdin (if\nyou specify -i), or words_file (otherwise),"
            " contains a GAP list of words in\n this alphabet.\n"
            "This program creates the FSA that accepts just the specified"
            " words.\n");
    so.usage(".fl");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,
                 String words_filename,
                 String output_filename,
                 bool labelled,
                 bool use_stdout,unsigned fsa_format_flags)
{
  FSA_Simple * fsa2 = 0;
  Word_List wl(maf.alphabet);
  maf.read_word_list(&wl,words_filename);
  fsa2 = FSA_Factory::finite_language(wl,labelled) ;

  if (fsa2)
  {
    String_Buffer sb;
    if (!use_stdout)
      output_filename = sb.make_destination(output_filename,words_filename,
                                            ".fl",String::MFSF_Replace);

    fsa2->save(output_filename,fsa_format_flags);
    delete fsa2;
  }
  delete &maf;
  return fsa2 ? 0 : 1;
}

