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


// $Log: fsacount.cpp $
// Revision 1.5  2010/06/10 13:56:59Z  Alun
// All tabs removed again
// Revision 1.4  2010/05/11 07:49:48Z  Alun
// help changed
// Revision 1.3  2009/09/12 18:47:18Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Language_Size data type introduced and used where appropriate
//
// Revision 1.2  2007/12/20 23:25:43Z  Alun
//

#include <limits.h>
#include <stdlib.h>
#include "mafword.h"
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,String filename,String word1,String word2,
                   State_ID nis,unsigned log_level);

int main(int argc,char ** argv)
{
  int i = 1;
  bool bad_usage = false;
  const char * word1 = "";
  const char * word2 = "";
  char * file1 = 0;
  Container & container = *Container::create();
  Standard_Options so(container,SO_STDIN);
  State_ID is = -1;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-is"))
      {
        if (i+1 == argc)
          bad_usage = true;
        else
          is = atoi(argv[i+1]);
        i += 2;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (!so.use_stdin && file1 == 0)
      file1 = argv[i++];
    else if (!*word1)
      word1 = argv[i++];
    else if (!*word2)
      word2 = argv[i++];
    else
      bad_usage = true;
  }
  int exit_code = 1;
  if (!bad_usage && ((file1 !=0) ^ so.use_stdin))
    exit_code = inner(&container,file1,word1,word2,is,so.log_level);
  else
  {
    cprintf("Usage: fsacount [loglevel] [-is n] -i | input_file [word1] [word2]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\nGASP FSA.\n"
            "If you specify word1, or, in the case of a 2-variable FSA, word1"
            " and word2,\nthen instead fsacount counts the size of the"
            " intersection between the accepted\nlanguage and the set of"
            " words, or word pairs, that begin with the specified\n"
            "word(s).\n"
            "[KBMAG] The -is n option changes the FSA so that its initial"
            " state is n.\n");
    so.usage();
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,String filename,String word1,
                 String word2,State_ID nis,unsigned log_level)
{
  FSA_Simple * fsa = FSA_Factory::create(filename,container);
  Language_Size answer = 0;

  if (fsa)
  {
    State_ID si = -1;

    if (nis != -1)
    {
      if (!fsa->is_valid_state(nis))
      {
        container->error_output("Invalid state specified with -is option\n");
        return 1;
      }
      fsa->set_single_initial(nis);
      fsa->change_flags(0,GFF_TRIM|GFF_MINIMISED|GFF_ACCESSIBLE);
      FSA_Simple * fsa_temp = FSA_Factory::minimise(*fsa);
      delete fsa;
      fsa = fsa_temp;
    }

    if (*word1 || *word2)
    {
      Ordinal_Word *ow1 = fsa->base_alphabet.parse(word1,WHOLE_WORD,0,*container);
      Ordinal_Word *ow2 = fsa->base_alphabet.parse(word2,WHOLE_WORD,0,*container);
      if (fsa->is_product_fsa())
        si = fsa->read_product(*ow1,*ow2);
      else
        si = fsa->read_word(*ow1);
      delete ow1;
      delete ow2;
    }
    answer = fsa->language_size(true,si);
    if (answer == LS_INFINITE)
      container->progress(1,"The accepted language is infinite\n");
    else if (answer == LS_HUGE)
      container->progress(1,"The accepted language is finite,"
                            " but too large to calculate\n");
    else
      container->progress(1,"The accepted language contains " FMT_LS
                            " words\n",
                          answer);
    if (log_level == 0)
      if (answer >= LS_HUGE)
        container->result("%ld\n",(long) answer);
      else
        container->result(FMT_LS "\n",answer);
    delete fsa;
  }
  return answer != LS_HUGE ? 0 : 2;
}
