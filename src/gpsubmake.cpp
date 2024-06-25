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


// $Log: gpsubmake.cpp $
// Revision 1.5  2010/07/02 13:04:50Z  Alun
// Filename was wrong for sub_power case
// Revision 1.4  2010/06/11 07:31:55Z  Alun
// Typo corrected
// Revision 1.3  2010/06/10 16:57:31Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.2  2010/06/10 13:57:19Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/18 09:19:02Z  Alun
// New file.
// Revision 1.4  2009/09/12 19:46:43Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.3  2009/03/03 14:28:04Z  Alun
// Typo corrected
// Revision 1.2  2008/11/02 21:45:54Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.1  2008/07/29 22:42:08Z  Alun
// New file.
//

#include <string.h>
#include <stdlib.h>
#include "maf.h"
#include "container.h"
#include "maf_so.h"
#include "mafctype.h"
#include "maf_sub.h"
#include "maf_sdb.h"
#include "fsa.h"

/**/

enum Substructure_Action
{
  SA_None,
  SA_Power_Subgroup,
  SA_Derived_Subgroup,
  SA_Named_Generators_Subgroup
};

int main(int argc,char ** argv);
  static int inner(MAF & maf,String sub_suffix,Substructure_Action action,int n);

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * sub_suffix = 0;
  Container & container = *MAF::create_container();
  Standard_Options so(container,0);
  bool bad_usage = false;
#define cprintf container.error_output
  MAF::Options options;
  Substructure_Action action(SA_None);

  options.no_differences = true;
  Word_Length power = 2;

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-derived"))
      {
        action = SA_Derived_Subgroup;
        i++;
      }
      else if (arg.is_equal("-named"))
      {
        action = SA_Named_Generators_Subgroup;
        i++;
      }
      else if (arg.is_equal("-power"))
      {
        action = SA_Power_Subgroup;
        so.parse_natural(&power,argv[i+1],0,arg);
        i += 2;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else if (group_filename == 0)
    {
      group_filename = argv[i];
      i++;
    }
    else if (sub_suffix == 0)
    {
      sub_suffix = argv[i];
      i++;
    }
    else
      bad_usage = true;
  }
  int exit_code;
  if (group_filename && !bad_usage && action != SA_None)
  {
    MAF & maf = * MAF::create_from_input(false,group_filename,0,&container,
                                         CFI_DEFAULT,&options);
    exit_code = inner(maf,sub_suffix,action,power);
  }
  else
  {
    cprintf("Usage:\n"
            "gpsubmake [loglevel] -derived | -named | -power n groupname [subsuffix]\n"
            "where groupname is a GASP rewriting system for a group.\n"
            "If -derived is specified a substructure file for the derived"
            " subgroup of\ngroupname is created. The substructure file is"
            " output to groupname.subsuffix\nor groupname.sub_derived if no"
            " subsuffix is specified on the command line.\n"
            "If -named is specified then groupname.subsuffix should be a"
            " substructure file\nfor a simple coset system whose automatic"
            " structure has previously been\ncomputed, and for which the"
            " subgroup presentation has been computed. A new\nsubstructure"
            " file, using named subgroup generators, will be computed where"
            " the\ngenerators are taken from groupname.subsuffix.rws.simplify"
            " if it exists, or\ngroupname.subsuffix.rws otherwise. The new"
            " substructure file is output to\ngroupname.subsuffix_named.\n"
            "If -power is specified then n>=2 should be a small integer, and"
            " groupname.wa\nmust have been previously computed. gpsubmake"
            " attempts to compute generators\nfor the g^n subgroup. The new"
            " substructure file is output to\ngroupname.subsuffix or to"
            " groupname.sub_power_n if no subsuffix is specified.\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,String sub_suffix,Substructure_Action action,int n)
{
  int exit_code = 0;
  Subalgebra_Descriptor sub(maf,action == SA_Derived_Subgroup);
  Container & container = maf.container;
  String_Buffer new_sub_suffix;

  if (action == SA_Power_Subgroup)
  {
    MAF * coset_system = sub.create_coset_system(false,false);
    coset_system->realise_rm();

    if (!sub_suffix)
    {
      new_sub_suffix.format("sub_power_%d",n);
      sub_suffix = new_sub_suffix.get();
    }
    const FSA * wa = maf.load_fsas(GA_WA);
    if (!wa)
    {
      container.error_output("Unable to load required word acceptor\n");
      delete coset_system;
      delete & maf;
      return 1;
    }
    int time_out = 0;
    FSA::Word_Iterator wi(*wa);
    Ordinal_Word lhs(coset_system->alphabet);
    Ordinal_Word rhs(coset_system->alphabet);
    rhs.append(coset_system->properties().coset_symbol);
    int max_length = 24;
    String_Buffer sb;

    for (Word_Length stop_length = 1; stop_length < max_length ;stop_length++)
      for (State_ID si = wi.first();
           si;si = wi.next(wi.word.length() < stop_length))
        if (wa->is_accepting(si) && wi.word.length() == stop_length)
        {
          if (wi.word.value(0) <= maf.inverse(wi.word.value(stop_length-1)))
          {
            if (container.status_needed(1))
            {
              wi.word.format(&sb);
              container.status(1,1,"Checking word %s. Generators now " FMT_ID
                                   ".Timeout %d\n",
                               sb.get().string(),sub.generator_count(),
                               20-time_out++);
            }

            lhs = rhs;
            for (int i = 0; i < n;i++)
              lhs += wi.word;
            if (coset_system->add_axiom(lhs,rhs,AA_ADD_TO_RM|AA_DEDUCE|AA_POLISH))
            {
              sub.add_generator(Subword(lhs,1));
              time_out = 0;
            }
            if (time_out > 20)
              break;
          }
        }
    delete coset_system;
  }

  if (action == SA_Derived_Subgroup)
  {
    if (!sub_suffix)
      sub_suffix = "sub_derived";
    Ordinal_Word ow(maf.alphabet);
    Ordinal nr_generators = maf.alphabet.letter_count();
    String_Buffer sb;

    for (Ordinal g0 = 0 ; g0 < nr_generators;g0++)
      for (Ordinal g1 = g0+1;g1 < nr_generators;g1++)
        if (maf.inverse(g0) >= g0 && maf.inverse(g1) >= g1)
        {
          ow.set_length(0);
          ow.append(maf.inverse(g0));
          ow.append(maf.inverse(g1));
          ow.append(g0);
          ow.append(g1);
          sub.add_generator(ow);
        }

  }

  if (action == SA_Named_Generators_Subgroup)
  {
    MAF & coset_system = * MAF::create_from_input(true,
                                                  maf.properties().filename,
                                                  sub_suffix,&container,
                                                  CFI_DEFAULT,
                                                  &maf.options);
    if (coset_system.properties().coset_symbol+1 !=
        coset_system.properties().nr_generators)
    {
      container.error_output("The substructure file already has named"
                             " subgroup generators.\n");
      delete &coset_system;
      delete &maf;
      return 1;
    }
    const FSA * migm = coset_system.load_fsas(GA_GM);
    if (!migm)
    {
      container.error_output("Unable to load general multiplier for coset system\n");
      delete &coset_system;
      delete &maf;
      return 1;
    }
    String_Buffer sb;
    sb.make_filename("",coset_system.properties().subgroup_filename,".rws.simplify");
    Input_Stream * is = container.open_input_file(sb.get(),0);
    if (!is)
    {
      sb.make_filename("",coset_system.properties().subgroup_filename,".rws");
      is = container.open_input_file(sb.get(),0);
    }
    if (!is)
    {
      container.error_output("Subgroup presentation has not been computed\n");
      delete &coset_system;
      delete &maf;
      return 1;
    }
    container.close_input_file(is);
    MAF & subgroup = * MAF::create_from_rws(sb.get(),&container,
                                            CFI_DEFAULT,&maf.options);
    Ordinal nr_sub_generators = subgroup.alphabet.letter_count();
    Ordinal_Word label_word(coset_system.alphabet);
    Ordinal g;
    for (g = 0; g < nr_sub_generators;g++)
    {
      Glyph name = subgroup.alphabet.glyph(g);
      bool valid = name.length() >2 && name[0] == '_' && name[1] == 'g';
      Ordinal v = 0;
      if (valid)
      {
        const Letter * s = name + 2;
        while (is_digit(*s))
          v = v*10 + *s++ - '0';
        valid = *s == 0;
      }
      if (!valid)
      {
        container.error_output("Subgroup generator %s does not have"
                               " expected form _gn\n",name.string());
        delete &subgroup;
        delete &coset_system;
        delete &maf;
        return 1;
      }
      v -= coset_system.properties().coset_symbol;
      migm->label_word(&label_word,v);
      sub.add_generator(label_word);
    }
    sub.create_alphabet();
    Ordinal i = 0;
    Ordinal coset_symbol =  coset_system.properties().coset_symbol;
    bool use_single_letters = nr_sub_generators + coset_symbol <= 52;
    String_DB gens(nr_sub_generators + coset_symbol+1);
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (g = 0; g <= coset_symbol;g++)
    {
      Glyph glyph = maf.alphabet.glyph(g);
      gens.insert(glyph,glyph.length());
    }
    for (g = 0; g < nr_sub_generators;g++)
    {
      if (subgroup.inverse(g) >= g)
      {
        do
        {
          if (use_single_letters)
            sb.format("%c",letters[i++]);
          else
            sb.format("h%d",i++);
        }
        while (gens.find(sb.get(),sb.get().length())!=0);
      }
      else
      {
        sb.set(sub.sub_alphabet().glyph(subgroup.inverse(g)));
        sb.reserve(0,0,true)[0] = char_classification.to_upper_case(*sb.get());
        while (gens.find(sb.get(),sb.get().length())!=0)
        {
          if (use_single_letters)
            sb.format("%c",letters[i++]);
          else
            sb.format("h%d",i++);
        }
      }
      sub.add_generator_name(sb.get());
      gens.insert(sb.get(),sb.get().length());
      sub.set_inverse(g,subgroup.inverse(g));
    }
    new_sub_suffix.set(sub_suffix ? sub_suffix : "sub");
    new_sub_suffix.append("_named");
    sub_suffix = new_sub_suffix.get();
    delete &coset_system;
    delete &subgroup;
  }

  sub.save_as(maf.properties().filename,sub_suffix);
  delete &maf;
  return exit_code;
}

