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


/*
$Log: maf_tc.h $
Revision 1.2  2010/06/10 13:58:13Z  Alun
All tabs removed again
Revision 1.1  2010/04/26 22:45:40Z  Alun
New file.
*/
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

// Options structure for Todd Coxeter coset enumerator
// Flags to indicate how coset enumeration phase should be done
const unsigned TCPF_APPLY_RELATORS = 1;
const unsigned TCPF_PRE_FILL_ROW = 2;
const unsigned TCPF_BALANCE_SCAN = 4;
const unsigned TCPF_BACK_SCAN = 8;
const unsigned TCPF_CHECK_CONSEQUENCES = 16;
const unsigned TCPF_PERMUTE_G = 32;
const unsigned TCPF_PERMUTE_N = 64;
const unsigned TCPF_PERMUTATION_TYPE_2 = 128;
const unsigned TCPF_PERMUTATION_TYPE_3 = 256;
const unsigned TCPF_GAP_STRATEGY_1 = 512; // fill gaps of length 1
const unsigned TCPF_GAP_USE_FILL_FACTOR = 1024; // ACE style gap fill limiting
const unsigned TCPF_GAP_REQUIRE_LOW = 2048; /* only allow gap filling where
                                              the relator passes through a
                                              completed row */
const unsigned TCPF_GAP_QUEUE_ONLY = 4096; /* delay gap filling */
const unsigned TCPF_GAP_STRATEGY_2 = 8192; /* apply one relator per row in the
                                              best position in the table for
                                              that relator*/
const unsigned TCPF_LOOK_AHEAD_ON = 16384;
const unsigned TCPF_LENGTH_BY_DEFINITIONS = 32768; /* measure phase length by
                                                      number of definitions
                                                      rather than number of
                                                      rows processed */
const unsigned TCPF_RANDOM_PHASE = 65536;
const unsigned TCPF_VARY_PHASE = 131072;

const unsigned TCPF_SIMS_DEFINITION_ORDER = 0;
const unsigned TCPF_ACE_DEFINITION_ORDER = TCPF_BACK_SCAN;
const unsigned TCPF_MAF_DEFINITION_ORDER = TCPF_BALANCE_SCAN;

const unsigned TCPF_HLT_PHASE = TCPF_APPLY_RELATORS;
const unsigned TCPF_HLTL_PHASE = TCPF_HLT_PHASE|TCPF_LOOK_AHEAD_ON;
const unsigned TCPF_HLTD_PHASE = TCPF_APPLY_RELATORS+TCPF_CHECK_CONSEQUENCES;

const unsigned TCPF_HLTA_PHASE = TCPF_APPLY_RELATORS|TCPF_ACE_DEFINITION_ORDER;
const unsigned TCPF_HLTAL_PHASE = TCPF_HLTA_PHASE|TCPF_LOOK_AHEAD_ON;
const unsigned TCPF_HLTAD_PHASE = TCPF_HLTA_PHASE|TCPF_CHECK_CONSEQUENCES;

const unsigned TCPF_HLTM_PHASE = TCPF_APPLY_RELATORS|TCPF_MAF_DEFINITION_ORDER;
const unsigned TCPF_HLTML_PHASE = TCPF_HLTM_PHASE|TCPF_LOOK_AHEAD_ON;
const unsigned TCPF_HLTMD_PHASE = TCPF_HLTM_PHASE+TCPF_CHECK_CONSEQUENCES;

const unsigned TCPF_C1HLT_PHASE = TCPF_APPLY_RELATORS+TCPF_PERMUTE_G+TCPF_PERMUTE_N;
const unsigned TCPF_C1HLTD_PHASE = TCPF_C1HLT_PHASE+TCPF_CHECK_CONSEQUENCES;

const unsigned TCPF_FELSCH_PHASE = TCPF_CHECK_CONSEQUENCES;

const unsigned TCPF_ACE_GAPS = TCPF_GAP_STRATEGY_1|TCPF_GAP_USE_FILL_FACTOR|TCPF_GAP_QUEUE_ONLY|TCPF_ACE_DEFINITION_ORDER;
const unsigned TCPF_MAF_GAPS = TCPF_GAP_STRATEGY_1|TCPF_GAP_QUEUE_ONLY|TCPF_MAF_DEFINITION_ORDER;

const unsigned TCPF_LONG_PHASE = TCPF_CHECK_CONSEQUENCES|TCPF_GAP_STRATEGY_2|TCPF_MAF_GAPS;


/* TC_Compaction_Mode controls how MAF deals with deductions after cosets
   are renumbered to remove eliminated cosets */
enum TC_Compaction_Mode
{
  TCCM_Discard, /* the deductions are discarded and a full scan is performed */
  TCCM_Special_Column, /* MAF uses one of the columns to renumber the deductions,
                          but keeps them in their original order. Some duplicate
                          entries may exist */
  TCCM_Rebuild_By_Row, /* MAF renumbers the deductions and eliminates any duplicates.
                          The deductions are now ordered by coset number then
                          generator */
  TCCM_Rebuild_By_Column, /* As before but deductions are now ordered by generator
                            then coset number */
  TCCM_Last = TCCM_Rebuild_By_Column
};

enum TC_Cyclic_Reduction
{
  TCCR_As_Is, /* The relators implied by the original input file are used directly */
  TCCR_Least_Relator, /* The relators are cyclically and freely reduced, and
                         each relator is replaced by the cyclic conjugate which is least in
                         the reduction ordering */
  TCCR_Best_Equation_Relator, /* The relators are cyclically and freely reduced,
                                 and each relator is replaced by the cyclic
                                 conjugate uV where u=v is the equation with
                                 the least u and v in the reduction ordering */
  TCCR_Last = TCCR_Best_Equation_Relator
};

enum TC_Equivalent_Presentation
{
  TCEP_As_Is, /* No further modifications */
  TCEP_Fewest_Definitions,/* The relators are applied to coset 1 in turn, and
                             for each new relator the cyclic conjugate which
                             requires fewest new cosets is used */
  TCEP_Most_Definitions, /* The relators are applied to coset 1 in turn, and
                             for each new relator the cyclic conjugate which
                             requires most new cosets is used */

  TCEP_Reverse, /* The order of the relators is reversed */
  TCEP_Reverse_Fewest_Definitions, /* TCEP_Reverse followed by TCEP_Fewest Definitions */
  TCEP_Reverse_Most_Definitions, /* TCEP_Reverse followed by TCEP_Most Definitions */
  TCEP_Last = TCEP_Reverse_Most_Definitions
};

enum TC_Enumeration_Strategy
{
  TCES_Default,
  TCES_Sims_1,
  TCES_HLT = TCES_Sims_1,
  TCES_Sims_2,
  TCES_Sims_3,
  TCES_HLTD = TCES_Sims_3,
  TCES_Sims_4,
  TCES_Sims_5,
  TCES_CHLT = TCES_Sims_5,
  TCES_Sims_6,
  TCES_Sims_7,
  TCES_CHLTD = TCES_Sims_7,
  TCES_Sims_8,
  TCES_Sims_9,
  TCES_Sims_10,
  TCES_ACE_Default,
  TCES_ACE_Easy,
  TCES_ACE_Felsch_0, /* ACE fel:0 */
  TCES_ACE_Felsch_1, /* ACE fel:1 */
  TCES_ACE_Hard,
  TCES_HLT_Lookahead,
  TCES_ACE_Pure_C,
  TCES_Long,
  TCES_Lucky,
  TCES_Last = TCES_Lucky
};

struct TC_Enumeration_Options
{
   struct Enumeration_Phase
   {
     unsigned phase_flags;
     Element_Count phase_length;
   };
  public:
    Enumeration_Phase phases[8];
    size_t work_space;
    Element_Count max_cosets;
    Element_Count nr_phases;
    Element_ID loop_phase;
    Element_Count fill_factor;
    Element_Count queued_definitions; /* this is the size of the queue used
                                         for queuing definitions at gaps of
                                         length 1. If this is set to -1 then
                                         gap filling is completely disabled,
                                         if it is set to 0 then when gap
                                         filling is on, gaps are filled at
                                         once, regardless of whether
                                         TCPF_GAP_QUEUE_ONLY is set */

    TC_Cyclic_Reduction cr_mode; /* cr_mode specifies initial modifications to
                                    presentation */

    TC_Equivalent_Presentation ep_mode; /* ep_mode specifies a second set of
                                           modifications to the presentation */
    TC_Compaction_Mode compaction_mode;
    unsigned max_hole_percentage; /* equivalent to ACE com parameter */
    Element_Count allow_holes; /* Don't compact until there more holes than this,
                                  even if the percentage of holes exceeds
                                  max_hole_percentage */
    bool build_standardised; /* if true the coset table will be standardised
                                as rows are filled */
    TC_Enumeration_Options(TC_Enumeration_Strategy strategy = TCES_Default) :
      max_cosets(0),
      work_space(0),
      cr_mode(TCCR_Least_Relator),
      ep_mode(TCEP_As_Is),
      queued_definitions(4096),
      allow_holes(10000),
      build_standardised(false)
    {
      set_strategy(strategy);
    }
    void empty()
    {
      nr_phases = 1;
      loop_phase = -1;
      phases[0].phase_flags = 0;
      phases[0].phase_length = -1;
    }
    void set_strategy(TC_Enumeration_Strategy strategy)
    {
      nr_phases = 1;
      phases[0].phase_length = -1;
      max_hole_percentage = 30;
      fill_factor = 0; // There is no need to set this to 1 like ACE does
                       // since GAP filling won't happen unless turned on
                       // explicitly.
      compaction_mode = TCCM_Discard;
      build_standardised = false;

      switch (strategy)
      {
        case TCES_Default:
          nr_phases = 3;
          compaction_mode = TCCM_Rebuild_By_Row;
          phases[0].phase_flags = TCPF_HLTML_PHASE;
          phases[0].phase_length = -1;
          phases[1].phase_flags = TCPF_LONG_PHASE|TCPF_LENGTH_BY_DEFINITIONS;
          phases[1].phase_length = 8000;
          phases[2].phase_flags = TCPF_LONG_PHASE|TCPF_HLTMD_PHASE|TCPF_LENGTH_BY_DEFINITIONS;
          phases[2].phase_length = 2000;
          loop_phase = 1;
          break;
        case TCES_ACE_Default:
          nr_phases = 3;
          phases[0].phase_flags = TCPF_HLTL_PHASE;
          phases[0].phase_length = -1;
          phases[1].phase_flags = TCPF_FELSCH_PHASE|TCPF_ACE_GAPS|TCPF_LENGTH_BY_DEFINITIONS;
          phases[1].phase_length = 1000;
          phases[2].phase_flags = TCPF_HLTAD_PHASE;
          phases[2].phase_length = 1;
          loop_phase = 1;
          break;
        case TCES_ACE_Hard:
          nr_phases = 2;
          fill_factor = 0;
          phases[0].phase_flags = TCPF_HLTAD_PHASE;
          phases[0].phase_length = 1;
          phases[1].phase_flags = TCPF_FELSCH_PHASE|TCPF_ACE_GAPS|TCPF_LENGTH_BY_DEFINITIONS;
          phases[1].phase_length = 1000;
          loop_phase = 0;
          break;

        case TCES_ACE_Easy:
          phases[0].phase_flags = TCPF_HLTA_PHASE;
          max_hole_percentage = 100;
          break;
        case TCES_HLT_Lookahead:
          phases[0].phase_flags = TCPF_HLTAL_PHASE;
          break;
        case TCES_Sims_2:
          build_standardised = true;
        case TCES_Sims_1:
          phases[0].phase_flags = TCPF_HLT_PHASE;
          break;
        case TCES_Sims_4:
          build_standardised = true;
        case TCES_Sims_3:
          phases[0].phase_flags = TCPF_HLTD_PHASE;
          break;
        case TCES_Sims_6:
          build_standardised = true;
        case TCES_Sims_5:
          phases[0].phase_flags = TCPF_C1HLT_PHASE;
          break;
        case TCES_Sims_8:
          build_standardised = true;
        case TCES_Sims_7:
          phases[0].phase_flags = TCPF_C1HLTD_PHASE;
          break;

        case TCES_ACE_Pure_C:
          phases[0].phase_flags = TCPF_FELSCH_PHASE|TCPF_ACE_DEFINITION_ORDER;
          max_hole_percentage = 100;
          break;
        case TCES_Sims_10:
          build_standardised = true;
        case TCES_Sims_9:
          phases[0].phase_flags = TCPF_FELSCH_PHASE;
          break;
        case TCES_ACE_Felsch_0:
          phases[0].phase_flags = TCPF_FELSCH_PHASE|TCPF_ACE_DEFINITION_ORDER;
          break;
        case TCES_ACE_Felsch_1:
          nr_phases = 2;
          phases[0].phase_length = 1;
          phases[0].phase_flags = TCPF_HLTAD_PHASE;
          phases[1].phase_length = -1;
          phases[1].phase_flags = TCPF_FELSCH_PHASE|TCPF_ACE_GAPS;
          loop_phase = 1;
          fill_factor = 0;
          break;
        case TCES_Long:
          phases[0].phase_flags = TCPF_LONG_PHASE;
          fill_factor = 0;
          break;
        case TCES_Lucky:
          nr_phases = 3;
          phases[0].phase_length = 91000;
          phases[0].phase_flags = TCPF_LONG_PHASE;
          phases[1].phase_length = 9000;
          phases[1].phase_flags = TCPF_CHECK_CONSEQUENCES|TCPF_GAP_STRATEGY_2|TCPF_GAP_STRATEGY_1|TCPF_LENGTH_BY_DEFINITIONS| TCPF_VARY_PHASE;
          phases[2].phase_length = 10;
          phases[2].phase_flags = TCPF_RANDOM_PHASE;
          loop_phase = 0;
          fill_factor = 0;
          break;
      }
    }
};

