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


// $Log: gpmult.cpp $
// Revision 1.7  2011/06/14 00:13:22Z  Alun
// No longer automatically adds inverse words to subgroup generators when sub
// option is selected
// Revision 1.6  2010/06/10 13:57:16Z  Alun
// All tabs removed again
// Revision 1.5  2010/05/16 23:22:55Z  Alun
// Help changed. Arguments of General_Multiplier::composite() have changed.
// With coset systems either determinised or MIDFA multipliers can be
// constructed
// Revision 1.4  2009/09/12 19:16:16Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2008/11/02 20:18:56Z  Alun
// Re-check in of source from web site after catastrophic loss of correct state
// of RCS archive.
// Revision 1.4  2008/11/02 21:18:55Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2008/10/02 07:41:15Z  Alun
// Leak fixed
// Revision 1.2  2007/10/24 21:15:32Z  Alun
//

#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_sub.h"
#include "maf_so.h"
#include "equation.h"

/**/

int main(int argc,char ** argv);
  static int inner(MAF & maf,String one_word,bool two,bool sub, bool pres,
                   bool midfa,String subgroup_suffix,unsigned fsa_format_flags);
    static void do_multiplier(MAF & maf,const Word_List & wl, bool midfa,
                              String output_filename,
                              unsigned fsa_format_flags);

int main(int argc,char ** argv)
{
  int i = 1;
  bool two = false;
  bool cosets = false;
  bool sub = false;
  bool pres = false;
  bool migm = false;
  char * group_filename = 0;
  char * sub_suffix = 0;
  String one_word = 0;
  int chosen = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,SO_FSA_FORMAT|SO_FSA_KBMAG_COMPATIBILITY);
  bool bad_usage = false;
#define cprintf container.error_output

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
      else if (arg.is_equal("-migm"))
      {
        migm = true;
        i++;
      }
      else if (arg.is_equal("-pres"))
      {
        chosen++;
        pres = true;
        i++;
      }
      else if (arg.is_equal("-sub"))
      {
        chosen++;
        sub = true;
        i++;
      }
      else if (arg.is_equal("-word"))
      {
        chosen++;
        one_word = argv[i+1];
        if (!one_word)
          bad_usage = true;
        i += 2;
      }
      else if (arg.is_equal("-2"))
      {
        chosen++;
        two = true;
        i++;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (group_filename == 0)
    {
      group_filename = argv[i];
      i++;
    }
    else if ((cosets || sub) && sub_suffix == 0)
    {
      sub_suffix = argv[i];
      i++;
    }
    else
      bad_usage = true;
  }
  int exit_code;
  if (group_filename && chosen <= 1 && !bad_usage)
  {
    MAF & maf = * MAF::create_from_input(cosets,group_filename,sub_suffix,
                                         &container,0);
    exit_code = inner(maf,one_word,two,sub,pres,migm,sub_suffix,so.fsa_format_flags);
    delete &maf;
  }
  else
  {
    cprintf("Usage:\n"
            "gpmult [loglevel] [format] [-cos] [-migm] [-pres | -sub | -2 | -word"
            " expression]\ngroupname [subsuffix]\n"
            "where groupname is a GASP rewriting system for a group, and, if"
            " the -cos or\n-sub options are used, groupname.subsuffix is a"
            " substructure file.\n"
            "An automatic structure for the relevant rewriting system must"
            " previously have\nbeen computed.\n"
            "This program can do several different things:\n"
            "With -2 it computes the general multiplier for words of length 2.\n"
            "With -sub it generates the multiplier for the sub-generators.\n"
            "With -pres it computes the composite multiplier for the relators\n"
            "With -word expression it computes the multiplier for the specified"
            " word\nOtherwise it computes the individual multipliers for"
            " each generator.\n"
            "For coset systems the -migm option causes gpmult to perform"
            " computations using\nthe MIDFA multiplier. The default is to use"
            " the determinised multiplier.\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,String one_word,bool two,bool sub,bool pres,bool midfa,
                 String subgroup_suffix,unsigned fsa_format_flags)
{
  String_Buffer sb;
  Word_List wl(maf.group_alphabet());
  Ordinal nr_generators = maf.group_alphabet().letter_count();
  Ordinal_Word word(maf.group_alphabet(),2);
  String filename = maf.properties().filename;
  bool is_coset_system = maf.properties().is_coset_system;
  if (!is_coset_system)
    midfa = false;

  if (!maf.load_fsas(midfa ? GA_GM : GA_DGM))
  {
    maf.container.error_output("Could not load general multiplier for %s!\n",
                               filename.string());
    return 1;
  }

  String output_filename;

  if (one_word)
  {
    Ordinal_Word * ow = maf.parse(one_word);
    wl.add(*ow);
    maf.multiplier_name(&sb,*ow,midfa);
    delete ow;
    do_multiplier(maf,wl,midfa,sb.get(),fsa_format_flags);
  }
  else if (pres)
  {
    const Linked_Packed_Equation *axiom;
    word.set_length(0);
    wl.add(word);

    for (axiom = maf.first_axiom();axiom;axiom = axiom->get_next())
    {
      Simple_Equation se(maf.alphabet,*axiom);
      if (se.lhs_word.value(0) < nr_generators)
      {
        word = se.lhs_word;
        maf.invert(&word,word);
        word += se.rhs_word;
        wl.add(word);
      }
    }
    do_multiplier(maf,wl,midfa,midfa ? ".migmr" : ".gmr",fsa_format_flags);
  }
  else if (sub)
  {
    if (!subgroup_suffix)
      subgroup_suffix = ".sub";
    String subgroup_filename =
      String::make_filename(&sb,"",is_coset_system ?
                            maf.properties().original_filename : filename,
                            subgroup_suffix);
    Subalgebra_Descriptor &sub =
      *Subalgebra_Descriptor::create(subgroup_filename,maf);
    if (sub.is_normal_closure())
    {
      maf.container.error_output("Cannot generate multipliers from normal closure substructure file %s\n",
                                 subgroup_filename.string());
      delete &sub;
      return 1;
    }
    word.set_length(0);
    wl.add(word);
    for (Element_ID sg = 0; sg < sub.generator_count();sg++)
    {
      word = sub.sub_generator(sg);
      wl.add(word);
    }
    String_Buffer sb2;
    String multiplier_suffix = String::make_filename(&sb2,"",subgroup_suffix,".gmg");
    do_multiplier(maf,wl,midfa,multiplier_suffix,fsa_format_flags);
    delete &sub;
  }
  else if (two)
  {
    for (Ordinal g1 = PADDING_SYMBOL;g1 < nr_generators;g1++)
    {
      Word_Length i = 0;
      if (g1 != PADDING_SYMBOL)
        word.set_code(i++,g1);
      for (Ordinal g2 = PADDING_SYMBOL;g2 < nr_generators;g2++)
      {
        Word_Length j = i;
        if (g2 != PADDING_SYMBOL)
          word.set_code(j++,g2);
        word.set_length(j);
        wl.add(word);
      }
    }
    word.set_length(0);
    wl.add(word);
    do_multiplier(maf,wl,midfa,midfa ? ".migm2" : ".gm2",fsa_format_flags);
  }
  else
  {
    for (Ordinal g1 = PADDING_SYMBOL;g1 < nr_generators;g1++)
    {
      Word_Length i = 0;
      wl.empty();
      if (g1 != PADDING_SYMBOL)
        word.set_code(i++,g1);
      word.set_length(i);
      wl.add(word);
      String_Buffer sb2;
      do_multiplier(maf,wl,midfa,maf.multiplier_name(&sb2,word,midfa),fsa_format_flags);
    }
  }
  return 0;
}

/**/

static void do_multiplier(MAF & maf,const Word_List & wl,bool midfa,
                          String suffix,unsigned fsa_format_flags)
{

  FSA_Simple * fsa = midfa ? maf.fsas.gm->composite(maf,wl) :
                             maf.fsas.dgm->composite(maf,wl);
  maf.save_fsa(fsa,suffix,fsa_format_flags);
  delete fsa;
}
