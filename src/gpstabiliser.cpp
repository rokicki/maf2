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


// $Log: gpstabiliser.cpp $
// Revision 1.2  2010/06/10 14:33:18Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.1  2010/05/11 18:03:28Z  Alun
// New file.
//

#include <stdlib.h>
#include "maf.h"
#include "mafctype.h"
#include "container.h"
#include "alphabet.h"
#include "mafword.h"
#include "maf_so.h"
#include "fsa.h"

int main(int argc,char ** argv)
{
  int i = 1;
  String group_filename = 0;
  String subgroup_suffix = 0;
  String output_name = 0;
  String one_word = 0;
  bool cosets = false;
  Container * container = MAF::create_container();
  Standard_Options so(*container,SO_REDUCTION_METHOD);
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
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else
    {
      if (group_filename == 0)
        group_filename = argv[i];
      else if (cosets && subgroup_suffix == 0)
        subgroup_suffix = argv[i];
      else if (!one_word)
        one_word = argv[i];
      else if (!output_name)
        output_name = argv[i];
      else
        bad_usage = true;
      i++;
    }
  }

  if (!bad_usage && group_filename && one_word)
  {
    bool use_stdout = output_name == 0;
    if (use_stdout)
      container->set_gap_stdout(true);
    MAF * maf = MAF::create_from_input(cosets,group_filename,subgroup_suffix,
                                       container);

    if (!maf->load_reduction_method(so.reduction_method))
    {
      cprintf("Unable to reduce words using selected mechanism\n");
      delete maf;
      delete container;
      return 1;
    }

    const FSA * wa = maf->load_fsas(GA_WA);
    if (!wa)
    {
      cprintf("A word-acceptor for the group is required\n");
      delete maf;
      delete container;
      return 1;
    }

    const FSA * coset_table = maf->load_fsas(GA_COSETS);
    if (!coset_table)
    {
      cprintf("A coset table for the group is required\n");
      delete maf;
      delete container;
      return 1;
    }

    FSA_Simple * factory = FSA_Factory::copy(*coset_table);
    factory->clear_accepting(true);
    Ordinal_Word * ow = maf->parse(one_word);
    Ordinal_Word iword(maf->alphabet);
    Ordinal_Word test(maf->alphabet);
    FSA::Word_Iterator wi(*wa);
    State_ID wa_si;
    bool is_coset_system = maf->properties().is_coset_system;
    Ordinal nr_generators = maf->group_alphabet().letter_count();
    for (wa_si = wi.first();wa_si; wa_si = wi.next(true))
    {
      if (is_coset_system)
      {
        maf->invert(&iword,wi.word);
        test.set_length(0);
        test.append(nr_generators);
        test += iword;
      }
      else
        maf->invert(&test,wi.word);
      test += *ow;
      test += wi.word;
      maf->reduce(&test,test);
      if (is_coset_system)
      {
        Word_Length l = test.length();
        const Ordinal * values = test.buffer();
        Word_Length i;
        for (i = 0; i < l;i++)
          if (values[i] < nr_generators)
            break;
        test = Subword(test,i,l);
      }
      State_ID coset_si = factory->read_word(wi.word);
      factory->set_is_accepting(coset_si,test == *ow);
      for (Ordinal g = 0; g < nr_generators;g++)
        if (!wa->new_state(wa_si,g,true))
          factory->set_transition(coset_si,g,0);
    }
    factory->set_label_type(LT_Unlabelled);
    FSA_Simple *stabiliser = FSA_Factory::minimise(*factory);
    delete factory;
    stabiliser->save(output_name,so.fsa_format_flags);
    delete stabiliser;
    delete ow;
    delete maf;
  }
  else
  {
    cprintf("Usage:\n"
            "gpstabiliser [loglevel] [reduction_method] rwsname [-cos [subsuffix]]"
            " word [output_file]\n"
            "where rwsname is a GASP rewriting system and,"
            " optionally, if the\n-cos option is specified, rwsname.subsuffix"
           " is a substructure\nfile.\n"
           "\"automata\" must already have been run against the relevant"
           " rewriting-system.\n"
           "A word-acceptor for the stabiliser of the specified word is created.\n");
    so.usage(0);
    delete container;
    return 1;
  }

  delete container;
  return 0;
}

