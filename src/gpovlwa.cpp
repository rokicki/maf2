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


// $Log: gpovlwa.cpp $
// Revision 1.5  2010/06/10 13:57:18Z  Alun
// All tabs removed again
// Revision 1.4  2010/05/17 07:50:13Z  Alun
// Command line argument parsing improved. Help changed. FSA is now constructed
// with an FSA_Factory method
// Revision 1.3  2009/09/12 18:47:35Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2007/10/24 21:15:32Z  Alun
//

#include <string.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"

/**/

int main(int argc,char ** argv);
  static int inner(Container & container,String input_file,String output_file,bool use_stdout,
                   unsigned fsa_format_flags);

int main(int argc,char ** argv)
{
  int i = 1;
  String input_file = 0;
  String output_file = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_GPUTIL|SO_STDOUT|SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY);
  bool bad_usage = false;
  int exit_code = 1;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      if (!so.recognised(argv,i))
        bad_usage = true;
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
  if (!bad_usage && input_file && !(output_file && so.use_stdout))
    exit_code = inner(container,input_file,output_file,so.use_stdout,so.fsa_format_flags);
  else
  {
    cprintf("Usage: gpovlwa [loglevel] [format] input_file [-o | output_file]\n"
            "where input_file contains a GASP rewriting system for which the\n"
            "L1 acceptor (language of minimally reduced words) has been"
            " computed.\n"
            "This program calculates the overlap word-acceptor\n");
    so.usage(".ovlwa");
  }
  delete &container;
  return exit_code;
}

static int inner(Container & container,String rws_filename,String output_file,bool use_stdout,
                 unsigned fsa_format_flags)
{
  MAF & maf = * MAF::create_from_rws(rws_filename,&container);
  const FSA * L1_acceptor = maf.load_fsas(GA_L1_ACCEPTOR);
  if (!L1_acceptor)
  {
    maf.container.error_output("The L1 acceptor (.minred FSA) is required\n");
    delete &maf;
    return 1;
  }
  FSA_Simple * overlap_wa = FSA_Factory::overlap_language(*L1_acceptor);
  if (overlap_wa)
  {
    String_Buffer sb;
    String var_name = sb.make_filename("",maf.properties().name,".ovlwa");
    overlap_wa->set_name(var_name);
    if (!use_stdout)
      output_file = sb.make_destination(output_file,rws_filename,".ovlwa");
    overlap_wa->save(output_file,fsa_format_flags);
    delete overlap_wa;
  }
  delete &maf;
  return 0;
}

