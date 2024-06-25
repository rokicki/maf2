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


// $Log: reduce.cpp $
// Revision 1.9  2010/05/11 19:54:46Z  Alun
// Argument parsing changed not to need strcmp(). Help changed
// Revision 1.8  2009/11/11 00:16:32Z  Alun
// Command line option handling changed.
// Code added for possible GAP interface - but will probably be moved to new
// utility combining reduce gporder and isconjugate
// Revision 1.7  2009/10/13 23:30:34Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.6  2009/09/12 19:10:09Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.5  2009/02/10 12:49:22Z  Alun
// Failed to close output file
// Revision 1.5  2008/11/02 18:57:14Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.4  2008/10/12 19:10:35Z  Alun
// -steps was previously only implemented when reducing a list of words
// Revision 1.3  2007/11/15 22:58:11Z  Alun
//

#include <stdlib.h>
#include "maf.h"
#include "mafctype.h"
#include "container.h"
#include "alphabet.h"
#include "mafword.h"
#include "maf_dr.h"
#include "maf_so.h"


int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * subgroup_suffix = 0;
  String output_name = 0;
  String one_word = 0;
  String words_file = 0;
  bool cosets = false;
  bool steps = false;
  bool gap_interface = false;
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
      else if (arg.is_equal("-interface"))
      {
        gap_interface = true;
        i++;
      }
      else if (arg.is_equal("-steps"))
      {
        steps = true;
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
       (one_word!=0) + so.use_stdin + (words_file!=0)==1 &&
       !(gap_interface && !so.use_stdin))
  {
    MAF * maf = 0;
    if (so.use_stdin || so.use_stdout)
      container->set_gap_stdout(true);
    maf = MAF::create_from_input(cosets,group_filename,subgroup_suffix,
                                 container);
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
      String_Buffer sb;
      Output_Stream * os = container->get_stdout_stream();
      Input_Stream *stream = container->get_stdin_stream();
      Letter c;
      int i;
      container->progress(1,"Input words for reduction\n"
                            "Separate words with ','.\n"
                            "Terminate input with ';'.\n");
      container->set_interactive(true);
      for (;;)
      {
        sb.empty();
        while ((i = container->read(stream,(Byte *) &c,sizeof(Letter)))==sizeof(Letter) &&
               c != ',' && c != ';')
          if (!is_white(c))
            sb.append(c);
        if ((c != ';' || sb.get().length() > 0) & maf->parse(&test,sb.get()))
        {
          if (steps)
          {
            container->output(os,"[");
            bool started = false;
            do
            {
              if (started)
                container->output(os,", ");
              test.print(*container,os);
              started = true;
            }
            while (maf->reduce(&test,test,WR_ONCE));
            container->output(os,"]\n");
          }
          else
          {
            maf->reduce(&test,test);
            if (gap_interface)
            {
              String_Buffer sb2;
              const Ordinal * buffer = test.buffer();
              Word_Length i,l = test.length();
              sb.set("[");
              if (l)
              {
                sb2.format("%d",buffer[0]+1);
                sb.append(sb2.get());
                for (i = 1 ; i < l;i++)
                {
                  sb2.format(",%d",buffer[i]+1);
                  sb.append(sb2.get());
                }
              }
              sb.append("]");
            }
            else
              test.format(&sb);
            container->output(os,"%s\n",sb.get().string());
          }
        }
        if (c == ';' || i==0)
          break;
      }
      container->close_output_file(os);
    }
    else if (one_word)
    {
      Output_Stream * os = container->get_stdout_stream();
      Ordinal_Word * ow = maf->parse(one_word);
      bool reducible = false;
      if (steps)
      {
        bool started = false;
        do
        {
          if (started)
          {
            reducible = true;
            container->output(os,",\n  ");
          }
          ow->print(*container,os);
          started = true;
        }
        while (maf->reduce(ow,*ow,WR_ONCE));
        container->output(os,",\n");
      }
      else
        reducible = maf->reduce(ow,*ow) != 0;
      String_Buffer sb;
      ow->format(&sb);
      if (reducible)
        container->output(os,"Word reduces to: %s\n",sb.get().string());
      else
        container->output(os,"Word is not reducible: %s\n",sb.get().string());
      delete ow;
    }
    else
    {
      Word_List wl(maf->alphabet);
      Ordinal_Word test(maf->alphabet);
      String_Buffer sb;
      output_name = sb.make_destination(output_name,words_file,".reduced");
      Output_Stream * os = container->open_text_output_file(output_name);
      container->output(os,"reduced_words :=\n[\n  ");

      maf->read_word_list(&wl,words_file);
      Element_Count count = wl.count();
      for (Element_ID i = 0; i < count;i++)
      {
        if (i)
          container->output(os,",\n  ");
        wl.get(&test,i);
        if (steps)
        {
          container->output(os,"[\n    ");
          bool started = false;
          do
          {
            if (started)
              container->output(os,",\n    ");
            test.print(*container,os);
            started = true;
          }
          while (maf->reduce(&test,test,WR_ONCE));
          container->output(os,"\n  ]");
        }
        else
        {
          maf->reduce(&test,test);
          test.print(*container,os);
        }
      }
      container->output(os,"\n];\n");
      container->close_output_file(os);
    }
    delete maf;
  }
  else
  {
    cprintf("Usage:\n"
            "reduce [loglevel] [reduction_method] [-steps] [-interface] rwsname [-cos [subsuffix]]"
            " word | -i | -read input_file [output_file]\n"
            "where rwsname is a GASP rewriting system and, if the -cos option"
            " is used,\nrwsname.subsuffix is a substructure file.\n"
            "An automaton that can peform word reduction must previously have"
            " been computed\nfor the relevant rewriting system.\n"
            "If a single word is given on the command line it is reduced.\n"
            "If the -i option is used words are reduced one at a time as they"
            " are entered.\nEnter ',' to terminate a word or a ';' to exit.\n"
            "If the -read option is specified then the input file must be a GAP"
            " list. You may\nspecify an output file (which will be a GAP list"
            " named reduced_words).\n"
            "The -steps option causes each step of the reduction to be printed.\n"
            "The -interface option (for -i only) outputs words in GAP letter"
            " representation.\n");
    so.usage(".reduced");
    delete container;
    return 1;
  }

  delete container;
  return 0;
}

