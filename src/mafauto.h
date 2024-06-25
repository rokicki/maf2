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
$Log: mafauto.h $
Revision 1.13  2010/05/17 10:27:54Z  Alun
Lots of changes so that utilties do not need this file. This is
not quite complete yet as gpminkb.cpp still needs this file.
Many methods of Group_Automata moved either to MAF itself or FSA_Factory.
Revision 1.12  2009/09/12 18:48:30Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.11  2008/10/30 19:17:52Z  Alun
Re-check in of source from web site after catastrophic loss of correct state
of RCS archive.
Revision 1.12  2008/10/30 20:17:51Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.11  2008/10/01 08:27:15Z  Alun
Paramters changed on some methods
Revision 1.10  2008/09/22 19:49:32Z  Alun
Final version built using Indexer.
Revision 1.4  2007/12/20 23:25:45Z  Alun
*/

#pragma once
#ifndef MAFAUTO_INCLUDED
#ifndef FSA_INCLUDED
#include "fsa.h"
#endif
#ifndef MAF_INCLUDED
#include "maf.h"
#endif

// Classes referred to and defined elsewhere
class Rewriter_Machine;
class FSA_Simple;
class Ordinal_Word;
class Diff_Reduce;
class Rewriting_System;
class Sorted_Word_List;
class Word_Collection;
class General_Multiplier;
class Word_DB;

// Classes defined in this header file
class Group_Automata;
class Difference_Machine_Index;

class Difference_Machine_Index
{
  private:
    const State_ID* dm_si_from_dm_label;
  public:
    const Alphabet &alphabet;
    Word_DB &initial_dm_labels;
  public:
    Difference_Machine_Index(const FSA & difference_machine);
    ~Difference_Machine_Index();
    bool find(const Word & word,State_ID * si) const;
};


class Group_Automata
{
  friend class Vital_Builder;
  private:
    FSA_Simple *dm1;
    FSA_Simple *dm2;
    FSA_Simple *word_acceptor;
    General_Multiplier *multiplier;
    Rewriting_System * rws;
    bool owner;
  public:
    Group_Automata() :
      dm1(0),
      dm2(0),
      word_acceptor(0),
      multiplier(0),
      rws(0),
      owner(true)
    {}
    ~Group_Automata()
    {
      erase();
    }
    void erase();
    bool build_vital(Rewriter_Machine * rm,bool is_confluent,int action);
    bool load_vital(MAF & maf);
    void grow_automata(Rewriter_Machine * rm,
                       FSA_Buffer * buffer = 0,
                       unsigned save_flags = GA_ALL,
                       unsigned retain_flags = 0,
                       unsigned exclude_flags = 0);
    /* transfer transfers ownership of the FSAS in automata to
       the specified FSA_Buffer. But the FSAs are retained in automata
       as well, just in case a further call to grow_automata() will
       create some of the other FSAs again */
    bool transfer(FSA_Buffer * fsas);
    bool word_accepted(const Ordinal * values,size_t length) const
    {
      return word_acceptor->accepts(values,length);
    }

#if 1

    /* Remaining functions are static to facilitate reuse. */

    static FSA_Simple * build_finite_gm(Rewriter_Machine * rm,
                                        const FSA * word_acceptor,
                                        const FSA * difference_machine);
    // Sorry about the different return type on this one
    // The method can fail to complete, so we need more return info than
    // from most of the other methods.
    static int build_gm(FSA_Simple **answer,Rewriter_Machine * rm,
                        const FSA * word_acceptor,
                        const FSA * difference_machine,
                        bool correct_only,State_Count *nr_factory_states = 0);
    static FSA_Simple * build_L1_acceptor(Rewriter_Machine * rm,const FSA * wa,
                                          const FSA * gwa,bool test_only);

    static int gm_valid(const General_Multiplier & gm,Rewriter_Machine * rm,unsigned gwd_flags);
    // dm is temporarily modified, but is unchanged on return by
    // build_primary_recogniser()
    static FSA_Simple * build_primary_recogniser(const FSA * L1_acceptor,
                                                 const FSA * wa,
                                                 FSA_Simple * dm);

    static void create_defining_equations(Rewriter_Machine * rm,FSA *wa);

    static FSA_Simple * reducer(const MAF & maf,
                                const General_Multiplier & multiplier,
                                const FSA & difference_machine,
                                bool full_length = false);

    static FSA_Simple *build_equation_recogniser_from_rr(FSA_Simple & rr);
    static int validate_difference_machine(const FSA * dm,
                                           const FSA * wa,
                                           Rewriter_Machine *rm);
#endif
  private:
    static void set_finite(MAF & maf,bool g_finite,bool coset_finite)
    {
      if (g_finite)
        maf.is_g_finite = true;
      if (coset_finite)
        maf.is_coset_finite = true;
    }
};


#endif
