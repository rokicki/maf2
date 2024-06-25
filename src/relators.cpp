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


// $Log: relators.cpp $
// Revision 1.2  2010/06/10 13:57:48Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/17 16:03:36Z  Alun
// New file.
//

/* In order to make the code for scanning relators in the various types
   of coset enumerations as simple as possible we are going to fish all
   relators to be scanned out of a Word_List containing the set of all
   cyclic permutations and their inverses of all relators and normal
   subgroup generators, which is also used for consequence processing.
   For each of the 10 possible choices arising from the three available
   CHLT permutations options, and the 4 combinations of G and N
   permutation we construct a list giving the positions of the relators
   to be scanned. This wastes some memory since we could extract all the
   conjugates from a pair of longer words (i.e the relator concatenated
   with itself, and the same for the inverse), but it is convenient for
   now. Also since words are limited in length doing things that way
   would result in the maximum relator length dropping to MAX_WORD/2;
*/

#include "relators.h"
#include "tietze.h"
#include "maf_tc.h"

Relator_Conjugator::Relator_Conjugator(MAF & maf_) :
  maf(maf_),
  nr_symbols(maf_.group_alphabet().letter_count()),
  relators(maf_.group_alphabet()),
  all_permuted_relators(maf_.group_alphabet()),
  extra_relators(0)
{
}

Element_Count Relator_Conjugator::create_relators(const Word_Collection &normal_subgroup_generators,
                                                  unsigned tth_flags)
{
  /* Create a relator set suitable for coset enumeration */


  if (tth_flags != 0)
  {
    Tietze_Transformation_Helper tth(maf,0,tth_flags);
    relators = tth.get_relators();
  }
  else
    maf.relators(&relators);
  extra_relators = normal_subgroup_generators.count() ? &normal_subgroup_generators : 0;
  conjugate_relators();
  return get_relator_set(0,false,false).count();
}

/**/

void Relator_Conjugator::tidy()
{
  all_permuted_relators.empty();
  definition_relators.set_capacity(0,false);
  for (int i = 0; i < 12;i++)
    relator_set[i].set_capacity(0,false);
}

/**/

void Relator_Conjugator::conjugate_relators()
{
  /* build a list of all cyclic permutations and their inverses of
     the relators and normal subgroup generators, and
     empty all the relator sets used for HLT style processing */
  Sorted_Word_List ** cyclic_relators = new Sorted_Word_List * [nr_symbols];
  Ordinal g;
  for (g = 0; g < nr_symbols;g++)
    cyclic_relators[g] = new Sorted_Word_List(relators.alphabet);
  conjugate_relators_inner(cyclic_relators,relators);
  if (extra_relators)
    conjugate_relators_inner(cyclic_relators,*extra_relators);
  all_permuted_relators.empty();
  definition_relators.set_capacity(nr_symbols+1,false);
  for (g = 0; g < nr_symbols;g++)
  {
    definition_relators[g] = all_permuted_relators.count();
    all_permuted_relators += *cyclic_relators[g];
    delete cyclic_relators[g];
  }
  definition_relators[g] = all_permuted_relators.count();
  minimum_scans_per_transition = definition_relators[1];
  /* Now find a number that will allow us to roughly estimate the
     cost of processing the new entry log */
  for (g = 1; g < nr_symbols;g++)
    if (g <= maf.inverse(g))
    {
      Element_Count temp = definition_relators[g+1] - definition_relators[g];
      if (temp < minimum_scans_per_transition)
        minimum_scans_per_transition = temp;
    }
  /* we won't set up the relator sets until we need them,
     except for the base set of relators, because we want to count
     it now. */
  for (int i = 0; i < 12;i++)
    relator_set[i].set_capacity(0,false);
  delete [] cyclic_relators;
}

/**/

void Relator_Conjugator::conjugate_relators_inner(Sorted_Word_List **cyclic_relators,
                                               const Word_Collection &relators)
{
  Element_Count nr_relators = relators.count();
  Ordinal_Word ow(relators.alphabet);
  Ordinal_Word relator(relators.alphabet);
  for (Element_ID r = 0; r < nr_relators;r++)
  {
    if (relators.get(&relator,r))
    {
      Word_Length l = relator.length();
      for (Word_Length i = 0; i < l; i++) // Note that this loop will skip
      {                                   // any "relators" of length 0
        ow = Subword(relator,i) + Subword(relator,0,i);
        cyclic_relators[ow.value(0)]->insert(ow);
        maf.invert(&ow,ow);
        cyclic_relators[ow.value(0)]->insert(ow);
      }
    }
  }
}

/**/

const Relator_Set & Relator_Conjugator::get_relator_set(int order_type,
                                                        bool permute_G,
                                                        bool permute_N)
{
  /* get, first creating if necessary, the approriate Relator_Set
     for the spcified style of scanning */

  int set_nr = (permute_N ? 6 : 0) + (permute_G ? 3 : 0) + order_type;
  Relator_Set &rs = relator_set[set_nr];
  if (rs.count())
    return rs;
  rs.set_capacity(all_permuted_relators.count());
  make_relator_set(&rs,order_type,permute_G,permute_N);
  return rs;
}

/**/

void Relator_Conjugator::make_relator_set(Relator_Set *relator_set,
                                          int order_type,
                                          bool permute_G,bool permute_N)
{
  Sorted_Word_List temp(relators.alphabet,all_permuted_relators.count());
  Relator_Set &rs = *relator_set;
  Element_ID * to = rs.buffer();
  make_relator_set_inner(to,&temp,relators,order_type,permute_G);
  if (extra_relators)
    make_relator_set_inner(to,&temp,*extra_relators,order_type,permute_N);
  rs.set_capacity(to-rs.buffer());
  if (order_type == 2)
  {
    Element_Count count = temp.count();
    Ordinal_Word ow(relators.alphabet);
    rs.set_capacity(count,false);
    Element_ID word_nr,new_word_nr;
    for (word_nr = 0;word_nr < count; word_nr++)
    {
      if (temp.get(&ow,word_nr) &&
          all_permuted_relators.find(ow,&new_word_nr))
        rs[word_nr] = new_word_nr;
    }
  }
}

/**/

void Relator_Conjugator::make_relator_set_inner(Element_ID * &to,
                                                Sorted_Word_List * temp,
                                                const Word_Collection &relators,
                                                int order_type,bool permute)
{
  Element_Count nr_relators = relators.count();
  Ordinal_Word ow(relators.alphabet);
  Ordinal_Word relator(relators.alphabet);
  Element_ID word_nr;

  if (permute)
  {
    if (order_type != 1)
    {
      for (Element_ID r = 0; r < nr_relators;r++)
        if (relators.get(&relator,r))
        {
          Word_Length l = relator.length();
          for (Word_Length i = 0; i < l; i ++)
          {
            ow = Subword(relator,i) + Subword(relator,0,i);
            if (temp->insert(ow))
              if (all_permuted_relators.find(ow,&word_nr))
                *to++ = word_nr;
          }
        }
    }
    else
    {
      for (Word_Length i = 0;; i++)
      {
        bool found = false;
        for (Element_ID r = 0; r < nr_relators;r++)
          if (relators.get(&relator,r))
          {
            Word_Length l = relator.length();
            if (i < l)
            {
              found = true;
              ow = Subword(relator,i) + Subword(relator,0,i);
              if (temp->insert(ow))
                if (all_permuted_relators.find(ow,&word_nr))
                  *to++ = word_nr;
            }
          }
        if (!found)
          break;
      }
    }
  }
  else
  {
    for (Element_ID r = 0; r < nr_relators;r++)
      if (relators.get(&ow,r) && temp->insert(ow) &&
          all_permuted_relators.find(ow,&word_nr))
        *to++ = word_nr;
  }
}

