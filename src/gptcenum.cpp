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


// $Log: gptcenum.cpp $
// Revision 1.3  2010/06/10 17:56:30Z  Alun
// Fixes to ensure clean compilation with g++
// Revision 1.2  2010/06/10 13:57:21Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/16 19:34:54Z  Alun
// New file.
//

#include <time.h>
#include "fsa.h"
#include "maf.h"
#include "container.h"
#include "maf_tc.h"
#include "maf_so.h"
#include "mafctype.h"

/**/

const unsigned WANT_TABLE = 1u;
const unsigned WANT_RS = 2u;
const unsigned WANT_GAP_PRES = 4u;
const unsigned WANT_WA = 8u;

int main(int argc,char ** argv);
  static int inner(MAF & maf,TC_Enumeration_Options & eo,unsigned flags);

int main(int argc,char ** argv)
{
  int i = 1;
  char * group_filename = 0;
  char * sub_suffix = 0;
  bool cosets = false;
  Container & container = *MAF::create_container();
  Standard_Options so(container,0);
  bool bad_usage = false;
  unsigned flags = 0;
  TC_Enumeration_Options eo;
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
      else if (arg.is_equal("-pres"))
      {
        flags |= WANT_GAP_PRES|WANT_RS;
        i++;
      }
      else if (arg.is_equal("-rs"))
      {
        flags |= WANT_RS;
        i++;
      }
      else if (arg.is_equal("-table"))
      {
        flags |= WANT_TABLE;
        i++;
      }
      else if (arg.is_equal("-wa"))
      {
        flags |= WANT_WA;
        i++;
      }
      else if (arg.is_equal("-strategy"))
      {
        static const char * strategy_name[] =
        {
          "default",
          "sims:1",
          "sims:2",
          "sims:3",
          "sims:4",
          "sims:5",
          "sims:6",
          "sims:7",
          "sims:8",
          "sims:9",
          "sims:10",
          "def",
          "easy",
          "fel:0",
          "fel:1",
          "hard",
          "hlt",
          "pure_c",
          "long",
          "lucky",
          0
        };
        bool found = false;
        String arg = argv[i+1];
        String_Length mod_pos = 0;
        bool modified = arg.find('/',&mod_pos);
        Letter * sname = modified ? arg.clone_substring(0,mod_pos) : arg.clone();
        if (*sname)
        {
          unsigned local;
          for (local = 0;strategy_name[local]!=0;local++)
            if (so.present(sname,strategy_name[local]))
            {
              found = true;
              break;
            }
          if (found)
            eo.set_strategy(TC_Enumeration_Strategy(local));
          else
          {
            cprintf("Bad strategy name %s specified for -strategy\n",sname);
            bad_usage = true;
          }
        }
        else
          eo.empty();
        delete sname;
        if (modified)
        {
          bool in_phase_length = false;
          bool negative = false;
          bool seen_digit = false;
          bool exceeded = false;
          bool expect_digit = true;
          bool invalid = false;
          TC_Enumeration_Options::Enumeration_Phase * phase = eo.phases;
          Element_ID phase_nr = 0;
          for (const Letter * s = arg + mod_pos+1;;s++)
          {
            if (*s == '/' || !*s)
            {
              if (invalid)
              {
                cprintf("Invalid phase length specification for phase "
                        FMT_ID "\n",phase_nr);
                bad_usage = true;
              }
              if (exceeded)
              {
                cprintf("Maximum phase length exceeded in phase "
                        FMT_ID "\n",phase_nr);
                bad_usage = true;
              }

              if (negative)
              {
                phase->phase_length = -phase->phase_length;
                if (phase->phase_length != -1)
                  container.error_output("Invalid phase length " FMT_ID
                                         " for phase " FMT_ID " in strategy"
                                         " string\n",phase->phase_length,
                                         phase_nr);

              }
              if (!*s)
                break;
              if (++phase_nr >= Element_Count(sizeof(eo.phases)/sizeof(eo.phases[0])))
              {
                cprintf("Maximum number of phases exceeded!\n");
                bad_usage = true;
                break;
              }

              phase++;
              if (phase_nr >= eo.nr_phases)
              {
                eo.nr_phases = phase_nr + 1;
                phase->phase_flags = 0;
                phase->phase_length = -1;
              }
              invalid = exceeded = in_phase_length = negative = seen_digit = false;
              expect_digit = true;
            }
            else if (!in_phase_length)
            {
              Letter c = char_classification.to_upper_case(*s);
              switch (c)
              {
                case ' ':
                  break;
                case ':':
                  in_phase_length = true;
                  phase->phase_length = 0;
                  break;
                case '*':
                  eo.loop_phase = phase_nr;
                  break;
                case '2':
                  phase->phase_flags |= TCPF_APPLY_RELATORS|TCPF_PERMUTATION_TYPE_2;
                  break;
                case '3':
                  phase->phase_flags |= TCPF_APPLY_RELATORS|TCPF_PERMUTATION_TYPE_3;
                  break;
                case 'A':
                  phase->phase_flags &= ~(TCPF_MAF_DEFINITION_ORDER|
                                          TCPF_SIMS_DEFINITION_ORDER);
                  phase->phase_flags |= TCPF_ACE_DEFINITION_ORDER;
                  break;
                case 'B':
                  phase->phase_flags &= ~(TCPF_ACE_DEFINITION_ORDER|
                                        TCPF_SIMS_DEFINITION_ORDER);
                  phase->phase_flags |= TCPF_MAF_DEFINITION_ORDER;
                  break;
                case 'C':
                  phase->phase_flags |= TCPF_CHECK_CONSEQUENCES;
                  break;
                case 'G':
                  phase->phase_flags |= TCPF_APPLY_RELATORS|TCPF_PERMUTE_G;
                  break;
                case 'H':
                  phase->phase_flags |= TCPF_APPLY_RELATORS;
                  break;
                case 'L':
                  phase->phase_flags |= TCPF_APPLY_RELATORS|TCPF_LOOK_AHEAD_ON;
                  break;
                case 'M':
                  phase->phase_flags |= TCPF_APPLY_RELATORS|TCPF_PERMUTE_G|TCPF_PERMUTE_N;
                  break;
                case 'N':
                  phase->phase_flags |= TCPF_APPLY_RELATORS|TCPF_PERMUTE_N;
                  break;
                case 'P':
                  phase->phase_flags |= TCPF_APPLY_RELATORS|TCPF_PRE_FILL_ROW;
                  break;
                case 'Q':
                  phase->phase_flags |= TCPF_CHECK_CONSEQUENCES|TCPF_GAP_STRATEGY_1|TCPF_GAP_QUEUE_ONLY;
                  phase->phase_flags &= ~(TCPF_GAP_REQUIRE_LOW|TCPF_GAP_USE_FILL_FACTOR);
                  break;
                case 'R':
                  phase->phase_flags |= TCPF_CHECK_CONSEQUENCES|TCPF_GAP_STRATEGY_1|TCPF_GAP_REQUIRE_LOW;
                  break;
                case 'S':
                  phase->phase_flags |= TCPF_CHECK_CONSEQUENCES|TCPF_GAP_STRATEGY_1;
                  phase->phase_flags &= ~(TCPF_GAP_QUEUE_ONLY|TCPF_GAP_REQUIRE_LOW|TCPF_GAP_USE_FILL_FACTOR);
                  break;
                case 'T':
                  phase->phase_flags &= ~(TCPF_ACE_DEFINITION_ORDER|
                                          TCPF_MAF_DEFINITION_ORDER);
                  phase->phase_flags |= TCPF_SIMS_DEFINITION_ORDER;
                  break;
                case 'U':
                  phase->phase_flags |= TCPF_GAP_STRATEGY_1|TCPF_GAP_USE_FILL_FACTOR|TCPF_CHECK_CONSEQUENCES;
                  break;
                case 'V':
                  phase->phase_flags |= TCPF_VARY_PHASE;
                  break;
                case 'W':
                  phase->phase_flags |= TCPF_RANDOM_PHASE;
                  break;
                case 'X':
                  phase->phase_flags |= TCPF_GAP_STRATEGY_2|TCPF_CHECK_CONSEQUENCES;
                  break;
               default:
                 cprintf("Unrecognised option '%c' found in -strategy string\n"
                         "(parsing %s)\n"
                         "(at      %*s)\n",
                         c,argv[i+1],int(s - arg+1),"^");
                 bad_usage = true;
                 break;
              }
            }
            else
            {
              switch (*s)
              {
                case ' ':
                  if (seen_digit)
                    expect_digit = false;
                  break;
                case '-':
                  if (!seen_digit && !negative)
                    negative = true;
                  else
                    invalid = true;
                  break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                  if (!expect_digit)
                    invalid = true;
                  else if (phase->phase_length <= (MAX_STATE_COUNT-(*s-'0'))/10)
                  {
                    phase->phase_length = 10*phase->phase_length+*s-'0';
                    seen_digit = true;
                  }
                  else
                    exceeded = true;
                  break;
                case 'k':
                  expect_digit = false;
                  if (!seen_digit)
                    invalid = true;
                  if (phase->phase_length <= MAX_STATE_COUNT/1000)
                    phase->phase_length = 1000*phase->phase_length;
                  else
                    exceeded = true;
                  break;
                case 'K':
                  expect_digit = false;
                  if (!seen_digit)
                    invalid = true;
                  if (phase->phase_length <= MAX_STATE_COUNT/1024)
                    phase->phase_length = 1024*phase->phase_length;
                  else
                    exceeded = true;
                  break;
                case 'm':
                  expect_digit = false;
                  if (!seen_digit)
                    invalid = true;
                  if (phase->phase_length <= MAX_STATE_COUNT/1000000)
                    phase->phase_length = 1000000*phase->phase_length;
                  else
                    exceeded = true;
                  break;
                case 'M':
                  expect_digit = false;
                  if (!seen_digit)
                    invalid = true;
                  if (phase->phase_length <= MAX_STATE_COUNT/(1024*1024))
                    phase->phase_length = (1024*1024)*phase->phase_length;
                  else
                    exceeded = true;
                  break;
                case 'r':
                case 'R':
                  if (seen_digit)
                    expect_digit = false;
                  phase->phase_flags &= ~TCPF_LENGTH_BY_DEFINITIONS;
                  break;
                case 'd':
                case 'D':
                  if (seen_digit)
                    expect_digit = false;
                  phase->phase_flags |= TCPF_LENGTH_BY_DEFINITIONS;
                  break;
               default:
                 cprintf("Unrecognised option '%c' found in -strategy string\n"
                         "(parsing %s)\n"
                         "(at      %*s)\n",
                         *s,argv[i+1],int(s - arg+1),"^");
                 bad_usage = true;
                 break;
              }
            }
          }
        }
        i += 2;
      }
      else if (so.present(arg,"-compression_mode"))
      {
        unsigned local = 0;
        so.parse_natural(&local,argv[i+1],TCCM_Last,arg);
        eo.compaction_mode = TC_Compaction_Mode(local);
        i += 2;
      }
      else if (so.present(arg,"-max_hole_percentage"))
      {
        so.parse_natural(&eo.max_hole_percentage,argv[i+1],100,arg);
        i += 2;
      }
      else if (so.present(arg,"-fill_factor"))
      {
        so.parse_natural(&eo.fill_factor,argv[i+1],0,arg);
        i += 2;
      }
      else if (so.present(arg,"-as_is"))
      {
        eo.cr_mode = TCCR_As_Is;
        eo.ep_mode = TCEP_As_Is;
        i++;
      }
      else if (so.present(arg,"-cr_mode"))
      {
        unsigned local = 0;
        so.parse_natural(&local,argv[i+1],TCCR_Last,arg);
        eo.cr_mode = TC_Cyclic_Reduction(local);
        i += 2;
      }
      else if (so.present(arg,"-ep_mode"))
      {
        unsigned local = 0;
        so.parse_natural(&local,argv[i+1],TCEP_Last,arg);
        eo.ep_mode = TC_Equivalent_Presentation(local);
        i += 2;
      }
      else if (so.present(arg,"-build_standardised"))
      {
        eo.build_standardised = true;
        i++;
      }
      else if (so.present(arg,"-max_cosets"))
      {
        so.parse_natural(&eo.max_cosets,argv[i+1],0,arg);
        i += 2;
      }
      else if (so.present(arg,"-queued_definitions"))
      {
        so.parse_natural(&eo.queued_definitions,argv[i+1],0,arg);
        i += 2;
      }
      else if (so.present(arg,"-work_space"))
      {
        Unsigned_Long_Long s = eo.work_space;
        so.parse_natural(&s,argv[i+1],0,arg);
        eo.work_space = s;
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
    else if (cosets && sub_suffix == 0)
    {
      sub_suffix = argv[i];
      i++;
    }
    else
      bad_usage = true;
  }
  if (i > argc)
    bad_usage = true;
  int exit_code = 1;
  if (group_filename && !bad_usage)
  {
    time_t now = time(0);
    MAF & maf = * MAF::create_from_input(cosets,group_filename,sub_suffix,&container,
                                         CFI_DEFAULT|CFI_ALLOW_CREATE);
    exit_code = inner(maf,eo,flags);
    container.progress(1,"Elapsed time %ld\n",long(time(0) - now));
  }
  else
  {
    cprintf("Usage:\n"
            "gptcenums [loglevel] [options] groupname"
            " [-cos [subsuffix | cossuffix]]\n"
            "where groupname is a GASP rewriting system for a group, and, if"
            " the -cos option\nis specified, groupname.subsuffix is a"
            " substructure file for the group.\n"
            "This program performs coset enumeration, over either the"
            " subgroup specified in\ngroupname.subsuffix, if the -cos option"
            " is specified, or the trivial subgroup otherwise.\n"
            "Valid otions include:\n"
            "-table : output the coset table to groupname.cosets or"
            " groupname.cossuffix.cosets\n"
            "-rs : (for coset systems only) compute a subgroup presentation on"
            " the Schreier\ngenerators. Output is to groupname.subsuffix.rws"
            " or groupname.subsuffix.pres if\n-pres option is specified.\n"
            "-work_space n : sets the maximum size of the table in bytes.\n"
            "-max_cosets n :  sets the maximum number of rows in the table.\n"
            "If neither option is specified MAF will try to resize the table"
            " as required.\n"
            "-strategy string : sets the strategy. This can contain the name of"
            " a pre-defined\nstrategy: sims:1, sims:2,... sims:10, fel:0, fel:1,"
            " easy, hlt, hard, long,\nlucky, pure_c, def, or default (the last"
            " 2 are not the same). Alternatively\nyou can specify a custom"
            " strategy with up to 8 phases.\n"
            "Each phase descriptor start with /. The strategy for a phase is"
            " set as follows:\n"
            "* marks the phase which is returned to when all phases have been"
            " executed\nH denotes an HLT type phase, M denotes a CHLT phase,\n"
            "G,N partial CHLT phases (only useful for normal closure coset"
            " systems)\nIf none of H,M,G,N are specified the phase is \"Felsch\""
            " type.\n2,3 select alternate orderings of the conjugated relators"
            " for CHLT type phases\nC enables the deduction stack for HLT/CHLT"
            " type phases.\nL enables \"look-ahead\" for HLT type phases.\n"
            "P causes an HLT type row to be filled before relators are applied.\n"
            "S enables short gap filling, Q enables queued short gap filling.\n"
            "U limits short gap filling using the fill factor.\n"
            "R limits short gap filling by requiring the scan to pass through"
            " a \"low\" row\n(one that is already filled).\n"
            "X enables \"long\" gap filling, and completes at most one scan per"
            " iteration,\nin a most nearly complete position; relators are"
            " considered round-robin fashion.\n"
            "A selects ACE definition order (back filled).\n"
            "B selects MAF definition order (balanced filling).\n"
            "T selects \"traditional\" definition order (forward filled).\n"
            "V causes some options to be randomly altered. Affects 23ABPQRTU\n"
            "W causes most options to be randomly altered. C option is always on\n"
            ": introduces the \"duration\" part of the phase. This consists of"
            " a number,\nfollowed by a D (duration measured by number of cosets"
            " defined), or R (duration\nmeasured by number of rows processed)."
            " If the number is -1 then the phase lasts\nfor ever, or, if"
            " look-ahead is enabled and there are more phases, until a\n"
            "look-ahead is performed.\n"
            "One can also combine these options with pre-defined strategies:\n"
            "e.g. \"-strategy sims:1/a\" would make modify the sims:1 strategy"
            " to behave as\nit would in ACE. (By default the \"sims\""
            " strategies exactly match Sims' book.)\n"
            "Another example: -strategy /mcpb:1r/bsx. All cyclic conjugates of"
            " the relators\nare applied to a pre-filled row 1 using balanced"
            " scans with a deduction stack.\nThen an unlimited Felsch phase is"
            " run which does unlimited immediate short-gap\nfilling, and applies"
            " one relator per filled row in round-robin fashion.\n"
            "Other options:\n"
            "-fill_factor n sets the fill-factor used by U limited short gap"
            " filling.\n"
            "-queued_definitions n sets the size of the queue for Q type"
            " short gap filling.\n"
            "-build_standardised forces the table to be built so in BFS order.\n"
            "-cr_mode n or -as_is : specifies how the input file is pre-processed."
            " Specify\n-cr_mode 0 or -as_is to use the presentation as is."
            " Specify -cr_mode 1 to use\ncyclically and freely reduced relators,"
            " where the conjugate picked is the first\nin the reduction "
            "ordering. Specify -cr_mode 2 to similarly reduce relators, but\n"
            "this time picking the conjugate which gives the best equation when"
            " right-\nbalanced. -ep_mode n : (n=0,1..5), selects further "
            "processing of the relators.\n0=no further processing, 1=conjugated"
            " to give fewest cosets when applied to \ncoset 1,"
            " 2=conjugated to give most cosets when applied to coset 1.\n"
            "3,4,5 are similar to 0,1,2, but the relators are applied in reverse"
            " order.\n"
            "-compression_mode n : n=0..3 determines what happens to the"
            " \"deduction stack\" if\nthe table is compressed mid-enumeration.\n"
            "-compression_mode 0 : discard deduction stack. -compression_mode 1"
            " : preserve\nstack renumbering as need be, which may leave"
            " duplicate entries. modes 2,3\nrebuild by column or row, removing"
            " duplicates. In any case the enumerator\ntries to avoid invoking"
            " compression when the deduction stack is not empty.\n"
            "-max_hole_percentage n : selects maximum percentage of holes. If"
            " this is set to\n100, compression will be disabled.\n"
            "-allow_holes n : sets a number of holes which will be tolerated if"
            " the\nenumeration is about to fail due to lack of space, or the"
            " table is going to be\nresized. Setting this to a high value will"
            " prevent fruitless \"look-aheads\"\nwhen HLT+Lookahead mode is"
            " used.\n");
    so.usage();
  }
  delete &container;
  return exit_code;
}

/**/

static int inner(MAF & maf,TC_Enumeration_Options & eo,unsigned flags)
{
  int exit_code = 1;
  FSA * coset_table = 0;
  Language_Size retcode = maf.enumerate_cosets(flags & WANT_RS+WANT_TABLE+WANT_WA ? &coset_table : 0,eo);
  if (retcode != 0)
  {
    exit_code = 0;
    maf.container.progress(1,"The subgroup index is " FMT_LS "\n", retcode);
    if (maf.options.log_level == 0)
      maf.container.result(FMT_LS "\n",retcode);
    if (flags & WANT_TABLE)
      maf.save_fsa(coset_table,GAT_Coset_Table);
    if (flags & WANT_WA)
    {
      FSA_Simple * wa = maf.build_acceptor_from_coset_table(*coset_table);
      if (wa)
      {
        maf.save_fsa(wa,GAT_WA);
        delete wa;
      }
    }
    if (flags & WANT_RS+WANT_GAP_PRES)
    {
      const Presentation_Data & pd = maf.properties();
      if (pd.is_coset_system)
      {
        maf.container.progress(1,"Computing subgroup presentation\n");
        MAF * maf_sub = maf.rs_presentation(*coset_table);
        if (maf_sub != 0)
        {
          String_Buffer sb;
          String filename = sb.make_filename("",
                                             pd.subgroup_filename !=0 ?
                                             pd.subgroup_filename.string() :
                                             pd.filename.string(),
                                             flags & WANT_GAP_PRES ? ".pres" : ".rws");

          Output_Stream * os = maf.container.open_text_output_file(filename);

          if (flags & WANT_GAP_PRES)
          {
            if (maf_sub->output_gap_presentation(os,true))
              exit_code = 0;
          }
          else
          {
            maf_sub->print(os);
            exit_code = 0;
          }
          maf.container.close_output_file(os);
          delete maf_sub;
        }
      }
    }
    if (coset_table)
      delete coset_table;
  }
  delete &maf;
  return exit_code;
}

