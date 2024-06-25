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


// $Log: gpwa.cpp $
// Revision 1.6  2010/05/16 20:59:50Z  Alun
// Argument parsing changed not to need strcmp(). Help changed
// Revision 1.5  2009/11/08 20:36:43Z  Alun
// Command line option handling changed.
// Revision 1.4  2009/09/14 09:57:54Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2008/10/02 08:01:34Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.3  2008/10/02 09:01:33Z  Alun
// Command line options added to allow selection of dm to use
// Also options to limit state length/number of states made available
// Revision 1.2  2007/10/24 21:15:32Z  Alun
//

#include <stdlib.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"

/**/

int main(int argc,char ** argv);

int main(int argc,char ** argv)
{
  int i = 1;
  bool cosets = false;
  char * group_filename = 0;
  char * sub_suffix = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_KBMAG_COMPATIBILITY|SO_FSA_FORMAT);
  bool bad_usage = false;
  bool outer_geodesic = false;
  Group_Automaton_Type dm_to_use = GAT_Primary_Difference_Machine;
  Word_Length max_length = 0;
  State_Count max_states = 0;

#define cprintf container.error_output

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
      else if (arg.is_equal("-diff1c"))
      {
        dm_to_use = GAT_Primary_Difference_Machine;
        i++;
      }
      else if (arg.is_equal("-diff2c"))
      {
        dm_to_use = GAT_Correct_Difference_Machine;
        i++;
      }
      else if (arg.is_equal("-diff2"))
      {
        dm_to_use = GAT_Full_Difference_Machine;
        i++;
      }
      else if (arg.is_equal("-pdiff1"))
      {
        dm_to_use = GAT_Provisional_DM1;
        i++;
      }
      else if (arg.is_equal("-pdiff2"))
      {
        dm_to_use = GAT_Provisional_DM2;
        i++;
      }
      else if (so.present(arg,"-outer_geodesic"))
      {
        outer_geodesic = true;
        i++;
      }
      else if (so.present(arg,"-max_length"))
      {
        if (i+2 <= argc)
        {
          int l = atoi(argv[i+1]);
          if (l <= MAX_WORD)
            max_length = Word_Length(l);
          else
            container.usage_error("Invalid value for max_length specified. Maximum is %d\n",MAX_WORD);
        }
        else
          bad_usage = true;
        i += 2;
      }
      else if (so.present(arg,"-max_states"))
      {
        if (i+2 <= argc)
          max_states = atol(argv[i+1]);
        else
          bad_usage = true;
        i += 2;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (group_filename == 0)
    {
      group_filename = argv[i];
      i++;
    }
    else if (cosets && sub_suffix == 0)
    {
      sub_suffix = argv[i];
      i++;
    }
    else
      bad_usage = true;
  }
  int exit_code = 1;
  if (!bad_usage && group_filename)
  {
    MAF & maf = * MAF::create_from_input(cosets,group_filename,sub_suffix,
                                         &container);
    const FSA * source = maf.load_fsas(1 << dm_to_use);
    if (source)
    {
      FSA_Simple * fsa =
        maf.build_acceptor_from_dm(source,false,outer_geodesic,
                                   false,max_length,max_states);
      if (fsa)
      {
        if (outer_geodesic)
          container.progress(1,"Outer geodesic word-acceptor with " FMT_ID
                             " states constructed\n",fsa->state_count());
        else
          container.progress(1,"Provisional word-acceptor with " FMT_ID
                             " states constructed\n",fsa->state_count());

        maf.save_fsa(fsa,outer_geodesic ? ".outer_geodesic_wa" : ".pwa",so.fsa_format_flags);
        delete fsa;
        exit_code = 0;
      }
      else
        cprintf("Unable to contruct word-acceptor."
                " Ordering may not support construction of automata\n");
    }
    else
      cprintf("Required word-difference machine not found\n");
    delete &maf;
  }
  else
  {
    cprintf("Usage:\ngpwa [loglevel] [dm_selector] [options]"
            " groupname [-cos [subsuffix | cossuffix]]\n"
            "where groupname is a GASP rewriting system for a group, and, if"
            " the -cos option\nis used, groupname.subsuffix is a substructure"
            " file describing a subgroup.\n"
            "A word-difference machine must have been computed.\n"
            "gpwa builds the FSA which accepts words that cannot be reduced"
            " using the\nspecified word-difference machine\n"
            "dm_selector can be one of: -diff2c, -diff1c, -diff2, -pdiff2,"
            " -pdiff1\n"
            "The following special options are accepted:\n"
            "If -outer_geodesic is specified gpwa constructs an FSA which"
            " accepts all words\nthat cannot be reduced to a shorter word by"
            " the word-difference machine. This\noption only makes sense if"
            " the word ordering for the rewriting system is\ngeodesic. Note"
            " that the FSA built in this case will usually also accept some\n"
            "words that can in fact be reduced to a shorter word.\n"
            "If -max_length n is specified, then when the FSA is being"
            " constructed, new\nstates are only created if the defining"
            " word has length at most n, so that the\noutput FSA will only"
            " reject words which can be reduced with a rule with a LHS\nword"
            " of length n or below.\n"
            "If -max_states n is specified, then prior to minimisation the FSA"
            " constructed\nis allowed to have at most n states. This has a"
            " similar effect to -max_length\nk for some k, but usually some"
            " rules of length k+1 would reject words.\n"
            "Both these options are to allow an FSA to be built when the build"
            " process would\notherwise fail due to time or space constraints.\n");
    so.usage();
  }
  delete &container;
  return exit_code;
}
