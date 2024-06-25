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


// $Log: maf_sub.cpp $
// Revision 1.15  2010/07/08 15:35:51Z  Alun_Williams
// printf() style argument corrections
// Revision 1.14  2010/06/17 11:14:15Z  Alun
// Changes to handling of filename fields for coset systems and substructure files
// needed by gpsublowindex
// Revision 1.13  2010/06/10 13:57:42Z  Alun
// All tabs removed again
// Revision 1.12  2010/05/08 18:08:17Z  Alun
// Lots of methods added for gpsubmake and gpsublowindex
// No longer needs to recreate coset system after generating it
// Revision 1.11  2009/09/16 07:39:46Z  Alun
// Additional source code changes needed to get clean compilation on latest GNU
//
// Revision 1.10  2009/09/13 20:26:46Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.9  2008/12/24 17:23:26Z  Alun
// Support for normal subgroups added
// Revision 1.8  2008/11/02 18:57:14Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.7  2008/09/25 14:16:21Z  Alun
// Final version built using Indexer.
// Revision 1.5  2007/12/20 23:25:42Z  Alun
//

#include "maf_sub.h"
#include "equation.h"
#include "container.h"

void Subalgebra_Descriptor::create_alphabet()
{
  if (alphabet)
    alphabet->detach();
  alphabet = Alphabet::create(AT_String,maf.container);
  alphabet->set_nr_letters(generators.count());
  alphabet->attach();
}

bool Subalgebra_Descriptor::add_generator_name(Glyph g)
{
  return alphabet->set_next_letter(g);
}

void Subalgebra_Descriptor::set_inverse(Ordinal g,Ordinal ig)
{
  Ordinal nr_sub_generators = generators.count();
  if (!inverses)
  {
    inverses = new Ordinal[nr_sub_generators];
    for (Ordinal i = 0; i < nr_sub_generators;i++)
      inverses[i] = INVALID_SYMBOL;
  }
  if (g >= 0 && g < nr_sub_generators && ig >= 0 && ig < nr_sub_generators)
  {
    inverses[g] = ig;
    inverses[ig] = g;
  }
  else
    MAF_INTERNAL_ERROR(maf.container,("Bad generator pair %d %d passed to"
                                    " Subalgebra_Descriptor::set_inverse()\n",
                                      g,ig));
}

MAF * Subalgebra_Descriptor::create_coset_system(bool name_generators,
                                                 bool no_inverses)
{
  if (maf.properties().is_coset_system)
  {
    maf.container.error_output("MAF does not support nested coset systems\n");
    return 0;
  }
  MAF &coset_system = *MAF::create(&maf.container,&maf.options);
  Ordinal original_nr_letters = maf.generator_count();
  Element_Count nr_letters = original_nr_letters;
  Element_Count nr_sub_letters = 0;
  Ordinal missing_inverse = 0;
  bool ok = true;

  nr_letters += 1; // for _H
  if (!alphabet)
  {
    if (name_generators)
      maf.container.input_error("subGenratorNames field missing from "
                                "substructure file\n");
    name_generators = false;
  }

  if (name_generators)
  {
    nr_letters += nr_sub_letters = generator_count(); // for the named subgroup generators

    if (!no_inverses)
    {
      missing_inverse = Ordinal(nr_letters); /* we won't use this value if the cast truncates */
      for (Element_ID h = 0; h < nr_sub_letters;h++)
        if (!inverses || inverses[h] == INVALID_SYMBOL)
          nr_letters++;
    }
  }
  Alphabet & a_new = coset_system.real_alphabet;
  const Alphabet & a_start = maf.alphabet;
  if (!coset_system.set_nr_generators(nr_letters))
  {
    maf.container.error_output("Total number of generators and sub generators is too large\n");
    ok = false;
  }
  Ordinal g,h;
  for (g = 0; g < original_nr_letters;g++)
    if (!coset_system.set_next_generator(a_start.glyph(g)))
      ok = false;
  if (!coset_system.set_next_generator(is_normal ? "_N" : "_H"))
    ok = false;
  if (name_generators && ok)
  {
    String_Buffer sb;
    for (h = 0;h < nr_sub_letters;h++)
      if (!coset_system.set_next_generator(alphabet->glyph(h)))
        ok = false;
    if (!no_inverses)
      for (h = 0; h < nr_sub_letters;h++)
        if (!inverses || inverses[h] == INVALID_SYMBOL)
        {
          sb.set(alphabet->glyph(h));
          sb.append("^-1");
          if (!coset_system.set_next_generator(sb.get()))
            ok = false;
        }
  }

  if (!ok)
  {
    delete &coset_system;
    return 0;
  }

  a_new.set_word_ordering(a_start.order_type() == WO_Right_Wreath_Product ||
                          a_start.order_type() == WO_Right_Recursive ?
                          WO_Right_Wreath_Product : WO_Wreath_Product);
  switch (a_start.order_type())
  {
    case WO_Coset:
      maf.container.usage_error("Coset ordering should not be used in the"
                                " rewriting system for a group!\n");
      break;
    default:
    case WO_Right_Shortlex:
    case WO_Short_Recursive:
    case WO_Short_Right_Recursive:
    case WO_Weighted_Lex:
    case WO_Weighted_Shortlex:
    case WO_Accented_Shortlex:
    case WO_Multi_Accented_Shortlex:
    case WO_Short_Weighted_Lex:
    case WO_Short_FPTP:
      maf.container.error_output("The selected ordering of group words is not"
                                 " yet supported in coset systems and\nis being"
                                 " changed to shortlex\n");
    case WO_Shortlex:
      for (g = 0; g < original_nr_letters;g++)
        a_new.set_level(g,2);
      for (; g < nr_letters;g++)
        a_new.set_level(g,1);
      break;
    case WO_Recursive:
    case WO_Right_Recursive:
      for (g = 0; g < original_nr_letters;g++)
        a_new.set_level(g,g+2);
      for (; g < nr_letters;g++)
        a_new.set_level(g,1);
      break;

    case WO_Grouped:
    case WO_Ranked:
    case WO_NestedRank:
      maf.container.error_output("The selected ordering of group words is not"
                                 " yet supported in coset systems and\nis being"
                                 " changed to Wreath Product\n");
    case WO_Right_Wreath_Product:
    case WO_Wreath_Product:
      for (g = 0; g < original_nr_letters;g++)
        a_new.set_level(g,a_start.level(g)+2);
      for (; g < nr_letters;g++)
        a_new.set_level(g,1);
      break;
  }

  for (g = 0;g < original_nr_letters;g++)
    coset_system.set_inverse(g,maf.inverse(g),0);
  Ordinal offset = original_nr_letters+1;
  if (name_generators)
  {
    Ordinal next_inverse = missing_inverse;
    for (h = 0; h < nr_sub_letters;h++)
      if (inverses && inverses[h] != INVALID_SYMBOL)
        coset_system.set_inverse(h+offset,inverses[h]+offset,0);
      else if (!no_inverses)
      {
        coset_system.set_inverse(h+offset,next_inverse,0);
        coset_system.set_inverse(next_inverse,h+offset,0);
        next_inverse++;
      }
  }
  const Linked_Packed_Equation * axiom;
  String_Buffer sb1,sb2;
  for (axiom = maf.first_axiom();axiom;axiom = axiom->get_next())
  {
    Simple_Equation se(maf.alphabet,*axiom);
    se.lhs_word.format(&sb1);
    se.rhs_word.format(&sb2);
    coset_system.add_axiom(sb1.get(),sb2.get(),false);
  }
  if (name_generators)
  {
    Ordinal_Word lhs_word(a_new,1);
    Ordinal_Word rhs_word(a_new,2);
    rhs_word.set_code(1,original_nr_letters);
    for (Ordinal h = 0; h < nr_sub_letters;h++)
    {
      rhs_word.set_code(0,offset+h);
      rhs_word.format(&sb1);
      lhs_word.set_code(0,original_nr_letters);
      lhs_word.set_length(1);
      lhs_word += Entry_Word(generators,h);
      lhs_word.format(&sb2);
      coset_system.add_axiom(sb2.get(),sb1.get(),false);
    }
  }
  else
  {
    Ordinal_Word lhs_word(a_new,1);
    sb1.set(a_new.glyph(original_nr_letters));
    nr_sub_letters = generators.count();
     /* There can be more than MAX_GENERATORS subgroup generators,
        though if they are all needed we have no hope of computing
        an automatic structure */
    for (Element_ID i = 0; i < nr_sub_letters;i++)
    {
      lhs_word.set_code(0,original_nr_letters);
      lhs_word.set_length(1);
      lhs_word += Entry_Word(generators,i);
      lhs_word.format(&sb2);
      coset_system.add_axiom(sb2.get(),sb1.get(),false);
    }
  }
  sb1.set(maf.name);
  sb1.append("_Cos");
  coset_system.name = sb1.get().clone();
  coset_system.subgroup_filename = filename.clone();
  coset_system.original_filename = maf.filename.clone();
  return &coset_system;
}

/**/

void Subalgebra_Descriptor::print(Output_Stream * stream) const
{
  Container & container = maf.container;

  container.output(stream,"%s := rec\n"
                          "(\n",
                          name.string() ? name.string().string() : "_RWS_Sub");
  container.output(stream,is_normal ? "  normalSubGenerators := " :
                                      "  subGenerators := ");
  generators.print(container,stream);
  container.output(stream,alphabet ? ",\n" : "\n");
  if (alphabet)
  {
    container.output(stream,"  subGeneratorNames :=        [");
    alphabet->print(container,stream,APF_Bare);
    container.output(stream,inverses ? "],\n" : "]\n");
    if (inverses)
    {
      container.output(stream,"  subGeneratorInverseNames := [");
      container.output(stream,"%s",alphabet->glyph(inverses[0]).string());
      Ordinal count = alphabet->letter_count();
      for (Ordinal i = 1; i < count;i++)
        container.output(stream,",%s",alphabet->glyph(inverses[i]).string());
      container.output(stream,"]\n");
    }
  }
  container.output(stream,");\n");
}

/**/

void Subalgebra_Descriptor::save(String save_filename)
{
  if (filename != save_filename)
    filename = save_filename.clone();
  Output_Stream * os = maf.container.open_text_output_file(save_filename);
  print(os);
  maf.container.close_output_file(os);
}

/**/

Letter * Subalgebra_Descriptor::coset_system_suffix(bool *is_sub_suffix,
                                                    String subgroup_suffix)
{
  Letter * coset_suffix = subgroup_suffix.clone();
  Letter * coset_end = coset_suffix + subgroup_suffix.length();
  Letter * c = coset_suffix;
  *is_sub_suffix = true;
  if (*c == '.')
    c++;
  if (strncmp(c,"cos",3)==0 || coset_end-coset_suffix > 4 &&
      strcmp(coset_end-4,"_cos")==0)
  {
    /* In this case the caller has supplied a "cosname" rather than a
       substructure suffix.*/
    *is_sub_suffix = false;
  }
  else if (strncmp(c,"sub",3)==0)
    memcpy(c,"cos",3);
  else
  {
    delete [] coset_suffix;
    String_Buffer sb;
    sb.set(subgroup_suffix);
    sb.append("_cos");
    coset_suffix = sb.get().clone();
  }
  return coset_suffix;
}

MAF * MAF::create_from_substructure(String group_filename,
                                    String subgroup_suffix,
                                    Container * container,
                                    unsigned flags,
                                    const Options * options)
{
  if (!container)
    container = MAF::create_container();

  String_Buffer sbsub;
  if (!subgroup_suffix)
    subgroup_suffix = ".sub";
  String subgroup_filename = sbsub.make_filename("",group_filename,
                                                 subgroup_suffix);

  bool have_sub_suffix;
  Letter * coset_suffix;
  coset_suffix = Subalgebra_Descriptor::coset_system_suffix(&have_sub_suffix,
                                                            subgroup_suffix);
  if (!have_sub_suffix)
    flags &= ~CFI_ALLOW_CREATE;
  MAF * coset_system = 0;
  String_Buffer sb_cos;
  String coset_filename = sb_cos.make_filename("",group_filename,coset_suffix);

  if (flags & CFI_ALLOW_CREATE)
  {
    MAF & maf = * MAF::create_from_rws(group_filename,container,0,options);
    Subalgebra_Descriptor &sub =
      *Subalgebra_Descriptor::create(subgroup_filename,maf);
    if (!sub.has_named_generators() && !(flags & CFI_REQUIRE_NAMED_H_GENERATORS))
      flags &= ~CFI_NAMED_H_GENERATORS;
    coset_system = sub.create_coset_system((flags & CFI_NAMED_H_GENERATORS)!=0,
                                           (flags & CFI_NO_INVERSES)!=0);
    coset_system->save(coset_filename);
    delete &sub;
    delete &maf;
  }

  /* at this point I used to delete and recreate the coset system if it had
     been created and CFI_CREATE_RM was specified, since the coset system,
     if it now exists, does not have a Rewriter_Machine yet. But this does not
     matter, because one will be created automatically when we try to use it
     since realise_rm() will get called. */
  if (!coset_system)
    coset_system = MAF::create_from_rws(coset_filename,container,
                                        (flags & CFI_CREATE_RM+CFI_RESUME) |
                                        CFI_CS_EXPECTED,options);
  delete coset_suffix;
  coset_system->original_filename = group_filename.clone();
  if (have_sub_suffix)
    coset_system->subgroup_filename = subgroup_filename.clone();
  return coset_system;
}
