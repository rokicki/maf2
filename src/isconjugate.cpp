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


// $Log: isconjugate.cpp $
// Revision 1.5  2010/05/11 18:11:36Z  Alun
// Argument parsing changed not to need strcmp(). Help changed.
// Revision 1.4  2009/10/12 22:08:44Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.3  2009/09/12 19:48:22Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2009/08/24 10:40:20Z  Alun
// Answer for conjugating element was wrong. Still probably needs more testing.
// removed irrelevant stuff from usage() message
// Revision 1.1  2008/12/07 16:40:18Z  Alun
// New file.
// Revision 1.5  2008/11/02 18:57:14Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.4  2008/10/12 19:10:35Z  Alun
// -steps was previously only implemented when reducing a list of words
// Revision 1.3  2007/11/15 22:58:11Z  Alun
//

#include <stdlib.h>
#include "maf.h"
#include "fsa.h"
#include "container.h"
#include "alphabet.h"
#include "mafword.h"
#include "maf_so.h"
#include "maf_wdb.h"

class Conjugate_DB : public Word_DB
{
  private:
    MAF & maf;
    Array_Of<Ordinal> defining_generator;
    Array_Of<unsigned> previous_conjugate;
    Ordinal nr_generators;
    Ordinal_Word test;
    Ordinal_Word work;
  public:
    Conjugate_DB(MAF &maf_,const Word & initial_word) :
      maf(maf_),
      Word_DB(initial_word.alphabet(),0),
      nr_generators(initial_word.alphabet().letter_count()),
      test(initial_word),
      work(initial_word)
    {
      manage(defining_generator);
      manage(previous_conjugate);
      add(initial_word);
      defining_generator[0] = IdWord;
      previous_conjugate[0] = 0;
    }
    int conjugate(Element_ID word_nr,Element_ID * join=0,Ordinal * join_g=0,const Conjugate_DB * other_db=0)
    {
      if (word_nr >= count())
        return -1;
      for (Ordinal g = 0; g < nr_generators;g++)
      {
        test.set_length(0);
        test.append(maf.inverse(g));
        get(&work,word_nr);
        test = test + work;
        test.append(g);
        maf.reduce(&test,test);
        Element_ID new_word;
        if (insert(test,(Element_ID *) &new_word))
        {
          defining_generator[new_word] = g;
          previous_conjugate[new_word] = word_nr;
          if (other_db && other_db->find(test,join))
          {
            *join_g = g;
            return 1;
          }
        }
      }
      return 0;
    }
    void get_conjugator(Word * word,Element_ID word_nr)
    {
      word->set_length(0);
      while (word_nr)
      {
        word->append(maf.inverse(defining_generator[word_nr]));
        word_nr = previous_conjugate[word_nr];
      }
      maf.invert(word,*word);
    }
};

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * subgroup_suffix = 0;
  char * word1 = 0;
  char * word2 = 0;
  bool cosets = false;
  Container * container = MAF::create_container();
  Standard_Options so(*container,/*SO_STDIN|SO_STDOUT|*/
                                 SO_REDUCTION_METHOD|SO_PROVISIONAL);
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
      else if (!word1)
        word1 = argv[i];
      else if (!word2)
        word2 = argv[i];
      else
        bad_usage = true;
      i++;
    }
  }

  if (!bad_usage && group_filename && word1)
  {
    MAF * maf = 0;
    maf = MAF::create_from_input(cosets,group_filename,subgroup_suffix,
                                 container);
    if (!maf->load_reduction_method(so.reduction_method))
    {
      cprintf("Unable to reduce words using selected mechanism\n");
      delete maf;
      delete container;
      return 1;
    }

    Ordinal_Word * ow1 = maf->parse(word1);

    if (word2)
    {
      Ordinal_Word * ow2 = maf->parse(word2);
      Conjugate_DB db1(*maf,*ow1);
      Conjugate_DB db2(*maf,*ow2);

      Element_ID w1 = 0;
      Element_ID w2 = 0;
      Element_ID join = 0;
      Ordinal join_g = 0;
      int answer;
      if (*ow1 != *ow2)
      {
        for (;;w1++,w2++)
        {
          if (!(char) w1)
            container->status(2,1,"Checking conjugacy " FMT_ID " " FMT_ID "/"
                                  FMT_ID " " FMT_ID "\n",
                              w1,w2,db1.count(),db2.count());
          answer = db1.conjugate(w1,&join,&join_g,&db2);
          if (answer != 0)
            break;
          answer = db2.conjugate(w2,&join,&join_g,&db1);
          if (answer != 0)
          {
            if (answer == 1)
              answer = 2;
            break;
          }
        }
      }
      else
      {
        answer = 1;
        join_g = IdWord;
        join = 0;
      }

      if (answer > 0)
      {
        Ordinal_Word answer_word(maf->group_alphabet());
        Ordinal_Word temp(maf->group_alphabet());
        if (answer == 1)
          w2 = join;
        else
          w1 = join;
        db1.get_conjugator(&answer_word,w1);
        if (join_g != IdWord)
        {
          if (answer == 1)
            answer_word.append(join_g);
          else
            answer_word.append(maf->inverse(join_g));
        }
        db2.get_conjugator(&temp,w2);
        maf->invert(&temp,temp);
        answer_word = answer_word + temp;
        maf->reduce(&answer_word,answer_word);
        String_Buffer sb;
        container->output(container->get_stdout_stream(),"%s^%s=%s",
                          word1,answer_word.format(&sb).string(),word2);
      }

      delete ow1;
      delete ow2;
      delete maf;
      delete container;
      return answer > 0 ? 0 : 2;
    }
    else
    {
      Conjugate_DB db1(*maf,*ow1);
      Ordinal_Word ow(maf->group_alphabet());

      Element_ID w1 = 0;
      bool started = false;
      Output_Stream * os = container->get_stdout_stream();
      container->output(os,"conjugates :=\n[\n");
      String_Buffer sb;
      for (;w1 < db1.count();w1++)
      {
        if (started)
          container->output(os,",\n");
        db1.get(&ow,w1);
        container->output(os,"  %s",ow.format(&sb).string());
        started = true;
        db1.conjugate(w1);
      }
      if (started)
        container->output(os,"\n");
      container->output(os,"];\n");

      delete ow1;
      delete maf;
      delete container;
      return 0;
    }
  }
  else
  {
    cprintf("Usage:\n"
            "isconjugate [loglevel] [reduction_method] groupname [-cos [subsuffix]]"
            " word1 [word2]\n"
            "where groupname is a GASP rewriting system for a group and, if"
            " the -cos option\nis used, rwsname.subsuffix is a substructure"
            " file desribing a subgroup.\n"
            "\"automata\" must already have been run against the relevant"
            " rewriting-system\n"
            "If two input words are given then isconjugate enumerates all"
            " group elements\nuntil it finds an element u such that word1^u="
            "u^-1*word1*u = word2. The exit\ncode is 0 if a conjugating"
            " element is found, or 2 if the group is finite and\nthere is no"
            " such element, or the group is infinite but either word1 or"
            " word2\nhave only finitely many conjugates and are not"
            " conjugate.\n"
            "If one input word is given the isconjugate enumerates all"
            " conjugate words.\n");
    so.usage();
    delete container;
    return 1;
  }
}
