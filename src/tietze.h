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
$Log: tietze.h $
Revision 1.1  2010/04/04 21:49:56Z  Alun
New file.
*/
#pragma once
#ifndef TIETZE_INCLUDED
#define TIETZE_INCLUDED

#ifndef MAFWORD_INCLUDED
#include "mafword.h"
#endif
#ifndef BITARRAY_INCLUDED
#include "bitarray.h"
#endif

// classes referred to but defined elsewhere
class MAF;

/**/

const unsigned FS_USE_WORD1 = 1;
const unsigned FS_USE_WORD2 = 2;
const unsigned FS_FORBIDDEN_GENERATOR = 8;
const unsigned FS_SIMPLIFY_NOW = 16;

const unsigned TTH_KEEP_GENERATORS = 1;
const unsigned TTH_NO_SIMPLIFY = 2;
const unsigned TTH_ABELIANISE = 4;
const unsigned TTH_KEEP_GOING = 8;
const unsigned TTH_BEST_EQUATION_RELATORS = 16;

class Tietze_Transformation_Helper
{
  struct Simplification
  {
    const Word *word1;
    const Word *word2;
    Word_Length start1;
    Word_Length start2;
    Word_Length match;
    Word_Length benefit;
    bool inverted;
    void construct(const Word & word1_,Word_Length start1_,
                   const Word & word2_,Word_Length start2_,
                   Word_Length match_,Word_Length benefit_,
                   bool inverted_)
    {
      word1 = &word1_;
      word2 = &word2_;
      start1 = start1_;
      start2 = start2_;
      match = match_;
      benefit = benefit_;
      inverted = inverted_;
    }
  };

  public:
    MAF & maf;
  private:
    const Ordinal nr_generators;
    Ordinal nr_new_generators;
    Ordinal last_g;
    Ordinal max_seen;
    Word_Length long_length;
    Word_Length max_elimination;
    // arrays indexed by generator
    unsigned long * total_weight;
    unsigned long * weight;
    bool * present;
    Ordinal_Word ** eliminated_generators;
    Sorted_Word_List relators;
    // indexed by relators
    Simplification **available;
    unsigned long *lex_timestamp;
    unsigned long *lex_checked_timestamp;
    Bit_Array in_available;
    unsigned long timestamp;
    //index of first relator which cannot be simplified at the moment
    Word_Length new_max_length;
    Word_Length allowed_max_length;
    Element_ID first_complete;
    Element_ID last_new_relator;
    Element_ID first_in_available;
    unsigned strategy;
    bool abelian;
    bool best_equation_relators;
    bool keep_generators;
    bool keep_going;
    bool no_simplify;
    bool ok;
  public:
    Tietze_Transformation_Helper(MAF & maf_,unsigned strategy_,
                                 unsigned tth_flags,
                                 Ordinal max_seen_ = 0,
                                 Word_Length max_elimination = 0,
                                 Word_Length long_length = 0);
    ~Tietze_Transformation_Helper()
    {
      delete [] weight;
      delete [] total_weight;
      delete [] present;
      if (lex_timestamp)
      {
        delete [] lex_timestamp;
        delete [] lex_checked_timestamp;
      }
      if (eliminated_generators)
      {
        for (Ordinal g = 0; g < nr_generators;g++)
          if (eliminated_generators[g])
            delete eliminated_generators[g];
        delete [] eliminated_generators;
        eliminated_generators = 0;
      }
    }
    bool eliminate();
    bool simplify();
    bool lex_changes();
    void polish();
    // polished_presentation returns a new MAF object containing the
    // simplified presentation. If catching_maf is non-zero then
    // it assumed a signal handler is installed that will call give_up()
    // for a global_maf, catching_maf should be &global_maf
    MAF * polished_presentation(unsigned aa_flags,MAF ** catching_maf = 0);
    const Sorted_Word_List &get_relators() const
    {
      return relators;
    }
  private:
    void do_simplification(const Simplification &s);
    bool find_simplifications(unsigned flags,Element_ID word1_nr = 0,
                              Element_ID word2_nr = 0);
    bool find_lex_changes();
    bool insert_relator(Sorted_Word_List &relators,const Word & word,bool make_canonical);
    void remove_available();
    bool remove_relator(const Word & word);
    bool reduce_and_insert_relator(Sorted_Word_List &relators,Ordinal_Word & new_relator);
    bool substitute(Ordinal_Word * answer,
                    bool * changed,
                    const Word &word,
                    Ordinal g1,const Ordinal_Word & g1_word,
                    Ordinal g2,const Ordinal_Word & g2_word);
};

#endif
