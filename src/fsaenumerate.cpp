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


// $Log: fsaenumerate.cpp $
// Revision 1.8  2010/06/10 13:57:00Z  Alun
// All tabs removed again
// Revision 1.7  2010/05/11 08:04:00Z  Alun
// help changed
// Revision 1.6  2009/09/13 20:24:51Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
//
//
// Revision 1.5  2008/11/02 20:37:02Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.5  2008/11/02 21:37:01Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.4  2008/09/23 13:02:11Z  Alun
// Final version built using Indexer.
// Revision 1.3  2007/12/20 23:25:43Z  Alun
//

#include <limits.h>
#include <stdlib.h>
#include "mafword.h"
#include "fsa.h"
#include "container.h"
#include "maf_so.h"

int main(int argc,char ** argv);
  static int inner(Container * container,bool bfs,Word_Length min_length,
                   Word_Length max_length,String filename,
                   String output_name,String word1,
                   String word2,State_ID nis,bool print_label,
                   bool print_state,bool equation_format,bool use_stdout);

int main(int argc,char ** argv)
{
  int i = 1;
  bool bfs = false;
  bool print_state = false;
  bool print_label = false;
  bool equation_format = false;
  bool bad_usage = false;
  Word_Length min_length = INVALID_LENGTH;
  Word_Length max_length = INVALID_LENGTH;
  bool seen_max_length = false;
  const char * word1 = "";
  const char * word2 = "";
  char * file1 = 0;
  char * file2 = 0;
  Container & container = *Container::create();
  Standard_Options so(container,SO_STDIN|SO_STDOUT);
  State_ID is = -1;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-bfs"))
      {
        bfs = true;
        i++;
      }
      else if (arg.is_equal("-dfs"))
      {
        bfs = false;
        i++;
      }
      else if (arg.is_equal("-l"))
      {
        print_label = true;
        i++;
      }
      else if (arg.is_equal("-s"))
      {
        print_state = true;
        i++;
      }
      else if (arg.is_equal("-equation"))
      {
        equation_format = true;
        i++;
      }
      else if (arg.is_equal("-is"))
      {
        if (i+1 == argc)
          bad_usage = true;
        else
          is = atoi(argv[i+1]);
        i += 2;
      }
      else if (arg.is_equal("-sw"))
      {
        if (i+1 == argc)
          bad_usage = true;
        else
          word1 = argv[i+1];
        i += 2;
      }
      else if (arg.is_equal("-rw"))
      {
        if (i+1 == argc)
          bad_usage = true;
        else
          word2 = argv[i+1];
        i += 2;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (min_length == INVALID_LENGTH)
      min_length = Word_Length(atoi(argv[i++]));
    else if (!seen_max_length)
    {
      max_length = Word_Length(atoi(argv[i++]));
      seen_max_length = true;
    }
    else if (!so.use_stdin && file1 == 0)
      file1 = argv[i++];
    else if (file2 == 0)
      file2 = argv[i++];
    else
      bad_usage = true;
  }
  int exit_code = 1;
  if (equation_format && (print_state | print_label))
    bad_usage = true;
  if (!bad_usage && ((file1 !=0) ^ so.use_stdin) && !(file2 && so.use_stdout))
  {
    exit_code = inner(&container,bfs,min_length,max_length,
                      file1,file2,word1,word2,is,print_label,print_state,
                      equation_format,so.use_stdout);
  }
  else
  {
    cprintf("Usage:\n"
            "fsaenumerate [loglevel] [-is n] [-bfs|-dfs] [[-l] [-s] | [-equation]] [-sw w1]"
            " [-rw w2]\nmin max -i | input_file [-o | output_file]\n"
            "where either stdin (if you specify -i), or input_file"
            " (otherwise), contains a\nGASP FSA .\n"
            "The words (or pairs of words in the case of a product FSA)"
            " accepted by the FSA\nare listed. Only words of length at least"
            " min, and at most max symbols are\nlisted. If the -sw option is"
            " used then only words having w1 as a prefix are\nlisted. In the"
            " case of a product FSA, if -sw is specified then so must be -rw,"
            "\nand in this case only word pairs [w1*u,w2*v] are listed.\n"
            "The -bfs option causes words to be listed in breadth-first"
            " search order. The\ndefault is depth first order (lexicographic"
            " order). -dfs is accepted as an\noption.\n"
            "The -l option causes the label number of the accepting state to"
            " be printed\nwith each accepted word or word-pair.\n"
            "The -s option causes the number of the accepting state to be"
            " printed.\n"
            "The -equation option only applies to multiplier like FSA. For these,"
            " the output is\nshown in the form of an equation\n."
            "[KBMAG] The -is n option changes the FSA so that its initial"
            " state is n.\n");
    so.usage(".enumerate");
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(Container * container,bool bfs,Word_Length min_length,
                 Word_Length max_length,String filename,
                 String output_name,String word1,String word2,State_ID nis,
                 bool print_label,
                 bool print_state,
                 bool equation_format,
                 bool use_stdout)
{
#undef cprintf
#define cprintf container->error_output
  if (use_stdout)
    container->set_gap_stdout(true);

  FSA_Simple * fsa = FSA_Factory::create(filename,container);

  if (fsa)
  {
    String_Buffer sb;
    if (!use_stdout)
      output_name = sb.make_destination(output_name,filename,".enumerate");
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

    Ordinal_Word *ow1 = 0;
    Ordinal_Word *ow2 = 0;
    if (*word1 || *word2)
    {
      ow1 = fsa->base_alphabet.parse(word1,WHOLE_WORD,0,*container);
      ow2 = fsa->base_alphabet.parse(word2,WHOLE_WORD,0,*container);
    }

    Output_Stream * os = container->open_text_output_file(output_name);
    output_name = sb.make_destination(filename,filename,".words");
    container->output(os,"%s := \n[\n  ",output_name.string());
    bool started = false;
    String_Buffer sb1,sb2;
    for (Word_Length stop_length = bfs ? min_length : max_length;
         stop_length <= max_length;
         stop_length++,min_length++)
    {
      if (fsa->is_product_fsa())
      {
        FSA::Product_Iterator pi(*fsa);
        Ordinal_Word ow(fsa->label_alphabet());
        for (State_ID si = pi.first(ow1,ow2);
             si;
             si = pi.next(pi.state_length() < stop_length))
          if (fsa->is_accepting(si) && pi.state_length() >= min_length &&
              pi.state_length() <= stop_length)
          {
            if (started)
              container->output(os,",\n  ");

            if (equation_format)
            {
              container->output(os,"[%s",pi.left_word.format(&sb1).string());
              Label_ID label = fsa->get_label_nr(si);
              if (label)
              {
                fsa->label_word(&ow,label);
                if (ow.length())
                  container->output(os," * %s,",ow.format(&sb1).string());
              }
              else
                container->output(os,",");

              label = fsa->get_label_nr(pi.start_state());
              if (label)
              {
                fsa->label_word(&ow,label);
                if (ow.length())
                  container->output(os,"%s * ",ow.format(&sb1).string());
              }
              container->output(os,"%s]",pi.right_word.format(&sb2).string());
            }
            else
            {
              if (print_label || print_state)
                container->output(os,"[");
              container->output(os,"[%s,%s]",pi.left_word.format(&sb1).string(),
                                         pi.right_word.format(&sb2).string());
              if (print_label)
                container->output(os,"," FMT_ID,fsa->get_label_nr(si));
              if (print_state)
                container->output(os,"," FMT_ID,si);
              if (print_label || print_state)
                container->output(os,"]");
            }
            started = true;
          }
      }
      else
      {
        FSA::Word_Iterator wi(*fsa);
        for (State_ID si = wi.first(ow1);
             si;
             si = wi.next(wi.word.length() < stop_length))
          if (fsa->is_accepting(si) && wi.word.length() >= min_length)
          {
            if (started)
              container->output(os,",\n  ");
            if (print_label || print_state)
              container->output(os,"[");
            container->output(os,"%s",wi.word.format(&sb1).string());
            if (print_label)
              container->output(os,"," FMT_ID,fsa->get_label_nr(si));
            if (print_state)
              container->output(os,"," FMT_ID,si);
            if (print_label || print_state)
              container->output(os,"]");
            started = true;
          }
      }
    }
    container->output(os,"\n];\n");
    container->close_output_file(os);
    if (ow1)
      delete ow1;
    if (ow2)
      delete ow2;
    delete fsa;
    return 0;
  }
  return 1;
}

