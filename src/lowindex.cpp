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


// $Log: lowindex.cpp $
// Revision 1.6  2011/05/20 10:16:49Z  Alun
// detabbed
// Revision 1.5  2010/07/08 15:33:50Z  Alun_Williams
// printf() style argument corrections
// Revision 1.4  2010/06/15 09:17:58Z  Alun
// gofaster code changes
// Revision 1.3  2010/06/10 14:31:17Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.2  2010/06/10 13:57:26Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/19 04:58:30Z  Alun
// New file.
//

/*
This module implements the low index subgroup algorithm. Although it has
a lot in common with coset enumeration we don't use MAF's coset enumerator
class. The reason for this is that most of the code in that isconcerned
with
*/

#include "container.h"
#include "arraybox.h"
#include "mafword.h"
#include "fsa.h"
#include "maf.h"
#include "relators.h"
#include "lowindex.h"

class Low_Index_Subgroup_Finder
{
  private:
    typedef State_ID Coset_ID;
    enum Scan_Result
    {
      SR_Incomplete,
      SR_Complete,
      SR_Deduction,
      SR_Coincidence
    };

    class Search_Level
    {
      public:
        FSA_Simple * fsa;
        Search_Level *prev;
        Transition_Count undefined_transitions;
        Transition_Count level;
        State_ID undefined_transition_si;
        Ordinal undefined_transition_g;
        State_ID next_si;
        State_ID last_coset;
        bool advanced;
      public:
        Search_Level(Low_Index_Subgroup_Finder &sf,
                     Search_Level *prev_ = 0) :
          prev(prev_),
          fsa(0),
          next_si(1),
          advanced(false)
        {
          if (prev)
          {
            undefined_transition_si = prev->undefined_transition_si;
            undefined_transition_g = prev->undefined_transition_g;
            undefined_transitions = prev->undefined_transitions;
            while (prev->fsa->fast_new_state(undefined_transition_si,
                                             undefined_transition_g))
            {
              if (++undefined_transition_g == sf.nr_generators)
              {
                undefined_transition_g = 0;
                if (++undefined_transition_si > sf.stop_index)
                {
                  MAF_INTERNAL_ERROR(sf.maf.container,("Bad call to "
                                     "Low_Index_Subgroup_Finder::Search_Level::Search_Level)\n"));
                  break;
                }
              }
            }
            last_coset = prev->last_coset;
            level = prev->level+1;
          }
          else
          {
            level = 1;
            undefined_transition_si = 1;
            undefined_transition_g = 0;
            undefined_transitions = sf.nr_generators;
            last_coset = 1;
          }
        }

        ~Search_Level()
        {
          if (fsa)
            delete fsa;
        }

        bool next_try(Low_Index_Subgroup_Finder & sf)
        {
          if (advanced)
            return false;

          while (fsa->fast_new_state(next_si,
                                     sf.maf.inverse(undefined_transition_g))!=0)
          {
            if (++next_si > last_coset)
              break;
          }

          if (next_si > sf.stop_index)
            return false;
          undefined_transitions = prev ? prev->undefined_transitions : sf.nr_generators;
          if (next_si > last_coset)
          {
            advanced = true;
            undefined_transitions += sf.nr_generators;
            last_coset++;
          }
          sf.set_action(undefined_transition_si,undefined_transition_g,
                        next_si++);
          return true;
        }

        bool first_in_class(Low_Index_Subgroup_Finder &sf)
        {
          /* We want only to return one subgroup in each conjugacy class
             of subgroups, and we want to pick that subgroup which has
             the standardised coset table which comes first when coset tables
             of the size are ordered such that which ever has the smaller
             state number at the first place where the tables differ comes
             first.

             Suppose WHw is a conjugate subgroup. Then HwWHw=Hw, so that
             Whw stabilises Hw. Conversely If Hwu=Hw then wuW is in H so
             u is in WHw.

             */

          bool first = true;
          for (Coset_ID si = 2; si <= last_coset; si++)
          {
            State_Count done = 1;
            sf.mu[1] = si;
            sf.nu[si] = 1;
            bool better = true;
            for (Coset_ID new_si = 1 ; new_si <= last_coset &&
                 better ; new_si++)
              for (Ordinal g = 0; g < sf.nr_generators ; g++)
              {
                Coset_ID gamma = fsa->fast_new_state(new_si,g);
                Coset_ID delta = fsa->fast_new_state(sf.mu[new_si],g);
                if (!gamma || !delta)
                {
                  better = false; /* we don't know either way */
                  break;
                }
                if (sf.nu[delta] == 0)
                {
                  done++;
                  sf.nu[delta] = done;
                  sf.mu[done] = delta;
                }
                if (sf.nu[delta] < gamma)
                  first = false;
                else if (sf.nu[delta] > gamma)
                {
                  better = false;
                  break;
                }
              }

            for (Coset_ID si = 1; si <= done ; si++)
              sf.nu[sf.mu[si]] = 0;
            if (!first)
              return false;
          }
          return true;
        }
    };
    friend class Search_Level;
  private:
    Transition_Count branch_nr;
    Element_Count start_index;
    Element_Count stop_index;
    Element_Count index;
    Search_Level * root;
    Search_Level * current;
    Array_Of<Coset_ID> mu;
    Array_Of<Coset_ID> nu;
    Relator_Conjugator rc;
    //consequence management - better known as the "deduction stack"
    Collection_Manager consequence_manager;
    Element_Count log_top;
    Array_Of<Coset_ID> log_entry_ci;
    Array_Of<Ordinal> log_entry_g;
  public:
    const Ordinal nr_generators;
    MAF & maf;
  public:
    Low_Index_Subgroup_Finder(MAF &maf_,Element_Count start_index_,
                              Element_Count stop_index_) :
      start_index(start_index_),
      stop_index(stop_index_),
      nr_generators(maf_.properties().nr_generators),
      maf(maf_),
      rc(maf_)
    {
      mu.set_capacity(stop_index+1);
      nu.set_capacity(stop_index+1);
      unsigned deduction_columns = 0;
      for (Ordinal g = 0 ; g < nr_generators; g++)
        if (g <=  maf.inverse(g))
          deduction_columns++;
      consequence_manager.add(log_entry_ci,false);
      consequence_manager.add(log_entry_g,false);
      consequence_manager.set_capacity(deduction_columns*stop_index);
    }

    const FSA_Simple * first(Word_Collection *subgroup_generators)
    {
      while (current)
      {
        Search_Level * temp = current;
        current = current->prev;
        delete temp;
      }
      subgroup_generators->empty();
      rc.create_relators(*subgroup_generators,0);
      root = 0;
      index = start_index;
      log_top = 0;
      branch_nr = 0;
      return next(subgroup_generators);
    }

    const FSA_Simple * next(Word_Collection *subgroup_generators)
    {
      for (;;)
      {
        if (!root)
          current = root = new Search_Level(*this);

        if (current->fsa)
        {
          delete current->fsa;
          current->fsa = 0;
        }

        bool found = false;
        if (current == root)
        {
          current->fsa = new FSA_Simple(maf.container,maf.alphabet,
                                         stop_index+1,nr_generators);
          current->fsa->set_single_accepting(1);
        }
        else
        {
          FSA_Simple & old_fsa = *current->prev->fsa;
          State_Count old_states = old_fsa.state_count();
          State_Count nr_states = old_states + 1;
          if (nr_states > stop_index)
            nr_states = stop_index+1;

          FSA_Simple & new_fsa = * new FSA_Simple(maf.container,maf.alphabet,
                                                  nr_states,nr_generators);
          new_fsa.set_single_accepting(1);
          memcpy(new_fsa.state_lock(1),
                 old_fsa.dense_transition_table()+nr_generators,
                 sizeof(State_ID)*nr_generators*(old_states-1));
          new_fsa.state_unlock(1);
          current->fsa = &new_fsa;
        }
        found = current->next_try(*this);

        if (found)
        {
          if (seek_consequences() && current->first_in_class(*this))
          {
            if (current->undefined_transitions==0)
            {
              if (current->last_coset >= start_index)
              {
                FSA_Simple * was = current->fsa;
                if (current->last_coset != stop_index)
                {
                  current->fsa = FSA_Factory::trim(*current->fsa);
                  delete was;
                }
                maf.label_coset_table(current->fsa);
                maf.subgroup_generators_from_coset_table(subgroup_generators,*current->fsa);
                branch_nr++;
                return current->fsa;
              }
            }
            else
              current = new Search_Level(*this,current);
          }
          else if (! (short) ++branch_nr)
            maf.container.status(2,1,"Searching for subgroups. "
                                 "Branch " FMT_TC " terminated at level " FMT_TC "\n",
                                 branch_nr,current->level);
        }
        else
        {
          Search_Level * temp = current;
          current = current->prev;
          delete temp;
          if (!current)
          {
            root = 0;
            return false;
          }
        }
      }
    }

  private:
    void set_action(Coset_ID ci,Ordinal g,Coset_ID nci)
    {
      /* set the action of g on coset ci to nci, and record the
        fact that this transition is known in the log */

      Ordinal ig = maf.inverse(g);
      current->fsa->set_transition(ci,g,nci);
      current->fsa->set_transition(nci,ig,ci);
      current->undefined_transitions -= ci != nci || g != ig ? 2 : 1;
      log(ci,g,nci);
    }

    void log(Coset_ID ci,Coset_ID g, Coset_ID nci)
    {
      /* we only record the coset transition for
         for the direction of the transition in which g <= ig.
         Since we are not allowing coincidences and we set the set
         of the deduction stack to the number of transitions that
         ever cause consequence checking we don't need to check for overflow.
      */
      Ordinal ig = maf.inverse(g);
      if (g <= ig)
      {
        log_entry_ci[log_top] = ci;
        log_entry_g[log_top] = g;
      }
      else
      {
        log_entry_ci[log_top] = nci;
        log_entry_g[log_top] = ig;
      }
      log_top++;
    }

    bool seek_consequences()
    {
      while (log_top)
      {
        log_top--;
        if (!scan_relators())
        {
          log_top = 0;
          return false;
        }
      }
      return true;
    }

    bool scan_relators()
    {
      Coset_ID si = log_entry_ci[log_top];
      Ordinal g = log_entry_g[log_top];
      Element_Count start,end;
      rc.get_range(&start,&end,g);
      Fast_Word fw;
      Scan_Result sr;
      for (Element_ID r = start; r < end;r++)
      {
        rc.get_relator(&fw,r);
        sr = scan_relator(si,fw);
        if (sr == SR_Coincidence)
          return false;
      }
      return true;
    }

    Scan_Result scan_relator(Coset_ID ci,const Fast_Word &word)
    {
      const Ordinal * fi = word.buffer;
      const Ordinal * bi = fi + word.length;
      FSA_Simple & fsa = *current->fsa;
      Coset_ID fci = fsa.fast_new_state(ci,*fi++),bci = ci,nci;

      for (; fi < bi; fi++,fci = nci)
      {
        nci = fsa.fast_new_state(fci,*fi);
        if (!nci)
          break;
      }

      if (fi == bi)
      {
        if (fci != ci)
          return SR_Coincidence;
        return SR_Complete;
      }

      for (; bi > fi; bi--,bci = nci)
      {
        nci = fsa.fast_new_state(bci,maf.inverse(bi[-1]));
        if (!nci)
          break;
      }

      if (fi == bi)
        return SR_Coincidence;

      if (fi + 1 == bi)
      {
        set_action(fci,*fi,bci);
        return SR_Deduction;
      }
      return SR_Incomplete;
    }
};

Subgroup_Iterator::Subgroup_Iterator(MAF &maf,Element_Count start_index,
                                     Element_Count stop_index) :
  finder(* new Low_Index_Subgroup_Finder(maf,start_index,stop_index))
{
}

Subgroup_Iterator::~Subgroup_Iterator()
{
  delete & finder;
}

const FSA_Simple * Subgroup_Iterator::first(Word_Collection *subgroup_generators)
{
  return finder.first(subgroup_generators);
}

const FSA_Simple * Subgroup_Iterator::next(Word_Collection *subgroup_generators)
{
  return finder.next(subgroup_generators);
}
