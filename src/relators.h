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


/*
$Log: relators.h $
Revision 1.2  2010/06/10 13:58:17Z  Alun
All tabs removed again
Revision 1.1  2010/05/17 16:02:26Z  Alun
New file.
*/
#pragma once
#ifndef RELATORS_INCLUDED
#define RELATORS_INCLUDED 1

#ifndef ARRAYBOX_INCLUDED
#include "arraybox.h"
#endif
#ifndef MAFWORD_INCLUDED
#include "mafword.h"
#endif
#ifndef MAF_INCLUDED
#include "maf.h"
#endif

typedef Array_Of<Element_ID> Relator_Set;

class Relator_Conjugator
{
  public:
    MAF & maf;
    const Ordinal nr_symbols; // This is the number of monoid generators
  private:
    Word_List relators;
    const Word_Collection *extra_relators; // holds normal subgroup generators
    Word_List all_permuted_relators; // by first symbol and reduction ordering
    Relator_Set definition_relators; // index for each generator into the above
    Relator_Set relator_set[12];     // indexes into all_permuted relators for
                                     // 12 possible R sets
    Element_Count minimum_scans_per_transition;
  public:
    Relator_Conjugator(MAF & maf_);
    /* return value from create_relators is the number of relators
       in a basic relator set, excluding all cyclic conjugates and inverses*/
    Element_Count create_relators(const Word_Collection &normal_subgroup_generators,
                                  unsigned tth_flags);
    void tidy();

    const Relator_Set &get_relator_set(int order_type,
                                       bool permute_G,bool permute_N);

    Relator_Set & open_base_relator_set()
    {
      return relator_set[0];
    }


    void get_relator(Word * word,Element_ID relator_nr) const
    {
      all_permuted_relators.get(word,relator_nr);
    }

    void get_relator(Fast_Word * fw,Element_ID relator_nr) const
    {
      all_permuted_relators.get_fast_word(fw,relator_nr);
    }

    void get_range(Element_Count * start,Element_Count * stop,Ordinal g) const
    {
      *start = definition_relators[g];
      *stop = definition_relators[g+1];
    }

    Element_Count count() const
    {
      return all_permuted_relators.count();
    }

    Element_Count minimum_deduction_cost() const
    {
      return minimum_scans_per_transition;
    }

    bool find(const Word &relator,Element_ID * relator_nr=0) const
    {
      return all_permuted_relators.find(relator,relator_nr);
    }
    bool have_quotient_relators() const
    {
      return extra_relators != 0;
    }
  private:

    void conjugate_relators();
    void conjugate_relators_inner(Sorted_Word_List **cyclic_relators,
                                  const Word_Collection &relators);

    void make_relator_set(Relator_Set *relator_set,int order_type,
                          bool permute_G,bool permute_N);

    void make_relator_set_inner(Element_ID * &to,Sorted_Word_List * temp,
                                const Word_Collection &relators,
                                int order_type,bool permute);

};

#endif
