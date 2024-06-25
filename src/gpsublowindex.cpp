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


// $Log: gpsublowindex.cpp $
// Revision 1.5  2011/04/01 10:52:39Z  Alun
// Did not work with named generators unless -simplify option was specified
// Revision 1.4  2010/06/18 00:43:36Z  Alun
// Added -table option and made sure spurious files are deleted.
// Revision 1.3  2010/06/11 06:41:14Z  Alun
// Typo corrected
// Revision 1.2  2010/06/10 17:50:04Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.1  2010/06/08 06:55:42Z  Alun
// New file.

#include <string.h>
#include <stdlib.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_so.h"
#include "mafctype.h"
#include "maf_sub.h"
#include "maf_sdb.h"
#include "lowindex.h"
#include "tietze.h"

const unsigned SLF_NAMED = 1u;
const unsigned SLF_SIMPLIFY = 2u;
const unsigned SLF_RWS = 4u;
const unsigned SLF_PRES = 8u;
const unsigned SLF_TABLE = 16u;

int main(int argc,char ** argv);
  static int inner(MAF & maf,String sub_suffix,
                   Element_Count start_index,Element_Count stop_index,
                   unsigned slf_flags);

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

  options.no_differences = true;
  Element_Count start_index = 2;
  Element_Count stop_index = 0;
  unsigned flags = 0;

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-named"))
      {
        flags |= SLF_NAMED;
        i++;
      }
      else if (arg.is_equal("-pres"))
      {
        flags |= SLF_PRES;
        i++;
      }
      else if (arg.is_equal("-rws"))
      {
        flags |= SLF_RWS;
        i++;
      }
      else if (arg.is_equal("-simplify"))
      {
        flags |= SLF_SIMPLIFY;
        i++;
      }
      else if (arg.is_equal("-table"))
      {
        flags |= SLF_TABLE;
        i++;
      }
      else if (arg.is_equal("-start"))
      {
        so.parse_natural(&start_index,argv[i+1],0,arg);
        i += 2;
      }
      else if (arg.is_equal("-stop"))
      {
        so.parse_natural(&stop_index,argv[i+1],0,arg);
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
  if (group_filename && !bad_usage && start_index >= 2 && stop_index >= start_index)
  {
    MAF & maf = * MAF::create_from_input(false,group_filename,0,&container,
                                         CFI_DEFAULT,&options);
    exit_code = inner(maf,sub_suffix,start_index,stop_index,flags);
  }
  else
  {
    cprintf("Usage:\n"
            "gpsublowindex [loglevel] [-start n] -stop n [-named] [-simplify]"
            " [-rws ] [-pres] [-table] groupname\n[subsuffix]\n"
            "This program finds conjugacy class representatives of the"
            " subgroups which\nhave an index between 2, or the value"
            " specified for -start, and the value\nspecified for -stop, and"
            " generates substructure files for them. The\nsubstructure file"
            " will be for a simple coset system unless the -named option\nis"
            " used. If the -simplify option is used MAF will eliminate as"
            " many of the\nspecified generators as it can before generating"
            " the substructure file.\nIf the -rws option is used a new input"
            " file is generated, which presents the\nspecified subgroup. The"
            " -pres option is similar but generates GAP source code.\n"
            "The new substructure files will be output to"
            " groupname.sublow_in1_n2, where\nn1 is the index of the subgroup"
            " and n2 is a sequence number. If a subsuffix\nis specified the"
            " files will be output to groupname.subsuffix_in1_n2 instead.\n"
            " The coset table of the new subgroup is saved if the -table"
            " option is used\n");
    so.usage();
    exit_code = 1;
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,String sub_suffix,
                 Element_Count start_index,Element_Count stop_index,
                 unsigned flags)
{
  int exit_code = 0;
  Container & container = maf.container;
  String_Buffer new_sub_suffix;

  Subgroup_Iterator subgroup_iterator(maf,start_index,stop_index);
  const FSA_Simple * coset_table;
  Word_List sub_gens(maf.alphabet);
  Word_List new_sub_gens(maf.alphabet);
  Ordinal_Word ow(maf.alphabet);
  Element_Count sequence = 0;
  if (!sub_suffix)
    sub_suffix = "sublow";

  for (coset_table = subgroup_iterator.first(&sub_gens);
       coset_table != 0;
       coset_table = subgroup_iterator.next(&sub_gens))
  {
    Subalgebra_Descriptor sub(maf,false);
    Element_Count sub_gen_count = sub_gens.count();
    container.status(2,1,"Subgroup of index " FMT_ID " found.\n",
                     coset_table->state_count()-1);
    new_sub_gens.empty();
    for (Element_ID h = 0; h < sub_gen_count;h++)
    {
      sub_gens.get(&ow,h);
      if (!new_sub_gens.contains(ow))
      {
        sub.add_generator(ow);
        new_sub_gens.add(ow);
        if (flags & SLF_NAMED)
        {
          maf.invert(&ow,ow);
          if (!new_sub_gens.contains(ow))
            sub.add_generator(ow);
        }
      }
    }
    String_Buffer new_suffix;
    new_suffix.format("%s_i" FMT_ID "_" FMT_ID, sub_suffix.string(),
                      coset_table->state_count()-1,++sequence);
    String_Buffer cosname;
    bool unused;
    Letter * cos_suffix = sub.coset_system_suffix(&unused,new_suffix.get());
    cosname.make_filename("",maf.properties().filename,cos_suffix);
    delete cos_suffix;
    if (flags & SLF_SIMPLIFY)
    {
      sub.save_as(maf.properties().filename,new_suffix.get());
      MAF & coset_system = * sub.create_coset_system(false,false);
      coset_system.options.detect_finite_index = 1;
      coset_system.options.assume_confluent = true;
      coset_system.options.no_differences = false;
      FSA_Buffer buffer;
      coset_system.grow_automata(&buffer,GA_SUBPRES,GA_GM);
      FSA * migm = (FSA *) buffer.gm;
      buffer.gm = 0;

      String_Buffer sb;
      sb.make_filename("",sub.get_filename(),".rws");
      MAF & old_subgroup = * MAF::create_from_rws(sb.get(),&container,
                                                  CFI_DEFAULT,&maf.options);
      Tietze_Transformation_Helper tth(old_subgroup,1,0,0,0,0);
      tth.polish();
      MAF & subgroup = *tth.polished_presentation(AA_DEDUCE_INVERSE,0);
      delete &old_subgroup;
      container.delete_file(sb.get()); // we should not have saved the presentation yet */

      Ordinal nr_sub_generators = subgroup.alphabet.letter_count();
      Ordinal_Word label_word(coset_system.alphabet);
      Ordinal g;
      Subalgebra_Descriptor sub(maf,false);

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
      if (flags & SLF_NAMED)
      {
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
          subgroup.rename_generator(g,sb.get());
        }
        sub.save_as(maf.properties().filename,new_suffix.get());
      }
      delete migm;
      if (flags & SLF_TABLE)
      {
        coset_system.save(cosname.get());
        coset_system.save_fsa((FSA *) coset_table,GAT_Coset_Table);
        container.delete_file(cosname.get());
      }
      delete &coset_system;

      if (flags & SLF_RWS)
        subgroup.save_as(sub.get_filename(),".rws");
      String filename = sb.make_filename("",sub.get_filename(),".pres");
      if (flags & SLF_PRES)
      {
        Output_Stream * os = container.open_text_output_file(filename);
        subgroup.output_gap_presentation(os,false);
        container.close_output_file(os);
      }
      else
        container.delete_file(filename);
      delete &subgroup;
    }
    else
    {
      if (flags & SLF_NAMED)
      {
        Ordinal coset_symbol = maf.generator_count();
        Ordinal nr_sub_generators = Ordinal(new_sub_gens.count());
        bool use_single_letters = nr_sub_generators + coset_symbol <= 52;
        String_DB gens(nr_sub_generators + coset_symbol);
        static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for (Ordinal g = 0; g < coset_symbol;g++)
        {
          Glyph glyph = maf.alphabet.glyph(g);
          gens.insert(glyph,glyph.length());
        }
        new_sub_gens.empty();
        String_Buffer sb;
        Ordinal nh = 0;
        int i = 0;
        sub.create_alphabet();
        for (Element_ID h = 0; h < sub_gen_count;h++)
        {
          sub_gens.get(&ow,h);
          if (!new_sub_gens.contains(ow))
          {
            String s;
            do
            {
              if (use_single_letters)
                s = sb.format("%c",letters[i++]);
              else
                s = sb.format("h%d",i++);
            }
            while (gens.find(s,s.length())!=0);
            sub.add_generator_name(s);
            new_sub_gens.add(ow);
            maf.invert(&ow,ow);
            if (!new_sub_gens.contains(ow))
            {
              sb.reserve(0,0,true)[0] = char_classification.to_upper_case(*sb.get());
              s = sb.get();
              while (gens.find(s,s.length())!=0)
              {
                if (use_single_letters)
                  s = sb.format("%c",letters[i++]);
                else
                  s = sb.format("h%d",i++);
              }
              sub.add_generator_name(s);
              sub.set_inverse(nh,nh+1);
              nh += 2;
            }
            else
            {
              sub.set_inverse(nh,nh);
              nh++;
            }
          }
        }
      }
      sub.save_as(maf.properties().filename,new_suffix.get());
      if (flags & SLF_TABLE)
      {
        MAF & coset_system = * sub.create_coset_system(false,false);
        coset_system.save(cosname.get());
        coset_system.save_fsa((FSA *) coset_table,GAT_Coset_Table);
        container.delete_file(cosname.get());
        delete & coset_system;
      }
      if (flags & SLF_RWS+SLF_PRES)
      {
        MAF * new_maf = maf.rs_presentation(*coset_table);
        if (flags & SLF_RWS)
          new_maf->save_as(sub.get_filename(),".rws");
        if (flags & SLF_PRES)
        {
          String_Buffer sb;
          String filename = sb.make_filename("",sub.get_filename(),".pres");
          Output_Stream * os = container.open_text_output_file(filename);
          new_maf->output_gap_presentation(os,false);
          container.close_output_file(os);
        }
        delete new_maf;
      }
    }
  }
  delete &maf;
  return exit_code;
}

