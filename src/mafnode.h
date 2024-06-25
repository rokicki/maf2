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
$Log: mafnode.h $
Revision 1.19  2011/01/30 20:15:43Z  Alun
Typo in comment corrected
Revision 1.18  2010/06/10 13:58:03Z  Alun
All tabs removed again
Revision 1.17  2010/05/29 08:16:59Z  Alun
Lots of changes required by decision to use Node_ID rather than Node *
in data members to save space in 64-bit version. Most methods which
formerly returned Node * now return Node_Reference, which has both Node_ID
and Node * members. Most methods which had Node * arguments now have
Node_Handle arguments. Many more methods now require a Node_Manager argument
so that the Node corresponding to a particular Node_ID can be found
Revision 1.16  2009/11/10 08:15:48Z  Alun
Revision 1.15  2009/09/13 15:19:23Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.14  2009/09/01 12:08:10Z  Alun_Williams
Comment updated
Revision 1.13  2008/12/31 10:57:28Z  Alun
Support for sparse nodes added
Revision 1.13  2008/11/03 01:17:09Z  Alun
Early November checkin
Completely reworked pool and Difference_Tracker
Ported to Darwin
Revision 1.12  2008/09/30 11:07:24Z  Alun
diagnostics removed
Revision 1.11  2008/09/26 16:49:02Z  Alun
Final version built using Indexer.
Revision 1.6  2007/11/15 22:58:12Z  Alun
*/

#pragma once
#ifndef MAFNODE_INCLUDED
#define MAFNODE_INCLUDED 1
#ifndef AWDEFS_INCLUDED
#include "awdefs.h"
#endif
#ifndef MAF_INCLUDED
#include "maf.h"
#endif
#ifndef MAFWORD_INCLUDED
#include "mafword.h"
#endif
#ifndef MAFNM_INCLUDED
#include "mafword.h"
#endif
#ifndef NODEBASE_INCLUDED
#include "nodebase.h"
#endif
#ifdef DEBUG
#ifndef CONTAINER_INCLUDED
#include "container.h"
#endif
#endif

// Classes defined in this header file
class Node;
class Node_Iterator;
class Find_Candidate;

// Classes referred to but defined elsewhere
class Rewriter_Machine;
class Equation_Word;
class Derivation;
class Node_Manager;
class Overlap_Filter;
class Node_List;
class Block_Manager;
struct Candidate;
#ifndef NODE_STATUS_INCLUDED
#include "node_status.h"
#endif

/* A Node contains the information Rewriter_Machine knows about about a
   word representing some element of the object being analysed. There are
   three kinds of Node, only two of which are proper.

1) A Node may represent a word that is not known to be reducible. For
   these Nodes the reduced member is valid. Irreducible nodes have transitions
   which may be either dense or sparse depending on the NF_SPARSE flag.

2) A Node may represent a word that is the LHS of an equation. These
   nodes have NF_REDUCIBLE set. If the equation is primary then EQ_PRIMARY
   is set and trailing_subword is the overlap suffix. Otherwise
   trailing_subword should be the primary.

3) A Node may represent a word that has been found to be reducible and
   has been removed from the tree. These nodes have NF_REMOVED set, and
   may also have NF_REDUCIBLE set if the node was previously an LHS. In
   the latter case most of the information about the equation is still valid,
   except for the trailing subword.

   I use Equation_Node * where pointers should always point to a reducible
   node (when valid),
   State where Node * may or may not be irreducible,
   and Node * where node should be irreducible.

   See maf_nm.cpp and mafnode.cpp for more information.
*/


/* Values to go in flags field of Node */
// 0x0f bits for equations
const Node_Flags EQ_AXIOM = 0x01;
const Node_Flags EQ_EXPANDED = 0x02;
const Node_Flags EQ_REDUNDANT = 0x04;
const Node_Flags EQ_PRIMARY = 0x08;
// 0x0f bits for nodes
const Node_Flags NF_NEW = 0x01;  // This bit is only set/used when
                                    // a node is first created
const Node_Flags NF_NEEDED = 0x01; // This bit is only set/used when
                                       // creating a Rewriting_System object
const Node_Flags NF_PARENT = 0x02; // node has children that are also nodes
const Node_Flags NF_SPARSE = 0x04; // reduced.child is sparse, and we
                                       // must find prefix changes recursively.
const Node_Flags NF_BAD_INVERSE = 0x08;
/* Bits 0xf0 mean different things according to whether word is reducible or
   not.
   For nodes the 0xf0 bits have some more information about word-differences
   For equations the 0xf0 bits are enumerating, not bit mask like */
const Node_Flags NF_NEAR_DIFFERENCE = 0x10;
const Node_Flags NF_IS_GM_DIFFERENCE = 0x20;
const Node_Flags NF_IS_DR_DIFFERENCE = 0x40;
const Node_Flags NF_INVISIBLE = 0x80; // set if the word for the node cannot occur as a subword of a coset word

const Node_Flags EQ_Unconjugated = 0x00; /* Either EQ_Unconjugated or EQ_Undifferenced
                                            must be zero, or else construct_equation()
                                            must set one of them.
                                         */
const Node_Flags EQ_Undifferenced = 0x10;
const Node_Flags EQ_Removed = 0x20;
const Node_Flags EQ_Uncorrected = 0x30;
const Node_Flags EQ_Repair_Pending = 0x40;
const Node_Flags EQ_Oversized = 0x50;
const Node_Flags EQ_Dubious_Uncorrected = 0x60;
const Node_Flags EQ_Dubious_Unconjugated = 0x70;
const Node_Flags EQ_Dubious_Undifferenced = 0x80;
const Node_Flags EQ_Dubious_Secondary = 0x90;
const Node_Flags EQ_Weak_Secondary = 0xb0;
const Node_Flags EQ_Adopted = 0xe0; /* This must not set any bits not in
                                       EQ_Expanded, otherwise set_status()
                                       needs to be changed */
const Node_Flags EQ_Expanded = 0xf0;
const Node_Flags EQ_State_Mask = 0xf0;

// 0x700 bits are for word-differences. They are used in both nodes and
// equations but with different meanings
const Node_Flags EQ_HAS_DIFFERENCES = 0x100;
const Node_Flags EQ_HAS_PRIMARY_DIFFERENCES = 0x200;
const Node_Flags NF_IS_DIFFERENCE = 0x100;
const Node_Flags NF_IS_PRIMARY_DIFFERENCE = 0x200;
const Node_Flags EQ_INTERESTING = 0x400;
const Node_Flags NF_IS_HALF_DIFFERENCE=0x400;

const Node_Flags NF_DIFFERENCES = NF_IS_DIFFERENCE |
                                  NF_IS_PRIMARY_DIFFERENCE |
                                  NF_IS_HALF_DIFFERENCE |
                                  NF_NEAR_DIFFERENCE |
                                  NF_IS_GM_DIFFERENCE |
                                  NF_IS_DR_DIFFERENCE;

// remaining bits are common
const Node_Flags EQ_QUEUED = 0x800;
const Node_Flags NF_PREFIX_CHECKED = 0x1000;
const Node_Flags NF_TARGET = 0x2000;
const Node_Flags NF_REDUCIBLE = 0x4000;
const Node_Flags NF_REMOVED = 0x8000;

enum Link_Check_Flag
{
  LCF_All,
  LCF_Accept_Only,
  LCF_Defer
};

const unsigned short BLOCK_SIZE = 8192;

class Node
{
  friend class Node_Manager;
  friend class Equation_Word;
  friend class Equation_Queue;
  friend class Rewriter_Machine;
  friend class Block_Manager;
  friend class Node_Iterator;
  private:

    struct Sparse_Node
    {
      Ordinal nr_children_allocated;
      Ordinal nr_children;
      Node_ID suffix; /* We don't attach this - if a suffix becomes reducible
                        anything it is a suffix of also becomes reducible */
      Node_ID * children;
      Sparse_Node() :
        nr_children_allocated(0),
        nr_children(0),
        children(0)
      {}
      ~Sparse_Node()
      {
        if (children)
          delete [] children;
      }
    };

  public:

    struct Reduced_Node
    {
      /* Things only reduced words need */
      union
      {
        Node_ID * dense;    /* State transitions from this node*/
        Sparse_Node * sparse;
      } child;
      Node_ID inverse;   /* Reduced word for inverse. We don't need this
                           at all for reducible words, since we can find it
                           in the reduction.
                        */
      Word_Length max_height;/* Maximum length of states recorded in this subtree */
      Word_Length word_length; /* Length of defining word for this state */
      Word_Length max_accepted; /* Maximum number of letters forgotten in non-final prefix changes */
      Ordinal lvalue; /* First letter of word */
    };


    struct Reducible_Node
    {
      /* Things only reducible words need */
      Node_ID reduced_node;  /* usually reduced word of defining word - but may
                              point to a reducible word during insertion of
                              new equation */
      union
      {
        Node_ID trailing_subword;
#if MAF_USE_LOOKUP
        Node_ID node_id; /* Only valid when the node has not been attached yet, or has been removed */
#endif
      };
      union
      {
        ID expand_timestamp;  /* highest ID when this node was last expanded,
                                 if node is adopted */
        Node_ID queue_next; /* pointer to next entry in queue for queued equations. */
      };
      union
      {
        ID id;           /*ID assigned to this equation if adopted*/
        ID removed_id;
        Node_ID queue_prev; /*pointer to previous entry in temporary list otherwise */
      };
    };
  BLOCKED(Node)
  private:
    /* The prefix and the rvalue together define the state */
    Node_ID prefix; /* word formed from all but last letter */
    Ordinal rvalue; /* value of last letter read, -1 at the root,
                      -3 for available nodes. */
    Node_Flags flags; /* Flags for this node */
    mutable unsigned reference_count;/* Number of links to this state */
    Node_ID lhs_node; /* For reduced nodes start of list of words that reduce
                         to this word. For reducible nodes, next lhs with
                         same reduction. This data used to be held in
                         Reduced_Node/Reducible_Node but has been moved
                         to improve memory utilisation in 64 bit version */
    union
    {
      Reduced_Node reduced;
      Reducible_Node reduction;
    }; /* squashed together because we may need millions of nodes */

    Node() :
      reference_count(0),
      flags(NF_REMOVED),
      rvalue(-2)
    {}

  public:

#if MAF_USE_LOOKUP
    static const Node *find_node(const Node_Manager &nm,Node_ID id)
    {
      Nodes &nodes = * (Nodes *) &nm;
      return id ? nodes.block_table[id/BLOCK_SIZE] + (id % BLOCK_SIZE) : 0;
    }
    static const Node *fast_find_node(const Node_Manager &nm,Node_ID id)
    {
      Nodes &nodes = * (Nodes *) &nm;
      return nodes.block_table[id/BLOCK_SIZE] + (id % BLOCK_SIZE);
    }
#else
   static const Node *find_node(const Node_Manager &,Node_ID id)
   {
     return id;
   }
   static const Node *fast_find_node(const Node_Manager &,Node_ID id)
   {
     return id;
   }
#endif
    Node_Reference child(const Node_Manager &nm,Node_ID nid,Ordinal value) const
    {
      if (!is_final())
        return fast_child(nm,nid,value);
      return Node_Reference(0,0);
    }
    int compare(const Node_Manager & nm,State other) const;
    int compare(const Node_Manager & nm,const Word & word) const;

    Node_Reference dense_transition(const Node_Manager & nm,Ordinal value) const
    {
      MAF_ASSERT(!flagged(NF_REDUCIBLE|NF_REMOVED|NF_SPARSE),container(nm),
                 ("Invalid call to Node::dense_transition()!"));
      return Node_Reference(nm,reduced.child.dense[value]);
    }

    Node_Reference difference(Node_Manager &nm,Node_ID nid,Ordinal lvalue,Ordinal rvalue) const;

    ID expanded_timestamp() const
    {
      // only valid for equations
      return is_adopted() ? reduction.expand_timestamp : 0;
    }

    Boolean fast_is_primary() const
    {
      // This is only safe if the node is already known to be reducible
      return flagged(EQ_PRIMARY);
    }

    Node_Reference fast_primary(const Node_Manager &nm) const
    {
      // only safe when node is known to correspond to a secondary
      // equation. If the node is primary we get the overlap suffix
      MAF_ASSERT(flagged(NF_REDUCIBLE) && !flagged(NF_REMOVED|EQ_PRIMARY),
                 container(nm),("Invalid call to Node::fast_primary()!"));
      return Node_Reference(nm,reduction.trailing_subword);
    }

    Ordinal first_letter(const Node_Manager &nm) const
    {
      if (!is_final())
        return reduced.lvalue;
      if (prefix)
      {
        State prefix_node = fast_find_node(nm,prefix);
        if (prefix_node->rvalue != -1)
          return prefix_node->first_letter(nm);
      }
      return rvalue;
    }

    Node_Reference first_lhs(Node_Manager & nm) const
    {
      MAF_ASSERT(!flagged(NF_REDUCIBLE|NF_REMOVED),container(nm),
                 ("Invalid call to Node::first_lhs()!"));
      return Node_Reference(nm,lhs_node);
    }

    Boolean flagged(unsigned flag) const
    {
      return (flags & flag);
    }

    Node_Flags get_flags() const
    {
      return flags;
    }

    ID id() const
    {
      // only valid for equations
      return (flags & EQ_State_Mask)>=EQ_Adopted ? reduction.id : 0;
    }

    ID removed_id() const
    {
      // only valid for equations
      return (flags & NF_REMOVED+NF_REDUCIBLE)==NF_REMOVED+NF_REDUCIBLE ? reduction.removed_id : 0;
    }

    Boolean is_adopted() const
    {
      // result is only meaningful if node is known to be an equation
      return (flags & EQ_State_Mask)>=EQ_Adopted;
    }

    Boolean is_equation() const
    {
      return flagged(NF_REDUCIBLE);
    }

    Boolean is_final() const
    {
      return flagged(NF_REMOVED+NF_REDUCIBLE);
    }

    Boolean is_prefix() const
    {
      return flagged(NF_PREFIX_CHECKED);
    }

    Boolean is_valid() const
    {
      return status() <= NS_Last_Valid;
    }

    Language language(const Node_Manager &nm) const;

    Ordinal last_letter() const
    {
      return rvalue;
    }

    Node_Reference left_half_difference(Node_Manager & nm,Node_ID nid,Ordinal value) const;

    Word_Length irreducible_length() const
    {
      return reduced.word_length;
    }

    Word_Length length(const Node_Manager &nm) const
    {
      if (!is_final())
        return reduced.word_length;
      Word_Length answer = 1;
      State s = fast_find_node(nm,prefix);
      while (s->is_final())
      {
        s = fast_find_node(nm,s->prefix);
        answer++;
      }
      return answer + s->reduced.word_length;
    }

    Word_Length lhs_length(const Node_Manager &nm) const
    {
      // only allowed for equations that are in the tree
      return fast_find_node(nm,prefix)->reduced.word_length+1;
    }

    Node_Reference overlap_suffix(const Node_Manager &nm) const
    {
      // only safe for reducible nodes
      return Node_Reference(nm,reduction.trailing_subword);
    }

    Node_Reference next_lhs(Node_Manager & nm) const
    {
      MAF_ASSERT(is_equation(),container(nm),
                 ("Invalid call to Node::next_lhs()!"));
      return Node_Reference(nm,lhs_node);
    }

    Node_Reference parent(const Node_Manager & nm) const
    {
      return Node_Reference(nm,prefix);
    }

    Node_Reference primary(const Node_Manager &nm,Node_ID nid) const
    {
      // safe for any non-removed node
      if ((flags & NF_REDUCIBLE+EQ_PRIMARY) != NF_REDUCIBLE)
        return Node_Reference(this,nid);
      return fast_primary(nm);
    }

    void print(Output_Stream * stream,const Node_Manager &nm,Node_ID nid) const;

    Word_Length raw_reduced_length(const Node_Manager & nm) const
    {
      State node = this;
      if (is_equation())
        node = fast_find_node(nm,reduction.reduced_node);
      return node->length(nm);
    }

    Node_Reference raw_reduced_node(const Node_Manager & nm) const
    {
      // appropriate for when we know we are at an equation, and we
      // either know the equation is valid or we want the raw rhs
      MAF_ASSERT(is_equation(),container(nm),
                 ("Invalid call to Node::raw_reduced_node()"));
      return Node_Reference(nm,reduction.reduced_node);
    }

    bool read_inverse(Word * word,const Node_Manager &nm) const
    {
      word->allocate(length(nm));
      return read_inverse_values(word->buffer(),nm);
    }

    bool read_inverse_values(Ordinal * values,const Node_Manager &nm) const;

    void read_values(Ordinal * values,const Node_Manager &nm,Word_Length i) const
    {
      State node = this;
      while (i--)
      {
        values[i] = node->rvalue;
        node = find_node(nm,node->prefix);
      }
    }

    void read_values(Ordinal * values,const Node_Manager &nm) const
    {
      read_values(values,nm,length(nm));
    }

    void read_reversed_values(Ordinal * values,const Node_Manager &nm) const
    {
      State node = this;
      while (node->prefix)
      {
        *values++ = node->rvalue;
        node = fast_find_node(nm,node->prefix);
      }
    }

    void read_states_and_values(Ordinal * values,Node_ID* state,const Node_Manager & nm,Node_ID nid,Word_Length i) const
    {
      Node_Reference nr(this,nid);
      while (i--)
      {
        values[i] = nr->rvalue;
        state[i] = nr;
        nr = Node_Reference(nm,nr->prefix);
      }
    }

    void read_states_and_values(Ordinal * values,Node_ID* state,const Node_Manager & nm,Node_ID nid) const
    {
      read_states_and_values(values,state,nm,nid,length(nm));
    }

    void read_word(Word * word,const Node_Manager &nm) const
    {
      Word_Length l = length(nm);
      word->allocate(l);
      read_values(word->buffer(),nm,l);
    }

    Word_Length reduced_length(const Node_Manager & nm) const
    {
      State node = this;
      while (node->is_equation())
        node = fast_find_node(nm,node->reduction.reduced_node);
      return node->length(nm);
    }

    Node_Reference fast_reduced_node(const Node_Manager & nm) const
    {
      // appropriate for when we know we are at an equation, but
      // the rhs may itself be reducible
      for (State node = this;;)
      {
        Node_Reference answer = Node_Reference(nm,node->reduction.reduced_node);
        if (!answer->is_equation())
          return answer;
        node = answer;
      }
    }

    Node_Reference reduced_node(const Node_Manager & nm,Node_ID nid) const
    {
      if (is_equation())
        return fast_reduced_node(nm);
      return Node_Reference(this,nid);
    }

    Node_Reference right_half_difference(Node_Manager & nm,Node_ID nid,Ordinal value) const;
    Node_Status status() const;

    Total_Length total_length(const Node_Manager &nm) const
    {
      return length(nm) + reduced_length(nm);
    }

    Node_Reference transition(const Node_Manager & nm,Ordinal value) const
    {
      if (!flagged(NF_SPARSE))
        return dense_transition(nm,value);
      return sparse_transition(nm,value);
    }

    Word_Length valid_length(const Node_Manager &nm) const
    {
      State answer = this;
      while (answer->is_final())
        answer = fast_find_node(nm,answer->prefix);
      /* we must stop by the root (unless called while machine is being
        destroyed) - which is not allowed. */
      return answer->reduced.word_length;
    }

    // commands
    Node * attach(Node_Manager &) const
    {
      /* Attach a reference to a state */
      ++reference_count;
      return (Node *) this;
    }

    Node & node(Node_Manager &) const
    {
      return * (Node *) this;
    }

    // more commands
    bool detach(Node_Manager &nm,Node_ID nid) const
    {
      /* Detach a state. A state is only destroyed when the last (counted)
         reference to it is removed.

        In infinite groups there will always be prefix changes to parent
        states that form a circular list of links. Therefore it is important
        that all these links have been broken previously through a call to
        remove_from_tree().
      */
      if (--reference_count==0)
      {
        node(nm).destroy(nm,nid);
        return true;
      }
      return false;
    }

    void clear_flags(unsigned old_flags)
    {
      flags &= ~old_flags;
    }

    void set_flags(unsigned new_flags)
    {
      flags |= new_flags;
    }

    Node_Reference set_inverse(Node_Manager &nm,Node_ID nid,Node_Reference inverse,bool important);

    Node_Reference set_inverse(Node_Manager &nm,Node_ID nid)
    {
      return set_inverse(nm,nid,Node_Reference(0,0),false);
    }

    bool set_status(Node_Manager &nm,Node_Status new_status);

  private:
    Node_Reference fast_child(const Node_Manager &nm,Node_ID nid,Ordinal value) const;
    void find_candidates(const Find_Candidate &find,Node_ID nid,
                         Total_Length total_length) const;
    Node_Reference inverse(Node_Manager &nm,Node_ID nid) const;
    void rebind(Node * prefix,Node_Manager &nm,bool check_height) const;
    Node_Reference sparse_child(const Node_Manager & nm,Ordinal value) const;
    Node_Reference sparse_transition(const Node_Manager & nm,Ordinal value) const;


    Node_Reference construct(Node_Manager &nm,Node_Reference prefix_,
                             Ordinal lvalue,Ordinal rvalue,
                             Word_Length word_length_);
    void construct_equation(Node_Manager &nm,Node_ID nid,
                            Node_Reference prefix_,
                            Ordinal rvalue_,unsigned flags_,
                            Node_Reference rhs,Node_Reference trailing_subword,
                            bool new_node);
    void destroy(Node_Manager &nm,Node_ID nid);
    void detach_rhs(Node_Manager &nm,Node_ID nid);
    void link(Node_Manager &nm,Node_ID nid,Ordinal value,
              Node_Reference new_child,
              Link_Check_Flag check_flag = LCF_All)
    {
      link_replace(nm,nid,value,new_child,transition(nm,value),check_flag);
    }
    void link_replace(Node_Manager &nm,Node_ID nid,Ordinal value,
                      Node_Reference new_child,Node_Reference old_child,
                      Link_Check_Flag check_flag = LCF_All);
    void make_reducible(Node_Manager &nm,Node_ID nid,
                        unsigned short new_language);
    void remove_from_tree(Node_Manager &nm,Node_ID nid)
    {
      make_reducible(nm,nid,language_A);
    }
    static Container & container(const Node_Manager &nm);

};

#if MAF_USE_LOOKUP
Node_Reference::Node_Reference(const Node_Manager &nm,Node_ID nid_) :
  s(Node::find_node(nm,nid_)),
  nid(nid_)
{}
#endif


class Node_Iterator
{
  private:
    const Node_Manager &nm;
    Ordinal_Word current_word;
    Node_Reference root;
    Node_Reference current;
    Node_Reference parent;
    int position;
    Word_Length depth;
    Ordinal child_start;
    Ordinal child_end;
  public:
    Node_Iterator(const Node_Manager &nm,
                  Node_Reference root = Node_Reference(0,0));
    ~Node_Iterator() {};
    void begin(Node_Reference root = Node_Reference(0,0));
    bool first(Node_Reference *nr,bool inside = true)
    {
      begin(root);
      return scan(nr,inside);
    }
    bool scan(Node_Reference *nr,bool inside = true)
    {
      if (!(position & 1))
        return next(nr,inside);
      *nr = current;
      if (position == 1 && !current.is_null())
      {
        position = 2;
        return true;
      }
      position = 3;
      return false;
    }
    const Ordinal_Word & word() const
    {
      return current_word;
    }
    Word_Length word_length() const
    {
      return depth;
    }
    // You must only call
    bool next(Node_Reference *nr,bool inside = true);
};


class Find_Candidate
{
  friend class Node;
  private:
    Node_Manager &nm;
    Node_List * candidates_list;
    ID min_id;
    ID max_id;
    Word_Length lhs_length;
    Word_Length rhs_length;
    Total_Length upper_overlap_limit; // maximum length of overlaps we consider
    Total_Length lower_overlap_limit; // all overlaps up to this length are considered
    Total_Length old_upper_overlap_limit; // old maximum length of overlaps we might have considered
    Total_Length old_lower_overlap_limit; // all overlaps up to this length were considered already
    Total_Length keep_limit;
    Total_Length old_keep_limit;
    Total_Length vital_limit;
    Total_Length check_limit;
    Total_Length old_check_limit;
    unsigned strategy;
    int probe_style;
    Node_Reference e1;
    const Equation_Word &word;
    Overlap_Search_Type ost;
    bool favour_differences;
    unsigned char e1_special;
  public:
    Find_Candidate(Node_Manager &nm_,
                   Node_List * candidates_list_,
                   ID min_id_,
                   ID max_id_,
                   Node_Handle e1_, Word_Length suffix_length,
                   const Equation_Word & word_,
                   Overlap_Search_Type ost_,
                   bool favour_differences);
  private:
    bool is_interesting(Candidate &c) const;
};

#endif
