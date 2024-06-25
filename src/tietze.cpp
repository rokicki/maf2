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


// $Log: tietze.cpp $
// Revision 1.5  2011/06/11 16:24:06Z  Alun
// printf() arguments corrected
// Revision 1.4  2010/07/08 11:27:52Z  Alun
// Ctrl+C no longer waits until current round of simplification has finished
// Revision 1.3  2010/06/10 17:25:09Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.2  2010/06/10 13:57:51Z  Alun
// All tabs removed again
// Revision 1.1  2010/06/05 10:27:12Z  Alun
// New file.
//

#include <signal.h>
#include "maf.h"
#include "tietze.h"
#include "equation.h"
#include "container.h"

Tietze_Transformation_Helper::Tietze_Transformation_Helper(MAF & maf_,
                                                           unsigned strategy_,
                                                           unsigned tth_flags,
                                                           Ordinal max_seen_,
                                                           Word_Length max_elimination_,
                                                           Word_Length long_length_) :
  maf(maf_),
  nr_generators(maf_.group_alphabet().letter_count()),
  nr_new_generators(nr_generators),
  last_g(maf_.generator_count()-1),
  total_weight(new unsigned long[nr_generators]),
  weight(new unsigned long[nr_generators]),
  present(new bool[nr_generators]),
  relators(maf.alphabet,maf.axiom_count()),
  keep_generators((tth_flags & TTH_KEEP_GENERATORS)!=0),
  keep_going((tth_flags & TTH_KEEP_GOING)!=0),
  no_simplify((tth_flags & TTH_NO_SIMPLIFY)!=0),
  abelian((tth_flags & TTH_ABELIANISE)!=0),
  best_equation_relators((tth_flags & TTH_BEST_EQUATION_RELATORS)!=0),
  ok(true),
  available(0),
  strategy(strategy_),
  lex_timestamp(0),
  lex_checked_timestamp(0),
  max_seen(max_seen_),
  max_elimination(max_elimination_),
  long_length(long_length_),
  eliminated_generators(new Ordinal_Word *[nr_generators])
{
  Ordinal_Word word(maf.alphabet);
  Ordinal g;
  allowed_max_length = MAX_WORD/2;
  for (g = 0;g < nr_generators;g++)
  {
    weight[g] = total_weight[g] = 0;
    eliminated_generators[g] = 0;
  }

  for (g = 0;g < nr_generators;g++)
    if (maf.inverse(g) == INVALID_SYMBOL)
    {
      maf.container.usage_error("Simplify only works with group"
                                " presentations.\n"
                                "Generator %s is not invertible\n",
                                maf.alphabet.glyph(g).string());
      ok = false;
    }


  /* Build a list of relators sorted by increasing length and count up
     the total number of occurrences of each generator */
  for (const Linked_Packed_Equation *axiom = maf.first_axiom();
       axiom;axiom = axiom->get_next())
  {
    Simple_Equation se(maf.alphabet,*axiom);
    if (se.first_letter() < nr_generators)
    {
      if (maf.invert(&word,se.lhs_word))
      {
        word += se.rhs_word;
        reduce_and_insert_relator(relators,word);
      }
      else
        MAF_INTERNAL_ERROR(maf.container,("Unexpected failure from MAF::invert()\n"));
    }
  }
}

/**/

static bool best_equation(Ordinal_Word * uword,
                          Ordinal_Word * vword,
                          const Word & test_relator,
                          const MAF & maf)
{
  Ordinal_Word new_uword(test_relator);
  Ordinal_Word new_vword(test_relator);
  Word_Length l = test_relator.length();
  bool retcode = false;
  new_vword.set_length(0);
  const Ordinal * values = new_uword.buffer();
  int cmp;

  for (Word_Length i = l; ;i--)
  {
    if (i != l)
      new_vword.append(maf.inverse(values[i]));
    new_uword.set_length(i);
    if ((!maf.properties().is_short || l-i >= i) &&
        new_uword.compare(new_vword) != 1)
      break;
  }
  l = new_vword.length();
  new_uword.append(maf.inverse(new_vword.value(l-1)));
  new_vword.set_length(l-1);
  if ((cmp = new_uword.compare(*uword)) == -1 ||
      cmp==0 && new_vword.compare(*vword) == -1)
  {
    *uword = new_uword;
    *vword = new_vword;
    retcode = true;
  }
  return retcode;
}

bool Tietze_Transformation_Helper::insert_relator(Sorted_Word_List & relators,
                                                  const Word & word,
                                                  bool make_canonical)
{
  Element_ID new_word_nr;
  Ordinal_Word new_relator(word);
  Word_Length l = word.length();

  if (make_canonical)
  {
    Word_Length i;
    Ordinal_Word test(word);

    if (abelian)
    {
      maf.abelian_invert(&test,test);
      if (new_relator.compare(test) > 0)
        new_relator = test;
    }
    else
    {
      test += word;
      if (!best_equation_relators)
      {
        for (i = 1; i < l;i++)
          if (new_relator.compare(Subword(test,i,i+l)) > 0)
            new_relator = Subword(test,i,i+l);
        maf.invert(&test,test);
        for (i = 0; i < l;i++)
          if (new_relator.compare(Subword(test,i,i+l)) > 0)
            new_relator = Subword(test,i,i+l);
      }
      else
      {
        Ordinal_Word uword(new_relator);
        Ordinal_Word vword(new_relator);
        vword.set_length(0);
        best_equation(&uword,&vword,new_relator,maf);

        for (i = 1; i < l;i++)
          if (best_equation(&uword,&vword,Subword(test,i,i+l),maf))
            new_relator = Subword(test,i,i+l);
        maf.invert(&test,test);
        for (i = 0; i < l;i++)
          if (best_equation(&uword,&vword,Subword(test,i,i+l),maf))
            new_relator = Subword(test,i,i+l);
      }
    }
  }

//  Ordinal_Word rhs(maf.alphabet);
//  String reason = 0;
//  if (!maf.is_valid_equation(&reason,new_relator,rhs))
//    MAF_INTERNAL_ERROR(maf.container,(reason));

  if (relators.insert(new_relator,&new_word_nr))
  {
    const Ordinal * values = new_relator.buffer();
    for (Word_Length i = 0; i < l;i++)
      total_weight[values[i]]++;
    if (l > new_max_length)
      new_max_length = l;
    last_new_relator = new_word_nr;
    if (first_complete >= new_word_nr)
      first_complete++;
    if (lex_timestamp)
    {
      Element_ID count = relators.count();
      for (Element_ID i = count; --i > new_word_nr;)
      {
        lex_timestamp[i] = lex_timestamp[i-1];
        lex_checked_timestamp[i] = lex_checked_timestamp[i-1];
      }
      lex_checked_timestamp[new_word_nr] = 0;
      lex_timestamp[new_word_nr] = ++timestamp;
    }
    if (available)
    {
      Element_ID count = relators.count();
      Bit_Array new_bits(count);
      for (Element_ID i = count; --i > new_word_nr;)
      {
        in_available.assign(i,in_available.get(i-1));
        available[i] = available[i-1];
      }
      available[new_word_nr] = 0;
      if (first_in_available > new_word_nr)
        first_in_available++;
      in_available.assign(new_word_nr,0);
      find_simplifications(FS_USE_WORD2,0,new_word_nr);
    }
    return true;
  }
  return false;
}

/**/

bool Tietze_Transformation_Helper::remove_relator(const Word & word)
{
  Word_Length l = word.length();
  const Ordinal * values = word.buffer();
  for (Word_Length i = 0; i < l;i++)
    total_weight[values[i]]--;
  Element_ID was_word_nr;
  if (relators.remove(word,&was_word_nr))
  {
    Element_ID count = relators.count();
    if (available)
    {
      if (first_complete > was_word_nr)
        first_complete--; /* this happens when we find a redundant relator */
      if (first_in_available > was_word_nr)
        first_in_available--;

      if (available[was_word_nr])
        delete available[was_word_nr];

      for (Element_ID i = was_word_nr; i < count;i++)
      {
        available[i] = available[i+1];
        in_available.assign(i,in_available.get(i+1));
      }
      available[count] = 0;
      in_available.assign(count,0);
    }
    if (lex_timestamp)
    {
      for (Element_ID i = was_word_nr; i < count;i++)
      {
        lex_timestamp[i] = lex_timestamp[i+1];
        lex_checked_timestamp[i] = lex_checked_timestamp[i+1];
      }
    }
    return true;
  }
  /* In the unlikely event that we tried to remove a non-relator
      correct the counts for each generator */
  for (Word_Length i = 0; i < l;i++)
    total_weight[values[i]]++;
  return false;
}

/**/

void Tietze_Transformation_Helper::polish()
{
  bool try_simplify = true;
  bool try_eliminate = true;
  bool try_lex = true;

  size_t was_total_length = 0;

  while (try_simplify || try_eliminate)
  {
    if (try_simplify)
    {
      try_simplify = false;
      if (!maf.aborting && !no_simplify)
        if (simplify())
          try_lex = try_eliminate = true;
    }
    if (maf.aborting)
    {
      keep_generators = true;
      try_lex = false;
    }

    if (try_eliminate)
    {
      if (!was_total_length || relators.total_length() < was_total_length)
        was_total_length = relators.total_length();
      try_eliminate = false;
      if (!keep_generators)
      {
        if (eliminate())
          try_lex = try_eliminate =
           try_simplify = true;
        if (!keep_going && relators.total_length() > was_total_length * 2 ||
            maf.aborting)
          keep_generators = true;
      }
      else
        remove_available();
    }
    if (!try_eliminate && !try_simplify && !no_simplify && try_lex)
    {
      try_lex = false;
      if (!best_equation_relators && find_lex_changes())
        try_eliminate = try_simplify = true;
    }
  }
}

/**/

bool Tietze_Transformation_Helper::reduce_and_insert_relator(Sorted_Word_List &new_relators,Ordinal_Word &new_relator)
{
  if (abelian)
  {
    maf.abelianise(&new_relator,new_relator);
    if (new_relator.length())
      return insert_relator(new_relators,new_relator,true);
  }
  else
  {
    Word_Length l = new_relator.length();
    Ordinal * values = new_relator.buffer();
    // freely reduce our new relator (which will remove inverse relations)
    {
      Ordinal last = INVALID_SYMBOL;
      Word_Length new_length = 0;

      for (Word_Length i = 0 ; i < l;i++)
      {
        Ordinal g = values[i];
        if (maf.inverse(g) != last)
          values[new_length++] = last = g;
        else
        {
          --new_length;
          last = new_length ? values[new_length-1] : INVALID_SYMBOL;
        }
      }
      new_relator.set_length(l = new_length);
    }
    Word_Length start = 0;
    while (start+1 < l && maf.inverse(values[start]) == values[l-1])
    {
      l--;
      start++;
    }
    if (start < l)
      return insert_relator(new_relators,Subword(new_relator,start,l),true);
  }
  return false;
}

void Tietze_Transformation_Helper::remove_available()
{
  if (available)
  {
    Element_ID count = relators.count();
    for (Element_ID i = 0; i < count;i++)
      if (available[i])
        delete available[i];
    delete [] available;
    available = 0;
  }
  in_available.empty();
}

/**/

bool Tietze_Transformation_Helper::substitute(Ordinal_Word * answer,
                                              bool * changed,
                                              const Word &word,
                                              Ordinal g1,
                                              const Ordinal_Word & g1_word,
                                              Ordinal g2,
                                              const Ordinal_Word & g2_word)
{
  Word_Length l = word.length();
  const Ordinal * values = word.buffer();
  answer->set_length(0);
  Total_Length new_length = 0;
  bool ok = true;
  Word_Length g1_length = g1_word.length();
  Word_Length g2_length = g2_word.length();
  *changed = false;

  for (Word_Length i = 0; i < l;i++)
  {
    Ordinal g = values[i];
    if (g == g1)
    {
      *changed = true;
      new_length += g1_length;
      if (new_length > allowed_max_length)
      {
        ok = false;
        break;
      }
      *answer = *answer + g1_word;
    }
    else if (g == g2)
    {
      *changed = true;
      new_length += g2_length;
      if (new_length > allowed_max_length)
      {
        ok = false;
        break;
      }
      *answer = *answer + g2_word;
    }
    else
    {
      new_length += 1;
      if (new_length > allowed_max_length)
      {
        ok = false;
        break;
      }
      answer->append(g);
    }
  }
  return ok;
}

bool Tietze_Transformation_Helper::eliminate()
{
  /* new_relators will contain a modified set of relators we are currently building */
  Sorted_Word_List new_relators(maf.alphabet);
  Ordinal_Word word(maf.alphabet);
  /* we will use the next variables for storing the word for an eliminated
     generator and its inverse */
  Ordinal_Word g_word(maf.alphabet);
  Ordinal_Word ig_word(maf.alphabet);
  Ordinal_Word new_word(maf.alphabet);/* used to contain a new relator */
  Ordinal g,ig;
  bool found;
  bool retcode = false;
  bool forbidden = false;
  Ordinal_Word forbidden_eliminations(maf.alphabet);
  size_t allowance = strategy & 2 ?
                     relators.total_length() + relators.total_length()/6*5 :
                     relators.total_length() + relators.total_length()/20;

  remove_available();

  // now we will repeatedly try to eliminate a generator and its inverse
  do
  {
    found = false;
    Element_ID word_nr;
    Element_ID count = relators.count();
    Element_ID best_relator = count; // we should not need to initialise
    long best_cost = 0;              // best_, but do so to stop spurious
    int best_seen = 0;               // compiler warnings.
    Ordinal best_g = 0;
    Ordinal last_ig = 0;
    unsigned long best_weight = ULONG_MAX;
    while (eliminated_generators[last_g]!=0)
      last_g--;

    if (!count)
      return retcode;

    /* look through our current relator set to try to find
       a generator that can be eliminated. */

    /* see if any generators have become trivial or reducible */
    {
      const Ordinal_Word * ow = relators.word(0);
      Word_Length l = ow->length();
      if (l <= 2)
      {
        /* Note:
           A relator (in our list) of length 2 cannot be a known inverse
           relation because such relations are never inserted into our
           list of relators, and get removed if an inverse is changed */
        found = true;
        best_relator = 0;
        if (l == 2 && ow->value(0) == maf.inverse(ow->value(1)))
          MAF_INTERNAL_ERROR(maf.container,("Unexpected Inverse Relator\n"));
        best_g = relators.word(0)->value(l-1);
      }
    }

    if (!found)
    {
      if (strategy & 1)
      {
        for (g = 0;g < last_g;g++)
        {
          ig = maf.inverse(g);
          if (ig != g)
          {
            if (total_weight[g] &&
                total_weight[g] + total_weight[ig]-1 < best_weight)
              best_weight = total_weight[g] + total_weight[ig] -1;
          }
          else if (total_weight[g] && total_weight[g]-1 < best_weight)
            best_weight = total_weight[g] -1;
        }
      }
      last_ig = maf.inverse(last_g);

      /* first look through our current relator set to try to find
         a generator that can be eliminated. */
      for (word_nr = 0;word_nr < count;word_nr++)
      {
        const Word * relator = relators.word(word_nr);
        Word_Length l = relator->length();
        const Ordinal * values = relator->buffer();

        if (max_elimination && l > max_elimination)
          break;
        Word_Length i;
        /* Potentially the number of generators is very high, and, if so, we
           should try to avoid loops through the generators */
        if (l < nr_generators)
        {
          for (i = 0; i < l;i++)
          {
            g = values[i];
            ig = maf.inverse(g);
            weight[g] = 0;
            weight[ig] = 0;
          }
        }
        else
          for (g = 0;g < nr_generators;g++)
             weight[g] = 0;

        int singletons = 0;
        int seen = 0;
        for (i = 0; i < l;i++)
        {
          g = values[i];
          if (++weight[g]==1)
          {
            singletons++;
            seen++;
          }
          else if (weight[g]==2)
            singletons--;
        }

        /* if a relator contains only one occurence of a generator and does
           not contain its inverse we could eliminate this generator and its
           inverse. We estimate what increase in the total relator size
           this causes. If we do this elimination this relator will be
           eliminated so we can subtract that from the cost.
        */
        if (max_seen && seen > max_seen)
          singletons = 0;
        if (singletons)
        {
          if (l < nr_generators)
          {
            for (i = 0;i < l;i++)
            {
              g = values[i];
              if (weight[g] == 1 && (weight[ig = maf.inverse(g)]==0 || ig==g) &&
                  (!forbidden ||
                   forbidden_eliminations.find_symbol(g)==INVALID_LENGTH))
              {
                long cost = (g!=ig ? total_weight[g] + total_weight[ig]-1 :
                                     total_weight[g]+1) * (l-2) -l;
                bool is_best = !found;

                if (!is_best)
                {
                  if (strategy & 1)
                    is_best = cost < best_cost || cost==best_cost && g > best_g;
                  else
                    is_best = cost < best_cost && (g == best_g || ig==best_g) ||
                              g > best_g && ig > best_g;
                  if (strategy & 8 && seen != best_seen)
                    is_best = seen < best_seen;
                }

                if (is_best)
                {
                  found = true;
                  best_relator = word_nr;
                  best_g = g;
                  best_cost = cost;
                  best_seen = seen;
                }
              }
            }
          }
          else
          {
            Ordinal low_g = strategy & 1 ? 0 : best_g;

            for (g = last_g; g >= low_g; g--)
            {
              if (weight[g] == 1 && (weight[ig = maf.inverse(g)]==0 || ig==g) &&
                  (!forbidden ||
                   forbidden_eliminations.find_symbol(g)==INVALID_LENGTH))
              {
                long cost = (g!=ig ? total_weight[g] + total_weight[ig]-1 :
                                     total_weight[g]+1) * (l-2) -l;
                bool is_best = !found;

                if (!is_best)
                {
                  if (strategy & 1)
                    is_best = cost < best_cost || cost==best_cost && g > best_g;
                  else
                    is_best = cost < best_cost && (g == best_g || ig==best_g) ||
                              g > best_g && ig > best_g;
                  if (strategy & 8 && seen != best_seen)
                    is_best = seen < best_seen;
                }

                if (is_best)
                {
                  found = true;
                  best_relator = word_nr;
                  best_g = g;
                  best_cost = cost;
                }
              }
            }
          }
        }
        if (found)
          if (!(strategy & 9))
          {
            if (best_g == last_g || best_g == last_ig)
              break;
          }
          else if (long((l-2)*best_weight-l) > best_cost)
            break;
      }
    }

    if (found)
    {
      /* we can eliminate a generator */
      const Word * relator = relators.word(best_relator);
      Ordinal ig = maf.inverse(best_g);
      ig_word.set_length(0);
      g_word.set_length(0);
      Word_Length l = relator->length();
      const Ordinal * values = relator->buffer();
      /* calculate the replacement strings for the eliminated generator
         and its inverse */
      Word_Length i;
      for (i = 0; i < l;i++)
      {
        g = values[i];
        if (g != best_g)
          g_word.append(g);
        else
          break;
      }
      while (++i < l)
        ig_word.append(values[i]);
      ig_word = ig_word + g_word;

      if (abelian)
        maf.abelian_invert(&g_word,ig_word);
      else
        maf.invert(&g_word,ig_word);

      if (l == 2)
      {
        /* We had a relator g0*g1 or possibly g0*g0.
           If g0 != g1 we want to replace g1 by G0 and G1 by g0,
           and we don't need to do anything special, because the
           code above has done the right thing.
           However if g0 == g1 then we should not eliminate g1 at all, but
           instead eliminate just G0, and we should change the inverse of
           g0 to g0.
        */
        if (values[0] == values[1])
        {
          /* ig_word contains g0, which is good.
             gword contains G0, which is wrong - we want g0 to stay as it is.
          */
          maf.set_inverse(values[0],values[0]); /* This is OK, this elimination
                                                   cannot possibly fail */
          best_g = INVALID_SYMBOL;
        }
      }

      /* now build a new list of relators */
      new_max_length = 0;
      bool overflow = false;
      bool changed = false;

      /* the relator containing the generator to be eliminated
         will disappear. However, if the generator being eliminated
         was an involution we need to add a new relator to
         express the fact that the word for the eliminated generator is
         an involution. */
      new_relators.grow(best_g != ig ? count-1 : count);

      /* save the current weights in case we decide to change our mind,
         and then zero the weights */
      memcpy(weight,total_weight,nr_generators*sizeof(weight[0]));
      for (g = 0; g < nr_generators;g++)
        total_weight[g] = 0;
      /* now calculate the new relators */
      for (word_nr = 0;word_nr < count;word_nr++)
      {
        if (word_nr != best_relator)
        {
          /* calculate the new word for this relator */
          if (substitute(&new_word,&changed,*relators.word(word_nr),
                         best_g,g_word,ig,ig_word))
          {
            if (changed)
              reduce_and_insert_relator(new_relators,new_word);
            else
              insert_relator(new_relators,new_word,false);
          }
          else
          {
            overflow = true;
            break;
          }
        }
      }

      if (best_g == ig)
      {
        if (g_word.length()*2 > allowed_max_length)
          overflow = true;
        else
          new_word = g_word + g_word;
        reduce_and_insert_relator(new_relators,new_word);
      }

      if (!overflow && (new_max_length <= allowed_max_length || l < 6))
      {
        if (best_g != INVALID_SYMBOL && best_g != ig)
        {
          nr_new_generators -= 2;
          maf.container.progress(2,"Eliminating generators %s %s\n",
                                 maf.alphabet.glyph(best_g).string(),
                                 maf.alphabet.glyph(ig).string());
        }
        else
        {
          nr_new_generators -= 1;
          maf.container.progress(2,"Eliminating generator %s\n",
                                 maf.alphabet.glyph(ig).string());
        }

        retcode = true;
        relators.take(new_relators);
        for (g = 0; g < nr_generators;g++)
          if (eliminated_generators[g])
            if (substitute(&new_word,&changed,*eliminated_generators[g],best_g,g_word,ig,ig_word))
            {
              if (abelian)
                maf.abelianise(&new_word,new_word);
              else
                maf.free_reduce(&new_word,new_word);
              *eliminated_generators[g] = new_word;
            }

        if (best_g != INVALID_SYMBOL)
          eliminated_generators[best_g] = new Ordinal_Word(g_word);
        if (ig != best_g)
          eliminated_generators[ig] = new Ordinal_Word(ig_word);
        maf.container.status(2,0,"Presentation size is now %d," FMT_ID ",%zu\n",
                             nr_new_generators,relators.count(),relators.total_length());
        forbidden_eliminations.set_length(0);
        forbidden = false;
        if (relators.total_length() > allowance)
          return retcode;
      }
      else
      {
        /* don't do this elimination after all */
        memcpy(total_weight,weight,sizeof(weight[0])*nr_generators);
        if (best_g != INVALID_SYMBOL)
        {
          forbidden_eliminations.append(best_g);
          forbidden = true;
        }
        if (ig != best_g)
        {
          forbidden_eliminations.append(ig);
          forbidden = true;
        }
        new_relators.empty();
        found = false;
      }
    }
  }
  while (found && !maf.aborting);
  return retcode;
}

/**/

void Tietze_Transformation_Helper::do_simplification(const Simplification &s)
{
  Ordinal_Word word1(maf.alphabet);
  Ordinal_Word word2(maf.alphabet);

  /* we have found a pair of relators some cyclic conjugates of which have the
     form uv1 and uv2 with |u| > |v2|. Since u=V2 we can replace uv1 by V2v1 */
  word2 = *s.word2;
  Word_Length l2 = word2.length();
  if (abelian)
  {
    /* In the abelian case more than half the symbols in the two relators
       are the same. We simply multiply the first relator by the inverse
       of the second.*/
    if (!s.inverted)
      maf.abelian_invert(&word2,word2);
    maf.abelian_multiply(&word1,*s.word1,word2);
    remove_relator(*s.word1);
    insert_relator(relators,word1,true);
  }
  else
  {
    word1 = *s.word1;
    Word_Length l1 = word1.length();
    if (s.inverted)
      maf.invert(&word2,word2);
    word1 = word1 + word1;
    word2 = word2 + word2;
    /* extract v2 and invert it */
    word2 = Subword(word2,s.start2+s.match,s.start2 + l2);
    maf.invert(&word2,word2);
    /* add v1 */
    word2 += Subword(word1,s.start1+s.match,s.start1 +l1);
    remove_relator(*s.word1);
    // this was inser_relator() but should surely be reduce_and_insert_relator()
    reduce_and_insert_relator(relators,word2);
  }
  maf.container.status(2,1,"Presentation size is now %d,"
                           FMT_ID ",%zu. " FMT_ID " relators to check.\n",
                       nr_new_generators,
                       relators.count(),relators.total_length(),first_complete);

}

/**/

bool Tietze_Transformation_Helper::find_simplifications(unsigned flags,
                                                        Element_ID word1_nr,
                                                        Element_ID word2_nr)
{
  bool retcode = false;
  Ordinal_Word word1(maf.alphabet);
  Ordinal_Word word2(maf.alphabet);
  Ordinal_Word word3(maf.alphabet);
  Ordinal_Word matches(maf.alphabet);
  Ordinal_Word non_matches(maf.alphabet);
  int best_benefit;
  Element_ID top = flags & FS_USE_WORD2 ? relators.count() : first_complete;
  Element_ID new_first_in_available = relators.count();

  /* Clear present[]  once */
  {
    for (Ordinal g = 0; g < nr_generators;g++)
      present[g] = 0;
  }

  for (;;)
  {
    if (!(flags & FS_USE_WORD1))
    {
      if (!top || maf.aborting)
        break;
      word1_nr = --top;
      if (flags & FS_USE_WORD2)
      {
        if (word1_nr <= word2_nr || word1_nr < first_in_available)
          break;
      }
      else
        maf.container.status(2,3,"Presentation size is now %d," FMT_ID ",%zu\n",
                             nr_new_generators,relators.count(),
                             relators.total_length());
    }

    if (relators.word(word1_nr)->length() > 2 &&
        (flags & FS_USE_WORD2 ? in_available.contains(word1_nr) : !in_available.contains(word1_nr)))
    {
      new_first_in_available = word1_nr;
      word1 = *relators.word(word1_nr);
      Word_Length l1 = word1.length();
      if (!abelian)
        word1 += *relators.word(word1_nr);
      if (flags & FS_USE_WORD2)
        word2_nr = word2_nr+1;
      else
        word2_nr = word1_nr;
      best_benefit = 0;
      /* Compute present for this relator and its inverse */
      const Ordinal * v1 = word1.buffer();
      for (Word_Length i = 0; i < l1;i++)
        present[v1[i]] = present[maf.inverse(v1[i])] = true;

      while (word2_nr-- > 0)
      {
        if (relators.word(word2_nr)->length() > best_benefit)
        {
          word2 = *relators.word(word2_nr);
          Word_Length l2 = word2.length();
          Word_Length p3 = l2/2;
          bool ok = true;

          if (abelian)
          {
            for (int pass = 0;pass < 2;pass++)
            {
              if (pass == 1)
                maf.abelian_invert(&word2,word2);
              const Ordinal * v2 = word2.buffer();
              const Ordinal * v1 = word1.buffer();
              Word_Length match = 0;
              Word_Length non_match = 0;
              Word_Length p1 = 0;
              Word_Length p2 = 0;
              while (p2 < l2 && p1 < l1)
              {
                if (v1[p1] == v2[p2])
                {
                  match++;
                  p1++;
                  p2++;
                }
                else if (v1[p1] > v2[p2])
                {
                  p2++;
                  if (++non_match > p3)
                    break;
                }
                else
                  p1++;
              }
              if (match > p3)
              {
                int benefit = match*2-l2;
                if (benefit > best_benefit)
                {
                  best_benefit = benefit;
                  Simplification * s = available[word1_nr];
                  if (!s)
                    s = available[word1_nr] = new Simplification;
                  s->construct(*relators.word(word1_nr),0,
                               *relators.word(word2_nr),
                               0,match,Word_Length(benefit),pass == 1);
                  if (flags & FS_USE_WORD2 && word1_nr >= first_complete)
                    first_complete = word1_nr+1;
                }
              }
              else if (match == p3 && !(l2 & 1) && !available[word1_nr])
              {
                /* If we have found nothing better try a geodesic shortlex
                   reduction */
                p1 = 0;
                p2 = 0;
                matches.set_length(0);
                non_matches.set_length(0);
                while (p2 < l2 && p1 < l1)
                {
                  if (v1[p1] == v2[p2])
                  {
                    matches.append(v2[p2]);
                    p1++;
                    p2++;
                  }
                  else if (v1[p1] > v2[p2])
                  {
                    non_matches.append(maf.inverse(v2[p2]));
                    p2++;
                  }
                  else
                    p1++;
                }
                while (p2 < l2)
                {
                  non_matches.append(maf.inverse(v2[p2]));
                  p2++;
                }

                maf.abelianise(&non_matches,non_matches);
                if (matches.compare(non_matches) > 0)
                {
                  Simplification * s = available[word1_nr] = new Simplification;
                  s->construct(*relators.word(word1_nr),0,
                               *relators.word(word2_nr),
                               0,match,0,pass == 1);
                  if (flags & FS_USE_WORD2 && word1_nr >= first_complete)
                    first_complete = word1_nr+1;
                }
              }
            }
          }
          else
          {
            /* See if it is worth considering this relator at all */
            {
              const Ordinal * v2 = word2.buffer();
              if (!present[v2[0]] && !present[maf.inverse(v2[0])] &&
                  !present[v2[p3]] && !present[maf.inverse(v2[p3])])
                ok = false;
            }

            if (ok)
            {
              word2 += *relators.word(word2_nr);
              const Ordinal * v2;
#if 1
              /* The algorithm here is to note that if a simplification
                 is possible then some position of the relator being
                 simplified contains either v2[0] or v2[p3] or their
                 inverses. So we look for places where this occurs and
                 look outwards for a match of the correct length.
                 This means we consider 4*l1 possible matches.
                 The more obvious code that is in the #else part considers
                 2*l1*l2 possible matches, so it is much slower for long
                 relators.
                 We could probably do a better by precomputing a
                 hash, but we would still need to check every possible hash
                 position, so it is not clear how much benefit we would get
                 by doing this.
              */

              for (int pass = 0;pass < 4;pass++)
              {
                if (pass == 2)
                  maf.invert(&word2,word2);
                v2 = word2.data(pass & 1 ? p3 : 0);
                Word_Length start;
                Word_Length match;

                for (Word_Length start1 = 0;start1 < l1;start1++)
                {
                  const Ordinal * v1 = word1.data(start1);
                  if (v1[0] == v2[0])
                  {
                    start = 0;
                    match = 1;
                    while (match < l2 && v1[match] == v2[match])
                      match++;
                    while (match < l2 && v1[l1-start-1] == v2[l2-start-1])
                    {
                      match++;
                      start++;
                    }
                    int benefit = match*2-l2; /* int because it might not be positive */
                    if (benefit > best_benefit)
                    {
                      best_benefit = benefit;
                      Simplification * s = available[word1_nr];
                      int start2 = pass & 1 ? p3 : 0;
                      if (start > start2)
                        start2 = start2 + l2 - start;
                      else
                        start2 -= start;
                      if (!s)
                        s = available[word1_nr] = new Simplification;
                      s->construct(*relators.word(word1_nr),start > start1 ?
                                   l1+start1-start : start1-start,
                                   *relators.word(word2_nr),
                                   Word_Length(start2),
                                   match,Word_Length(benefit),pass >= 2);
                      if (flags & FS_USE_WORD2 && word1_nr >= first_complete)
                        first_complete = word1_nr+1;
                    }
                  }
                }
              }
#else
              bool inverted = false;
              for (;;) /* we are going to go round twice, once with original word2, once with its inverse */
              {
                for (Word_Length start1 = 0;start1 < l1;start1++)
                {
                  const Ordinal * v1 = word1.data(start1);
                  for (Word_Length start2 = 0;start2 < l2;start2++)
                  {
                    const Ordinal * v2 = word2.data(start2);
                    Word_Length match = 0;
                    if (v2[p3] == v1[p3])
                    {
                      while (match < l2 && v1[match] == v2[match])
                        match++;
                      int benefit = match*2-l2;
                      if (benefit > best_benefit)
                      {
                        best_benefit = benefit;
                        Simplification * s = available[word1_nr];
                        if (!s)
                          s = available[word1_nr] = new Simplification;
                        s->construct(*relators.word(word1_nr),start1,
                                     *relators.word(word2_nr),start2,
                                     match,benefit,inverted);
                        if (flags & FS_USE_WORD2 && word1_nr >= first_complete)
                          first_complete = word1_nr+1;
                      }
                    }
                  }
                }
                if (inverted)
                  break;
                maf.invert(&word2,word2);
                inverted = true;
              }
#endif
            }
          }
        }
        if (strategy & 4 && best_benefit > 0 && flags & FS_SIMPLIFY_NOW)
          break;
        if (flags & FS_USE_WORD2)
          break;
      }
      /* Clear present */
      v1 = word1.buffer();
      for (Word_Length i = 0; i < l1;i++)
        present[v1[i]] = present[maf.inverse(v1[i])] = false;
    }
    if (flags & FS_SIMPLIFY_NOW)
    {
      first_complete = word1_nr;
      if (available[word1_nr])
      {
        retcode = true;
        do_simplification(*available[word1_nr]);
      }
      else
      {
        in_available.assign(word1_nr,1);
        if (word1_nr < first_in_available)
          first_in_available = word1_nr;
      }
    }

    if (flags & FS_USE_WORD1)
      break;
    if (!(flags & FS_USE_WORD2) && top < first_complete)
      top = first_complete;
  }
  if (flags & FS_USE_WORD2 && word2_nr <= first_in_available)
    first_in_available = new_first_in_available;
  return retcode;
}

/**/

bool Tietze_Transformation_Helper::find_lex_changes()
{
  /* Look for even length relators. When we find one find all the
     geodesic equations they give rise to, and apply them to all later
     relators */

  bool retcode = false;
  Ordinal_Word word1(maf.alphabet);
  Ordinal_Word word2(maf.alphabet);
  Ordinal_Word lhs(maf.alphabet);
  Ordinal_Word rhs(maf.alphabet);
  Element_ID next_word2_nr;
  Element_ID first_word2_nr = relators.count();

  remove_available();

  if (abelian)
    return false;

  if (!lex_timestamp)
  {
    lex_checked_timestamp = new unsigned long[relators.count()];
    lex_timestamp = new unsigned long[relators.count()];

    for (Element_ID word_nr = 0; word_nr < relators.count(); word_nr++)
    {
      lex_checked_timestamp[word_nr] = 0;
      lex_timestamp[word_nr] = 1;
    }
    timestamp = 1;
  }

  for (Element_ID word2_nr = 0; word2_nr < relators.count(); word2_nr = next_word2_nr)
  {
    next_word2_nr = word2_nr + 1;
    word2 = *relators.word(word2_nr);
    Word_Length l2 = word2.length();
    maf.container.status(2,1,"Lex: Presentation size is now %d,"
                             FMT_ID ",%zu. Checking geodesics for " FMT_ID "\n",
                         nr_new_generators,
                         relators.count(),relators.total_length(),word2_nr);
    if (!maf.aborting && !(l2 & 1) &&
        (l2 != 2 || word2.value(0) != maf.inverse(word2.value(1))))
    {
      /* This relator potentially gives rise to geodesic reductions */
      bool some_changes = false;
      word2 += *relators.word(word2_nr);
      Word_Length match_length = l2/2;
      unsigned long new_timestamp = timestamp;
      unsigned long was_timestamp = lex_checked_timestamp[word2_nr];
      for (Word_Length i = 0;i < l2;i++)
      {
        lhs = Subword(word2,i,i+match_length);
        rhs = Subword(word2,i+match_length,i+l2);
        for (int pass = 0; pass < 2;pass++)
        {
          if (pass == 0)
            maf.invert(&rhs,rhs);
          else
          {
            rhs = Subword(word2,i+match_length,i+l2);
            maf.invert(&lhs,lhs);
          }
          if (lhs.compare(rhs) > 0)
          {
            /* we have found an LHS. Now look for occurrences of it */
            const Ordinal * lhs_values = lhs.buffer();
            const Ordinal * rhs_values = rhs.buffer();
            if (word2_nr < first_word2_nr)
              first_word2_nr = word2_nr;
            Element_ID next_word1_nr = relators.count()-1;
            while (next_word1_nr > word2_nr)
            {
              Element_ID word1_nr = next_word1_nr;
              next_word1_nr = word1_nr-1;
              if (lex_timestamp[word1_nr] > was_timestamp &&
                  lex_timestamp[word1_nr] <= new_timestamp)
              {
                bool changed = false;
                word1 = *relators.word(word1_nr);
                Total_Length l1 = word1.length();
                Ordinal * dest = word1.buffer();
                for (Total_Length i = 0; i+match_length <= l1;i++)
                  if (dest[i+match_length-1] == lhs_values[match_length-1] &&
                      dest[i] == lhs_values[0] &&
                      !words_differ(dest+i,lhs_values,match_length-1))
                  {
                    retcode = some_changes = changed = true;

                    word_copy(dest+i,rhs_values,match_length);
                    if (i < match_length)
                      i = -1;
                    else
                      i -= match_length;
                  }
                if (changed)
                {
                  remove_relator(*relators.word(word1_nr));
                  if (reduce_and_insert_relator(relators,word1))
                  {
                    if (last_new_relator < first_word2_nr &&
                        !(relators.word(last_new_relator)->length() & 1))
                      first_word2_nr = last_new_relator;
                    if (last_new_relator <= word2_nr)
                    {
                      word2_nr++;
                      next_word2_nr++;
                    }
                    if (last_new_relator < word1_nr)
                      next_word1_nr = word1_nr;
                  }
                  maf.container.status(2,1,"Lex: Presentation size is now %d,"
                                           FMT_ID ",%zu\n",
                                       nr_new_generators,relators.count(),
                                       relators.total_length());
                }
              }
            }
          }
        }
      }
      lex_checked_timestamp[word2_nr] = new_timestamp;
      if (some_changes)
      {
        next_word2_nr = first_word2_nr;
        first_word2_nr = relators.count();
      }
    }
  }

  return retcode;
}

/**/

bool Tietze_Transformation_Helper::simplify()
{
  in_available.change_length(first_in_available = first_complete = relators.count(),BAIV_Zero);
  available = new Simplification *[relators.count()];
  for (Element_ID i = 0; i < first_complete;i++)
    available[i] = 0;

  return find_simplifications(FS_SIMPLIFY_NOW);
}

/**/

MAF * Tietze_Transformation_Helper::polished_presentation(unsigned aa_flags,MAF**catching_maf)
{
  if (!ok)
    return 0;

  maf.container.progress(2,"Creating new rewriting system\n");
  Ordinal ig,g;

  // Create a new MAF object for the polished presentation
  MAF::Options options = maf.options;
  options.dense_rm = true;
  MAF & maf1 = *MAF::create(&maf.container,&options,AT_String,PT_Monoid_Without_Cancellation);
  // and a second MAF object for a non redundant subset of these to live in
  options.dense_rm = false;
  MAF & maf2 = *MAF::create(&maf.container,&options,AT_String,PT_Monoid_Without_Cancellation);

  Ordinal nr_new_generators = 0;
  // create the new alphabet
  for (g = 0; g < nr_generators;g++)
    if (!eliminated_generators[g])
      nr_new_generators++;
  maf1.set_nr_generators(nr_new_generators);
  maf2.set_nr_generators(nr_new_generators);
  for (g = 0; g < nr_generators;g++)
    present[g] = !eliminated_generators[g];
  Ordinal ng = 0;
  for (g = 0; g < nr_generators;g++)
    if (present[g])
    {
      maf1.set_next_generator(maf.alphabet.glyph(g));
      maf2.set_next_generator(maf.alphabet.glyph(g));
      ig = maf.inverse(g);
      if (ig != g)
      {
        maf1.set_next_generator(maf.alphabet.glyph(ig));
        maf2.set_next_generator(maf.alphabet.glyph(ig));
        maf1.set_inverse(ng,ng+1);
        maf2.set_inverse(ng,ng+1);
        ng += 2;
        present[ig] = false;
      }
      else
      {
        maf1.set_inverse(ng,ng);
        maf2.set_inverse(ng,ng);
        ng++;
      }
    }

  String_Buffer sb1;
  String_Buffer sb2;
  Ordinal_Word word(maf.alphabet);
  Ordinal to_do = nr_new_generators;

  if (abelian && !maf.aborting)
  {
    to_do = nr_new_generators;
    for (Ordinal g1 = 0;g1 < nr_generators;g1++)
      if (present[g1])
      {
        maf.container.status(2,1,"Adding abelian axioms (%d of %d to do)\n",to_do--,nr_new_generators);
        for (Ordinal g2 = g1+1;g2 < nr_generators;g2++)
        {
          if (present[g2])
          {
            word.set_length(0);
            word.append(g2);
            word.append(g1);
            word.format(&sb1);
            word.set_length(0);
            word.append(g1);
            word.append(g2);
            word.format(&sb2);
            maf1.add_axiom(sb1.get(),sb2.get(),aa_flags);
          }
        }
      }
  }

  // insert the remaining new axioms
  Element_ID count = relators.count();
  if (nr_new_generators > 50)
    maf1.aborting = true;
  MAF * save_catcher = 0;
  if (catching_maf)
  {
    save_catcher = *catching_maf;
    *catching_maf = &maf1;
  }

  for (Element_ID word_nr = 0;word_nr < count;word_nr++)
  {
    const Word * relator = relators.word(word_nr);
    maf.container.status(2,1,"Processing relators (" FMT_ID " of " FMT_ID 
                             " to do)\n",count-word_nr,count);
    if (maf.aborting || maf1.aborting)
    {
      if (aa_flags & AA_ELIMINATE)
      {
        maf.aborting = maf1.aborting = false;
        aa_flags &= ~AA_ELIMINATE;
      }
      else
      {
        maf.aborting = maf1.aborting = true;
        aa_flags &= ~(AA_ELIMINATE|AA_DEDUCE);
        if (catching_maf && *catching_maf)
        {
          signal(SIGINT,SIG_DFL);
          signal(SIGTERM,SIG_DFL);
          *catching_maf = 0;
        }
      }
    }
    relator->format(&sb1);
    maf1.add_axiom(sb1.get(),"",relator->length() < long_length ? aa_flags & ~AA_ELIMINATE : aa_flags);
  }

  if (catching_maf)
    *catching_maf = save_catcher;

  for (g = 0;g < nr_new_generators;g++)
    if ((ig = maf1.inverse(g)) != INVALID_SYMBOL)
      maf2.set_inverse(g,ig,0);
  for (const Linked_Packed_Equation * axiom = maf1.first_axiom();axiom;
       axiom = axiom->get_next())
  {
    Simple_Equation se(maf1.alphabet,*axiom);
    maf2.add_axiom(se.lhs_word,se.rhs_word,0);
  }
  delete &maf1;
  return &maf2;
}

