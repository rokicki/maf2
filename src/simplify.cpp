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


// $Log: simplify.cpp $
// Revision 1.12  2010/07/08 14:53:52Z  Alun
// Now follows standard scheme for output files used by utilities that output
// only one file
// Revision 1.11  2010/06/10 16:58:06Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.10  2010/06/10 13:57:49Z  Alun
// All tabs removed again
// Revision 1.9  2010/06/02 09:10:52Z  Alun
// Almost all the real code moved to tietze.cpp and now inside MAF library
// Argument prseing changed not to need strcmp(). Help and other messages
// changed. -overwrite option added since typically simplify() gets executed
// multiple times
// Revision 1.8  2009/11/11 00:16:43Z  Alun
// Responds better to signal
// Revision 1.7  2009/10/13 20:20:15Z  Alun
// Compatibility of FSA utilities with KBMAG improved.
// Compatibility programs produce fewer FSA KBMAG would not (if any)
// Revision 1.6  2009/09/13 18:42:46Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.5  2009/08/25 08:09:08Z  Alun
// Usage message improved. Comment corrected
// Revision 1.4  2008/12/30 11:23:16Z  Alun
// Support for abelianisation added + many changes to improve behaviour on
// really horrible presentations
// Revision 1.4  2008/11/02 18:57:13Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.3  2008/10/01 06:36:21Z  Alun
// Changed type of presentation requested to suppress warnings about non-invertible generators
// Revision 1.2  2008/09/30 09:25:12Z  Alun
// Final version built using Indexer.
//

#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include "maf.h"
#include "container.h"
#include "maf_so.h"
#include "alphabet.h"
#include "mafword.h"
#include "fsa.h"
#include "equation.h"
#include "bitarray.h"
#include "tietze.h"

static MAF * global_maf;

void signal_handler(int /*signal_nr*/)
{
  if (global_maf)
    global_maf->give_up();
  signal(SIGINT,signal_handler);
  signal(SIGTERM,signal_handler);
}

/**/

int main(int argc,char ** argv)
{
  int i = 1;
  String group_filename = 0;
  String output_filename = 0;
  Container * container = MAF::create_container();
  Standard_Options so(*container,SO_GPUTIL|SO_STDOUT);
  bool bad_usage = false;
  bool overwrite = false;
  FSA_Simple * fsa = 0;
  unsigned strategy = 0;
  unsigned aa_flags = AA_DEDUCE_INVERSE|AA_ADD_TO_RM|AA_DEDUCE|AA_POLISH;
  unsigned tth_flags = 0;
  Ordinal max_seen = 0;
  Word_Length max_elimination = 0;
  Word_Length long_length = 0;
#define cprintf container->error_output
  MAF::Options options;

  options.conjugation = 1;
  options.partial_reductions = 0;
  options.special_overlaps = 0;
  options.no_differences = true;

  while (i < argc && !bad_usage)
  {
    if (argv[i][0] == '-')
    {
      String arg = argv[i];
      if (arg.is_equal("-abelian"))
      {
        tth_flags |= TTH_ABELIANISE;
        i++;
      }
      else if (so.present(arg,"-best_equations"))
      {
        tth_flags |= TTH_BEST_EQUATION_RELATORS;
        i++;
      }
      else if (arg.is_equal("-check"))
      {
        fsa = FSA_Factory::create(argv[i+1],container);
        i += 2;
      }
      else if (arg.is_equal("-conjugation"))
      {
        so.parse_natural(&options.conjugation,argv[i+1],4,arg);
        i += 2;
      }
      else if (arg.is_equal("-kb"))
      {
        aa_flags |= AA_ELIMINATE;
        i++;
      }
      else if (so.present(arg,"-keep_generators"))
      {
        tth_flags |= TTH_KEEP_GENERATORS;
        i++;
      }
      else if (so.present(arg,"-keep_going"))
      {
        tth_flags |= TTH_KEEP_GOING;
        i++;
      }
      else if (so.present(arg,"-long_length"))
      {
        long_length = Word_Length(atoi(argv[i+1]));
        i += 2;
      }
      else if (so.present(arg,"-max_elimination"))
      {
        max_elimination = Word_Length(atoi(argv[i+1]));
        i += 2;
      }
      else if (so.present(arg,"-max_seen"))
      {
        max_seen = Ordinal(atoi(argv[i+1]));
        i += 2;
      }
      else if (so.present(arg,"-no_simplify"))
      {
        tth_flags |= TTH_NO_SIMPLIFY;
        i++;
      }
      else if (so.present(arg,"-overwrite"))
      {
        overwrite = true;
        i++;
      }
      else if (so.present(arg,"-partial_reductions"))
      {
        so.parse_natural(&options.partial_reductions,argv[i+1],2,arg);
        i += 2;
      }
      else if (so.present(arg,"-pool_above"))
      {
        so.parse_total_length(&options.pool_above,argv[i+1],0,arg);
        i += 2;
      }
      else if (arg.is_equal("-raw"))
      {
        aa_flags = AA_DEDUCE_INVERSE;
        i++;
      }
      else if (arg.is_equal("-strategy"))
      {
        strategy = atoi(argv[i+1]);
        i += 2;
      }
      else if (!so.recognised(argv,i))
        bad_usage = true;
    }
    else
    {
      if (group_filename == 0)
        group_filename = argv[i];
      else if (output_filename == 0)
        output_filename = argv[i];
      else
        bad_usage = true;
      i++;
    }
  }

  if (!bad_usage && group_filename &&
      !(output_filename && (overwrite || so.use_stdout)))
  {
    time_t now = time(0);
    if (so.use_stdout)
      container->set_gap_stdout(true);
    MAF & maf = * MAF::create_from_rws(group_filename,container,CFI_DEFAULT,&options);
    if (fsa)
      maf.set_validation_fsa(fsa);
    global_maf = &maf;
    signal(SIGINT,signal_handler);
    signal(SIGTERM,signal_handler);
    Tietze_Transformation_Helper tth(maf,strategy,tth_flags,max_seen,max_elimination,long_length);
    tth.polish();
    MAF * new_maf = tth.polished_presentation(aa_flags,&global_maf);
    if (new_maf)
    {
      String_Buffer sb;
      if (!so.use_stdout)
        output_filename = sb.make_destination(output_filename,group_filename,
                                              overwrite ? "" : ".simplify");
      new_maf->save(output_filename);
      delete new_maf;
    }
    delete &maf;
    cprintf("Time %ld\n",long(time(0) - now));
  }
  else
  {
    cprintf("Usage:\n"
            "simplify [logevel] [-strategy n] [-abelian] [-keep_generators]"
            " [-no_simplify]\n[-kb] [-keep_going] [-long_length n]"
            " [-max_elimination n] [-max_seen n] [-raw]\ninput_file [-overwrite | -o | output_file]\n"
            "where input_file contains a GASP rewriting system for a group.\n"
            "simplify creates a new RWS for the same group with some "
            "redundant generators\nand axioms eliminated.\n"
            "-strategy n, where n is a number from 0 to 15, influences the"
            " order in which\ngenerators and axioms are eliminated."
            " 0 or 1 is usually best, 4-7 often poor.\n"
            "-keep_generators tells simplify not to eliminate generators."
            " -max_elimination n\nlimits elimination of generators to relators"
            " of length n or below. -max_seen n\nlimits elimination of"
            " generators to relators containing at most n distinct\ngenerators"
            " but with no limit on the length of the relator.\n"
            "-no_simplify tells simplify not to try to eliminate axioms, except"
            " when\neliminating an unnecessary generator.\n"
            "-overwrite causes simplify to overwrite the original input file"
            " instead of\ngenerating a new input file.\n"
            "-abelian tells simplify to compute the presentation of the abelian"
            " quotient\nof the presentation.\n"
            "-best_equation_relators causes simplify to keep the cyclic"
            " conjugate that gives\nthe best equation when right balanced."
            " The default is to keep the least\nconjugate in the word-ordering."
            " The default usually works better.\n"
            "-raw speeds simplify up by not filtering the axioms through a"
            " MAF rewriting system.\n"
            "-keep_going tells simplify to eliminate as many generators as"
            " possible, even if\nthe size of the presentation increases a lot.\n"
            "-kb tells MAF to perform Knuth-Bendix expansion to eliminate more"
            " axioms. In\nthis case -long_length n limits this options to"
            " axioms of length <n.\n");
    so.usage(".simplify");
    delete container;
    return 1;
  }

  delete container;
  return 0;
}

#undef cprintf
#define cprintf maf.container.error_output

