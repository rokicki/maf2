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


// $Log: gpsubpres.cpp $
// Revision 1.6  2010/05/16 20:54:45Z  Alun
// Option to extract subgroup presentation from H equations of coset system
// added.
// Added signal handling so we can make the program hurry up if simplification
// is taking too long
// Revision 1.5  2009/11/08 20:42:57Z  Alun
// Command line option handling changed.
// Revision 1.4  2009/09/12 18:47:36Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2008/09/18 19:13:42Z  Alun
// "Mid Sep 2008 snapshot"
// Revision 1.1  2007/11/15 22:57:50Z  Alun
// New file.
//

#include <signal.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"

static MAF * global_maf;

void signal_handler(int /*signal_nr*/)
{
  if (global_maf)
    global_maf->give_up();
}

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,Group_Automaton_Type reduction_method,
                   unsigned method,bool pres,bool change_alphabet);

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * sub_suffix = 0;
  String prefix = "_x";
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_REDUCTION_METHOD);
  bool change_alphabet = true;
  unsigned method = 0;
  bool pres = false;
  bool bad_usage = false;
#define cprintf container.error_output
  MAF::Options options;
  options.no_composite = false;
  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-cos"))
        i++;
      else if (arg.is_equal("-eliminate") || arg.is_equal("-kb"))
      {
        options.eliminate = true;
        i++;
      }
      else if (arg.is_equal("-keep"))
        change_alphabet = false;
      else if (so.present(arg,"-no_composite"))
      {
        options.no_composite = true;
        i++;
      }
      else if (arg.is_equal("-pres"))
      {
        pres = true;
        i++;
      }
      else if (arg.is_equal("-rs"))
      {
        method = 1;
        i++;
      }
      else if (arg.is_equal("-rws"))
      {
        method = 2;
        i++;
      }
      else if (arg.is_equal("-schreier"))
      {
        options.use_schreier_generators = true;
        i++;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (group_filename == 0)
    {
      group_filename = argv[i];
      i++;
    }
    else if (sub_suffix == 0)
    {
      sub_suffix = argv[i];
      i++;
    }
    else
      bad_usage = true;
  }
  int exit_code;
  if (group_filename && !bad_usage)
  {
    MAF & maf = * MAF::create_from_input(true,group_filename,sub_suffix,&container,
                                         CFI_DEFAULT,&options);
    signal(SIGINT,signal_handler);
    signal(SIGTERM,signal_handler);
    global_maf = &maf;
    exit_code = inner(maf,so.reduction_method,method,pres,change_alphabet);
  }
  else
  {
    cprintf("Usage:\n"
            "gpsubpres [loglevel] [reduction_method] [-rs | -rws] [-schreier]"
            " groupname [subsuffix | cossuffix]\n"
            "where groupname is a GASP rewriting system for a group, and the"
            " coset system\ndescribed by the substructure file"
            " groupname.subssuffix has been successfully\nprocessed.\n"
            "This program computes a presentation of the subgroup\n"
            "If -schreier is specified (or the .migm file is in KBMAG format)"
            " then new\ngenerators are used, otherwise the original subgroup"
            " generators are used.\nThe presentation is normally output to a"
            " new rewriting system file called\ngroupname.subsuffix.rws. If"
            " -pres is specified then the presentation is output\nas GAP"
            " source code to groupname.subsuffix.pres. In this case the"
            " generators\n are renamed unless -keep is specified.\n"
            "If -kb is specified, MAF tries to eliminate axioms"
            " that can be deduced\nfrom earlier axioms\nIf -no_composite is"
            " specified MAF first tries to calculate the presentation\n"
            "without building the composite multiplier for the group"
            " relators. This is\noften, but not always, faster, but usually"
            " results in a more redundant\npresentation. -kb will help"
            " in this case, but offset any speed improvement\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,Group_Automaton_Type reduction_method,unsigned method,bool pres, bool change_alphabet)
{
  int exit_code = 1;
  MAF * maf_sub = 0;

  if (method == 0)
  {
    if (maf.load_fsas(GA_GM))
      maf_sub = maf.subgroup_presentation(*maf.fsas.gm,
                                          maf.options.use_schreier_generators);
    else
      maf.container.error_output("The coset general multiplier is required\n");
  }
  else if (method==1)
  {
    const FSA * coset_table = maf.load_fsas(GA_COSETS);
    if (!coset_table)
    {
      const FSA * wa = maf.load_fsas(GA_WA);
      maf.load_reduction_method(reduction_method);
      Word_Reducer * wr = maf.take_word_reducer();
      if (wa && wr)
      {
        FSA_Simple * temp = maf.coset_table(*wr,*wa);
        if (temp)
          maf.save_fsa(temp,GAT_Coset_Table);
        delete temp;
        coset_table = maf.load_fsas(GA_COSETS);
      }
      else
        maf.container.error_output("The coset word-acceptor and a method of word reduction are required\n");
      if (wr)
        delete wr;
    }
    if (coset_table)
      maf_sub = maf.rs_presentation(*coset_table);
  }
  else if (method == 2)
  {
    if (maf.load_fsas(GA_RWS))
      maf_sub = maf.subgroup_presentation(*maf.fsas.min_rws,
                                          maf.options.use_schreier_generators);
  }

  if (maf_sub != 0)
  {
    String_Buffer sb;
    const Presentation_Data & pd = maf.properties();
    String filename = sb.make_filename("",
                                       pd.subgroup_filename !=0 ?
                                       pd.subgroup_filename.string() :
                                       pd.filename.string(),
                                       pres ? ".pres" : ".rws");

    Output_Stream * os = maf.container.open_text_output_file(filename);

    if (pres)
    {
      if (maf_sub->output_gap_presentation(os,change_alphabet))
        exit_code = 0;
    }
    else
    {
      maf_sub->print(os);
      exit_code = 0;
    }
    maf.container.close_output_file(os);
    delete maf_sub;
  }

  delete &maf;
  return exit_code;
}

