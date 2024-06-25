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


// $Log: maf_tc.cpp $
// Revision 1.2  2010/06/10 13:57:43Z  Alun
// All tabs removed again
// Revision 1.1  2010/05/18 04:27:00Z  Alun
// New file.
//

/*
This module implements a flexible Coset Enumerator similar to ACE3. It supports
all ten of Sims TEN_CE strategies and many more besides. Refer to the books
mentioned in this comment if you are unfamiliar with coset enumeration
strategies.

To perform a coset enumeration one can either just ask an existing MAF
object to perform the enumeration or, one can explicitly create a
Coset_Enumerator object for the for the desired MAF object and then call the
enumerate() method passing the desired set of subgroup generators and options.

The enumeration consists of up to eight phases controlled by a
TC_Enumeration_Options::Enumeration_Phase object. At least one of these
phases must either have a phase_length of -1, or a loop_phase must be set,
otherwise the enumeratiom will usually fail. The enumerator will usually run
the phases repeatedly until the enumeration either completes or fails.

Non-normal subgroup generators are processed in the first phase, but
otherwise there is nothing special about this phase.

When a phase begins it will set up any additional data required that does not
yet exist, and sanitise the requested set of options.

Any one phase will be in one of three styles.
1) HLT without a deduction stack, but possibly with look ahead
   ("relator based"/ ACE R style)
2) HLT with a deduction stack (ACE R* style)
3) Felsch style with a deduction stack. ("coset table based" /ACE C style)

For phases with a deduction stack various gap filling strategies are available.
For HLT style phases up to 10 different combinations of relator sets are
available.

In this code the arrays containing the data needed for the deduction stack are
called log_entry_x. It seems to me to be more helpful to think of this data
structure as a transaction log of the changes to the coset table. It certainly
is that, and the data it records are not necessarily deductions and will not
necessarily give rise to deductions either. I have called the function that
examines this transaction log seek_consequences() rather than
process_deductions() which is what both Sims and Holt call it.

R style enumeration is fast but also chews up memory fast. "Look ahead" is
needed to avoid running out of memory on hard enumerations.

R* style enumeration can be slow but still chews up memory fast, but is more
likely to work.

C style enumeration seems to be the best choice for hard enumerations when
all the relators are fairly short and the enumeration is hard. It is not good
where there are long relators, though gap filling can help fix this.

Experiments indicate that while coset enumeration is faster for easy finite
groups, and will usually require less memory than a shortlex KB approach in
that case, MAF's KB is likely to be superior unless an HLT + lookahead
succeeds quite quickly. HLT+lookahead seems to have its biggest successes
precisely when MAF's KB is bad and KBMAG's is good!

Most of the code in this module is based on Chapter 5 of Sims "Computation
with finitely presented groups", or Chapter 5 of Holt's "Handbook of
computational group theory", though I also looked at the source code of ACE.
However, there are several new ideas:

1) When it is desired to reclaim the space from dead cosets, it is possible
to rebuild the log in various ways so that no information is lost.
It is not clear when there is any real benefit in doing so. The Rebuild
options can save quite a bit of time, but this is mostly because the code
tries hard not to throw away log entries even when Discard mode is set, and
probably throwing away the log and doing a full scan would often be quicker.

2) MAF uses a different gap filling strategy from ACE. ACE does its preferred
definition gap filling in place of C style row filling. MAF does it when
the log is empty, and always fills the row normally as well. MAF won't do short
gap filling for relators of length 3.

MAF follows the original suggestion of George Havas and uses a circular
buffer, but there is no need for the size of this to be a power of 2.

In addition MAF supports a second GAP filling strategy of remembering the
lowest position with the shortest gap for the scan for each relator. This
seems often to work better when there is a mix of short and long relators
since short relators will not be favoured so much. It seems to work well
with many, but by no means all, "difficult" enumerations.

3) MAF uses a data structure "Cursor" which makes it easy to remember as many
particular cosets as desired and which takes care of adjusting the coset
number if the table is renumbered. So in general we pass cursors around rather
than coset numbers if there is any risk that the table might get rebuilt while
the number is still is needed.

If the coset table is to be returned then it is standardised, and labelled
so that each coset is labelled by the last generator for the reduced word for
that coset. The code for doing this is in mafcoset.cpp, because it is also
needed by Low_Index_Subgroup_Finder. The labels are correct for the chosen
word-ordering.

The code for generating the list of relators, which was originally just
part of this enumerator has been separated out into the Relator_Conjugator
class implemented in relators.cpp. Relator_Conjugator is also used by
Low_Index_Subgroup_Finder.
*/

#include "maf.h"
#include "fsa.h"
#include "arraybox.h"
#include "maf_el.h"
#include "mafword.h"
#include "tietze.h"
#include "container.h"
#include "maf_tc.h"
#include "relators.h"

class Coset_Enumerator : public FSA_Common
{
  private:
    typedef Element_ID Coset_ID;
    typedef Array_Of<Coset_ID> Coset_List;

    enum Scan_Result
    {
      SR_No_Coset,
      SR_No_Space,
      SR_Incomplete,
      SR_Complete,
      SR_Deduction,
      SR_Coincidence
    };

    /* Merge Edge is used for managing coincidence processing */
    struct Merge_Edge
    {
      Merge_Edge * next;
      Coset_ID source_ci;
      Coset_ID dest_ci;
      Ordinal g;
      Merge_Edge() :
        source_ci(0),
        dest_ci(0),
        g(-1),
        next(0)
      {}
    };

    /* We are going to need a lot of variables which help us to cycle
       through the set of live cosets.

       This is done using instances of the Cursor class, which is
       managed by the unique instance of Coset_Manager. Coset_Manager is
       reponsible for updating all the data in all the cursors when
       cosets are renumbered.

       Most cursors are sequential and long lasting, but it is perfectly
       possible to create a temporary cursor, and to change the position of
       a cursor to any desired coset.

       Cursors will be deleted automatically when the Coset_Enumerator
       object is destroyed, but they can also be deleted explicitly.
    */

    class Cursor;
    class Coset_Manager;

    class Cursor_Manager
    {
      /* This is the base class for Coset_Manager, only needed so that we can
         make all the Cursor methods inline */
      friend class Cursor;
      protected:
        Coset_ID last_ci; /* We could manage this with a cursor as well, but
                             it would make the code a lot more mysterious */
        Cursor_Manager() :
          last_ci(0)
        {}
        virtual ~Cursor_Manager()
        {
        }
        virtual void hook_cursor(Cursor &cursor) = 0;
        virtual void unhook_cursor(Cursor &cursor) = 0;
      public:
        Coset_ID last_coset() const
        {
          return last_ci;
        }
    };

    class Cursor
    {
      friend class Coset_Manager;
      private:
        // data pertaining to the actual cursor
        Coset_ID current_ci;
        Coset_ID next_ci;
        Element_Count adjustment; // used when cosets are renumbered
        // data pertaining to the cursor manager
        Cursor_Manager &manager;
        Element_ID cursor_id;

      public:
        Cursor(Cursor_Manager &manager_) :
          current_ci(0),
          next_ci(1),
          adjustment(0),
          manager(manager_)
        {
          manager.hook_cursor(*this);
        }
        ~Cursor()
        {
          manager.unhook_cursor(*this);
        }
        Coset_ID current() const
        {
          return current_ci;
        }
        Coset_ID next() const
        {
          return next_ci;
        }
        Coset_ID position() const
        {
          if (current_ci)
            return current_ci;
          return next();
        }
        void begin()
        {
          next_ci = 1;
        }
        Coset_ID advance()
        {
          /* Note that this only actually advances if there is no current
             item. The code currently needs advance() to work like this, even
             though it is a potential source of hangs if a call to end_current()
             is forgotten. */
          if (current_ci)
            return current_ci;
          if (next_ci <= manager.last_coset())
            return current_ci = next_ci++;
          return 0;
        }
        void end_current()
        {
          /* This must be called if you want advance() actually to advance().
          */
          current_ci = 0;
        }
        void set_position(Coset_ID ci)
        {
          current_ci = ci;
          next_ci = ci+1;
          adjustment = 0;
        }
      private:
        void begin_adjustment()
        {
          adjustment = 0;
        }
        void count_hole(Coset_ID ci)
        {
          if (ci < next_ci)
            adjustment++;
          if (ci == current_ci)
            current_ci = 0;
        }
        void end_adjustment()
        {
          if (current_ci)
            current_ci -= adjustment;
          next_ci -= adjustment;
        }
        void update_id(Element_ID new_id)
        {
          cursor_id = new_id;
        }
        void limit(Coset_ID last_ci)
        {
          if (current_ci > last_ci)
            current_ci = 0;
          if (next_ci > last_ci+1)
            next_ci = last_ci + 1;
        }
    };

    class Coset_Manager : public Cursor_Manager
    {
      private:
        Array_Of<Cursor*> cursors;
        Element_Count cursor_count;
        Coset_Enumerator &owner;
        Element_Count coset_capacity;
        Element_Count adjustment;
      public:
        Coset_Manager(Coset_Enumerator &owner_) :
          cursor_count(0),
          owner(owner_),
          coset_capacity(0)
        {}
        ~Coset_Manager()
        {
          empty();
        }

        void set_coset_capacity(Element_Count coset_capacity_)
        {
          coset_capacity = coset_capacity_;
        }

        Coset_ID add_coset()
        {
          if (last_ci + 1 < coset_capacity)
            return ++last_ci;
          return 0;
        }

        bool can_grow(Element_Count amount = 1) const
        {
          return last_ci + amount < coset_capacity;
        }

        Cursor * get_new_cursor()
        {
          return new Cursor(*this);
        }

        void check()
        {
          /* called by coincidence to stop cursors pointing at dead
             cosets. This is a potential inner loop when there are a
             lot of small coincidences, especially if the type 2 gap
             filling strategy is being used. */
          for (Element_ID i = 0 ; i < cursor_count;i++)
          {
            Coset_ID ci = cursors[i]->current();
            if (ci && !owner.is_live_coset(ci))
              cursors[i]->end_current();
          }
        }

        /* The next three methods are used by compress() to update
           all the cursors correctly after holes are removed */

        void begin_adjustment()
        {
          adjustment = 0;
          for (Element_ID i = 0 ; i < cursor_count;i++)
            cursors[i]->begin_adjustment();
        }

        void count_hole(Coset_ID ci)
        {
          adjustment++;
          for (Element_ID i = 0 ; i < cursor_count;i++)
            cursors[i]->count_hole(ci);
        }

        Element_Count end_adjustment()
        {
          for (Element_ID i = 0 ; i < cursor_count;i++)
            cursors[i]->end_adjustment();
          last_ci -= adjustment;
          return adjustment;
        }

        void empty()
        {
          while (cursor_count)
            delete cursors[cursor_count-1];
          cursor_count = 0;
          last_ci = 0;
        }

        Element_Count recover_space()
        {
          Element_Count adjustment = 0;
          while (last_ci > 1 && !owner.is_live_coset(last_ci))
          {
            last_ci--;
            adjustment++;
          }
          if (adjustment)
            for (Element_ID i = 0 ; i < cursor_count;i++)
              cursors[i]->limit(last_ci);

          return adjustment;
        }

      private:

        void hook_cursor(Cursor & cursor)
        {
          if (cursor_count == cursors.capacity())
            cursors.set_capacity((cursor_count+cursor_count/8+8) & ~7);
          cursor.cursor_id = cursor_count;
          cursors[cursor_count++] = &cursor;
        }

        void unhook_cursor(Cursor & cursor)
        {
          MAF_ASSERT(cursors[cursor.cursor_id] == &cursor,
                     owner.container,
                     ("Bad call to Coset_Manager::unhook_cursor()"));
          cursors[cursor.cursor_id] = 0;
          if (cursor.cursor_id < --cursor_count)
          {
            cursors[cursor.cursor_id] = cursors[cursor_count];
            cursors[cursor.cursor_id]->update_id(cursor.cursor_id);
          }
        }
    };

    /* Scan_And_Fill_Candidate is the class used to implement GAP_STRATEGY_2.
       For each relator we can remember one scan which is nearer to
       completion than any other */
    class Scan_And_Fill_Candidate
    {
      public:
        Cursor * cursor;
        Word_Length gap;
        Scan_And_Fill_Candidate() :
          cursor(0),
          gap(0)
        {}
        ~Scan_And_Fill_Candidate()
        {
          if (cursor)
            delete cursor;
        }
        void update(Coset_Manager & manager,Coset_ID ci,Word_Length gap_)
        {
          if (!cursor || !cursor->current() || gap_ < gap || gap_ == gap &&
              ci < cursor->current())
          {
            if (!cursor)
              cursor = manager.get_new_cursor();
            cursor->set_position(ci);
            gap = gap_;
          }
        }
        Cursor * take()
        {
          Cursor * answer = cursor;
          cursor = 0;
          return answer;
        }
        void exchange(Coset_ID ci1,Coset_ID ci2)
        {
          /* called by the code that standardises as it builds */
          if (cursor)
            if (cursor->current() == ci1)
              cursor->set_position(ci2);
            else if (cursor->current() == ci2)
              cursor->set_position(ci1);
        }
        void check()
        {
          if (cursor && !cursor->current())
          {
            delete cursor;
            cursor = 0;
          }
        }
        void cancel(Coset_ID ci)
        {
          if (cursor && cursor->current()==ci)
          {
            delete cursor;
            cursor = 0;
          }
        }
        Coset_ID current() const
        {
          return cursor ? cursor->current() : 0;
        }
    };

  public:
    MAF & maf;
  private:
    // first data related to the columns of the coset table
    Element_Count max_cosets; /* absolute maximum that will ever be allocated */
    Element_Count current_max_cosets; /* current capacity of columns */
    Collection_Manager data_manager;
    Coset_List *columns;
    Coset_ID ** column_data;
    Coset_ID * rep_column_data;
    Coset_ID * link_column_data;
    Transition_Count undefined_transitions; //count of zero entries in live cosets
    const Ordinal nr_symbols; // This is the number of monoid generators
    const Ordinal nr_columns; // This is at least 2, otherwise nr_symbols
    Ordinal rep_column; // special column for remembering which cosets are alive
    Ordinal link_column;// special column for coincidence processing
    Ordinal ren_column; // special column for renumbering
    // control variables for the rows of the table
    Coset_Manager coset_manager;
    Cursor * scanner;
    Cursor * filler;
    Cursor * standardiser;
    Cursor * full_scanner;
    // stats about the enumeration
    Element_Count dead_cosets;  // currently completely dead cosets
    Element_Count live_cosets;  // currently live cosets
    Element_Count max_live_cosets;
    Element_Count total_cosets; // number of successful calls to define_coset()
    //coincidence management
    Element_Count coincidences; // number of coincidences completely processed;
    Coset_ID merge_head;        // next coset to be killed off
    Coset_ID merge_tail;        // most recently coset scheduled to be killed
    Merge_Edge *inner_head;     // edges removed to make room for special column data
    Merge_Edge **inner_tail;
    Merge_Edge *free_edge;
    Element_Count queue_length; // cosets currently turning from live to dead
    Element_Count max_queue_length;
    //consequence management - better known as the "deduction stack"
    Collection_Manager consequence_manager;
    Element_Count log_max;     // current capacity
    Array_Of<Coset_ID> log_entry_ci;
    Array_Of<Ordinal> log_entry_g;
    Array_Of<Byte> log_entry_packed_g;
    Element_Count log_top;     // most recent entry
    Element_Count max_log_top; // highest number of entries ever present
    // gap management - similar to consequence managment, but using a circular
    // buffer
    Collection_Manager gap_manager;
    Array_Of<Coset_ID> gap_ci;
    Array_Of<Ordinal> gap_g;
    Array_Of<Byte> gap_packed_g;
    Element_Count gap_start;
    Element_Count gap_end;
    Element_Count gaps_taken;
    // strategy 2 gap management
    Scan_And_Fill_Candidate * scan_candidates;
    Element_ID next_candidate;
    unsigned long random_seed;
    Relator_Conjugator rc;
    Element_Count nr_full_scan_relators;
    // control flags
    TC_Enumeration_Options options;
    bool back_fill;        // sanitised phase flags set at start of do_phase()
    bool check_fill_factor;
    bool consequences_checked;
    bool front_fill;
    bool gap_fill;
    bool gap_require_low;
    bool look_ahead_on;
    bool pre_fill;
    bool queue_gaps;
    // state flags that pertain to the whole enumeration
    bool consequences_skipped; //this flag is true if a transition is ever
                               // set when consequence checking is off
    bool failed;        // set when it is time to give up due to lack of space
    bool full_scan_required; //set if look_ahead needed or log entries discarded
    bool has_overflowed;
    bool no_candidates;
    bool log_purge_required; // true when coincidences may have left log entries
                              // for dead states. It is expensive to
                              // keep filtering the list
    // global properties of the enumeration
    bool dynamic;             // true if no maximum coset table size specified
    bool pack_transitions; // indicator to choose between xx_packed_g and xx_g
                           // members
  public:
    Coset_Enumerator(MAF & maf_) :
      maf(maf_),
      FSA_Common(maf_.container,maf_.group_alphabet()),
      nr_symbols(maf_.group_alphabet().letter_count()),
      nr_columns(nr_symbols < 2 ? 2 : nr_symbols),
      max_cosets(0),
      current_max_cosets(0),
      log_top(0),
      merge_head(0),
      merge_tail(0),
      inner_head(0),
      inner_tail(&inner_head),
      free_edge(0),
      coset_manager(*this),
      gap_start(0),
      gap_end(0),
      dynamic(false),
      scan_candidates(0),
      rc(maf_)
    {
      // In here we only set up data which will be the same for every
      // enumeration we might possibly do with this Coset_Enumerator

      rep_column = 0;
      link_column = 1;
      Ordinal g,ig;
      for (g = 0 ; g < nr_symbols && g < 2; g++)
        if ((ig = maf.inverse(g)) != g)
        {
          rep_column = g;
          link_column = ig;
          break;
        }

      columns = new Coset_List[nr_columns];
      column_data = new Coset_ID *[nr_columns];
      for (Ordinal i = 0; i < nr_columns;i++)
      {
        data_manager.add(columns[i],false);
        column_data[i] = 0;
      }

      pack_transitions = base_alphabet.packed_word_size(size_t(0)) == 1;
      consequence_manager.add(log_entry_ci);
      if (pack_transitions)
        consequence_manager.add(log_entry_packed_g,false);
      else
        consequence_manager.add(log_entry_g,false);
      consequence_manager.set_capacity(log_max = 1000);
      gap_manager.add(gap_ci);
      if (pack_transitions)
        gap_manager.add(gap_packed_g,false);
      else
        gap_manager.add(gap_g,false);

      /* create the objects used for handling special edge coincidences.
         We need 4 of these.
         The queue is at most length 2, but we don't free the item we
         are currently processing until after new items are queued, and
         we can create a fourth item which we then process without queuing it. */
      for (int i = 0;i < 4;i++)
      {
        Merge_Edge * me = new Merge_Edge;
        me->next = free_edge;
        free_edge = me;
      }
    }

    ~Coset_Enumerator()
    {
      delete [] columns;
      delete [] column_data;
      while (free_edge)
      {
        Merge_Edge * me = free_edge;
        free_edge = free_edge->next;
        delete me;
      }
    }

    Language_Size enumerate(const Word_Collection &normal_subgroup_generators,
                   const Word_Collection &subgroup_generators,
                   const TC_Enumeration_Options & options_,bool table_wanted)
    {
      begin_enumeration(normal_subgroup_generators,options_);

      do_phase(options.phases[0],&subgroup_generators);
      Element_ID i = 0;
      while (undefined_transitions && !failed)
      {
        if (++i == options.nr_phases)
        {
          i = options.loop_phase;
          if (i < 0)
            break;
        }
        do_phase(options.phases[i]);
      }

      if (!failed && !undefined_transitions)
      {
        /* It ought not to be possible to have failed true unless
           undefined_transitions is non-zero, but the options might have
           specified a phase that only lasts for a limited time, so
           we could drop out of the enumeration before we actually failed */
        status("Finishing",0,1,0);
        if (consequences_skipped)
          finish();
        if (table_wanted)
        {
          container.progress(1,"Standardising\n");
          standardise();
          change_flags(GFF_TRIM|GFF_MINIMISED|GFF_ACCESSIBLE|GFF_DENSE|GFF_BFS,0);
          set_single_accepting(1);
          maf.label_coset_table(this);
        }
        container.progress(1,"Enumeration complete\n");
      }
      else
      {
        failed = true;
        container.progress(1,"Enumeration failed\n");
      }

      end_enumeration();
      return failed ? 0 : live_cosets;
    }

  public:
    /* Methods required by FSA interface */
    Transition_ID alphabet_size() const
    {
      return nr_symbols;
    }

    State_Count state_count() const
    {
      return coset_manager.last_coset()+1;
    }

    State_ID new_state(State_ID si,Transition_ID ti,bool) const
    {
      return column_data[ti][si];
    }

    bool set_transitions(State_ID, const State_ID *)
    {
      return false;
    }

    /* Method required by Coset_Manager */
    bool is_live_coset(Coset_ID ci) const
    {
      return rep_column_data[ci] >= 0;
    }

  private:
    /* Implementation methods */

    unsigned long random(unsigned long limit)
    {
      /* This is the BCPL random number generator*/
      random_seed = (random_seed * 2147001325ul + 715136305ul) & 0xfffffffful;
      return (random_seed >> 3) %limit;
    }

    void begin_enumeration(const Word_Collection &normal_subgroup_generators,
                           const TC_Enumeration_Options & options_)
    {
      /* Set up all the data structures for a new enumeration
         1) allocate an initial coset table,
         2) Create the set of all permutations of all relators
         3) Create coset 1 and initialise the control variables */

      random_seed = 0;
      options = options_;
      if (!options.fill_factor)
        options.fill_factor = 5*(nr_symbols+2)/4;

      /* Work out how big a table to create. In 32-bit world max_max_cosets
         is certainly an overestimate because max_word_space is only just
         short of 4GB, and it is very unlikely a 32-bit OS would be willing
         to allocate much more than 3GB to user address space. Some versions
         of Windows only allow 2GB. */
      size_t max_work_space = sizeof(Coset_ID)*2*MAX_STATES;
      Element_Count max_max_cosets = Element_Count(max_work_space/(nr_columns*sizeof(Coset_ID)));
      if (options.work_space > max_work_space)
        options.work_space = max_work_space;
      max_cosets = options.max_cosets;
      if (max_cosets)
        max_cosets += 1; // so users get the number they expect
      if (!max_cosets)
        max_cosets = options.work_space/(nr_columns*sizeof(Coset_ID));
      dynamic = max_cosets == 0;
      if (max_cosets && max_cosets > max_max_cosets)
        max_cosets = max_max_cosets;

      /* If the user specified a table size we use that straight away to
         reduce risk of fragmentation, but otherwise we start with a rather
         small table, because we hope to fit it in the initial 4MB heap
         allocation. The enumeration will be faster if we can complete it
         within this limit, because the CPU RAM cache will be used more
         effectively. */
      Element_Count initial_max_cosets = max_cosets;
      if (!initial_max_cosets)
        initial_max_cosets = 512*512;
      if (options.phases[0].phase_length > initial_max_cosets)
        initial_max_cosets = options.phases[0].phase_length;
      if (max_cosets && initial_max_cosets > max_cosets)
        initial_max_cosets = max_cosets;
      set_capacity(initial_max_cosets,false);

      if (!max_cosets)
        max_cosets = max_max_cosets;

      if (options.compaction_mode == TCCM_Special_Column)
      {
        ren_column = -1;
        for (Ordinal g = nr_symbols; g-- >0 ;)
          if (g != maf.inverse(g) && g != rep_column)
            ren_column = g;
        if (ren_column == -1)
        {
          container.error_output("Presentation is incompatible with use of"
                                 " special column compaction.\nUsing full"
                                 " scan compaction instead.\n");
          options.compaction_mode = TCCM_Discard;
        }
      }

      unsigned flags = 0;
      if (options.cr_mode != TCCR_As_Is)
      {
        unsigned flags = TTH_KEEP_GENERATORS;
        if (options.cr_mode != TCCR_Least_Relator)
          flags |= TTH_BEST_EQUATION_RELATORS;
      }
      nr_full_scan_relators = rc.create_relators(normal_subgroup_generators,flags);

      /* Now put the coset table and control variables into the initial state */
      container.progress(1,"Starting coset enumeration\n");
      reset_table();
      if (options.ep_mode != TCEP_As_Is)
        equivalent_presentation();

      /* initialise some cursors */
      full_scanner = 0;
      scanner = coset_manager.get_new_cursor();
      filler = coset_manager.get_new_cursor();
      if (options.build_standardised)
      {
        standardiser = coset_manager.get_new_cursor();
        standardiser->advance();
      }
      else
        standardiser = 0;

      /* initialise gap management */
      if (options.queued_definitions > 0)
        gap_manager.set_capacity(options.queued_definitions);
      else
        gap_manager.set_capacity(0);
    }

    void reset_table()
    {
      consequences_skipped = failed = full_scan_required = false;
      coset_manager.empty();
      gaps_taken = max_queue_length = dead_cosets = 0;
      coincidences = 0;
      max_log_top = log_top = 0;
      gap_start = gap_end = 0;
      total_cosets = max_live_cosets = live_cosets = 1;
      coset_manager.add_coset();
      action(0,rep_column) = -1; // ensure is_live_coset() return false on 0
      for (Ordinal g = 0 ; g < nr_symbols; g++)
        action(1,g) = 0;
      undefined_transitions = nr_symbols;
    }

    void equivalent_presentation()
    {
      /* For each relator we pick the cyclic conjugate which requires
         the fewest new coset definitions */
      Relator_Set & rs = rc.open_base_relator_set();
      Element_Count nr_relators = rs.count();
      Element_ID r;
      Fast_Word fw;
      bool want_most_definitions = options.ep_mode == TCEP_Most_Definitions ||
                                   options.ep_mode == TCEP_Reverse_Most_Definitions;
      bool want_fewest_definitions = options.ep_mode == TCEP_Fewest_Definitions ||
                                   options.ep_mode == TCEP_Reverse_Fewest_Definitions;

      front_fill = back_fill = consequences_checked = gap_fill = false;

      if (options.ep_mode >= TCEP_Reverse)
      {
        for (r = 0;; r++)
        {
          Element_ID other = nr_relators-1 - r;
          if (r >= other)
            break;
          Element_ID temp = rs[r];
          rs[r] = rs[other];
          rs[other] = temp;
        }
      }

      if (want_most_definitions || want_fewest_definitions)
      {
        rc.get_relator(&fw,rs[0]);
        scan_and_fill_inner(1,fw);
        Ordinal_Word test(base_alphabet);
        for (Element_ID r = 1; r < nr_relators;r++)
        {
          rc.get_relator(&test,rs[r]);
          fw.length = test.length();
          fw.buffer = test.buffer();
          Element_Count gap = scan_count(1,fw);
          Element_Count best = 0;
          if (gap != 0)
          {
            test += test;
            fw.buffer = test.buffer()+1;
            for (Word_Length i = 1; i < fw.length; i++,fw.buffer++)
            {
              Element_Count new_gap = scan_count(1,fw);
              if (want_most_definitions ? new_gap > gap : new_gap < gap)
              {
                gap = new_gap;
                best = i;
              }
            }
            /* Since scanning the inverse of a relator and scanning the relator
               visit exactly the same positions we don't need to worry about
               inverse relators */
          }

          if (best != 0)
          {
            test = Subword(test,best,best+fw.length);
            rc.find(test,&rs[r]);
          }
          rc.get_relator(&fw,rs[r]);
          scan_and_fill_inner(1,fw);
        }
        reset_table();
      }
    }

    void set_capacity(Element_Count count,bool keep = true)
    {
      if (count != current_max_cosets)
      {
        data_manager.set_capacity(current_max_cosets = count,keep);
        coset_manager.set_coset_capacity(current_max_cosets);
        for (Ordinal i = 0; i < nr_columns;i++)
          column_data[i] = columns[i].buffer();
        rep_column_data = column_data[rep_column];
        link_column_data = column_data[link_column];
      }
    }

    void grow_capacity()
    {
      /* Attempt to grow the coset table. This code will only do anything
         in the case where the user did not specify either a work_space size
         or a maximum number of cosets. */
      Element_Count desired_capacity;

      /* Originally the default number of cosets is chosen to fit four
         columns inside the first heap block. Later on it won't so it is best
         to ensure that each column will be a "big" allocation so that it will be freed
         immediately next time we need to increase the number of cosets allowed.
         We increase the number of cosets rapidly at first, but more slowly
         later on, though the size of the table will increase by at least
         12MB (since no sane enumeration will have fewer than three columns)
         each time it is growm, and soon more. */
      if (current_max_cosets >= 8192*1024)
      {
        if (MAX_STATES - current_max_cosets <= current_max_cosets/8)
          desired_capacity = MAX_STATES;
        else
          desired_capacity = current_max_cosets + current_max_cosets/8 + 1;
      }
      else if (current_max_cosets < 1024*512)
        desired_capacity = 1024*1024;
      else
        desired_capacity = current_max_cosets * 2;

      if (dynamic && look_ahead_on && desired_capacity >= 8192*1024)
      {
        /* If dynamic is false a full scan will be triggered when
           define_coset() fails anyway.
           Here I'm saying that an initial HLT+look_ahead phase should
           end once we have got to 4 million cosets. It is probably time
           to switch to some kind of "hard" strategy. If there is only
           one phase then it won't matter that we trigger a look_ahead
           now. It is probably a good time to do one. */
        schedule_full_scan();
        has_overflowed = true;
      }
      desired_capacity = (desired_capacity + 255) & ~255;
      if (max_cosets && desired_capacity > max_cosets)
        desired_capacity = max_cosets;
      set_capacity(desired_capacity);
    }

    Coset_ID &action(Coset_ID ci,Ordinal g)
    {
      // note that this is returned as a reference so we can update the action
      // directly. However that should usually be done through set_action()
      // so that the log is updated if it is being used.
      return column_data[g][ci];
    }

    /**/

    void end_enumeration()
    {
      // get rid of stuff we no longer need
      // we do not get rid of the column data, because the coset table
      // is needed to allow the enumerator to function as an FSA
      rc.tidy();
      if (scan_candidates)
      {
        delete [] scan_candidates;
        scan_candidates = 0;
      }
    }

    /**/

    void finish()
    {
      /* Once there are no undefined transitions we know that
         the index is finite. It might continue to decrease because
         we may not have spotted all the coincidences yet */
      const Relator_Set & rs = rc.get_relator_set(0,false,false);
      Element_Count nr_relators = rs.count();
      Fast_Word fw;

      // there is quite a high probability that the index is correct now
      // so it is probably worth compressing immediately since we then
      // get better cache utilisation and branch prediction in scan_final()
      recover_space(true);

      while (scanner->advance())
      {
        status("Validating",scanner);
        for (Element_ID r = 0; r < nr_relators; r++)
        {
          rc.get_relator(&fw,rs[r]);
          if (scan_final(scanner->current(),fw) == SR_No_Coset)
            break;
        }
        scanner->end_current();
      }
    }

    Scan_Result scan_final(Coset_ID ci,const Fast_Word &word)
    {
      if (!is_live_coset(ci))
        return SR_No_Coset;
      const Ordinal * fi = word.buffer;
      const Ordinal * bi = fi + word.length;
      Coset_ID fci = action(ci,*fi++);

      while (fi != bi)
        fci = action(fci,*fi++);
      if (fci == ci)
        return SR_Complete;
      coincidence(fci,ci);
      return SR_Coincidence;
    }

    /**/

    void do_phase(const TC_Enumeration_Options::Enumeration_Phase &phase,
                  const Word_Collection * subgroup_generators = 0)
    {
      /* This method does all the processing associated with an enumeration
         phase. First we set up the control variables and any
         extra data required that we may not yet have constructed. */

      unsigned phase_flags = phase.phase_flags;
      Element_Count phase_length = phase.phase_length;
      const Relator_Set *rs = 0;
      Element_Count was_total_cosets = total_cosets;
      bool count_definitions = (phase_flags & TCPF_LENGTH_BY_DEFINITIONS)!=0;

      if (phase_flags & TCPF_RANDOM_PHASE)
        phase_flags = random(16384)|TCPF_CHECK_CONSEQUENCES;
      else if (phase_flags & TCPF_VARY_PHASE)
        phase_flags ^= random(16384) & ~(TCPF_APPLY_RELATORS|
                                         TCPF_CHECK_CONSEQUENCES|
                                         TCPF_PERMUTE_G|TCPF_PERMUTE_N|
                                         TCPF_GAP_STRATEGY_2|
                                         TCPF_GAP_STRATEGY_1);
      if (phase_flags & TCPF_APPLY_RELATORS)
      {
        bool permute_G = (phase_flags & TCPF_PERMUTE_G)!=0;
        bool permute_N = (phase_flags & TCPF_PERMUTE_N)!=0 && rc.have_quotient_relators();
        int order_type = 0;
        if (permute_G || permute_N)
        {
          if (phase.phase_flags & TCPF_PERMUTATION_TYPE_3)
            order_type = 2;
          else if (phase.phase_flags & TCPF_PERMUTATION_TYPE_2)
            order_type = 1;
        }
        rs = &rc.get_relator_set(order_type,permute_G,permute_N);
        consequences_checked = (phase_flags & TCPF_CHECK_CONSEQUENCES)!=0;
        pre_fill = (phase_flags & TCPF_PRE_FILL_ROW)!=0;
        look_ahead_on = !consequences_checked && (phase_flags & TCPF_LOOK_AHEAD_ON)!=0;
      }
      else
      {
        consequences_checked = true;
        look_ahead_on = false;
        if (consequences_skipped)
          schedule_full_scan();
      }
      // although front_fill and back_fill are mostly only relevant to R style
      // processing they are used anyway if this is the initial phase
      // and there are non-normal subgroup generators
      front_fill = !(phase_flags & TCPF_BALANCE_SCAN+TCPF_BACK_SCAN);
      back_fill = !(phase_flags & TCPF_BALANCE_SCAN) && !front_fill;
      // gap stuff
      gap_fill = consequences_checked &&
                 (phase_flags & TCPF_GAP_STRATEGY_1)!=0 &&
                 options.queued_definitions >= 0;
      queue_gaps = gap_fill && (phase_flags & TCPF_GAP_QUEUE_ONLY)!=0 &&
                   options.queued_definitions > 0;
      check_fill_factor = gap_fill && (phase_flags & TCPF_GAP_USE_FILL_FACTOR)!=0;
      gap_require_low = gap_fill && (phase_flags & TCPF_GAP_REQUIRE_LOW)!=0;
      if (consequences_checked && phase_flags & TCPF_GAP_STRATEGY_2)
      {
        if (!scan_candidates)
        {
          scan_candidates = new Scan_And_Fill_Candidate[rc.count()];
          next_candidate = 0;
          no_candidates = true;
        }
      }
      else if (scan_candidates)
      {
        delete [] scan_candidates;
        scan_candidates = 0;
      }
      has_overflowed = false;

      if (subgroup_generators)
      {
        /* process the (non-normal) subgroup generators */
        Element_Count nr_subgroup_generators = subgroup_generators->count();
        Ordinal_Word ow(base_alphabet);

//      Not sure what I'm trying to achieve in the commented out lines below
//      Something to do with "default" initial phase length.
//        if (phase_length == -1 && !look_ahead_on)
//          phase_length = max_cosets;

        Fast_Word fw;
        scanner->advance();
        for (Element_ID i = 0; i < nr_subgroup_generators;i++)
          if (subgroup_generators->get(&ow,i))
          {
            fw.buffer = ow.buffer();
            fw.length = ow.length();
            scan_and_fill(scanner,fw);
            if (log_top)
              seek_consequences();
          }
      }
      else if (log_top)
        seek_consequences();

      Element_Count nr_relators = rs ? rs->count() : 0;
      Fast_Word relator;

      while (undefined_transitions && !failed)
      {
        if (rs != 0)
        {
          /* Process the next row using an HLT style coset enumeration step
             The very first time in scanner->advance() will return 1 whether
             or not subgroup generators have already been processed.
             because end_current() has not been called to move on.
          */

          const Relator_Set &relator_set = *rs;
          if (!failed && scanner->advance())
          {
            status("Scanning",scanner);
            if (pre_fill && scanner->current() >= filler->next())
              fill_row();
            /* process the given set of relators for the coset
               currently selected for scanning, while there is such a coset */
            for (Element_ID i = 0; i < nr_relators; i++)
            {
              rc.get_relator(&relator,relator_set[i]);
              if (scan_and_fill(scanner,relator) == SR_No_Coset)
                break;
            }
            if (scanner->current() >= filler->next())
              fill_row();
            scanner->end_current();
          }
        }
        else
        {
          status("Defining",filler);
          fill_row(); /* Process the next row C style */
        }

        if (phase_length != -1)
        {
          if (!count_definitions)
          {
            if (!--phase_length)
              break;
          }
          else if (total_cosets - was_total_cosets >= phase_length)
            break;
        }
        else if (has_overflowed && look_ahead_on && options.nr_phases != 1)
          break;
      }

      if (look_ahead_on && !failed && undefined_transitions)
        schedule_full_scan();
    }

    void status(String s,const Cursor * cursor, unsigned level = 2,int gap = 1) const
    {
      Coset_ID ci = cursor ? cursor->position() : 0;
      bool wanted;
      if (ci)
        wanted = container.status(level,gap,
                         "Unknown transitions:" FMT_TC " Activity: %s at " FMT_ID "\n",
                         undefined_transitions,s.string(),ci);
      else
        wanted = container.status(level,gap,
                         "Unknown transitions:" FMT_TC " Activity: %s\n",
                         undefined_transitions,s.string());

      if (wanted)
      {
        container.progress(level,"Cosets: live now/max:" FMT_ID "/" FMT_ID
                                 " total now/ever:" FMT_ID "/" FMT_ID,
                           live_cosets,max_live_cosets,
                           coset_manager.last_coset(),total_cosets);

        container.progress(level,"\nCoincidences: ever:" FMT_ID
                           " queued now/max:" FMT_ID "/" FMT_ID,
                           coincidences,queue_length,max_queue_length);
        if (consequences_checked)
        {
          container.progress(level,"\nDeduction stack: now/max:"
                                   FMT_ID "/" FMT_ID,
                             log_top,max_log_top);
          if (gap_fill)
           container.progress(level," Short gaps: queued/taken:"
                                    FMT_ID "/" FMT_ID,
                               gap_end >= gap_start ? gap_end - gap_start :
                               options.queued_definitions - (gap_start-gap_end),
                               gaps_taken);

        }
        container.progress(level,"\n");
      }
    }

    /**/

    void fill_row()
    {
      if (failed || !filler->advance())
        return;

      ensure_space(nr_symbols);

      if (!standardiser)
      {
        for (Ordinal g = 0; g < nr_symbols;g++)
        {
          if (!ensure_defined(filler,g))
            break;
          if (log_top)
            seek_consequences();
        }
      }
      else
      {
        Cursor nci_cursor(coset_manager);

        for (Ordinal g = 0; g < nr_symbols;g++)
        {
          Coset_ID nci = ensure_defined(filler,g);
            if (!nci)
              break;
          if (log_top)
          {
            /* Since none of the cosets before the one we are filling
               have empty slots, and coincidence() always keeps the lowest
               possible coset, if nci survives seek_consequences() then
               it is still the next coset in BFS order */
            nci_cursor.set_position(nci);
            seek_consequences();
            nci = nci_cursor.current();
          }

          if (nci >= standardiser->next())
          {
            /* at this point we are sure there are no unchecked
               consequences, or short gaps, but there may be
               scan candidates. We'll renumber them here rather
               than doing stuff with coset manager, because we'd have
               to worry about standardiser itself otherwise */
            standardiser->advance();
            Coset_ID bfs_ci = standardiser->current();
            if (nci != bfs_ci)
              exchange(bfs_ci,nci);
            if (scan_candidates)
            {
              Element_Count count = rc.count();
              for (Element_ID r = 0 ; r < count;r++)
                scan_candidates[r].exchange(nci,bfs_ci);
            }
          }
        }
      }

      if (scan_candidates && !no_candidates)
      {
        Cursor * c = 0;
        bool again = true;
        // I experimented with scanning the "next_candidate" on the
        // current row to give it a chance to become the best option
        // in the case where there is no candidate yet. But this seems
        // to be bad on the whole.
        // I also experimented with trying to notice when a scan that
        // completes is a candidate. It seems better not to do this
        // presumably because it results in this code making fewer
        // relator applications

        while ((c = scan_candidates[next_candidate].take())==0)
          if (++next_candidate == rc.count())
          {
            next_candidate = 0;
            if (again)
              again = false;
            else
            {
              no_candidates = true;
              return;
            }
          }
        if (c->current())
        {
          Fast_Word relator;
          rc.get_relator(&relator,next_candidate);
          scan_and_fill(c,relator);
          delete c;
          if (++next_candidate == rc.count())
            next_candidate = 0;
        }
      }

      filler->end_current();
    }

    /* methods to update the coset table directly, used by fill_row()
       and scan functions */

    Coset_ID ensure_defined(Cursor * cursor,Ordinal g)
    {
      /* Ensure the the action of g on the coset pointed to by the cursor is
         defined, creating a new coset if necessary, and retrying if there is
         no space but we can either get more space or recover dead space. */
      Coset_ID nci = 0;
      bool again = true;
      for (;;)
      {
        failed = false;
        Coset_ID ci = cursor->current();
        if (!is_live_coset(ci))
          break;
        if ((nci = action(ci,g))!=0)
          break;
        if ((nci = define_coset(ci,g))!=0 || !again)
          break;
        if (look_ahead_on)
        {
          schedule_full_scan();
          seek_consequences();
        }
        else
          recover_space(true);
        again = false;
      }
      return nci;
    }

    Coset_ID define_coset(Coset_ID ci,Ordinal g)
    {
      /* create a new coset, and make it the result of the action of g
         on ci */

      if (!coset_manager.can_grow())
        grow_capacity();

      Coset_ID nci = coset_manager.add_coset();
      if (!nci)
      {
        has_overflowed = failed = true;
        return nci;
      }
      for (Ordinal i = 0 ; i < nr_symbols;i++)
        action(nci,i) = 0;
      undefined_transitions += nr_symbols;
      if (++live_cosets > max_live_cosets)
        max_live_cosets = live_cosets;
      total_cosets++;
      set_action(ci,g,nci);
      return nci;
    }

    void set_action(Coset_ID ci,Ordinal g,Coset_ID nci)
    {
      /* set the action of g on coset ci to nci, and record the
         fact that this transition is known in the log */

      MAF_ASSERT(is_live_coset(ci) && is_live_coset(nci),container,
                 ("Attempting to set invalid transition in coset table\n"));

      Ordinal ig = maf.inverse(g);
      Coset_ID &fci = action(ci,g) = nci;
      Coset_ID &bci = action(nci,ig) = ci;
      undefined_transitions -= 1 + (&fci != &bci);
      if (!consequences_checked)
        consequences_skipped = true;
      else if (!full_scan_required)
      {
        if (log_top == log_max)
        {
          if (log_top > live_cosets/2)
          {
            container.status(2,0,"Deduction stack full (" FMT_ID " entries)."
                                " Scheduling full scan\n",log_top);
            schedule_full_scan();
            return;
          }
          if (log_purge_required)
            clean_log();
          consequence_manager.grow_capacity();
          log_max = consequence_manager.capacity();
        }

        /* to reduce the chances of duplicate consequences arising from
           coincidence processing, we only record the coset transition for
           for the direction of the transition in which g <= ig.
        */
        if (g <= ig)
        {
          log_entry_ci[log_top] = ci;
          if (pack_transitions)
            log_entry_packed_g[log_top] = g;
          else
            log_entry_g[log_top] = g;
        }
        else
        {
          log_entry_ci[log_top] = nci;
          if (pack_transitions)
            log_entry_packed_g[log_top] = ig;
          else
            log_entry_g[log_top] = ig;
        }
        if (++log_top > max_log_top)
          max_log_top = log_top;
      }
    }

    /**/

    /* I've decided to implement multiple scan functions, even though
       it would be possible to combine most or even all of them into
       one. The reason for this is that these functions are very much
       "inner loops" and even a small amount of extra code can affect
       performance severely. This is especially the case for the
       functions used in lookahead/consequence processing. */

    Scan_Result scan_and_fill(Cursor * cursor,const Fast_Word & word)
    {
      /* This is an outer loop for HLT style scanning which deals with
         lookahead after an SR_No_Space return from the real method */
      Scan_Result sr;
      bool again;

      ensure_space(word.length);

      do
      {
        again = false;
        sr = scan_and_fill_inner(cursor->current(),word);
        if (sr == SR_No_Coset)
          return sr;
        if (sr == SR_No_Space)
        {
          again = true;
          if (look_ahead_on)
            schedule_full_scan();
          else
            recover_space(true);
        }
        if (log_top)
          seek_consequences();
        if (again)
        {
          if (!cursor->current() || coset_manager.can_grow())
            failed = false;
          if (failed)
            return SR_No_Space;
        }
      }
      while (again);
      return sr;
    }

    Scan_Result scan_and_fill_inner(Coset_ID ci,const Fast_Word &w)
    {
      /* scan function for HLT style processing */
      if (!is_live_coset(ci))
        return SR_No_Coset;
      Coset_ID fci = ci,bci = ci,nci;
      const Ordinal * fi = w.buffer;
      const Ordinal * bi = fi + w.length;

      for (;;)
      {
        for (; fi < bi; fi++,fci = nci)
        {
          nci = action(fci,*fi);
          if (!nci)
            break;
        }

        if (fi == bi)
        {
          if (fci != bci)
          {
            coincidence(fci,bci);
            return SR_Coincidence;
          }
          return SR_Complete;
        }

        for (; bi > fi; bi--,bci = nci)
        {
          nci = action(bci,maf.inverse(bi[-1]));
          if (!nci)
            break;
        }

        if (fi == bi)
        {
          /* at this point, if the scan were to complete properly, we would
             have bci == fci. But this cannot be, because if so the forward
             scan would have completed. So we must have a coincidence */
          coincidence(fci,bci);
          return SR_Coincidence;
        }
        else if (bi == fi+1)
        {
          /* here we are making a deduction and setting fci^g and bci^ig
             which are currently undefined */
          set_action(fci,*fi,bci);
          return SR_Deduction;
        }
        else
        {
          if ((front_fill || fci <= bci) && !back_fill)
          {
            if (!define_coset(fci,*fi))
              return SR_No_Space;
          }
          else if (!define_coset(bci,maf.inverse(bi[-1])))
            return SR_No_Space;
        }
      }
    }

    Element_Count scan_count(Coset_ID ci,const Fast_Word &w)
    {
      /* scan function for best_equivalent_presention() processing */
      Coset_ID fci = ci,bci = ci,nci;
      const Ordinal * fi = w.buffer;
      const Ordinal * bi = fi + w.length;

      for (; fi < bi; fi++,fci = nci)
      {
        nci = action(fci,*fi);
        if (!nci)
          break;
      }

      for (; bi > fi; bi--,bci = nci)
      {
        nci = action(bci,maf.inverse(bi[-1]));
        if (!nci)
          break;
      }

      if (bi <= fi+1)
        return 0;
      return bi - fi - 1;
    }

    /**/

    Scan_Result scan_look_ahead(Coset_ID ci,const Fast_Word &word)
    {
      /* scan appropriate for "full scan" consequence checking,
         where we are unsure if any transitions are set yet */
      if (!is_live_coset(ci))
        return SR_No_Coset;
      const Ordinal * fi = word.buffer;
      const Ordinal * bi = fi + word.length;
      Coset_ID fci = ci,bci = ci,nci;

      for (; fi < bi; fi++,fci = nci)
      {
        nci = action(fci,*fi);
        if (!nci)
          break;
      }

      if (fi == bi)
      {
        if (fci != ci)
        {
          coincidence(fci,ci);
          return SR_Coincidence;
        }
        return SR_Complete;
      }

      for (; bi > fi; bi--,bci = nci)
      {
        nci = action(bci,maf.inverse(bi[-1]));
        if (!nci)
          break;
      }

      if (fi == bi)
      {
        coincidence(fci,bci);
        return SR_Coincidence;
      }

      if (fi + 1 == bi)
      {
        /* here we are making a deduction and setting fci^g and bci^ig
           which are currently undefined, (but which may possibly be the
           same transition!) */
        set_action(fci,*fi,bci);
        return SR_Deduction;
      }

      return SR_Incomplete;
    }

    /**/

    /* In the remaining functions we are sure that at least the first
       forward transition is present */

    Scan_Result scan_gap00(Coset_ID ci,const Fast_Word &word)
    {
      if (!is_live_coset(ci))
        return SR_No_Coset;
      const Ordinal * fi = word.buffer;
      const Ordinal * bi = fi + word.length;
      Coset_ID fci = action(ci,*fi++),bci = ci,nci;

      for (; fi < bi; fi++,fci = nci)
      {
        nci = action(fci,*fi);
        if (!nci)
          break;
      }

      if (fi == bi)
      {
        if (fci != ci)
        {
          coincidence(fci,ci);
          return SR_Coincidence;
        }
        return SR_Complete;
      }

      for (; bi > fi; bi--,bci = nci)
      {
        nci = action(bci,maf.inverse(bi[-1]));
        if (!nci)
          break;
      }

      if (fi == bi)
      {
        coincidence(fci,bci);
        return SR_Coincidence;
      }

      if (fi + 1 == bi)
      {
        /* here we are making a deduction and setting fci^g and bci^ig
           which are currently undefined, (but which may possibly be the
           same transition!) */
        set_action(fci,*fi,bci);
        return SR_Deduction;
      }
      return SR_Incomplete;
    }

    /**/

    Scan_Result scan_gap01(Coset_ID ci,const Fast_Word &word)
    {
      if (!is_live_coset(ci))
        return SR_No_Coset;
      const Ordinal * fi = word.buffer;
      const Ordinal * bi = fi+word.length;
      Coset_ID full_ci = filler->position();
      bool fillable = !gap_require_low || ci < full_ci;
      Coset_ID fci = action(ci,*fi++),bci = ci,nci;

      for (; fi < bi; fi++,fci = nci)
      {
        nci = action(fci,*fi);
        if (!nci)
          break;
        if (nci < full_ci)
          fillable = true;
      }

      if (fi == bi)
      {
        if (fci != bci)
        {
          coincidence(fci,bci);
          return SR_Coincidence;
        }
        return SR_Complete;
      }

      for (; bi > fi; bi--,bci = nci)
      {
        nci = action(bci,maf.inverse(bi[-1]));
        if (!nci)
          break;
        if (nci < full_ci)
          fillable = true;
      }

      if (fi + 2 < bi)
        return SR_Incomplete;

      if (fi+2 == bi)
      {
        if (fillable && word.length > 3)
        {
          if (!check_fill_factor ||
              filler->position() * options.fill_factor > live_cosets)
          {
            if ((front_fill || fci <= bci) && !back_fill)
            {
              if (queue_gaps)
              {
                queue_definition(fci,*fi);
                return SR_Incomplete;
              }
              else
              {
                fci = define_coset(fci,*fi++);
                if (!fci)
                {
                  // if we can't fill the gap don't fail here
                  // but just forget about it
                  failed = false;
                  return SR_Incomplete;
                }
                gaps_taken++;
                /* and make the deduction that is inevitable.
                   We have to check the action is not yet defined,
                   in case we initially had fci and bci equal, and
                   the relators were not cyclically reduced and we
                   were at a xX gap! */
                if (action(fci,*fi)!=0)
                  set_action(fci,*fi,bci);
                return SR_Deduction;
              }
            }
            else
            {
              if (queue_gaps)
              {
                queue_definition(bci,maf.inverse(bi[-1]));
                return SR_Incomplete;
              }
              else
              {
                bci = define_coset(bci,maf.inverse(*--bi));
                if (!bci)
                {
                  // if we can't fill the gap don't fail here
                  // but just forget about it
                  failed = false;
                  return SR_Incomplete;
                }
                gaps_taken++;
                /* and make the deduction that is inevitable.
                   We have to check the action is not yet defined,
                   in case we initially had fci and bci equal, and
                   the relators were not cyclically reduced and we
                   were at a xX gap! */
                if (action(fci,*fi)!=0)
                  set_action(fci,*fi,bci);
                return SR_Deduction;
              }
            }
          }
        }
        return SR_Incomplete;
      }

      if (fi == bi)
      {
        coincidence(fci,bci);
        return SR_Coincidence;
      }
      /* here we are making a deduction and setting fci^g and bci^ig
         which are currently undefined, (but which may possibly be the
         same transition!) */
      set_action(fci,*fi,bci);
      return SR_Deduction;
    }

    /**/

    void queue_definition(Coset_ID ci,Ordinal g)
    {
      gap_ci[gap_end] = ci;
      if (pack_transitions)
        gap_packed_g[gap_end] = g;
      else
        gap_g[gap_end] = g;
      if (++gap_end == options.queued_definitions)
        gap_end = 0;
      if (gap_end == gap_start)
      {
        // We have to keep one slot empty so we can tell the difference between
        // full and empty. We just throw oldest entry away
        if (++gap_start == options.queued_definitions)
          gap_start = 0;
      }
    }

    /**/

    Scan_Result scan_gap10(Coset_ID ci,const Fast_Word &word,Element_ID relator_nr)
    {
      if (!is_live_coset(ci))
        return SR_No_Coset;
      const Ordinal * fi = word.buffer;
      const Ordinal * bi = fi + word.length;
      Coset_ID fci = action(ci,*fi++),bci = ci,nci;

      for (; fi < bi; fi++,fci = nci)
      {
        nci = action(fci,*fi);
        if (!nci)
          break;
      }

      if (fi == bi)
      {
        if (fci != ci)
        {
          coincidence(fci,ci);
          return SR_Coincidence;
        }
        return SR_Complete;
      }

      for (; bi > fi; bi--,bci = nci)
      {
        nci = action(bci,maf.inverse(bi[-1]));
        if (!nci)
          break;
      }

      if (fi == bi)
      {
        coincidence(fci,bci);
        return SR_Coincidence;
      }

      if (fi + 1 == bi)
      {
        /* here we are making a deduction and setting fci^g and bci^ig
           which are currently undefined, (but which may possibly be the
           same transition!) */
        set_action(fci,*fi,bci);
        return SR_Deduction;
      }
      no_candidates = false;
      scan_candidates[relator_nr].update(coset_manager,ci,Word_Length(bi - fi));
      return SR_Incomplete;
    }

    /**/

    Scan_Result scan_gap11(Coset_ID ci,const Fast_Word &word,Element_ID relator_nr)
    {
      if (!is_live_coset(ci))
        return SR_No_Coset;
      const Ordinal * fi = word.buffer;
      const Ordinal * bi = fi+word.length;
      Coset_ID full_ci = filler->position();
      bool fillable = !gap_require_low || ci < full_ci;
      Coset_ID fci = action(ci,*fi++),bci = ci,nci;

      for (; fi < bi; fi++,fci = nci)
      {
        nci = action(fci,*fi);
        if (!nci)
          break;
        if (nci < full_ci)
          fillable = true;
      }

      if (fi == bi)
      {
        if (fci != bci)
        {
          coincidence(fci,bci);
          return SR_Coincidence;
        }
        return SR_Complete;
      }

      for (; bi > fi; bi--,bci = nci)
      {
        nci = action(bci,maf.inverse(bi[-1]));
        if (!nci)
          break;
        if (nci < full_ci)
          fillable = true;
      }

      if (fi + 2 < bi)
      {
        no_candidates = false;
        scan_candidates[relator_nr].update(coset_manager,ci,Word_Length(bi - fi));
        return SR_Incomplete;
      }

      if (fi+2 == bi)
      {
        if (fillable && word.length > 3)
        {
          if (!check_fill_factor ||
              filler->position() * options.fill_factor > live_cosets)
          {
            if ((front_fill || fci <= bci) && !back_fill)
            {
              if (queue_gaps)
              {
                queue_definition(fci,*fi);
                return SR_Incomplete;
              }
              else
              {
                fci = define_coset(fci,*fi++);
                if (!fci)
                {
                  // if we can't fill the gap don't fail here
                  // but just forget about it
                  failed = false;
                  return SR_Incomplete;
                }
                gaps_taken++;
                /* and make the deduction that is inevitable.
                   We have to check the action is not yet defined,
                   in case we initially had fci and bci equal, and
                   the relators were not cyclically reduced and we
                   were at a xX gap! */
                if (action(fci,*fi)!=0)
                  set_action(fci,*fi,bci);
                return SR_Deduction;
              }
            }
            else
            {
              if (queue_gaps)
              {
                queue_definition(bci,maf.inverse(bi[-1]));
                return SR_Incomplete;
              }
              else
              {
                bci = define_coset(bci,maf.inverse(*--bi));
                if (!bci)
                {
                  // if we can't fill the gap don't fail here
                  // but just forget about it
                  failed = false;
                  return SR_Incomplete;
                }
                gaps_taken++;
                /* and make the deduction that is inevitable.
                   We have to check the action is not yet defined,
                   in case we initially had fci and bci equal, and
                   the relators were not cyclically reduced and we
                   were at a xX gap! */
                if (action(fci,*fi)!=0)
                  set_action(fci,*fi,bci);
                return SR_Deduction;
              }
            }
          }
        }
        no_candidates = false;
        scan_candidates[relator_nr].update(coset_manager,ci,Word_Length(bi - fi));
        return SR_Incomplete;
      }

      if (fi == bi)
      {
        coincidence(fci,bci);
        return SR_Coincidence;
      }
      /* here we are making a deduction and setting fci^g and bci^ig
         which are currently undefined, (but which may possibly be the
         same transition!) */
      set_action(fci,*fi,bci);
      return SR_Deduction;
    }

    void clean_log()
    {
      if (!full_scan_required)
      {
        Element_ID new_log_top = 0,i;
        for (i = 0; i < log_top; i++)
          if (is_live_coset(log_entry_ci[i]))
          {
            if (i != new_log_top)
            {
              log_entry_ci[new_log_top] = log_entry_ci[i];
              if (pack_transitions)
                log_entry_packed_g[new_log_top] = log_entry_packed_g[i];
              else
                log_entry_g[new_log_top] = log_entry_g[i];
            }
            new_log_top++;
          }
        log_top = new_log_top;
      }
    }

    void schedule_full_scan()
    {
      full_scan_required = true;
      log_top = 1; /* this makes it easier to test whether to
                            call seek_consequences(), which will do
                            the right thing */
    }

    void seek_consequences()
    {
      /* This method is the heart of Felsch style coset enumeration. It seeks
         to discover all consequences of the data already in the coset table.
         In order to do so it examines the log detailing where entries have
         been added or changed and checks all such entries to see if new
         deductions or coincidences result.

         In the absence of such a log the only option is to scan the entire
         table. I call this process is called "full scanning", though it
         is more usually referred to as "look ahead".
      */
      unsigned loops = 0;
      Cursor cursor(coset_manager);

      while (log_top)
      {
        if (undefined_transitions == 0)
          gap_start = gap_end = 0;
        // if the log is very large it is probably quicker to do a full scan.
        if (Unsigned_Long_Long(log_top) * rc.minimum_deduction_cost() >
            Unsigned_Long_Long(live_cosets) * nr_full_scan_relators)
          schedule_full_scan();

        if (!full_scan_required)
        {
          /* while none of the scans we do yield new information we can
             perform an inner loop here. But as soon as we have new information
             we need to drop out so that we can think again about whether to
             switch to full scanning, or doing a final check */
          bool ok_to_continue = true;
          do
          {
            log_top--;
            if (is_live_coset(log_entry_ci[log_top]))
            {
              cursor.set_position(log_entry_ci[log_top]);
              if (!(char) ++loops)
                status("Consequence checking",&cursor);

              if (undefined_transitions != 0)
              {
                if (!scan_candidates)
                {
                  if (!(gap_fill ? gap01_scan(cursor) : gap00_scan(cursor)))
                    ok_to_continue = false;
                }
                else if (!(gap_fill ? gap11_scan(cursor) : gap10_scan(cursor)))
                  ok_to_continue = false;
              }
              else if (!final_scan(cursor))
                ok_to_continue = false;
              cursor.end_current();
            }
            /* Because all_permuted_relators includes inverses as well as cyclic
               permutations there is no need to scan the position of the reverse
               transition */
          }
          while (log_top && ok_to_continue);

          /* if we are about to run out of consequences from the last
             definition then gap fill if possible */
          while (gap_start != gap_end && log_top==0)
          {
            Coset_ID ci = gap_ci[gap_start];
            Ordinal g = pack_transitions ? gap_packed_g[gap_start] : gap_g[gap_start];
            if (++gap_start == options.queued_definitions)
              gap_start = 0;
            if (is_live_coset(ci) && !action(ci,g))
              if (!define_coset(ci,g))
              {
                /* we won't treat a failure to fill a gap as fatal because
                   there may be space to recover. We just drop out of
                   deduction processing (look_ahead can't be on so a full
                   scan won't be scheduled), so we will drop through to
                   recover_space() */
                failed = false;
                break;
              }
              else
                gaps_taken++;
          }
        }
        else
          do_full_scan();
      }

      recover_space();

#ifdef DEBUG
      validate();
#endif
    }


    /* The next five methods only differ in the scan function that is called.
       I tried using a function pointer,but these methods are so performance
       critical that it seems better just to repeat the code. Separating
       things out like this is probably good since the function to
       be called will almost always be predicted correctly. */

    bool gap00_scan(Cursor & cursor)
    {
      Ordinal g = pack_transitions ? log_entry_packed_g[log_top] :
                                     log_entry_g[log_top];
      Element_Count start,end;
      rc.get_range(&start,&end,g);
      Fast_Word fw;
      Scan_Result sr;
      bool retcode = true;
      for (Element_ID r = start; r < end;r++)
      {
        rc.get_relator(&fw,r);
        sr = scan_gap00(cursor.current(),fw);
        if (sr == SR_No_Coset)
          return false; // something must have changed since coset was live
        if (sr > SR_Complete)
          retcode = false;
      }
      return retcode;
    }

    bool gap01_scan(Cursor & cursor)
    {
      Ordinal g = pack_transitions ? log_entry_packed_g[log_top] :
                                     log_entry_g[log_top];
      Element_Count start,end;
      rc.get_range(&start,&end,g);
      Fast_Word fw;
      Scan_Result sr;
      bool retcode = true;
      for (Element_ID r = start; r < end;r++)
      {
        rc.get_relator(&fw,r);
        sr = scan_gap01(cursor.current(),fw);
        if (sr == SR_No_Coset)
          return false; // something must have changed since coset was live
        if (sr > SR_Complete)
          retcode = false;
      }
      return retcode;
    }

    bool gap10_scan(Cursor & cursor)
    {
      Ordinal g = pack_transitions ? log_entry_packed_g[log_top] :
                                     log_entry_g[log_top];
      Element_Count start,end;
      rc.get_range(&start,&end,g);
      Fast_Word fw;
      Scan_Result sr;
      bool retcode = true;
      for (Element_ID r = start; r < end;r++)
      {
        rc.get_relator(&fw,r);
        sr = scan_gap10(cursor.current(),fw,r);
        if (sr == SR_No_Coset)
          return false; // something must have changed since coset was live
        if (sr > SR_Complete)
          retcode = false;
      }
      return retcode;
    }

    bool gap11_scan(Cursor & cursor)
    {
      Ordinal g = pack_transitions ? log_entry_packed_g[log_top] :
                                     log_entry_g[log_top];
      Element_Count start,end;
      rc.get_range(&start,&end,g);
      Fast_Word fw;
      Scan_Result sr;
      bool retcode = true;
      for (Element_ID r = start; r < end;r++)
      {
        rc.get_relator(&fw,r);
        sr = scan_gap11(cursor.current(),fw,r);
        if (sr == SR_No_Coset)
          return false; // something must have changed since coset was live
        if (sr > SR_Complete)
          retcode = false;
      }
      return retcode;
    }

    bool final_scan(Cursor & cursor)
    {
      Ordinal g = pack_transitions ? log_entry_packed_g[log_top] :
                                     log_entry_g[log_top];
      Element_Count start,end;
      rc.get_range(&start,&end,g);
      Fast_Word fw;
      Scan_Result sr;
      bool retcode = true;
      for (Element_ID r = start; r < end;r++)
      {
        rc.get_relator(&fw,r);
        sr = scan_final(cursor.current(),fw);
        if (sr == SR_No_Coset)
          return false; // something must have changed since coset was live
        if (sr > SR_Complete)
          retcode = false;
      }
      return retcode;
    }

    void do_full_scan()
    {
      /* Check whether any deductions or coincidences follow from the
         current state of the coset table. This method is used if either
         we are doing an HLT style "look ahead", or we are entering a
         "Felsch" style phase from one which did not log changes to the
         coset table, or we have discarded the log for whatever reason.

         In this method I only scan with the base relators ("R type" scanning),
         and not any of the cyclic permutations ("C type" scanning). ACE does,
         or at least supports, C type full scanning. It is not obvious that
         only doing an R type scan is correct when we are doing a Felsch style
         enumeration, or that we can skip any rows that have already been
         processed HLT style. We certainly hope it is OK because a C type
         scan is horribly slow, especially if the relators are long.

         We can see that this is OK via the following argument.

         1) The only scans that could possibly affect the eventual fate of
         the enumeration are those which complete now, whether normally,
         with a coincidence, or with a deduction, since all incomplete scans
         will be triggered again sooner or later by a new entry in a missing
         position. Conversely, if a scan completes (possibly after making
         a deduction), then unless there is a coincidence that affects the
         scan later on, we will never get another chance to perform it again
         (unless a full scan is triggered) since no new transitions that might
         trigger a relevant scan are possible. So the next point is vital.

         2) Any such critical scan must be an R type scan starting from some
         position, and in this position the same conclusions will be drawn
         as from any other. If the scan is an R type scan in one of the
         already HLT processed rows we *know* it completes correctly.
         If a scan of a permuted relator would yield new information in one of
         these omitted rows it cannot be an R type scan in one of those rows,
         so it must be an R type scan in a row we are not going to omit.
         Since we don't ever stop before the end, except if we are going to
         start another full scan, we shall draw the correct conclusions
         at some point.

         I am not sure why ACE does C type scans. There might be some point
         in doing this if, short of a full log, one knew a range of rows outside
         which there were no entries that had not been scanned Felsch style.
      */

      consequences_skipped = full_scan_required = false;
      log_top = 0;
      const Relator_Set &rs = rc.get_relator_set(0,false,false);
      const Element_Count nr_relators = rs.count();
      Coset_ID ok_ci = scanner->next();
      full_scanner = coset_manager.get_new_cursor();
      if (ok_ci > 1)
      {
        /* -1 because we might not have finished current row */
        full_scanner->set_position(ok_ci-1);
      }
      Fast_Word fw;
      while (full_scanner->advance())
      {
        if (is_live_coset(full_scanner->current()))
        {
          status("All positions scan",full_scanner);
          for (Element_ID r = 0; r < nr_relators;r++)
          {
            rc.get_relator(&fw,rs[r]);
            // possibly here we could look for rows that scan correctly
            // on all relators and bump up scanner position.
            if (scan_look_ahead(full_scanner->current(),fw) == SR_No_Coset)
              break;
          }
          if (full_scan_required)
            break;
        }
        full_scanner->end_current();
      }
      delete full_scanner;
      full_scanner = 0;
      if (log_purge_required)
        clean_log();
      if (look_ahead_on && dead_cosets && !full_scan_required)
      {
        /* I experimented with scheduling another full scan here
           in the case where the number of dead cosets had increased a
           lot, on the assumption that many more cosets might be
           amalgamated. This seems not to be the case in practice,
           even if a collapse is imminent.
        */
        recover_space(true);
      }
#ifdef DEBUG
      validate();
#endif
    }

    void validate()
    {
      Transition_Count tc = 0;
      Element_Count live = 0;
      Coset_ID partial_ci = filler->current();
      if (!partial_ci)
        partial_ci = filler->next();
      Coset_ID last_ci = coset_manager.last_coset();

      for (Coset_ID ci = 1; ci <= last_ci;ci++)
        if (is_live_coset(ci))
        {
          live++;
          for (Ordinal g = 0; g < nr_symbols; g++)
          {
            Coset_ID nci = action(ci,g);
            if (nci < 0)
              * (char *) 0 = 0;
            else if (nci)
            {
              tc++;
              if (action(nci,maf.inverse(g)) != ci)
                * (char *) 0 = 0;
              if (!is_live_coset(nci))
                * (char *) 0 = 0;
            }
            else if (ci < partial_ci)
              * (char *) 0 = 0;
          }
        }
      if (live != live_cosets || tc + undefined_transitions != Transition_Count(live)*nr_symbols)
        * (char *) 0 = 0;
    }

    void coincidence(Coset_ID ci1,Coset_ID ci2)
    {
      /* coincidence processing is atomic. While it is taking place
         the coset table has inconsistencies. We have to process all
         the coincidences we discover in one go. While it is taking
         place we must not attempt to scan relators or define new
         cosets.
         Following the appendix of Sims book we create the queue of
         coincidences in two special columns of the coset table to
         save memory.
      */
      unsigned loops = 0;
      queue_length = 0; // these three assignments should be superfluous, but might help CPU
      merge_head = merge_tail = 0;
      merge_pair(ci1,ci2); /* ci1 and ci2 are both live */

      if (rep_column == 0 && link_column == 1)
      {
        for (;merge_head; merge_head = link_column_data[merge_head])
        {
          queue_length--;
          for (Ordinal g = 2; g < nr_symbols;g++)
            queue_normal_edge(g);
          dead_cosets++;
          undefined_transitions -= nr_symbols;
          if (! (char) ++loops)
            status("Outer merge",0);
        }
      }
      else
      {
        for (;merge_head; merge_head = link_column_data[merge_head])
        {
          queue_length--;
          for (Ordinal g = 0; g < nr_symbols;g++)
            if (g != rep_column && g != link_column)
              queue_normal_edge(g);
          dead_cosets++;
          undefined_transitions -= nr_symbols;
          if (! (char) ++loops)
            status("Outer merge",0);
        }
      }

      if (queue_length)
        MAF_INTERNAL_ERROR(container,
                           ("Queue corrupted in Coset_Enumerate::coincidence()!\n"));
      coset_manager.check();
      /* If we are now finite get rid of any queued gaps, so recover_space()
         is more likely to purge off the top */
      if (!undefined_transitions)
        gap_start = gap_end = 0;

      /* The number of holes may now be such that compaction should be
         triggered. If compaction does not discard the log, or the log is
         really empty we might as well do this now, unless we are in
         a full table scan, in which case we can expect the number of dead
         cosets to increase further and there is no harm in waiting because no
         new cosets are being defined. But if compress would discard the log
         it is best not to compress now, and to wait until seek_consequences()
         has processed it (even though most of it may be rubbish), since then
         we do not have to trigger a full scan now.
      */
      if (!recover_space())
        log_purge_required = log_top > 0 && !full_scan_required;
#ifdef DEBUG
      validate();
#endif
    }

    void queue_normal_edge(Ordinal g)
    {
      Coset_ID action_ci = action(merge_head,g);
      if (action_ci)
      {
        Ordinal ig = maf.inverse(g);
        action(merge_head,g) = 0;
        action(action_ci,ig) = 0;
        undefined_transitions += 1 + (action_ci != merge_head || g != ig);
        Coset_ID rep_ci = representative(merge_head);
        action_ci = representative(action_ci);
        Coset_ID action_rep_ci = action(rep_ci,g);
        if (action_rep_ci)
        {
          merge_pair(action_rep_ci,action_ci); /* action_ci is live,
                                                  action_rep_ci may not be */
        }
        else
        {
          action_rep_ci = action(action_ci,ig);
          if (action_rep_ci)
            merge_pair(action_rep_ci,rep_ci); /* rep_ci is live,
                                                 action_rep_ci might not be */
          else
            set_action(rep_ci,g,action_ci);
        }
      }
    }

    void merge_pair(Coset_ID ci1,Coset_ID ci2)
    {
      /* this is more or less COINCSP() from appendix of Sims */
      inner_tail = &inner_head;
      *inner_tail = 0;
      unsigned loops = 0;

      merge_inner(representative(ci1),ci2); /* c2 is live, and we must ensure
                                               c1 is as well */
      while (inner_head)
      {
        Merge_Edge & me = *inner_head;
        Ordinal ig = maf.inverse(me.g);
        me.source_ci = representative(me.source_ci);
        me.dest_ci = representative(me.dest_ci);
        Coset_ID action_ci = action(me.source_ci,me.g);
        if (action_ci)
        {
          /* me.dest_ci is live,and action_ci must be because
             there can be no link in a special column to a coset
             being amalgamated.
             This seems to be by far the commonest case */
          merge_inner(action_ci,me.dest_ci);
        }
        else
        {
          action_ci = action(me.dest_ci,ig);
          if (!action_ci)
            set_action(me.source_ci,me.g,me.dest_ci);
          else
          {
            /* me.source_ci is live,and action_ci must be because
               there can be no link in a special column to a coset
               being amalgamated */
            /* This case seems not to arise when enumerating over a trivial
               subgroup */
            merge_inner(action_ci,me.source_ci); /* me.source_ci is live and so is action_ci */
          }
        }
        inner_head = me.next;
        me.next = free_edge;
        free_edge = &me;
        if (!(char) ++loops)
          status("Inner merge",0);
      }
    }

    void merge_inner(Coset_ID ci1,Coset_ID ci2)
    {
      /* This is based on MERGE from the appendix of Sims */

      MAF_ASSERT(is_live_coset(ci1) && is_live_coset(ci2),container,
                 ("Invalid parameter passed to Coset_Enumerator::merge_inner()\n"));
      if (ci1 != ci2)
      {
        if (ci1 < ci2)
        {
          Coset_ID temp = ci1;
          ci1 = ci2;
          ci2 = temp;
        }
        live_cosets--;
        queue_special_edge(ci1,rep_column,ci2);
        if (link_column < nr_symbols)
          queue_special_edge(ci1,link_column,ci2);
        // now we change the representative of ci1 to ci2
        rep_column_data[ci1] = -ci2;
        link_column_data[ci1] = 0;
        if (!merge_head)
          merge_head = merge_tail = ci1;
        else
          merge_tail = link_column_data[merge_tail] = ci1;
        coincidences++;
        if (++queue_length > max_queue_length)
          max_queue_length = queue_length;
      }
    }

    void queue_special_edge(Coset_ID ci1,Ordinal g,Coset_ID ci2)
    {
      /* On entry ci1 is a live coset which we are about to begin merging
         with ci2, by changing its entries in rep_column and
         link_column.
         Notice that any links from rep_column and link_column into a
         coset being amalagamated are removed before merging begins.
         Therefore ci1^g, if it is non-zero, is for another currently
         live coset, OR POSSIBLY ci1 !
         We don't need to set action(ci1,g) to 0 because it gets over-written
         by merge_inner() and cannot be looked at again before it is.
      */
      Coset_ID dest_ci = action(ci1,g);
      if (dest_ci != 0)
      {
        /* we are about to place an edge on our inner queue.
           Sims claims the length of this queue can never exceed 2, but this
           is not true with his MERGE.
           Suppose we have the following cosets and links in the special columns

           a            A                a=A          b=B
           ci1 d11 d12  ci2 d21 d22      ci1 d11 d12  ci2 d21 d22
           d11 e11 ci1  d21 e21 ci2   or d11 ci1 e11  d21 ci2 e21
           d12 ci1 e12  e22 ci2 e22      d12 e12 ci1  e22 e22 ci2

           It is certainly true that processing ci1 can add just two items to
           the queue, and it appears that when we pluck d11 that it will add at
           most one item to the queue, since the ci1 entry has been changed to 0.

           However, when we come to d11 we may decide to amalgamate with d21,
           and keep d11 rather than d21, and so d21 can add two items to the
           queue. However now d11 has an empty special column, so we can
           usually simply transfer a link rather than queuing it.
           With this modification the queue length does never exceed 2 */

        Merge_Edge & me = *free_edge;
        me.dest_ci = dest_ci;
        me.source_ci = ci2; /* because ci1 will be amalgamated with ci2 */
        MAF_ASSERT(me.dest_ci > 0 && is_live_coset(me.dest_ci),container,
                   ("Bad edge encountered in Coset_Enumerator::queue_special_edge()\n"));
        Ordinal ig = maf.inverse(g);
        action(dest_ci,ig) = 0;
        bool transfer = action(ci2,g) == 0;

        if (me.dest_ci == ci1)     // we must check for a link to self otherwise
        {                          // we create a link from a live to a dead
          me.dest_ci = ci2;        // coset in the special columns
          undefined_transitions += 1 + (g != ig);
          if (action(me.dest_ci,ig)!=0)
            transfer = false;
        }
        else
          undefined_transitions += 2;

        if (transfer)
          set_action(ci2,g,me.dest_ci);
        else
        {
          me.g = g;
          free_edge = me.next;
          me.next = 0;
          *inner_tail = &me;
          inner_tail = &me.next;
        }
      }
    }

    Coset_ID representative(Coset_ID ci)
    {
      Coset_ID nci = rep_column_data[ci];

      while (nci < 0)
      {
        ci = -nci;
        nci = rep_column_data[ci];
      }
      return ci;
    }

    void ensure_space(Element_Count required)
    {
      /* if we may run out of space with a non empty log try to recover space
         now.
         It only makes sense to call this while there are no consequences to
         check yet, but there is nothing to be gained by obtaining the space
         early if consequences are not being checked. */
      if (consequences_checked && !coset_manager.can_grow(required))
      {
        recover_space(true);
        grow_capacity();
      }
    }

    bool recover_space(bool want_compress = false)
    {
      /* We try to recover some space if allowed, first by adjusting the last
         coset down past any final run of dead cosets, and then by removing
         all holes if it is time to do so. The return code is false unless
         compress() is called, because we need to know if one has been
         done, but don't care about anything else */
      if (options.max_hole_percentage == 100 || !dead_cosets)
        return false ; /* Not allowed or nothing to do in this case */
      /* if the log or gap queue is non-empty we cannot recover dead
         cosets off the top because this would cause trouble if the
         coset numbers were reused while entries referring to the old
         dead cosets were present */
      if (gap_start == gap_end && (log_top == 0 || full_scan_required))
        dead_cosets -= coset_manager.recover_space();

      /* We try to avoid calling compress() if this would force a new full
         scan, or if we are in a full scan (during which no new cosets are
         created).
         Otherwise we compress if percentage of dead cosets is large enough.
         We test against allow_holes to avoid doing compressions when the
         current table size is tiny, and not to waste time doing lots of
         compressions when we are about to run out of space for good.*/
      if ((!log_top || full_scan_required ||
           options.compaction_mode != TCCM_Discard) && !full_scanner)
      {
        if (Unsigned_Long_Long(dead_cosets) * 100 >
            Unsigned_Long_Long(current_max_cosets)*options.max_hole_percentage)
          want_compress = true;
        if (want_compress && dead_cosets > options.allow_holes)
        {
          compress();
          return true;
        }
      }
      return false;
    }

    void compress()
    {
      /* Remove all the holes from the coset table, by moving cosets down.

         When we do this we can easily fix up most variables that control the
         enumeration as we proceed through the table.
         However, we cannot easily sort out the consequence stack as we need
         to renumber all its entries, but they are not sorted.

         Various possible solutions suggest themselves:

         1) Simply throw away the consequence stack and scan all defined
         positions.

         2) Create an array which describes the mapping from old to new
         coset numbers and use that to renumber the log entries.

         3) Somehow modify the entries in the coset table to indicate positions
         where a scan is required. Then create a new log afterwards.

         4) Sort the log by coset id, so that it can be renumbered as we go
         along.

         Solution 1 is the simplest - requiring very little code and no
         space, but there is possibly a considerable performance penalty.
         It is certainly worth implementing.

         Solution 2 on the face of it requires an extra column of data,
         but as long as there is at least one non-involutory generator that
         is not a special column we can sacrifice one entire column and
         then recreate it afterwards. This seems like a good option, as
         it preserves the order of the log, but we cannot eliminate duplicates
         in this case.

         Solution 3 requires us to sacrifice one bit from the Coset_ID to use
         as a scan needed indicator. We also need to do this in a way that
         does not interfere with the special column used for representatives.
         When we do this we have to recreate the log afterwards, and the order
         of the entries will have changed.

         Solution 4 does not require much in the way of extra data, but we
         would need to create code to sort the log, and then to deal with
         the renumbering. I have decided not to bother with this.
      */

      if (scan_candidates)
      {
        /* In this case we may have a lot of cursors. The compaction
           will be faster if we get rid of the ones whose coset is
           dead */
        Element_Count nr_relators = rc.count();
        for (Element_ID r = 0; r < nr_relators; r++)
          scan_candidates[r].check();
      }
      // now we can pop dead cosets off the top.
      // This can speed up compression a lot after a big collapse
      // especially we are using rebuild or special column compression
      dead_cosets -= coset_manager.recover_space();

      if (log_top > 0 && !full_scan_required || gap_start != gap_end)
      {
        /* so complete is false */
        switch (options.compaction_mode)
        {
          case TCCM_Discard:
            if (log_top && !full_scan_required)
              schedule_full_scan();
            gap_start = gap_end = 0;
            break;
          case TCCM_Special_Column:
            special_column_compress();
            return;
          case TCCM_Rebuild_By_Row:
          case TCCM_Rebuild_By_Column:
            compress_rebuild();
            return;
        }
      }

      /* if we are here then we don't have to worry about the
         log or gap list */
      Coset_ID last_coset = coset_manager.last_coset();
      Coset_ID last_ci = 0;
      coset_manager.begin_adjustment();
      Cursor c(coset_manager);

      for (Coset_ID ci = 1;ci <= last_coset;ci++)
      {
        if (!(char) ci)
        {
          c.set_position(ci);
          status("Compressing",&c);
        }
        if (is_live_coset(ci))
        {
          if (++last_ci != ci)
          {
            for (Ordinal g = 0; g < nr_symbols;g++)
            {
              Coset_ID nci = action(ci,g);
              if (nci != 0)
              {
                if (nci == ci)
                  nci = last_ci;
                action(nci,maf.inverse(g)) = last_ci;
              }
              action(last_ci,g) = nci;
            }
          }
        }
        else
          coset_manager.count_hole(ci);
      }
      if (coset_manager.end_adjustment() != dead_cosets)
        MAF_INTERNAL_ERROR(container,
                           ("Bad dead coset count in"
                            " Coset_Enumerator::compress()"));
      dead_cosets = 0;
    }

    void special_column_compress()
    {
      /* see long comment in compress() */

      Coset_ID last_coset = coset_manager.last_coset();
      Coset_ID last_ci = 0;
      Coset_ID * ren_column_data = column_data[ren_column];

      coset_manager.begin_adjustment();
      Cursor c(coset_manager);

      for (Coset_ID ci = 1;ci <= last_coset;ci++)
      {
        if (!(char) ci)
        {
          c.set_position(ci);
          status("Compressing",&c);
        }
        if (is_live_coset(ci))
        {
          if (++last_ci != ci)
          {
            for (Ordinal g = 0; g < nr_symbols;g++)
            {
              Coset_ID nci = action(ci,g);
              if (nci != 0)
              {
                Ordinal ig = maf.inverse(g);
                if (nci == ci)
                  nci = last_ci;
                /* we want the special column to point to the correct coset_id
                   until we have processed that coset */
                if (ig != ren_column || nci > ci)
                  action(nci,ig) = last_ci;
              }
              if (g != ren_column)
                action(last_ci,g) = nci;
            }
          }
          ren_column_data[ci] = last_ci;
        }
        else
        {
          coset_manager.count_hole(ci);
          ren_column_data[ci] = 0;
        }
      }
      if (coset_manager.end_adjustment() != dead_cosets)
        MAF_INTERNAL_ERROR(container,
                           ("Bad dead coset count in "
                            "Coset_Enumerator::special_column_compress()"));
      dead_cosets = 0;

      /* Now update the log */
      Element_ID new_log_top = 0,i;
      for (i = 0; i < log_top;i++)
      {
        if (ren_column_data[log_entry_ci[i]])
        {
          log_entry_ci[new_log_top] = ren_column_data[log_entry_ci[i]];
          if (pack_transitions)
            log_entry_packed_g[new_log_top] = log_entry_packed_g[i];
          else
            log_entry_g[new_log_top] = log_entry_g[i];
          new_log_top++;
        }
      }
      log_top = new_log_top;

      Element_ID log_entry_gap_end = i = gap_start;
      while (i != gap_end)
      {
        Ordinal g = pack_transitions ? gap_packed_g[i] :
                                       gap_g[i];
        Coset_ID ci = ren_column_data[gap_ci[i]];
        if (ci && !action(ci,g))
        {
          if (log_entry_gap_end != i)
          {
            gap_ci[log_entry_gap_end] = ci;
            if (pack_transitions)
              gap_packed_g[log_entry_gap_end] = g;
            else
              gap_g[log_entry_gap_end] = g;
          }
          if (++log_entry_gap_end == options.queued_definitions)
            log_entry_gap_end = 0;
        }
        if (++i == options.queued_definitions)
          i = 0;
      }
      gap_end = log_entry_gap_end;

      /* Finally rebuild the special column. First we set up
         all the known entries from the inverse column, then we
         check all the entries are paired correctly and erase those
         that are not */
      Coset_ID * iren_column_data = column_data[maf.inverse(ren_column)];
      for (Coset_ID ci = 1; ci <= last_ci; ci++)
        ren_column_data[iren_column_data[ci]] = ci;
      for (Coset_ID ci = 1; ci <= last_ci; ci++)
        if (iren_column_data[ren_column_data[ci]] != ci)
          ren_column_data[ci] = 0;
    }

    void compress_rebuild()
    {
      /* We indicate transitions with scans pending by multiplying
         all the entries in the able by 2, and then using the 1 bit to
         mark entries needing scans. So long as the number of cosets
         is not too big we can do this without getting integer overflow
         which might make live cosets look like eliminated ones.

         In the 32 bit world we can certainly do this because there is no
         way a coset table could have more than 2^30 entries in total, so that
         there can never be more than 2^30/3 rows (and in fact we'd be lucky
         even to get to 2^29/3 rows in practice).

         But in the 64 bit world we might reach MAX_STATES for Coset_ID
         (even if MAF supported unsigned Element_IDs, we are limited to 2^31
         cosets with 4 byte Coset_ID, because the representative special
         column means coset_ID must be signed). If Coset_ID can exceed 2^30 we
         run into sign troubles, so we take evasive action by building a
         bitset to remember the live cosets and handle the conversion of
         Coset_IDs differently.
      */

      Bit_Array * live_coset_set = 0;
      Coset_ID last_coset = coset_manager.last_coset();

      /* decide whether we are going to have integer overflow trouble and
         if so remember live/dead state in a big bitset */
      if (last_coset * 2 < 0)
      {
        if (live_cosets >= dead_cosets)
        {
          live_coset_set = new Bit_Array(last_coset+1,BAIV_One);
          for (Coset_ID ci = 1; ci <= last_coset;ci++)
            if (!is_live_coset(ci))
              live_coset_set->assign(ci,0);
        }
        else
        {
          live_coset_set = new Bit_Array(last_coset+1,BAIV_Zero);
          for (Coset_ID ci = 1; ci <= last_coset;ci++)
            if (is_live_coset(ci))
              live_coset_set->assign(ci,1);
        }
      }

      /* adjust all the coset table entries to make room for the
         bit to indicate a pending deduction */
      for (Ordinal g = 0; g < nr_symbols;g++)
      {
        Coset_ID * col_data = column_data[g];
        for (Coset_ID ci = 1;ci <= last_coset;ci++)
          col_data[ci] *= 2;
      }

      /* now set the deduction bit. We shall just set this in
         still live cosets, because if a coset has been eliminated,
         then either its transitions get transferred, in which case
         new log entries were created, or they do not, in which case
         they are now irrelevant. */
      if (pack_transitions)
        for (Element_ID i = 0; i < log_top; i++)
        {
          Coset_ID ci = log_entry_ci[i];
          if (live_coset_set ? live_coset_set->contains(ci) :is_live_coset(ci))
            action(ci,log_entry_packed_g[i]) |= 1;
        }
      else
        for (Element_ID i = 0; i < log_top; i++)
        {
          Coset_ID ci = log_entry_ci[i];
          if (live_coset_set ? live_coset_set->contains(ci) :is_live_coset(ci))
            action(ci,log_entry_g[i]) |= 1;
        }

      /* we also set the scan bit for any preferred definitions */
      while (gap_start != gap_end)
      {
        Coset_ID ci = gap_ci[gap_start];
        Ordinal g = pack_transitions ? gap_packed_g[gap_start] : gap_g[gap_start];
        if (++gap_start == options.queued_definitions)
          gap_start = 0;
        if (is_live_coset(ci) && !action(ci,g))
          action(ci,g) = 1;
      }
      gap_start = gap_end = 0;

      Coset_ID last_ci = 0;
      coset_manager.begin_adjustment();
      Cursor c(coset_manager);

      for (Coset_ID ci = 1;ci <= last_coset;ci++)
      {
        if (!(char) ci)
        {
          c.set_position(ci);
          status("Compressing",&c);
        }
        if (live_coset_set ? live_coset_set->contains(ci) : is_live_coset(ci))
        {
          if (++last_ci != ci)
          {
            for (Ordinal g = 0; g < nr_symbols;g++)
            {
              Coset_ID nci = action(ci,g);
              if (nci != 0 && nci != 1)
              {
                /* find the real coset number. The code below should
                   be safe whether or not integer overflow can happen*/
                Coset_ID true_nci = Coset_ID(Element_UID(nci) >> 1);
                if (true_nci == ci)
                  true_nci = last_ci;
                nci = (nci & 1) | (true_nci * 2);
                Coset_ID &rev = action(true_nci,maf.inverse(g));
                rev = (rev & 1) | (last_ci*2);
              }
              action(last_ci,g) = nci;
            }
          }
        }
        else
          coset_manager.count_hole(ci);
      }
      if (coset_manager.end_adjustment() != dead_cosets)
        MAF_INTERNAL_ERROR(container,
                           ("Bad dead coset count in"
                            " Coset_Enumerator::compress_rebuild()"));
      dead_cosets = 0;

      if (live_coset_set)
        delete live_coset_set;

      /* Now undo the damage to the coset table and rebuild the log
         and preferred definitions.
         It is marginally quicker to do this in column order, (and the
         deductions will probably be processed more quickly because of
         improved cache utilisation as we keep using the same relators).
         On the other hand this might significantly affect the course of the
         enumeration if there are many relators which do not contain all
         generators, though not necessarily adversely. So we offer both
         options */

      log_top = 0;
      gap_start = gap_end = 0;
      last_coset = coset_manager.last_coset();
      if (options.compaction_mode == TCCM_Rebuild_By_Column)
      {
        for (Ordinal g = 0; g < nr_symbols;g++)
        {
          Coset_ID * col_data = column_data[g];
          for (Coset_ID ci = 1;ci <= last_coset;ci++)
          {
            if (!(char) ci)
            {
              c.set_position(ci);
              status("Rebuilding deduction stack",&c);
            }
            Coset_ID &nci = col_data[ci];
            Coset_ID true_nci = Coset_ID(Element_UID(nci) >> 1);
            if (nci & 1)
            {
              if (true_nci)
              {
                log_entry_ci[log_top] = ci;
                if (pack_transitions)
                  log_entry_packed_g[log_top] = g;
                else
                  log_entry_g[log_top] = g;
                log_top++;
              }
              else
              {
                gap_ci[gap_end] = ci;
                if (pack_transitions)
                  gap_packed_g[gap_end] = g;
                else
                  gap_g[gap_end] = g;
                gap_end++;
              }
            }
            nci = true_nci;
          }
        }
      }
      else
      {
        for (Coset_ID ci = 1;ci <= last_coset;ci++)
        {
          if (!(char) ci)
          {
            c.set_position(ci);
            status("Rebuilding deduction stack",&c);
          }
          for (Ordinal g = 0; g < nr_symbols;g++)
          {
            Coset_ID &nci = action(ci,g);
            Coset_ID true_nci = Coset_ID(Element_UID(nci) >> 1);
            if (nci & 1)
            {
              if (true_nci)
              {
                log_entry_ci[log_top] = ci;
                if (pack_transitions)
                  log_entry_packed_g[log_top] = g;
                else
                  log_entry_g[log_top] = g;
               log_top++;
              }
              else
              {
                gap_ci[gap_end] = ci;
                if (pack_transitions)
                  gap_packed_g[gap_end] = g;
                else
                  gap_g[gap_end] = g;
                gap_end++;
              }
            }
            nci = true_nci;
          }
        }
      }
    }

    void standardise()
    {
      /* we only call this when an enumeration for which the coset table is
         required successfully completes */
      Coset_ID last_ci = coset_manager.last_coset();
      if (live_cosets != coset_manager.last_coset())
      {
        compress();
        last_ci = coset_manager.last_coset();
      }

      if (!options.build_standardised)
      {
        /* full standardisation is required unless we set the
           build_standardised_option. If we did that the coset
           table must now be already standardised */
        Coset_ID bfs_ci = 2;
        for (Coset_ID ci = 1;bfs_ci <= last_ci;ci++)
        {
          for (Ordinal g = 0; g < nr_symbols;g++)
          {
            Coset_ID nci = action(ci,g);
            if (nci >= bfs_ci)
            {
              if (nci > bfs_ci)
                exchange(bfs_ci,nci);
              if (++bfs_ci > last_ci)
                return;
            }
          }
        }
      }
    }

    void exchange(Coset_ID dest_ci, Coset_ID src_ci)
    {
      /* exchange states dest_ci and src_ci. We have to exchange
         x/X pairs in one go to avoid making mistakes, but this
         is much better than the method suggested in the "Handbook of
         computational group theory".
         On entry src_ci is guaranteed to be live, but dest_ci might
         not be.
      */

      if (is_live_coset(dest_ci))
      {
        for (Ordinal g = 0; g < nr_symbols;g++)
        {
          Ordinal ig = maf.inverse(g);
          if (ig >= g)
          {
            Coset_ID nci_d = action(dest_ci,g);
            Coset_ID nci_s = action(src_ci,g);
            Coset_ID nci_id = action(dest_ci,ig);
            Coset_ID nci_is = action(src_ci,ig);
            if (nci_d == src_ci)
              nci_d = dest_ci;
            else if (nci_d == dest_ci)
              nci_d = src_ci;
            if (nci_s == src_ci)
              nci_s = dest_ci;
            else if (nci_s == dest_ci)
              nci_s = src_ci;

            action(dest_ci,g) = nci_s;
            action(src_ci,g) = nci_d;
            if (nci_s != 0)
              action(nci_s,ig) = dest_ci;
            if (nci_d != 0)
              action(nci_d,ig) = src_ci;

            if (g != ig) /* in fact we could execute this code for involutions
                            as well, but it might be marginally quicker this way */
            {
              if (nci_id == src_ci)
                nci_id = dest_ci;
              else if (nci_id == dest_ci)
                nci_id = src_ci;
              if (nci_is == src_ci)
                nci_is = dest_ci;
              else if (nci_is == dest_ci)
                nci_is = src_ci;
              action(dest_ci,ig) = nci_is;
              action(src_ci,ig) = nci_id;
              if (nci_is != 0)
                action(nci_is,g) = dest_ci;
              if (nci_id != 0)
                action(nci_id,g) = src_ci;
            }
          }
        }
      }
      else
      {
        for (Ordinal g = 0; g < nr_symbols;g++)
        {
          Ordinal ig = maf.inverse(g);
          if (ig >= g)
          {
            Coset_ID nci_s = action(src_ci,g);
            Coset_ID nci_is = action(src_ci,ig);
            if (nci_s == src_ci)
              nci_s = dest_ci;

            action(dest_ci,g) = nci_s;
            if (nci_s != 0)
              action(nci_s,ig) = dest_ci;
            if (g != ig) /* in fact we could execute this code for involutions
                            as well, but it might be marginally quicker this way */
            {
              if (nci_is == src_ci)
                nci_is = dest_ci;
              action(dest_ci,ig) = nci_is;
              if (nci_is != 0)
                action(nci_is,g) = dest_ci;
            }
          }
        }
        rep_column_data[src_ci] = -1;
      }
    }
};

/**/

Language_Size MAF::enumerate_cosets(FSA **coset_table,
                                    const Word_Collection &normal_closure_generators,
                                    const Word_Collection &subgroup_generators,
                                    const TC_Enumeration_Options & options)
{
  /* Enumerate the cosets of the specified, possibly trivial subgroup, of the
     underlying group of the current MAF object */

  Coset_Enumerator * ce = new Coset_Enumerator(*this);
  Language_Size retcode = ce->enumerate(normal_closure_generators,
                                        subgroup_generators,options,
                                        coset_table!=0);
  if (!coset_table)
    delete ce;
  else
    *coset_table = ce;
  return retcode;
}

/**/

Language_Size MAF::enumerate_cosets(FSA **coset_table,const TC_Enumeration_Options & options)
{
  /* Enumerate the cosets of the subgroup naturally associated with the current
     MAF object, i.e. the subgroup for the coset system, or the trivial subgroup
     for a group MAF object */

  Word_List sub_generators(group_alphabet());
  Word_List normal_sub_generators(group_alphabet());
  Ordinal_Word ow(group_alphabet());
  if (is_normal_coset_system)
    subgroup_generators(&normal_sub_generators);
  else if (is_coset_system)
    subgroup_generators(&sub_generators);
  return enumerate_cosets(coset_table,normal_sub_generators,
                          sub_generators,options);
}
