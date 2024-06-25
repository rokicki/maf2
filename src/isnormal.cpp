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


// $Log: isnormal.cpp $
// Revision 1.5  2010/05/11 18:02:16Z  Alun
// Argument parsing changed not to need strcmp(). Help and other messages changed.
// Revision 1.4  2009/10/12 22:08:31Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.3  2009/09/12 19:15:21Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.2  2008/11/03 17:03:58Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.2  2008/11/03 18:03:57Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.1  2008/09/30 06:58:30Z  Alun
// New file.
//

#include <stdlib.h>
#include "maf.h"
#include "container.h"
#include "alphabet.h"
#include "mafword.h"
//#include "maf_dr.h"
#include "maf_so.h"
#include "equation.h"

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * subgroup_suffix = 0;
  String output_name = 0;
  Container * container = MAF::create_container();
  Standard_Options so(*container,SO_STDIN|SO_STDOUT|SO_REDUCTION_METHOD|SO_PROVISIONAL);
  bool bad_usage = false;
  int retcode = 0;
#define cprintf container->error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-cos"))
        i++;
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else
    {
      if (group_filename == 0)
        group_filename = argv[i];
      else if (subgroup_suffix == 0)
        subgroup_suffix = argv[i];
      else
        bad_usage = true;
      i++;
    }
  }

  if (!bad_usage && group_filename)
  {
    MAF * maf = 0;
    maf = MAF::create_from_input(true,group_filename,subgroup_suffix,container);
    if (!maf->load_reduction_method(so.reduction_method))
    {
      cprintf("Unable to reduce words using selected mechanism\n");
      delete maf;
      delete container;
      return 1;
    }

    if (maf->properties().is_normal_coset_system)
    {
      cprintf("isnormal does not work for normal closure coset systems\n");
      delete maf;
      delete container;
      return 1;
    }

    /* Initialise the list of generators for the subgroup.
       We shall read the axioms to do this, as this technique
       will work for any kind of coset system.
       If there are redundant generators we won't necessarily see a g_word
       for these because the axiom may have been simplified to an h equation,
       but this does not matter.*/
    const Presentation_Data & pd = maf->properties();
    Sorted_Word_List generators(maf->group_alphabet());
    Ordinal_Word ow(maf->alphabet);
    for (const Linked_Packed_Equation * axiom = maf->first_axiom();
         axiom;
         axiom = axiom->get_next())
    {
      Simple_Equation se(maf->alphabet,*axiom);
      if (se.lhs_word.value(0) == pd.coset_symbol)
      {
        ow = Subword(se.lhs_word,1);
        Word_Length r = se.rhs_word.length();
        Ordinal g;
        while (r-- > 0 && (g = se.rhs_word.value(r)) < pd.coset_symbol)
          ow.append(maf->inverse(g));
        generators.insert(ow);
      }
    }

    Element_Count nr_sub_generators = generators.count();
    Ordinal_Word test(maf->alphabet);
    bool ok = true;
    String_Buffer sb1,sb2;
    for (Ordinal g = 0;ok && g < pd.coset_symbol;g++)
      for (Element_ID h = 0; ok && h < nr_sub_generators;h++)
      {
        test.set_length(0);
        test.append(pd.coset_symbol);
        test.append(maf->inverse(g));
        test += *generators.word(h);
        generators.word(h)->format(&sb1);
        test.append(g);
        maf->reduce(&test,test);
        test.format(&sb2);
        if (test.value(test.length()-1) != pd.coset_symbol)
        {
          ok = false;
          container->progress(2,"%s*(%s)^%s reduces to %s (not in subgroup)\n",
                              maf->alphabet.glyph(pd.coset_symbol).string(),
                              sb1.get().string(),
                              maf->alphabet.glyph(g).string(),
                              sb2.get().string());
        }
        else
        {
          container->progress(2,"%s*(%s)^%s reduces to %s (in subgroup)\n",
                              maf->alphabet.glyph(pd.coset_symbol).string(),
                              sb1.get().string(),
                              maf->alphabet.glyph(g).string(),
                              sb2.get().string());
        }
      }
    delete maf;
    retcode = ok ? 0 : 1;
    if (ok)
      container->progress(1,"Subgroup is normal\n");
    else
      container->progress(1,"Subgroup is not normal\n");
  }
  else
  {
    cprintf("Usage:\n"
            "isnormal [loglevel] [reduction_method] rwsname [subsuffix]\n"
            "where rwsname is a GASP rewriting system for a group and"
            " rwsname.subsuffix is a\nsubstructure file\n"
            "\"automata\" must already have successfully run against the"
            " relevant coset\n system.\n"
            "The substructure file must not be for a normal closure coset"
            " system.\n");
    so.usage();
    delete container;
    return 1;
  }

  delete container;
  return retcode;
}

