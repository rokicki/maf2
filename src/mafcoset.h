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
$Log: mafcoset.h $
Revision 1.2  2016/09/04 20:18:49Z  Alun_Williams
Fixed Coset_Table_Reducer to work with words containing coset symbol
isnormal utility was broken if a coset table had been created
Revision 1.1  2010/05/19 10:01:42Z  Alun
New file.
*/
#pragma once
#ifndef MAFCOSET_INCLUDED
#define MAFCOSET_INCLUDED 1

#ifndef MAF_INCLUDED
#include "maf.h"
#endif

/* classes referred to but defined elsewhere */
class Word;
class FSA;

class Coset_Table_Reducer : public Word_Reducer
{
  public:
    const MAF & maf;
    const FSA & coset_table;
  private:
    Ordinal * generators;
  public:
    Coset_Table_Reducer(const MAF & maf,const FSA & coset_table_);
    ~Coset_Table_Reducer();
    unsigned reduce(Word * word,const Word & start_word,unsigned flags,
                    const FSA *);
    /* read_word() sets word to the defining word of the coset
       specified by si*/
    void read_word(Word * word,State_ID si,bool want_coset_symbol=false) const;
    Ordinal generator(Element_ID label) const
    {
      return generators[label];
    }
};

#endif
