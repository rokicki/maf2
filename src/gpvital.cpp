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


// $Log: gpvital.cpp $
// Revision 1.9  2010/05/11 20:56:12Z  Alun
// Argument parsing changed not to need strcmp(). Help changed
// Revision 1.8  2009/11/08 20:46:13Z  Alun
// Command line option handling changed.
// Revision 1.7  2009/10/12 22:20:06Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.6  2009/09/12 18:47:37Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.5  2009/08/23 19:25:14Z  Alun_Williams
// New options included in help message
// Revision 1.4  2008/12/02 21:16:46Z  Alun
// New options added  - but not yet included in help message! REVIEW!
// Revision 1.3  2008/09/30 09:25:12Z  Alun
// Final version built using Indexer.
//

//#include <stdlib.h>
#include "maf.h"
#include "container.h"
#include "maf_so.h"
#include "alphabet.h"
#include "mafword.h"
#include "fsa.h"
#include "equation.h"

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * subgroup_suffix = 0;
  bool cosets = false;
  bool axioms = false;
  bool eliminate = false;
  bool raw = false;
  Group_Automaton_Type dm_to_use = GAT_Primary_Difference_Machine;
  Container * container = MAF::create_container();
  Standard_Options so(*container,SO_STDOUT|SO_GPUTIL);
  bool bad_usage = false;
  int retcode = 0;
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
      else if (so.present(arg,"-no_axioms"))
      {
        axioms = false;
        i++;
      }
      else if (arg.is_equal("-axioms"))
      {
        axioms = true;
        i++;
      }
      else if (arg.is_equal("-kb") || arg.is_equal("-eliminate"))
      {
        eliminate = true;
        i++;
      }
      else if (arg.is_equal("-raw"))
      {
        raw = true;
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
    MAF & maf = * MAF::create_from_input(cosets,group_filename,subgroup_suffix,
                                         container);
    const FSA * dm = maf.load_fsas(1 << dm_to_use);
    if (dm)
    {
      MAF::Options options = maf.options;
      options.no_differences = true;
      options.special_overlaps = 0;
      options.partial_reductions = 0;
      // Create a new MAF object for the equations implied by the dm to live in
      MAF & maf1 = *MAF::create(container,&options,AT_String,PT_Group);
      // and a second MAF object for a non redundant subset of these to live in
      MAF & maf2 = *MAF::create(container,&options,AT_String,PT_Group);

      Ordinal nr_generators = dm->base_alphabet.letter_count();
      const Alphabet &a_start = dm->base_alphabet;
      Ordinal g;

      maf1.set_nr_generators(nr_generators);
      maf2.set_nr_generators(nr_generators);
      for (g = 0; g < nr_generators;g++)
      {
        maf1.set_next_generator(a_start.glyph(g));
        maf2.set_next_generator(a_start.glyph(g));
      }
      for (g = 0; g < nr_generators;g++)
      {
        maf1.set_inverse(g,maf.inverse(g));
        maf2.set_inverse(g,maf.inverse(g),0);
      }
      State_Count nr_states = dm->state_count();
      State_Count nr_transitions = dm->alphabet_size();
      Ordinal_Word start_word(a_start);
      Ordinal_Word lhs_word(a_start);
      Ordinal_Word rhs_word(a_start);
      unsigned aa_flags = raw ? 0 : (eliminate ?
               AA_ADD_TO_RM|AA_DEDUCE|AA_ELIMINATE|AA_POLISH :
               AA_ADD_TO_RM|AA_DEDUCE|AA_POLISH);

      if (axioms)
        for (const Linked_Packed_Equation * axiom = maf.first_axiom();axiom;
             axiom = axiom->get_next())
        {
          Simple_Equation se(maf1.alphabet,*axiom);
          maf1.add_axiom(se.lhs_word,se.rhs_word,aa_flags);
        }

      for (State_ID si = 1; si < nr_states;si++)
      {
        dm->label_word(&start_word,si);
        for (Transition_ID ti = 0; ti < nr_transitions;ti++)
        {
          State_ID nsi = dm->new_state(si,ti);
          if (nsi)
          {
            Ordinal g1,g2;
            a_start.product_generators(&g1,&g2,ti);
            lhs_word.set_length(0);
            dm->label_word(&rhs_word,nsi);
            if (g1 != PADDING_SYMBOL)
              lhs_word.append(maf.inverse(g1));
            lhs_word += start_word;
            if (g2 != PADDING_SYMBOL)
              lhs_word.append(g2);
            maf1.add_axiom(lhs_word,rhs_word,aa_flags);
          }
        }
      }

      for (const Linked_Packed_Equation * axiom = maf1.first_axiom();axiom;
           axiom = axiom->get_next())
      {
        Simple_Equation se(maf1.alphabet,*axiom);
        maf2.add_axiom(se.lhs_word,se.rhs_word,0);
      }
      maf2.save_as(maf.properties().filename,".vital");
      delete &maf1;
      delete &maf2;
    }
    else
    {
      cprintf("Required difference machine not found\n");
      retcode = 3;
    }
    delete &maf;
  }
  else
  {
    cprintf("Usage:\n"
            "gpvital [loglevel] [-axioms] [-raw | -eliminate] [-diff2c] groupname [-cos [subsuffix]]\n"
            "where groupname is a GASP rewriting system for a group, and,"
            " if the -cos\noption is used, groupname.subsuffix"
            " is a substructure file.\n"
            "A correct word-difference machine for the object must be"
            " available.\ngpvital creates a new rewriting system for the same"
            " object with axioms deduced\nfrom the transitions in the"
            " word-difference machine. Axioms are only included\nif they"
            " cannot easily be deduced from earlier ones.\n"
            "-diff2c selects the diff2c machine. The default is the diff1c"
            " machine\n"
            "-axioms causes the original axioms to be considered first.\n"
            "If -raw is specified no attempt is made to eliminate redundant"
            " axioms.\nIf -kb is specified MAF tries harder to"
            " eliminate unecessary axioms.\n");
    so.usage(".vital");
    retcode = 1;
  }

  delete container;
  return retcode;
}

