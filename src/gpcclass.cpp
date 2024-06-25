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


// $Log: gpcclass.cpp $
// Revision 1.2  2010/05/16 20:00:42Z  Alun
// factory methods moved to mafconj.cpp. Added option to construct two-variable
// FSA that finds least conjugating element for each element
// Revision 1.1  2009/11/02 14:53:02Z  Alun
// New file.
/* This program creates an FSA which acts as a function that maps each
   element of a group to the minimal representative of the conjugacy class
*/
#include <stdlib.h>
#include "fsa.h"
#include "container.h"
#include "maf_so.h"
#include "maf.h"

int main(int argc,char ** argv)
{
  int i = 1;
  int retcode = 1;
  char * group_filename = 0;
  char * subgroup_suffix = 0;
  bool cosets = false;
  bool conjugator = false;
  Container * container = MAF::create_container();
  Standard_Options so(*container,SO_REDUCTION_METHOD);
  bool bad_usage = false;
#define cprintf container->error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-conjugator"))
      {
        conjugator = true;
        i++;
      }
      else if (arg.is_equal("-cos"))
      {
        cosets = true;
        i++;
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
      else
        bad_usage = true;
      i++;
    }
  }

  if (!bad_usage && group_filename)
  {
    MAF * maf = MAF::create_from_input(cosets,group_filename,subgroup_suffix,
                                       container);
    if (!maf->properties().g_is_group)
      cprintf("gpcclass only works with group rewriting systems\n");
    else if (!maf->load_reduction_method(GAT_Minimal_RWS))
      cprintf("A confluent rewriting system is required\n");
    else
    {
      FSA_Simple * fsa = maf->build_class_table(*maf->fsas.min_rws);
      if (fsa != 0)
      {
        maf->save_fsa(fsa,GAT_Conjugacy_Class);
        if (conjugator)
        {
          FSA_Simple * fsa2 = maf->build_conjugator(*fsa,*maf->fsas.min_rws);
          maf->save_fsa(fsa2,".conjugator",so.fsa_format_flags);
          delete fsa2;
        }
        retcode = 0;
        delete fsa;
      }
    }
    delete maf;
  }
  else
  {
    cprintf("Usage:\n"
            "gpclass [loglevel] [reduction_method] [-conjugator] groupname"
            "\n[-cos [subsuffix | cossuffix]]\n"
            "where groupname is a GASP rewriting system for a finite group,"
            " or, if the -cos\noption is used, groupname.subsuffix is a"
            " finite index subgroup of a group\n.An automaton that can perform"
            " word reduction for the relevant rewriting system\nmust"
            " previously have been computed.\n"
            "An FSA is computed with the same states and transitions as the"
            " coset table.\nThe accepted language consists of words that are"
            " equal as elements to the\nminimal representative of the"
            " conjugacy class of the element. Every state is\nlabelled by its"
            " conjugacy class representative\nThe FSA is output to"
            " groupname.cclasses. A file groupname.cc_statistics\ncontaining"
            " a summary of the conjugacy information is also generated\n\n"
            "If the -conjugator option is specified an additional 2-variable"
            " FSA is\ncomputed and output to groupname.conjugator.\n"
            "This accepts the word pair (u,v) at a state labelled with w if"
            " u and v are\naccepted words and v is the least accepted word"
            " such that v^-1*u*v reduces\nto the conjugacy class"
            " representative w. This FSA is not computed by default\nbecause"
            " it can be very large.\n");
    so.usage();
  }
  delete container;
  return retcode;
}
