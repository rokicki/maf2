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


// $Log: gpgeowa.cpp $
// Revision 1.11  2010/07/17 09:52:31Z  Alun
// Change supposedly made in revision 1.10 (to fix crash on take off)
// was not present in RCS archive
// Revision 1.9  2010/05/17 09:25:44Z  Alun
// Help changed. All real code moved to library.
// Revision 1.8  2009/09/12 19:19:45Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.7  2008/11/03 00:33:04Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.8  2008/11/03 01:33:03Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.7  2008/10/01 08:29:47Z  Alun
// parameters for build_acceptor_from_dm() have changed
// Revision 1.6  2008/08/22 07:56:16Z  Alun
// "Early Sep 2008 snapshot"
// Revision 1.3  2007/10/24 21:15:31Z  Alun
//
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"

/**/

int main(int argc,char ** argv);
  static int inner(Container & container,String group_filename,
                   bool near_machine,unsigned fsa_format_flags,
                   Group_Automaton_Type reduction_method);

int main(int argc,char ** argv)
{
  int i = 1;
  bool near_machine = false;
  char * group_filename = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY|SO_REDUCTION_METHOD);
  bool bad_usage = false;
#define cprintf container.error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-near") || arg.is_equal("-n"))
      {
        near_machine = true;
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
    else
      bad_usage = true;
  }
  int exit_code;
  if (!bad_usage && group_filename)
    exit_code = inner(container,group_filename,near_machine,so.fsa_format_flags,so.reduction_method);
  else
  {
    cprintf("Usage: gpgeowa [loglevel] [format] [reduction_method] groupname\n"
           "where groupname is a GASP rewriting system for a group for which"
           " a weakly\ngeodesic automatic structure has been computed.\n"
           "This program calculates the geodesic word-acceptor and related"
           " FSA for a\nword-hyperbolic group. For other groups the it will"
           " never complete.\n"
           "If -near is specified a geodesic multiplier is computed, which"
           " accepts all\nword pairs (ux,v) where u and v are geodesics and x"
           " is the identity or\nan irreducible generator.\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

static int inner(Container & container,String group_filename,
                 bool near_machine,unsigned fsa_format_flags,
                 Group_Automaton_Type reduction_method)
{
  MAF & maf = * MAF::create_from_rws(group_filename,&container,CFI_CREATE_RM);
  maf.options.no_equations = true;
  maf.options.force_differences = true;
  maf.options.partial_reductions = 0;
  maf.load_fsas(GA_WA+GA_DIFF2);

  if (!maf.properties().is_short)
  {
    cprintf("Cannot compute the geodesic word-acceptor when the underlying"
            " word-ordering\nis not geodesic!\n");
    delete &maf;
    return 1;
  }

  if (!maf.fsas.diff2 || !maf.fsas.wa)
  {
    cprintf("Required automata do not exist. gpgeowa cannot run until an"
            " automatic structure\nhas been computed for the input file\n");
    delete &maf;
    return 1;
  }

  if (!maf.load_reduction_method(reduction_method))
  {
    cprintf("Unable to reduce words using selected mechanism\n");
    delete &maf;
    return 1;
  }

  maf.grow_geodesic_automata(near_machine,fsa_format_flags);

  delete &maf;
  return 0;
}
