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


// $Log: rwsprint.cpp $
// Revision 1.4  2010/05/18 11:03:58Z  Alun
// Argument parsing changed not to need strcmp(). Help and other messages changed.
// Revision 1.3  2009/09/12 18:47:59Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2007/10/24 21:15:32Z  Alun
//

#include "maf.h"
#include "fsa.h"
#include "container.h"
#include "maf_rws.h"

/**/

int main(int argc,char ** argv)
{
  Container & container = *MAF::create_container();
  int i = 1;
  char * filename1 = 0;
  String filename2 = 0;
  bool bad_usage = false;
  bool pres = false;
  bool change_alphabet = true;
  int exit_code = 0;
#define cprintf container.error_output

  while (i < argc)
  {
    if (*argv[i] != '-')
    {
      if (filename2)
        bad_usage = true;
      else if (filename1)
        filename2 = argv[i];
      else
        filename1 = argv[i];

      i++;
    }
    else
    {
      String arg = argv[i];
      if (arg.is_equal("-keep"))
        change_alphabet = false;
      else if (arg.is_equal("-pres"))
        pres = true;
      else
        bad_usage = true;
      i++;
    }
  }

  if (!change_alphabet && !pres)
    bad_usage = true;
  if (filename1 && !bad_usage)
  {
    if (!filename2)
      container.set_gap_stdout(true);
    MAF & maf = * MAF::create_from_rws(filename1,&container,CFI_RAW);
    String_Buffer sb;

    filename2 = sb.make_destination(filename2,filename1,pres ?
                                    ".pres" : ".rwsprint");
    Output_Stream * os = container.open_text_output_file(filename2);
    if (pres)
    {
      if (!maf.output_gap_presentation(os,change_alphabet))
      {
        cprintf("This rewriting system is not for a group.\n");
        exit_code = 1;
      }
    }
    else
      maf.print(os);
    container.close_output_file(os);
    delete &maf;
  }
  else
  {
    cprintf("Usage:\n"
            "rwsprint [-pres] [-keep] input_file [output_file]\n"
            "where input_file contains a GASP rewriting system.\n"
            "Unless the -pres option is specified the RWS is output"
            " unchanged, except\nthat it is reformatted and any comments"
            " are lost. With the -pres option,\nwhich is only allowed if all"
            " generators are invertible, GAP source code to\ncreate the group"
            " in GAP is output. In this case the generators are changed to\n"
            "_x1,_x2..., unless you also specify the -keep option, in which"
            " case MAF will\nkeep the names of as many generators as"
            " possible. However, the names of one of\neach pair of inverse"
            " generators will be changed to name^-1, with the other\n"
            "keeping its name.\n");
    exit_code = 1;
  }

  delete &container;
  return exit_code;
}

