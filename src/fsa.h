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
$Log: fsa.h $
Revision 1.18  2011/06/01 07:30:05Z  Alun
Added labelled_multiplier parameter to FSA_Factory::composite()
Revision 1.17  2010/06/15 08:59:22Z  Alun
Added fast_new_state() to speed up gpsublowindex
Revision 1.16  2010/05/17 07:45:59Z  Alun
Various new methods added to FSA_Factory.
Some data members made const in the hope of helping optimising compilers.
Changes to management of initial and accepting states to reduce memory
churn
Revision 1.15  2009/11/05 11:49:04Z  Alun
Couple for methods made APIMETHOD, and new private method for use by
FSA_Factory::minimise() .
Revision 1.14  2009/10/12 22:33:13Z  Alun
Compatibility of FSA utilities with KBMAG improved.
Compatibility programs produce fewer FSAs KBMAG would not (if any)
Revision 1.13  2009/09/13 20:31:34Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Language_Size data type introduced and used where appropriate
Revision 1.12  2009/06/18 10:57:35Z  Alun
Various new FSA_Factory methods, and methods for seeing if word can be
repeated infinitely often
Revision 1.12  2008/11/03 00:49:52Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.11  2008/10/09 08:40:23Z  Alun
New FSA_Factory methods added
Revision 1.10  2008/09/23 12:19:03Z  Alun
Final version built using Indexer.
Revision 1.4  2007/11/15 22:58:12Z  Alun
*/
#ifndef FSA_INCLUDED
#pragma once
#define FSA_INCLUDED 1

/*
   In this implementation most routines that work with FSAs will expect a
   FSA *; the FSA class is an abstract interface for FSAs. There are
   currently three main implementations of the interface in the package.
   FSA_Simple implements the interface using either dense or sparse
   transition structure. It is not suitable for FSAs where the number of
   states is not known in advance, and states can only be added or removed
   by creating a new FSA which is mostly a copy of the original. It is however
   possible to change the transitions between the states.
   Keyed_FSA (defined in keyedfsa.h) is mostly used only for building FSAs in
   MAF, and can cope with addition and removal of states. Delegated_FSA is a
   base class for objects which  should contain an FSA which they want to
   expose (possibly partially) but which will also expose other interfaces.
   All FSAs are actually built on top of the FSA_Common base class, which has
   generic code for dealing with labels, and accepting and initial states.

   All the code is written on the assumption that that state 0,
   represents the failure state. In all deterministic FSAs built by this
   implementation the initial state is 1, but the code should cope with FSAs
   that perversely use some other state.

   Both MIDFAs and FSAs are supported. However, MIDFA support was added late
   and some methods may not work properly with them, either because they
   should refuse to work but do not, or because they should work but have not
   yet been made to do so. In quite a few cases MIDFAs are supported by doing
   the work on a determinised version of the FSA.
   In this case it would advisable for the caller to do the conversion
   itself first, because if the conversions are repeated there is an
   obvious performance impact.

   Similar comments apply to index automata.

   There are few checks for accessibility of states at the moment because
   none of the automata MAF builds ever have inaccessible states.
*/

#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif
#ifndef AWCC_INCLUDED
#include "awcc.h"
#endif
#ifndef MAF_SSI_INCLUDED
#include "maf_ssi.h"
#endif

// classes referred to but defined elsewhere
class Alphabet;
class Container;
class Element_List;
class MAF;
class Ordinal_Word;
class Output_Stream;
class Special_Subset;
class Word;
class Word_List;
class Packed_Word;
class Word_Collection;

typedef Element_ID Label_ID;
typedef Label_ID Label_Count;

//classes defined in this header file
class Transition_Compressor;
class FSA;
class FSA_Common;
class FSA_Simple;
class Delegated_FSA;

typedef Special_Subset_Iterator State_Subset_Iterator;

class Transition_Compressor
{
  private:
    const Transition_ID nr_symbols;
    const size_t mask_size;
    const size_t buffer_size;
    bool expect1;
  public:
    Byte * const cdata;
  public:
    Transition_Compressor(Transition_ID nr_symbols);
    ~Transition_Compressor();
    size_t compress(const State_ID * buffer);
    size_t key_for_decided_state(State_ID key); /* for use by minimise() */
    void decompress(State_ID * buffer,const Byte * cdata) const;
    State_ID new_state(const Byte * cdata,Transition_ID ti) const;
};

struct State_Definition
{
  State_ID state;
  Transition_ID symbol_nr;
};

struct Accept_Definition
{
  unsigned distance;
  Transition_ID symbol_nr;
};

enum Transition_Storage_Format
{
  TSF_Default,
  TSF_Dense,
  TSF_Sparse
};

/* output format flags for FSAs */
const unsigned FF_DENSE = 1;
const unsigned FF_SPARSE = 2;
const unsigned FF_COMMENT = 4;
const unsigned FF_ANNOTATE = 8;
const unsigned FF_ANNOTATE_TRANSITIONS = 16;

enum FSR_Type
{
  FSR_Simple,
  FSR_Identifiers,
  FSR_Strings,
  FSR_Words,
  FSR_List_Of_Words,
  FSR_List_Of_Integers,
  FSR_Labelled,
  FSR_Labeled,
  FSR_Product
};

enum Label_Type
{
  LT_Unlabelled = FSR_Simple,
  LT_Identifiers = FSR_Identifiers,
  LT_Strings = FSR_Strings,
  LT_Words = FSR_Words,
  LT_List_Of_Words = FSR_List_Of_Words,
  LT_List_Of_Integers = FSR_List_Of_Integers,
  LT_Last_Supported = LT_List_Of_Integers,
  LT_Custom
};

enum Label_Association
{
  LA_Auto,
  LA_Direct,
  LA_Mapped
};

typedef Special_Subset_Format Accept_Type;
typedef Special_Subset_Format Initial_Type;

/* GAP Style FSA flags
   The high-level FSA_Factory methods, and the automata building functions of
   MAF should set these flags correctly.
   If you mess around with an FSA, and manually alter its transition table,
   either by calling set_transitions(), or by editing a GAP file, then the
   flags will most likely be wrong. If in doubt about a flag then clear it.
   Most FSA methods in MAF do not look at these flags at all.
*/
const unsigned GFF_DFA = 1; /* FSA has a deterministic transition table and a
                               single initial state */
const unsigned GFF_NFA = 2; /* FSA may have epsilon transitions and more than
                               one initial state - MAF does not support these */
const unsigned GFF_MIDFA = 4; /* FSA may have more than one initial state but
                                 has deterministic transtion table */
const unsigned GFF_MINIMISED = 8; /* The FSA has a deterministic transition
                                    table and as few states as possible for
                                    the language it accepts */
const unsigned GFF_MINIMIZED = GFF_MINIMISED;
const unsigned GFF_ACCESSIBLE = 16; /* All states are reachable from an
                                       initial state (except possibly
                                       the failure state) */
const unsigned GFF_TRIM = 32; /* Every state is reachable from an initial
                                 state (except possibly the failure state)
                                 and an accepted state can be reached from
                                 every state except the failure state */
const unsigned GFF_SPARSE = 64; /* A sparse storage format is advised */
const unsigned GFF_DENSE = 128; /* A dense storage output is advised */
const unsigned GFF_BFS = 256; /* The states occur in breadth first search
                                 order as determined by the alphabet */
const unsigned GFF_RWS = 512; /* The transition table may contain -ve numbers
                                 which are keys into a list of rewriting
                                 rules */


class FSA : public Special_Subset_Owner
{
  friend class Delegated_FSA;
  protected:
    Letter * name;
  public:
    class Word_Iterator;
    class Principal_Value_Cache
    {
      friend class FSA;
      private:
        Packed_Word * defining_prefix;
        Packed_Word * accepting_suffix;
      public:
        Principal_Value_Cache(const FSA &fsa);
        ~Principal_Value_Cache();
    };
    const Alphabet & base_alphabet;
    static String const flag_names[];
    static String const state_types[];
  public:
    FSA(Container & container_,const Alphabet &alphabet);
    virtual ~FSA();
    /* Methods deriving class must implement. Although this will
       appear to be an intimidatingly long list, most of the methods
       are implemented by FSA_Common and are pure virtual here so that
       Delegated_FSA can derive direct from FSA */
    virtual State_ID accepting_state() const = 0;
    virtual State_ID accepting_state(State_Subset_Iterator &ssi,bool first) const = 0;
    virtual Accept_Type accept_type() const = 0;
    virtual Transition_ID alphabet_size() const = 0;
    virtual void create_definitions(bool fast = false) = 0;
    virtual void create_accept_definitions() = 0;
    virtual const State_ID * dense_transition_table() const
    {
      return 0;
    }
    virtual void ensure_dense() {};
    /* get_definition() returns true if state definitions are available and
       state is not an initial state, in which case the supplied buffer is
       filled in */
    virtual bool get_definition(State_Definition * definition,
                                State_ID state) const = 0;
    /* get_accept_definition() returns true if state definitions are available
       and state is not an accept state, in which case the supplied buffer is
       filled in */
    virtual bool get_accept_definition(Accept_Definition * definition,
                                       State_ID state) const = 0;
    virtual unsigned get_flags() const = 0;
    virtual const void * get_label_data(Label_ID label) const = 0;
    virtual Label_ID get_label_nr(State_ID state) const = 0;
    virtual State_ID initial_state() const = 0;
    virtual State_ID initial_state(State_Subset_Iterator &ssi,bool first) const = 0;
    virtual Initial_Type initial_type() const = 0;
    virtual bool is_accepting(State_ID state) const = 0;
    virtual bool is_initial(State_ID state) const = 0;
    virtual const Alphabet & label_alphabet() const = 0;
    virtual Label_Count label_count() const = 0;
    virtual size_t label_size(Label_ID label) const = 0;
    /* A non-negative value for index retrieves the text for the nth value in
       the list when the label has type List_Of_Words or List_Of_Integers,
       If index is -1 then the entire list of labels is retrieved. */
    virtual String label_text(String_Buffer * buffer,Label_ID label,
                              int index = -1) const = 0;
    virtual Label_Type label_type() const = 0;
    /* main transition function. buffer should be true if more than one
       transition from the same initial state will be read. So it should
       be false if you are following the state chain for a single word */
    virtual State_ID new_state(State_ID initial_state,
                               Transition_ID symbol_nr,
                               bool buffer = true) const = 0;

    virtual State_Count nr_accepting_states() const = 0;
    virtual State_Count nr_initial_states() const = 0;
    virtual void print_accepting(Output_Stream * stream) const = 0;
    virtual void print_initial(Output_Stream * stream) const = 0;
    virtual Element_Count label_word_count(Label_ID) const = 0;
    // Virtual methods implemented in the base
    virtual void save(String filename,unsigned tpf = 0);
    virtual void save_as(String basefilename,String basename,String suffix,
                         unsigned tpf = 0)
    {
      String_Buffer sb;
      set_name(String::make_filename(&sb,"",basename,suffix));
      save(String::make_filename(&sb,"",basefilename,suffix),tpf);
    }
    virtual void set_name(String name_)
    {
      if (name)
        delete [] name;
      name = name_.clone();
    }

    /* state_count() should return number of states including failure state.
       So if state_count() returns n, the highest state is n-1 */
    virtual State_Count state_count() const = 0;
    virtual void get_transitions(State_ID * buffer,State_ID state) const
    {
      const Transition_ID nr_symbols = alphabet_size();
      for (Transition_ID symbol_nr = 0; symbol_nr < nr_symbols;symbol_nr++)
        buffer[symbol_nr] = new_state(state,symbol_nr);
    }
    virtual bool has_multiple_initial_states() const
    {
      return nr_initial_states() > 1;
    }
    /* Non virtual methods. First const methods - "queries" */
    /* accepting_path() only works if accept definitions are available. If so
       the shortest word which leads from this state to an accept state is
       placed in lhs_word and true is returned. The word will be length 0 if
       the state is an accept state.
       The return value is false if either definitions are unavailable or
       there is no path to an accept state.
       If the FSA is a product FSA and you want to know both words in the
       defining word pair pass a non-null value for rhs_word.
       If you want to know the accept state that is reached pass
       a non-null value for accept */
    bool accepting_path(Ordinal_Word * lhs_word,State_ID state,
                        Ordinal_Word * rhs_word = 0,State_ID * accept = 0) const;
    /* accessible states counts the number of states, including the failure
       state, that can be reached from the specified start_state. If the
       FSA is an RWS then rewrites are treated as the failure state */
    State_Count accessible_states(bool * accessible,State_ID start_state) const;

    /* defining_length() returns the length of the shortest input word that
       reaches the specified state from the initial state. The return value is
       INVALID_LENGTH if the state is inaccessible, or it is discovered that
       the FSA is not BFS sorted before the defining transition is
       encountered. This method will be faster if state_definitions
       are available. */
    virtual Word_Length defining_length(State_ID state) const;
    /* defining_word() only works if state definitions are available. If so
       the defining word for the state is placed in lhs_word and true is
       returned. If the state is an initial state the word will be length 0
       but true is still returned.
       The return value is false if either definitions are unavailable or
       the state is inaccessible. If the length of the word is already known,
       passing it as the optional third parameter will speed the method up
       slightly.
       If the FSA is a product FSA and you want to know both words in the
       defining word pair pass a non-null value for rhs_word.
       If you want to know the initial state that reaches the state pass
       a non-null value for initial */
    APIMETHOD bool defining_word(Ordinal_Word * lhs_word,State_ID state,
                                 Word_Length length = WHOLE_WORD,
                                 Ordinal_Word *rhs_word = 0,
                                 State_ID * initial = 0) const;

    String get_name() const
    {
      return name;
    }
    bool labels_are_words() const
    {
      Label_Type lt = label_type();
      return lt == LT_Words || lt == LT_List_Of_Words;
    }
    APIMETHOD bool is_product_fsa() const;
    bool is_valid_state(State_ID state) const
    {
      return state > 0 && (State_Count) state < state_count();
    }
    bool is_valid_target(State_ID state) const
    {
      return state <= 0 || (State_Count) state < state_count();
    }
    /* Both of these methods will work if an FSA has labels of type
       "Words" or "List of Words". The label will silently be converted to
       the requested format */
    APIMETHOD bool label_word(Ordinal_Word * answer,Label_ID label,Element_ID word_nr = 0) const;
    bool label_word_list(Word_List * answer,Label_ID label) const;
    /* language_size returns ULONG_MAX if language is infinite,
       ULONG_MAX-1 if language is finite, but extremely large, or
       the correct answer if it is < ULONG_MAX-1
       if exact = false is specified, then for finite languages the
       answer is always 2, unless the FSA is trivial, in which case
       the answer is 0.
       If start_state is -1 then the total size of the accepted language
       is calculated. Otherwise the answer is the number of accepted
       words that can be reached from the specified starting state. The FSA
       should be trim before this function is called, otherwise the language
       may be reported as infinite when it is not.
    */
    APIMETHOD Language_Size language_size(Word_Length * word_length,bool exact = true,State_ID start_state = -1) const;
    Language_Size language_size(bool exact = true,State_ID start_state = -1) const
    {
      return language_size(0,exact,start_state);
    }
    /* nr_accessible_states() returns the number of non-failure states of the
       FSA accessible with an input string no longer than the specified
       length. The input FSA must be BFS sorted */
    State_Count nr_accessible_states(Word_Length max_input_length = WHOLE_WORD) const;
    /* principal_value returns the shortest word(s) which pass through the
       state (and the specified transition if it is not -1) on the way from
       an initial state to an accept state */
    bool principal_value(Principal_Value_Cache * cache,
                         Ordinal_Word * lhs_word,
                         State_ID state,Transition_ID ti = -1,
                         Ordinal_Word * rhs_word = 0,
                         State_ID *initial_state = 0,
                         State_ID * accept = 0) const;
    APIMETHOD void print(Output_Stream * stream,unsigned flags = 0) const;
    APIMETHOD bool product_accepted(const Word &lhs_word,const Word & rhs_word) const;
    APIMETHOD State_ID read_product(const Word & word1,const Word & word2) const;
    APIMETHOD State_ID read_word(State_ID initial_state,const Word & word) const;
    State_ID read_word(const Word & word) const
    {
      return read_word(initial_state(),word);
    }
    /* recurrent_states() finds which states have a path from themselves
       back to themselves, and which therefore recur, fills in the recurrent
       parameter accordingly, and returns the count of recurring states
       The count includes the failure state, which always recurs.
       Note that it is perfectly possible for inaccessible states to be
       recurrent, since the method does not care whether the state is
       accessible from an initial state.
       If the all_infinite flag is true, then as well as the recurrent
       states all states reachable from the recurring states are included
       and counted. Assuming the original automaton has no inaccessible states
       this means that recurrent is instead flagging all the states that
       can occur infinitely often*/
    State_Count recurrent_states(bool * recurrent,bool all_infinite = false) const;
    APIMETHOD bool accepts(const Ordinal * word,size_t symbol_length) const;
    APIMETHOD bool accepts(const Transition_ID * word,size_t symbol_length) const;
    APIMETHOD bool accepts(const Word &word) const;
    /* repetend() returns a state from which following the transitions specified
       by the given word returns to the same state if there is such a state
       and 0 otherwise
       if allow_repeat is true, it also returns a non-zero state if there is a
       state from which reading the word repeatedly eventually returns to
       the same state.
       In neither case is there a check that the recurring state is actually
       accepting, as this would cause problems, it is assumed that
       some accept state can be reached from the repetend but that
       any required suffix is irrelevant (as for example when the
       function is used in code for drawing Kleinian limit sets) */
    APIMETHOD State_ID repetend(const Word &word,bool allow_repeat = false) const;

    /* is_repetend() returns true if and only if, starting from the specified
       state  the word can be read infinitely often. As with repetend() there
       is no check that the recurring state is accepting.*/
    APIMETHOD bool is_repetend(State_ID si,const Word &word) const;
  private:
    /* Implementation of Special_Subset_Owner interface */
    virtual Element_Count element_count() const
    {
       return state_count();
    }
    virtual bool is_valid_element(Element_ID element) const
    {
      return is_valid_state(element);
    }
  public:
    /* Word Iterator can be used to help iterate through the language of a
       FSA that reads single words. Call first(),specifying an optional
       start word, and then call next() repeatedly. You can stop the
       iterator going to a deeper level at any point by passing false as
       the "inside" parameter.
       The iterator returns all non-failed words, not just accepted words.
       Therefore you will usually want to filter the results afterwards by
       calling is_accepting() on the returned State_ID. The main reason for
       doing this is to allow the caller to break out of very deep levels
       which contain few accept states at any time, and not just on accepted
       words */
    class Word_Iterator
    {
      private:
        const FSA & fsa;
        State_ID *state;
        Word_Length depth;
        Ordinal_Word *current;
        const Ordinal nr_symbols;
        Special_Subset_Iterator ssi;
      public:
        const Ordinal_Word & word;
      public:
        Word_Iterator(const FSA & fsa_);
        ~Word_Iterator();
        State_ID first(const Ordinal_Word * start_word = 0);
        State_ID next(bool inside = true);
        State_ID start_state() const
        {
          return state[0];
        }
    };
    class Product_Iterator
    {
      private:
        const FSA & fsa;
        State_ID *state;
        Transition_ID *transitions;
        Word_Length depth;
        Ordinal_Word *current_left;
        Ordinal_Word *current_right;
        const Transition_ID nr_symbols;
        Ordinal * left_generators;
        Ordinal * right_generators;
        Special_Subset_Iterator ssi;
      public:
        const Ordinal_Word & left_word;
        const Ordinal_Word & right_word;
      public:
        Product_Iterator(const FSA & fsa_);
        ~Product_Iterator();
        State_ID first(const Ordinal_Word * start_left = 0,
                       const Ordinal_Word * start_right = 0);
        State_ID next(bool inside = true);
        Word_Length state_length() const
        {
          return depth;
        }
        State_ID start_state() const
        {
          return state[0];
        }
    };
};

class FSA_Common : public FSA
{
  friend class FSA_Factory;
  friend class FSA_Reader;
  protected:
    Special_Subset &initial;
    Special_Subset &accepting;
    Label_Count nr_labels;
    Label_Type label_format;
    Label_Association label_association;
    Label_ID *label_nr;
    const Alphabet * label_alphabet__;
    Byte ** label_data; // should be a Byte_Buffer *, but needs a little work
    Transition_Compressor * compressor;
    State_Definition *state_definitions;
    Accept_Definition *accept_definitions;
    Word_Length *lengths;
    unsigned flags;
  public:
    FSA_Common(Container & container_,const Alphabet &alphabet);
    virtual ~FSA_Common();
    // virtual method which deriving class must implement
    virtual bool set_transitions(State_ID state,
                                 const State_ID * buffer) = 0;

    virtual State_ID accepting_state() const;
    virtual State_ID accepting_state(State_Subset_Iterator &ssi,bool first) const
    {
      return ssi.item(accepting,first);
    }
    virtual Accept_Type accept_type() const;
    virtual unsigned change_flags(unsigned flags_to_set = 0,
                                  unsigned flags_to_clear = 0)
    {
      return flags = (flags & ~flags_to_clear) | flags_to_set;
    }
    APIMETHOD void clear_accepting(bool expect_big_bigset = true);
    void clear_initial(bool expect_big_bitset = false);
    Label_Count copy_labels(const FSA & fsa_start,Label_Association la = LA_Auto);
    void create_definitions(bool fast = false);
    void create_accept_definitions();
    virtual Word_Length defining_length(State_ID state) const;
    virtual bool get_definition(State_Definition * definition,State_ID state) const
    {
      if (state_definitions && is_valid_state(state) &&
          state_definitions[state].state)
      {
        *definition = state_definitions[state];
        return true;
      }
      return false;
    }
    virtual bool get_accept_definition(Accept_Definition * definition,State_ID state) const
    {
      if (accept_definitions && is_valid_state(state))
      {
        *definition = accept_definitions[state];
        return accept_definitions[state].distance != 0;
      }
      definition->distance = 0;
      return false;
    }
    virtual unsigned get_flags() const
    {
      return flags;
    }
    virtual const void * get_label_data(Label_ID label) const
    {
      if (label && label < nr_labels)
        return label_data[label];
      return 0;
    }
    virtual Label_ID get_label_nr(State_ID state) const;
    virtual Label_Count label_count() const
    {
      return nr_labels;
    }
    virtual State_ID initial_state() const;
    virtual State_ID initial_state(State_Subset_Iterator &ssi,bool first) const
    {
      return ssi.item(initial,first);
    }
    virtual Initial_Type initial_type() const;
    bool is_accepting(State_ID state) const;
    bool is_initial(State_ID state) const;
    bool is_valid_label(Label_ID label) const
    {
      return label > 0 && label < nr_labels;
    }
    const Alphabet & label_alphabet() const
    {
      return *label_alphabet__;
    }
    /* the default implementation of the next two methods
       can understand labels in any of the label
       types except LT_Custom. Override if need be. */
    virtual size_t label_size(Label_ID label) const;
    virtual String label_text(String_Buffer * buffer,Label_ID label,
                                   int index = 0) const;
    virtual Label_Type label_type() const
    {
      return label_format;
    }
    virtual Element_Count label_word_count(Label_ID) const;
    virtual State_Count nr_accepting_states() const;
    virtual State_Count nr_initial_states() const;
    void print_accepting(Output_Stream * stream) const;
    void print_initial(Output_Stream * stream) const;
    virtual void set_accept_all();
    virtual void set_initial_all();
    APIMETHOD bool set_is_accepting(State_ID state,bool is_accepting);
    bool set_is_initial(State_ID state,bool is_initial);
    void set_label_alphabet(const Alphabet & alphabet,bool repack = false);
    APIMETHOD bool set_label_nr(State_ID state,Label_ID label,bool grow = false);
    bool set_label_statelist(Label_ID label_id,const Element_List & label);
    APIMETHOD void set_label_type(Label_Type label_type_);
    /* Both of these methods will work if an FSA has labels of type
       "Words" or "List of Words". The data will silently be converted to
       the format specified in the call to set_label_type() */
    bool set_label_word(Label_ID label_id,const Word & label);
    bool set_label_word_list(Label_ID label_id,const Word_List & label);
    virtual void set_nr_labels(Label_Count nr_labels_,Label_Association la = LA_Auto);
    virtual void set_single_accepting(State_ID si);
    virtual void set_single_initial(State_ID si);
    /* set_transition() can be used to update a single transition from a state.
       The base implementation reads all the current transitions from the
       state using get_transitions(), updates the buffer,
       and then calls set_transitions().
       If the deriving class can easily implement updating a single update
       more efficiently it should over-ride this function. */
    virtual bool set_transition(State_ID state,Transition_ID symbol_nr,
                                State_ID new_state);
    void sort_labels(bool trim_only = true);
    /* tidy is called by methods that build FSAs to notify the FSA that it is
       complete. Classes derived from FSA_Common can use this as an opportunity
       to optimise the FSA or make other changes as they wish, and should then
       finally call FSA_Common::tidy() */
    virtual void tidy();
  protected:
    void allocate_compressor();
    bool set_label_data(Label_ID label_nr,const void * data,
                        size_t data_size);

    bool set_label_data(Label_ID label_nr,const void * data);
  private:
    void delete_labels();
};

/**/

class FSA_Simple : public FSA_Common
{
  private:
    State_Count nr_states;
    const Transition_ID nr_symbols;
    State_ID *dense_transitions;
    Byte_Buffer * sparse_transitions;
    mutable State_ID current_state;
    State_ID * current_transition;
    State_ID locked_state;
  public:
    FSA_Simple(Container & container,const Alphabet &alphabet,
               State_Count nr_states_,Transition_ID nr_symbols_,
               Transition_Storage_Format tsf = TSF_Default);
    ~FSA_Simple()
    {
      if (dense_transitions)
        delete [] dense_transitions;
      if (sparse_transitions)
        delete [] sparse_transitions;
      if (current_transition)
        delete [] current_transition;
    }
    virtual void ensure_dense();
    bool set_transition(State_ID si,Transition_ID ti,
                        State_ID new_state)
    {
      if (is_valid_state(si) && ti < nr_symbols && is_valid_target(new_state))
      {
        State_ID *state = state_get(si);
        state[ti] = new_state;
        if (!dense_transitions)
        {
          size_t size = compressor->compress(state);
          sparse_transitions[si].clone(compressor->cdata,size);
        }
        return true;
      }
      return false;
    }
    bool set_transitions(State_ID si,const State_ID * transitions);
    /* Virtual methods required by FSA */
    Transition_ID alphabet_size() const
    {
      return nr_symbols;
    }
    State_Count state_count() const
    {
      return nr_states;
    }
    virtual void get_transitions(State_ID * buffer,State_ID state) const
    {
      const State_ID * transitions = state_get(state);
      for (Transition_ID symbol_nr = 0; symbol_nr < nr_symbols;symbol_nr++)
        buffer[symbol_nr] = transitions[symbol_nr];
    }
    virtual const State_ID * dense_transition_table() const
    {
      return dense_transitions;
    }
    virtual State_ID new_state(State_ID initial_state,Transition_ID symbol_nr,bool buffer = true) const
    {
      if (dense_transitions || buffer || !is_valid_state(initial_state))
        return state_get(initial_state)[symbol_nr];
      return compressor->new_state(sparse_transitions[initial_state],symbol_nr);
    }
    State_ID fast_new_state(State_ID initial_state,
                            Transition_ID symbol_nr) const
    {
      /* This is a method that reads the dense transition table
          directly with no checks whatever. */
      return dense_transitions[initial_state*nr_symbols + symbol_nr];
    }

    /* remove_rewrites() removes any rewrite targets from an FSA */
    APIMETHOD void remove_rewrites();
    void sort_bfs();
    /* compare returns 0 if the two FSAs have the same accepting states
       and transition tables and -1 otherwise.
       If the return value is 0 both FSAs accept the same language.
       If the return value is -1 the FSAs may still accept the same
       language unless both have been standardised, in which case you
       can be confident that the languages are different */
    APIMETHOD int compare(const FSA_Simple &other) const;
    const State_ID * state_access(State_ID si) const
    {
      return state_get(si);
    }
  private:
    State_ID * state_get(State_ID si) const
    {
      if (!is_valid_state(si))
        si = 0;
      if (dense_transitions)
        return dense_transitions+si*nr_symbols;
      if (si != current_state)
        compressor->decompress(current_transition,sparse_transitions[current_state = si]);
      return current_transition;
    }
    void permute_states(const State_ID *permutation,bool inverse = false);
    /* Two functions below are to help avoid confusion about meaning of parameters */
  public:
    void renumber_states(const State_ID *new_state_nrs)
    {
      /* new numbers should be a permutation of 0..n-1 with 0,1 not moving!*/
      permute_states(new_state_nrs,false);
    }
    void sort_states(const State_ID *sort_order)
    {
      /* sort_order contains the current numbers of the states in the order
         in which they should appear in the rearranged FSA. If what you have
         is a list of numbers containing the desired position of the
         corresponding state then use renumber_states */
      permute_states(sort_order,true);
    }
    State_ID * state_lock(State_ID si)
    {
      if (!locked_state)
        return state_get(locked_state = si);
      return 0;
    }
    bool state_unlock(State_ID si,bool dirty = true)
    {
      if (si == locked_state)
      {
        if (sparse_transitions && dirty)
        {
          size_t size = compressor->compress(current_transition);
          sparse_transitions[si].clone(compressor->cdata,size);
        }
        locked_state = 0;
        return true;
      }
      return false;
    }
};

/* class Transition_Realiser may be used during intensive computations
   using an FSA. It will be much faster than calling new_state() repeatedly
   and somewhat faster than calling get_transitions(), especially if the
   FSA uses compression, and each row is accessed multiple times.
   Transition_Realiser will attempt to build a dense transition table, if
   one is not already available. In the case where it cannot you should
   must request enough cached rows to avoid unintentional aliasing between
   realised rows.
   Keyed_FSA also uses Transition_Realiser to provide a dense cache of
   compressed transitions, primarily for the purpose of speeding up
   Strong_Diff_Reduce.
*/

class Transition_Realiser
{
  private:
    State_ID *realised_transitions;
    const State_ID *dense_table;
    State_ID *cached_transitions;
    State_ID *cached_rows;
    Transition_ID alphabet_size;
    State_Count nr_cached_rows;
  public:
    const FSA & fsa;
    Transition_Realiser(const FSA & fsa_,size_t ceiling = 0x2000000,
                        State_Count nr_cached_rows_ = 2);
    State_ID * take()
    {
      if (realised_transitions)
      {
        State_ID * answer = realised_transitions;
        dense_table = realised_transitions = 0;
        return answer;
      }
      return 0;
    }
    void unrealise()
    {
      if (realised_transitions)
      {
        delete [] realised_transitions;
        dense_table = realised_transitions = 0;
      }
    }
    ~Transition_Realiser()
    {
      unrealise();
      if (cached_transitions)
      {
        delete [] cached_transitions;
        delete [] cached_rows;
      }
    }
    const State_ID *transition_table()
    {
      return dense_table;
    }
    const State_ID * realise_row(State_ID si,State_ID position = 0)
    {
      if (dense_table)
        return dense_table+si*alphabet_size;
      State_ID * buffer = cached_transitions+position*alphabet_size;
      cached_rows[position] = si;
      fsa.get_transitions(buffer,si);
      return buffer;
    }
    void invalidate_line(State_ID position)
    {
      cached_rows[position] = -1;
    }
    State_ID * cache_line(State_ID position) const
    {
      /* returns a pointer to the transitions in the specified row.
         It is up to the caller to use this responsibly */
      return cached_transitions + position*alphabet_size;
    }
    State_ID state_at(State_ID position) const
    {
      /* returns the state_id whose transitions are stored at the specified
         row of the cache */
      return cached_rows ? cached_rows[position] : -1;
    }
};

class Delegated_FSA : public FSA
{
  protected:
    FSA * fsa__;
    bool owner;
    Delegated_FSA(FSA *fsa_,bool owner_ = true) :
      fsa__(fsa_),
      owner(owner_),
      FSA(fsa_->container,fsa_->base_alphabet)
    {}
    // Alternate constructor for classes which the FSA is not available
    // at the point of construction
    Delegated_FSA(Container & container,const Alphabet & alphabet,
                  bool owner_ = true) :
      fsa__(0),
      owner(owner_),
      FSA(container,alphabet)
    {}
    Delegated_FSA(Delegated_FSA & other) :
      fsa__(other.fsa__),
      owner(other.owner),
      FSA(other.container,other.base_alphabet)
    {
      other.owner = false;
    }
    virtual ~Delegated_FSA()
    {
      if (owner)
        delete fsa__;
    }
  public:
    const FSA * fsa() const
    {
      return fsa__;
    }
    /* Delegated virtual methods to implement FSA interface */
    virtual State_ID accepting_state() const
    {
      return fsa__->accepting_state();
    }
    virtual State_ID accepting_state(State_Subset_Iterator &ssi,bool first) const
    {
      return fsa__->accepting_state(ssi,first);
    }
    virtual Accept_Type accept_type() const
    {
      return fsa__->accept_type();
    }
    virtual Transition_ID alphabet_size() const
    {
      return fsa__->alphabet_size();
    }
    virtual void create_accept_definitions()
    {
      fsa__->create_accept_definitions();
    }
    virtual void create_definitions(bool fast = false)
    {
      fsa__->create_definitions(fast);
    }
    virtual Word_Length defining_length(State_ID state) const
    {
      return fsa__->defining_length(state);
    }
    virtual const State_ID * dense_transition_table() const
    {
      return fsa__->dense_transition_table();
    }
    virtual void ensure_dense()
    {
      fsa__->ensure_dense();
    }
    virtual bool get_accept_definition(Accept_Definition * definition,
                                       State_ID state) const
    {
      return fsa__->get_accept_definition(definition,state);
    }
    virtual bool get_definition(State_Definition * definition,
                                       State_ID state) const
    {
      return fsa__->get_definition(definition,state);
    }
    virtual unsigned get_flags() const
    {
      return fsa__->get_flags();
    }
    virtual const void * get_label_data(Label_ID label) const
    {
      return fsa__->get_label_data(label);
    }
    virtual Label_ID get_label_nr(State_ID state) const
    {
      return fsa__->get_label_nr(state);
    }
    virtual Label_Count label_count() const
    {
      return fsa__->label_count();
    }
    virtual State_Count state_count() const
    {
      return fsa__->state_count();
    }
    virtual void get_transitions(State_ID * buffer,State_ID state) const
    {
      fsa__->get_transitions(buffer,state);
    }
    virtual bool has_multiple_initial_states() const
    {
      return fsa__->has_multiple_initial_states();
    }
    virtual State_ID initial_state() const
    {
      return fsa__->initial_state();
    }
    virtual State_ID initial_state(State_Subset_Iterator &ssi,bool first) const
    {
      return fsa__->initial_state(ssi,first);
    }
    virtual Initial_Type initial_type() const
    {
      return fsa__->initial_type();
    }
    virtual bool is_accepting(State_ID state) const
    {
      return fsa__->is_accepting(state);
    }
    virtual bool is_initial(State_ID state) const
    {
      return fsa__->is_initial(state);
    }
    virtual const Alphabet & label_alphabet() const
    {
      return fsa__->label_alphabet();
    }
    virtual size_t label_size(Label_ID label) const
    {
      return fsa__->label_size(label);
    }
    virtual String label_text(String_Buffer * buffer,Label_ID label,
                              int index = 0) const
    {
      return fsa__->label_text(buffer,label,index);
    }
    virtual Label_Type label_type() const
    {
      return fsa__->label_type();
    }
    virtual Element_Count label_word_count(Label_ID label_id) const
    {
      return fsa__->label_word_count(label_id);
    }
    virtual State_ID new_state(State_ID initial_state,
                               Transition_ID symbol_nr,
                               bool buffer = true) const
    {
      return fsa__->new_state(initial_state,symbol_nr,buffer);
    }
    virtual State_Count nr_accepting_states() const
    {
      return fsa__->nr_accepting_states();
    }
    virtual State_Count nr_initial_states() const
    {
      return fsa__->nr_initial_states();
    }
    void print_accepting(Output_Stream * stream) const
    {
      fsa__->print_accepting(stream);
    }
    void print_initial(Output_Stream * stream) const
    {
      fsa__->print_initial(stream);
    }
    void set_name(String name_)
    {
      fsa__->set_name(name_);
      FSA::set_name(name_);
    }
};

/**/


/* flags for FSA_Factory::kernel() */
const unsigned KF_ACCEPT_ALL = 1; /* Consider all states the original FSA to
                                     be accepting (except for 0) */
const unsigned KF_NO_MINIMISE = 2;
const unsigned KF_INCLUDE_ALL_INFINITE = 4; /* Allow any transition from which
                                               it may take infinitely long to
                                               reach state 0, even if an accept
                                               state cannot be reached */
const unsigned KF_MIDFA = 8;                /* Return the new FSA as a MIDFA
                                               with all states initial */

class FSA_Factory
{
  public:
    /* all_words(n) returns an FSA that accepts all words of length n or less */
    static FSA_Simple * all_words(Container & container,
                                 const Alphabet & alphabet,int max_length);
    /* flags for binop() */
    enum Binop_Flag {BF_And,BF_Or,BF_And_Not,BF_Not_And,BF_And_Not_First,BF_And_Not_First_Trim,BF_And_First};
    /* Creates an FSA with the accepted language defined by the two starting
       FSAs and the binary operation. The states are labelled using the
       labels from the first FSA, which is why both and_not and not_and
       are provided. */
    static FSA_Simple * binop(const FSA & fsa_0,const FSA & fsa_1,Binop_Flag opcode);
    // sorry about these names. "and" and "or" are now C++ reserved words
    static FSA_Simple * fsa_and(const FSA & fsa_0,const FSA & fsa_1)
    {
      return binop(fsa_0,fsa_1,BF_And);
    }
    static FSA_Simple * fsa_and_first(const FSA & fsa_0,const FSA & fsa_1)
    {
      return binop(fsa_0,fsa_1,BF_And_First);
    }
    static FSA_Simple * and_not(const FSA & fsa_0,const FSA & fsa_1)
    {
      return binop(fsa_0,fsa_1,BF_And_Not);
    }
    /* and_not_first returns the FSA which accepts w iff both of the following
       are true
       1 fsa_0 accepts w and fsa_1 does not accept w
       2 for every prefix p of w either fsa_0 did not accept p or fsa_1 did
    */
    static FSA_Simple * and_not_first(const FSA & fsa_0,const FSA & fsa_1)
    {
      return binop(fsa_0,fsa_1,BF_And_Not_First);
    }
    static FSA_Simple * and_not_first_trim(const FSA & fsa_0,const FSA & fsa_1)
    {
      /* and_not_first_trim() returns an FSA with the same language as
         and_not_first() but the returned FSA is only guaranteed to be
         trim and will not usually be minimised. This operation is provided
         because and_not_first() FSAs are frequently used for such purposes
         as correcting multipliers or word-acceptors. It is very often the
         case that such FSAs are quite large, and that the minimisation
         algorithm requires very many passes and hardly reduces the
         number of states. So in this case it is often best not to minimise
         the returned FSA (and this might allow more corrections to be made)
      */
      return binop(fsa_0,fsa_1,BF_And_Not_First_Trim);
    }
    static FSA_Simple * not_and(const FSA & fsa_0,const FSA & fsa_1)
    {
      return binop(fsa_0,fsa_1,BF_Not_And);
    }
    static FSA_Simple * fsa_or(const FSA & fsa_0,const FSA & fsa_1)
    {
      return binop(fsa_0,fsa_1,BF_Or);
    }
    /* create an FSA from a GAP format record */
    static FSA_Simple * create(String filename,Container * container,
                               bool must_succeed = true,MAF * maf = 0);
    /* cartesian_product returns the FSA that accepts (u,v) if fsa_0 accepts
       u and fsa_1 accepts v */
    static FSA_Simple * cartesian_product(const FSA & fsa_0,const FSA & fsa_1);

    /* for composite() fsa_0, and fsa_1 must be product FSAs on same alphabet.
       The returned fsa accepts (u,v) if and only if there is some w such
       that fsa_0 accepts (u,w) and fsa_1 accepts (w,v) */
    static FSA_Simple * composite(const FSA &fsa_0,const FSA & fsa_1,bool labelled_multiplier = false);
    /* concat() returns an FSA for which the accepted language consists
       of the words w0w1 where w0 is accepted by fsa_0, and w1 by fsa_1 */
    static FSA_Simple *concat(const FSA & fsa_0,const FSA & fsa_1);
    /* copy() returns an FSA identical to the original (except that the
       starting FSA was not necessarily in FSA_Simple format) */
    static FSA_Simple * copy(const FSA &original,Transition_Storage_Format tsf = TSF_Default);
    /* cut() returns an FSA similar to the original, with the minimum
       number of changes required to make the language finite */
    static FSA_Simple * cut(const FSA & fsa_original);
    /* determinise() returns an FSA accepting the same language as original
       (which is presumed to be a MIDFA, or at least does not have 1 as
       its initial state), and which is an ordinary 1 initial state FSA.
       However if partial is true the returned FSA may still be a MIDFA,
       but with as few as possible initial states */
    enum Determinise_Flag {DF_All,DF_Equal,DF_Identical};
    static FSA_Simple * determinise(const FSA &original,
                                    Determinise_Flag partial = DF_All,
                                    bool merge_labels = false,
                                    Transition_Storage_Format tsf = TSF_Default);

    /* determinise_multiplier() returns a determinised version of
       a MIDFA multiplier. This performs the additional processing
       needed for the labels to be correct */
    static FSA_Simple * determinise_multiplier(const FSA &multiplier);

    /* for diagonal() fsa_start must be a product FSA.
       The returned FSA accepts w if and only if fsa_start accepts (w,w)
    */
    static FSA_Simple * diagonal(const FSA &fsa_start);

    /* for exists() fsa_start must be a product FSA. If sticky is false,
       the returned FSA accepts w_1 if and only if fsa_start accepts(w_1,w_2)
       for some w_2. If sticky is true then the FSA accepts any word which
       contains such a word w_1 as a leading subword. if swapped is true
       then the rule for acceptance is that fsa_start accepts(w_2,w_1) */
    static FSA_Simple * exists(const FSA &fsa_start,bool sticky,bool swapped = false);
    /* filtered_copy() returns an FSA in which states si for which mapping[si]
       is 0 have been removed, and the other states have been retained. The
       states are renumbered. mapping[] is deleted when the function has
       completed.
    */
    static FSA_Simple * filtered_copy(const FSA &original,State_ID * mapping,
                                      Transition_Storage_Format = TSF_Default);

    /* finite language returns an FSA accepting precisely the words specified
       in the "words" collection. If labelled is false the returned FSA is
       minimised, which means that all the words will share the same accept
       state (except for any that are prefixes of another word).
       If labelled is true the accept states will be labelled by the accepted
       word, which may make the FSA more useful, but probably also
       much bigger */
    static FSA_Simple * finite_language(const Word_Collection & words,
                                        bool labelled = false);

    /* fsa_not() returns the FSA that accepts words if and only if the original
       FSA does not if first_only is false. If first_only is true then
       extensions of words that reached failure state in original language
       are rejected*/
    static FSA_Simple * fsa_not(const FSA &original,bool first_only = false);
    /* kernel returns an FSA that accepts words if and only if the original FSA
       accepts them and there is a path from the accepting state to itself,
       so that the state is recurring */
    static FSA_Simple * kernel(const FSA & fsa_start,unsigned kf_flags = 0);
    /* markov() returns an FSA which accepts all words but where the transitions
       for each generator cause a transition to a state labelled by that
       generator. It may be useful to and another FSA with this one
       if you want to associate a Markov process with the FSA */
    static FSA_Simple * markov(Container & container,
                               const Alphabet & alphabet);
    /* merge returns an FSA where original states are merged and renumbered
       according to new_states. returns 0 if the merge is not possible.
       If new_states is 0 then states with the same label are merged. */
    static FSA_Simple * merge(const FSA &original,const State_ID * new_states);
    static FSA_Simple * merge(const FSA_Common &original)
    {
      return merge(original,original.label_nr);
    }
    /* minimise() returns smallest FSA accepting same language as original,
       and with same labels on states. The new FSA is not BFS sorted unless
       the orginal also was.
       If merge_labels is MLF_All (only allowed if labels_are_words() is true)
       then labels are not preserved, but each new state is labelled by a list
       of words including all the words that labelled the original states
       mapped to that state.
       If merge_labels is MLF_Non_Accepting then labels on accept states are
       preserved, but labels on other states are merged in the same manner.
    */
    enum Merge_Label_Flag {MLF_None,MLF_Non_Accepting,MLF_All};
    static FSA_Simple * minimise(const FSA &original,
                                 Transition_Storage_Format tsf = TSF_Default,
                                 Merge_Label_Flag merge_labels = MLF_None);

    static FSA_Simple * overlap_language(const FSA & L1_acceptor);

    /* pad_language() returns the FSA which accepts all correctly padded
       pairs of words */
    static FSA_Simple * pad_language(Container & container,
                                     const Alphabet & alphabet);
    /* product_intersection() returns a product FSA which accepts (w1,w2)
       iff fsa_0 accepts (w1,w2),fsa_1 accepts w1, and fsa_2 accepts w2. */
    static FSA_Simple * product_intersection(const FSA & fsa_0,
                                             const FSA & fsa_1,
                                             const FSA & fsa_2,
                                             String description = "intersection",
                                             bool geodesic_reducer = false);
    /* prune() returns an FSA in which states of the original FSA from
       which the accepted language is finite have been removed. Note
       the accepted language is therefore not the same as in the original. */
    static FSA_Simple * prune(const FSA &original);
    /* restriction() returns an FSA based on that part of a starting
       FSA which is common between the original alphabet and the new
       alphabet. The translation between the old and new alphabet is
       based on string comparison of the glyphs */
    static FSA_Simple * restriction(const FSA & fsa_start,
                                    const Alphabet &new_alphabet);

    /* reverse() returns an FSA accepting the same language as original
       but with the words read from the end back to the beginning instead
       of forwards. If create_midfa is true there is one initial state
       for each accept state of the original FSA. If labelled is true
       the states are labelled with a list of integers representing the
       states of the original FSA that each state corresponds to */
    static FSA_Simple * reverse(const FSA &original,
                                bool create_midfa = false,
                                bool labelled = false);

    /* separate() returns an FSA accepting the same language as the original,
       but with the states modified so that no state can be reached by
       more than one input symbol */
    static FSA_Simple * separate(const FSA &original);

    /* shortlex() returns the product FSA which accepts w1,w2 iff
       w1 > w2 in shortlex order */
    static FSA_Simple * shortlex(Container & container,
                                 const Alphabet & alphabet,
                                 bool allow_interior_right_padding);
    /* star() returns an fsa that accepts the star of the language of
       fsa_start(). i.e. it accepts any string of accepted words from the
       original language */
    static FSA_Simple * star(const FSA & fsa_start);
    /* transpose returns an fsa that accepts (u,v) if and only if fsa_start()
       accepts (v,u) */
    static FSA_Simple * transpose(const FSA &fsa_start);
    /* trim() returns an FSA accepting same language as original with failing
      states removed. It may be faster to call trim() then minimise() than
      to call minimise() directly if the starting FSA has many failing states*/
    static FSA_Simple * trim(const FSA &original);
    /* see comment in FSA.cpp for purpose and use of this function */
    static FSA_Simple * truncate(const FSA & wa_start,
                                 Word_Length max_definition_length,
                                 State_Count max_states = 0,int round=-1);
    /* unaccepted_prefix() returns an FSA which accepts just those words which
       were prefixes of accepted words, but not themselves accepted in the
       original FSA. This is useful if an FSA should be prefix closed but
       currently is not.*/

    /* two_way_scanner returns a product FSA which accepts a word pair
       (u,v) if and only if uv is one of the words in Word_Collection
       language. Interior padding is disallowed.
       If labelled is false no information is available about which
       word has been read.
       If labelled is true the accept states will be labelled by a list
       of integers designating the words that have been matched (so
       the list will consist of a single element unless the Word_Collection
       contained duplicate words.
       This FSA is intended for optimising Todd_Coxeter coset enumeration */
    static FSA_Simple * two_way_scanner(const Word_Collection &language,
                                        bool labelled = false);

    static FSA_Simple * unaccepted_prefix(const FSA &original);
    /* universal() returns the FSA which accepts all words */
    static FSA_Simple * universal(Container & container,
                                 const Alphabet & alphabet);
};

class Label_Set_Owner : public Special_Subset_Owner
{
  public:
    FSA & owning_fsa;
  public:
    Label_Set_Owner(FSA & owning_fsa_) :
      Special_Subset_Owner(owning_fsa_.container),
      owning_fsa(owning_fsa_)
    {}
    Element_Count element_count() const
    {
      return owning_fsa.label_count();
    }
    bool is_valid_element(Element_ID element) const
    {
      return element>0 && element < owning_fsa.label_count();
    }
};

#endif
