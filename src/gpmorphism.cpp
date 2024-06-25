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


// $Log: gpmorphism.cpp $
// Revision 1.2  2010/06/10 13:57:16Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/11 23:26:06Z  Alun
// New file.
//

#include <stdlib.h>
#include "maf_tc.h"
#include "maf.h"
#include "container.h"
#include "alphabet.h"
#include "mafword.h"
#include "maf_dr.h"
#include "maf_so.h"
#include "fsa.h"
#include "mafctype.h"
#include "equation.h"

static int inner(char * output_filename,MAF * source,MAF * target,
                 bool check_index,Group_Automaton_Type reduction_method);

int main(int argc,char ** argv)
{
  int i = 1;
  char * source_filename = 0;
  char * target_filename = 0;
  char * output_filename = 0;
  bool check_index = false;
  Container * container = MAF::create_container();
  int retcode = 1;

  Standard_Options so(*container,SO_STDOUT|SO_REDUCTION_METHOD);
  bool bad_usage = false;
#define cprintf container->error_output

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-check_index"))
      {
        check_index = true;
        i++;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else
    {
      if (source_filename == 0)
        source_filename = argv[i];
      else if (target_filename == 0)
        target_filename = argv[i];
      else if (output_filename == 0)
        output_filename = argv[i];
      else
        bad_usage = true;
      i++;
    }
  }

  if (!bad_usage && source_filename && target_filename)
  {
    MAF * source = MAF::create_from_rws(source_filename,container);
    if (source)
    {
      MAF * target = MAF::create_from_rws(target_filename,container);
      if (target)
      {
        retcode = inner(output_filename,source,target,check_index,so.reduction_method);
        delete target;
      }
      else
        cprintf("Cannot read %s\n",target_filename);
      delete source;
    }
    else
      cprintf("Cannot read %s\n",source_filename);
  }
  else
  {
    cprintf("Usage:\n"
            "gpmorphism [loglevel] [reduction_method] [-check_index] sourcerws"
            " targetrws\n[outputfile]\n\n"
            "where both sourcerws and targetrws contain GASP rewriting"
            " systems.\n"
            "An automaton that solves the word problem for targetrws must"
            " previously have\nbeen computed.\n"
            "gpmorphism tries to compute mappings from the\n"
            " generators of sourcerws\nto words in the generators of targetrws"
            " in which the axioms of sourcerws are\nsatisfied. If successful"
            " the computed mapping defines a homomorphism between\nthe groups"
            " or monoids corresponding to sourcerws and targetrws.\n"
            "If -check_index is specified gpmorphism performs a coset enumeration"
            " so that it\ncan be checked if the mapping is an epimorphism.\n");
    so.usage();
  }

  delete container;
  return retcode;
#undef cprintf
}

/**/

class Homomorphism_Enumerator
{
  private:
    MAF & source;
    MAF & target;
    FSA::Word_Iterator **wi;
    Total_Length *height_so_far;
    Ordinal_Word gen;
    Ordinal level;
    Ordinal last_generator;
    Total_Length height;
    bool new_level;
    bool ok;
    bool success;
    bool seen;
  public:
    const FSA * wa;
    const FSA * cclass;
    const Ordinal nr_generators;
    Container & container;

    Homomorphism_Enumerator(MAF & source_,MAF & target_,
                            Group_Automaton_Type reduction_method) :
      source(source_),
      target(target_),
      wa(target_.load_fsas(GA_WA)),
      cclass(target_.load_fsas(GA_CCLASS)),
      nr_generators(source_.properties().nr_generators),
      container(target_.container),
      wi(0),
      height_so_far(0),
      gen(target_.alphabet)
    {
      ok = false;

      if (!wa)
      {
        container.error_output("Cannot load word-acceptor for target\n");
        return;
      }

      if (!target.load_reduction_method(reduction_method))
      {
        container.error_output("Unable to reduce words using selected mechanism\n");
        return;
      }
      ok = true;
      wi = new FSA::Word_Iterator *[nr_generators];
      height_so_far = new Total_Length[nr_generators];
      for (Ordinal g = 0; g < nr_generators;g++)
        if (source.inverse(g) >= g || source.inverse(g) == INVALID_SYMBOL)
          wi[g] = new FSA::Word_Iterator(*wa);
        else
          wi[g] = 0;
    }

    ~Homomorphism_Enumerator()
    {
      if (height_so_far)
        delete [] height_so_far;
      if (wi)
      {
        for (Ordinal i = 0; i < nr_generators;i++)
          if (wi[i])
            delete wi[i];
        delete [] wi;
      }
    }

    const Word &image(Ordinal g)
    {
      if (!success || g < 0 || g >= nr_generators)
      {
        gen.set_length(0);
        return gen;
      }
      if (wi[g])
        return wi[g]->word;
      target.invert(&gen,wi[source.inverse(g)]->word);
      target.reduce(&gen,gen);
      return gen;
    }

    bool first()
    {
      if (!ok)
        return false;
      new_level = true;
      level = 0;
      height = 0;
      seen = false;
      return next();
    }

    bool next()
    {
      success = false;
      if (level < 0)
        return false;

      for (;;)
      {
        if (level < nr_generators)
        {
          if (wi[level])
          {
            State_ID found;
            if (new_level)
              found = wi[level]->first();
            else
            {
              bool inside = true;
              Total_Length so_far = level ? height_so_far[level-1] : 0;
              so_far += (wi[level]->word.length()+1);
              inside = so_far <= height;
              found = wi[level]->next(inside);
            }
            if (!found)
            {
              if (level-- == 0)
              {
                height++;
                if (!seen)
                  return false;
                seen = false;
                new_level = true;
                level = 0;
              }
            }
            else if (wa->is_accepting(found) &&
                     (level !=0 && height_so_far[level-1] || may_be_first(level)))
            {
              Total_Length so_far = level ? height_so_far[level-1] : 0;
              height_so_far[level] = so_far + wi[level]->word.length();
              level++;
              new_level = true;
            }
            else
              new_level = false;
          }
          else if (new_level)
          {
            height_so_far[level] = height_so_far[level-1];
            level++;
            new_level = true;
          }
          else
          {
            level--;
            new_level = false;
          }
        }

        if (level == nr_generators)
        {
          success = height_so_far[level-1] == height;
          if (success)
            seen = true;
          Ordinal next_level = level-1;

          for (Ordinal g = 0; g < nr_generators;g++)
            if (source.inverse(g) == g)
            {
              gen = image(g);
              gen += gen;
              target.reduce(&gen,gen);
              if (gen.length())
              {
                success = false;
                next_level = g;
                break;
              }
            }

          Ordinal_Word lhs_word(target.alphabet);
          Ordinal_Word rhs_word(target.alphabet);
          if (success && image(0).length())
          {
            /* Although we can't easily exlude all homomorphisms except
               the first, we can at least see if the image of the second
               generator is obviously not minimal by conjugating by the
               image of the first generator */
            for (Ordinal g = 1; g < nr_generators;g++)
              if (wi[g])
              {
                lhs_word = image(source.inverse(0));
                lhs_word += wi[g]->word;
                target.reduce(&lhs_word,lhs_word);
                lhs_word += image(0);
                target.reduce(&lhs_word,lhs_word);
                if (lhs_word.compare(wi[g]->word) < 0)
                {
                  success = false;
                  if (next_level > g)
                    next_level = g;
                }
                lhs_word = image(0);
                lhs_word += wi[g]->word;
                target.reduce(&lhs_word,lhs_word);
                lhs_word += image(source.inverse(0));
                target.reduce(&lhs_word,lhs_word);
                if (lhs_word.compare(wi[g]->word) < 0)
                {
                  success = false;
                  if (next_level > g)
                    next_level = g;
                }
                break;
              }
          }
          for (const Linked_Packed_Equation * axiom = source.first_axiom();
               axiom;axiom = axiom->get_next())
          {
            Simple_Equation se(source.alphabet,*axiom);
            last_generator = 0;
            bool ok = true;
            if (make_word(&lhs_word,se.lhs_word) &&
                make_word(&rhs_word,se.rhs_word))
            {
              if (lhs_word != rhs_word)
                ok = success = false;
            }
            else
              ok = success = false;
            if (!ok)
            {
              if (last_generator < next_level)
                next_level = last_generator;
              if (next_level == 0)
                break;
            }
          }
          level = next_level;
          new_level = false;
          if (success)
            return true;
        }
      }
    }

    void format(String_Buffer * sb)
    {
      Word_List wl(target.alphabet);
      for (Ordinal g = 0; g < nr_generators;g++)
        wl.add(image(g),false);
      wl.format(sb);
    }

    Language_Size image_index()
    {
      Word_List wl(target.alphabet);
      Word_List dummy(target.alphabet);
      for (Ordinal g = 0; g < nr_generators;g++)
        wl.add(image(g),false);
      TC_Enumeration_Options tc(TCES_Long);
      tc.max_cosets = 4096*1024;
      unsigned log_level = container.get_log_level();
      container.set_log_level(0);
      Language_Size answer = target.enumerate_cosets(0,dummy,wl,tc);
      container.set_log_level(log_level);
      return answer;
    }

  private:
    bool make_word(Ordinal_Word * target_word,const Word &source_word)
    {
      target_word->set_length(0);
      const Ordinal * values = source_word.buffer();
      Word_Length l = source_word.length();
      for (Word_Length i = 0; i < l ; i++)
      {
        const Word & gen = image(values[i]);
        if (values[i] > last_generator)
          last_generator = values[i];
        if (gen.length() + target_word->length() > MAX_WORD)
          return false;
        *target_word += gen;
        target.reduce(target_word,*target_word);
      }
      return true;
    }

    bool may_be_first(Ordinal level)
    {
      const Ordinal * values = wi[level]->word.buffer();
      Word_Length l = wi[level]->word.length();
      if (l == 0)
        return true;
      if (l > 1)
      {
        if (values[0] == target.inverse(values[l-1]))
          return false;
      }
      for (Word_Length i = 1;i < l;i++)
      {
        gen = (Subword((Word &)wi[level]->word,i,l) +
              Subword((Word &)wi[level]->word,0,i));
        if (gen.compare(wi[level]->word) < 0)
          return false;
      }
      if (cclass && !cclass->accepts(wi[level]->word))
        return false;
      return true;
    }

};

/**/

static int inner(char * output_filename,MAF * source,MAF * target, bool check_index,
                 Group_Automaton_Type reduction_method)
{
  Homomorphism_Enumerator hom(*source,*target,reduction_method);
  Container & container = target->container;

  if (!output_filename)
    container.set_gap_stdout(true);
  Output_Stream * os = 0;
  bool first = true;
  for (bool ok = hom.first();ok;ok = hom.next())
  {
    String_Buffer sb;
    hom.format(&sb);
    if (check_index)
    {
      Language_Size index;
      if (first)
        index = target->fsas.wa->language_size(true);
      else
        index = hom.image_index();
      if (first)
      {
        os = container.open_text_output_file(output_filename);
        container.output(os,"morphisms :=\n"
                        "[\n");
      }
      else
        container.output(os,",\n");
      container.output(os,"  [%s,"  FMT_LS "]",sb.get().string(),index);
    }
    else
    {
      if (first)
      {
        os = container.open_text_output_file(output_filename);
        container.output(os,"morphisms :=\n"
                        "[\n");
      }
      container.output(os,first ? "  %s" : ",\n  %s" ,sb.get().string());
    }
    first = false;
  }
  if (os)
  {
    container.output(os,"\n]\n");
    container.close_output_file(os);
  }
  return os!=0 ? 0 : 1;
}
