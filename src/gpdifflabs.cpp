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


// $Log: gpdifflabs.cpp $
// Revision 1.5  2010/05/16 21:29:03Z  Alun
// Changed to be compatible with KBMAG version of utility: now uses
// difflabs suffix, and second argument can be either a full FSA name or
// just the suffix (which is the only possibility with KBMAG)
// Revision 1.4  2009/09/12 18:47:29Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2007/11/15 22:58:11Z  Alun
//

#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,String filename1,String filename2,
                   bool use_stdout,unsigned fsa_format_flags,
                   Group_Automaton_Type reduction_method,bool accept_only);

int main(int argc,char ** argv)
{
  int i = 1;
  String group_filename = 0;
  String input_file = 0;
  String output_file = 0;
  bool accept_only = false;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|SO_REDUCTION_METHOD|
                      SO_STDIN|SO_STDOUT);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-accept"))
      {
        accept_only = true;
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
    else if (input_file == 0)
    {
      input_file = argv[i];
      i++;
    }
    else if (output_file == 0)
    {
      output_file = argv[i];
      i++;
    }
    else
      bad_usage = true;
  }
  int exit_code;
  if (!bad_usage && ((input_file!=0) ^ so.use_stdin) &&
      !(output_file && so.use_stdout))
  {
    if (so.use_stdout)
      container.set_gap_stdout(true);
    MAF & maf = * MAF::create_from_rws(group_filename,&container);

    exit_code = inner(maf,input_file,output_file,
                      so.use_stdout,so.fsa_format_flags,
                      so.reduction_method,accept_only);
  }
  else
  {
    cprintf("Usage:\n"
            "gpdifflabs [loglevel] [format] [reduction method] [-accept]"
            " groupname\n -i | input_file [-o | output_file]\n"
            "where groupname is a GASP rewriting system for a group.\n"
            "An automaton able to perform word reduction must previously have"
            " been computed.\n"
            "Either stdin (if you specify -i), or input_file"
            " (otherwise), should contain a\nGASP 2-variable FSA. If the file"
            " to be used as input has the name\ngroupname.fsasuffix then you"
            " can specify it simply as fsasuffix .\n"
            "gpdifflabs labels its states on the assumption that the label"
            " should be the\ndifference between the left and right words"
            " for each state.\n"
            "The input file should be a multiplier FSA for some group"
            " element, but might\nbe a multiplier for a coset system for the"
            " group.\n"
            "If -accept is specified then only accept states are labelled\n");
    so.usage(".difflabs");
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,String filename1,String filename2,
                 bool use_stdout,unsigned fsa_format_flags,
                 Group_Automaton_Type reduction_method,bool accept_only)
{
  if (!maf.load_reduction_method(reduction_method))
  {
    maf.container.error_output("Unable to reduce words using selected mechanism\n");
    delete &maf;
    return 1;
  }

  FSA_Simple * fsa = FSA_Factory::create(filename1,&maf.container,false);
  String_Buffer sb1;
  if (!fsa)
  {
    filename1 = sb1.make_filename("",maf.properties().filename,filename1);
    fsa = FSA_Factory::create(filename1,&maf.container,true);
  }

  FSA_Simple * fsa2 = 0;
  if (fsa)
  {
    fsa2 = maf.labelled_product_fsa(*fsa,accept_only);
    if (fsa2)
    {
      String_Buffer sb2;
      if (!use_stdout)
        filename2 = sb2.make_destination(filename2,filename1,".difflabs");
      fsa2->save(filename2,fsa_format_flags);
      delete fsa2;
    }
    else
    {
      delete fsa;
      delete &maf;
      return 1;
    }
    delete fsa;
  }

  delete &maf;
  return fsa2==0;
}
