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


// $Log: gporder.cpp $
// Revision 1.6  2010/05/11 18:02:34Z  Alun
// Argument parsing changed not to need strcmp(). Help changed. Computation
// of order is now done using MAF::order() which was created from code
// formerly in this module
// Revision 1.5  2009/11/11 00:15:16Z  Alun
// Works a bit better with infinite groups
// Revision 1.4  2009/10/13 23:28:56Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.3  2009/09/12 19:15:45Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2009/08/24 11:14:21Z  Alun
// Various things that should have changed when this program was copied from reduce.cpp hadn't
// Revision 1.1  2009/08/23 16:45:04Z  Alun
// New file.
//

#include <stdlib.h>
#include "maf.h"
#include "container.h"
#include "alphabet.h"
#include "mafword.h"
#include "maf_dr.h"
#include "maf_so.h"
#include "fsa.h"
#include "mafctype.h"

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * subgroup_suffix = 0;
  String output_name = 0;
  String one_word = 0;
  String words_file = 0;
  bool cosets = false;
  Container * container = MAF::create_container();
  Standard_Options so(*container,SO_STDIN|SO_STDOUT|
                                 SO_REDUCTION_METHOD|SO_PROVISIONAL|SO_WORDUTIL);
  bool bad_usage = false;
#define cprintf container->error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-cos"))
      {
        cosets = true;
        i++;
      }
      else if (arg.is_equal("-read"))
      {
        words_file = argv[i+1];
        i += 2;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else
    {
      if (group_filename == 0)
        group_filename = argv[i];
      else if (cosets && subgroup_suffix == 0)
        subgroup_suffix = argv[i];
      else if (!so.use_stdin && !words_file)
        one_word = argv[i];
      else if (!output_name && words_file)
        output_name = argv[i];
      else
        bad_usage = true;
      i++;
    }
  }

  if (!bad_usage && group_filename &&
       (one_word!=0) + so.use_stdin + (words_file!=0)==1)
  {
    MAF * maf = MAF::create_from_input(cosets,group_filename,subgroup_suffix,
                                       container);
    const FSA * wa = maf->load_fsas(GA_WA);
    if (wa && wa->language_size(false) != LS_INFINITE)
      wa = 0;
    if (!maf->load_reduction_method(so.reduction_method))
    {
      cprintf("Unable to reduce words using selected mechanism\n");
      delete maf;
      delete container;
      return 1;
    }

    if (so.use_stdin)
    {
      Ordinal_Word test(maf->alphabet);
      Ordinal_Word save(maf->alphabet);
      String_Buffer sb;
      Output_Stream * os = container->get_stdout_stream();
      Input_Stream *stream = container->get_stdin_stream();
      Letter c;
      container->progress(1,"Input words whose order is to be found\n"
                            "Separate words with ','.\n"
                            "Terminate input with ';'.\n");
      container->set_interactive(true);
      for (;;)
      {
        sb.empty();
        while (container->read(stream,(Byte *) &c,sizeof(Letter))==sizeof(Letter) &&
               c != ',' && c != ';')
          if (!is_white(c))
            sb.append(c);
        if ((c != ';' || sb.get().length() > 0) & maf->parse(&test,sb.get()))
        {
          unsigned long answer = maf->order(&test,wa);
          if (answer != 0)
            container->output(os,"%lu\n",answer);
          else
            container->output(os,"infinity\n");
        }
        if (c == ';')
          break;
      }
      container->close_output_file(os);
    }
    else if (one_word)
    {
      Output_Stream * os = container->get_stdout_stream();
      Ordinal_Word * ow = maf->parse(one_word);
      unsigned long answer = maf->order(ow,wa);

      if (answer != 0)
        container->output(os,"Element has order: %lu\n",answer);
      else
        container->output(os,"Element has order: infinity\n");
      delete ow;
    }
    else
    {
      Word_List wl(maf->alphabet);
      Ordinal_Word test(maf->alphabet);
      String_Buffer sb;

      if (!so.use_stdout)
        output_name = sb.make_destination(output_name,words_file,".orders");
      else
      {
        container->set_gap_stdout(true);
        output_name = 0;
      }
      Output_Stream * os = container->open_text_output_file(output_name);
      container->output(os,"word_orders :=\n[\n  ");

      maf->read_word_list(&wl,words_file);
      Element_Count count = wl.count();
      for (Element_ID i = 0; i < count;i++)
      {
        if (i)
          container->output(os,",\n  ");
        wl.get(&test,i);
        test.format(&sb);
        unsigned long answer = maf->order(&test,wa);
        if (answer != 0)
          container->output(os,"  [%s,%lu]",sb.get().string(),answer);
        else
          container->output(os,"  [%s,infinity]",sb.get().string());
      }
      container->output(os,"\n];\n");
      container->close_output_file(os);
    }
    delete maf;
  }
  else
  {
    cprintf("Usage:\n"
            "gporder [loglevel] [reduction_method] rwsname [-cos [subsuffix]]"
            " word | -read input_file | -i [output_file]\n"
            "where rwsname is a GASP rewriting system, and, if the -cos"
            " option is used,\nrwsname.subsuffix is a substructure file.\n"
            "\"automata\" must already have been run against the relevant"
            " rewriting-system\n"
            "If a single word is given on the command line its order is computed,"
            "If the -i option is used orders are computed one at a time as words"
            " are entered.\nEnter ',' to terminate a word or a ';' to exit.\n"
            "If the -read option is specified then the input file must be a GAP"
            " list. You may\nspecify an output file (which will be a GAP list"
            " named word_orders).\n");
    so.usage(".orders");
    delete container;
    return 1;
  }

  delete container;
  return 0;
}
