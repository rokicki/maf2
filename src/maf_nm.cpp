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


// $Log: maf_nm.cpp $
// Revision 1.20  2011/05/20 10:17:01Z  Alun
// detabbed
// Revision 1.19  2011/01/30 20:32:21Z  Alun
// Change for DEBUG version
// Revision 1.18  2010/06/10 13:57:39Z  Alun
// All tabs removed again
// Revision 1.17  2010/06/03 19:53:56Z  Alun
// Major changes to allow for removal of almost all pointers from Node data
// type. 64-bit version now uses IDs rather than pointers to reduce memory
// usage.
// Recursion removed from check_transitions()
// Revision 1.16  2009/11/10 08:26:10Z  Alun
// various minor changes, mostly to improve code readability
// Revision 1.15  2009/09/14 09:58:08Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.14  2009/09/01 09:44:46Z  Alun_Williams
// Much of former long_comment from maf_rm.cpp moved to here
// Revision 1.13  2009/01/01 21:21:12Z  Alun
// inspect() method now static to avoid stack overflow in DEBUG build
// Changes resulting from support for sparse transitions
// Revision 1.13  2008/10/23 17:45:56Z  Alun
// Early November checkin
// Completely reworked pool and Difference_Tracker
// Ported to Darwin
// Revision 1.12  2008/10/01 01:27:29Z  Alun
// Diagnostics removed
// Revision 1.11  2008/09/27 10:26:01Z  Alun
// Final version built using Indexer.
// Revision 1.7  2007/12/20 23:25:42Z  Alun
//
#include <string.h>
#include "mafnode.h"
#include "maf_nm.h"
#include "maf_rm.h"
#include "maf_jm.h"
#include "container.h"
#include "maf_we.h"
#include "arraybox.h"
#include "maf_el.h"

/*
   Node_Manager maintains the data describing the equations in a rewriting
   system that MAF is processing on behalf of Rewriter_Machine. Although
   Node_Manager objects cannot exist outside a Rewriter_Machine, many parts
   of MAF interact with the Rewriter_Machine via Node_Manager.
   The data managed by Node_Manager consists of a collection of "Node"
   records linked together in a rather complex web, and functioning in
   various ways. Each node may correspond both to words in the alphabet and
   to elements of the object defined by the presentation. Not all of the
   links in a node are  always valid. Not all nodes are accessible by
   following links from the root, but any node that exists has a unique word
   associated with it than can be found by following links back to the root.

   This web of links can be used in various ways.

1) In the first place it functions as an FSA telling us how to reduce a word.
   Starting from the root, we follow the appropriate State links by calling
   Node::transition() for each letter in the word, until we either reach the
   end of the word, and/or we reach a node that has an equation. It is
   important to realise that when we do this, the node that we finish up at
   does not necessarily represent the entire word. When we follow this chain,
   we never encounter a 0 pointer, because at any point where following a link
   would take us to a word that is not in the tree, there is a "prefix change"
   and we finish up at a node representing some suffix of our word. So, if we
   were at AB and follow C we might end up at ABC,BC or C, (or even back at
   the root, though that can only happen if we encounter a letter that never
   occurs as the first letter of the LHS of an equation).
   If everything is working as it should be, then we never end up at a node
   that causes us to miss a prefix. Thus if there is an equation with an LHS
   beginning with BC, following the link from AB is not allowed to take us to
   C, but only to ABC or BC. When a new equation is added we must seek out and
   correct prefix changes that miss the new equation. (However we do not always
   do this for secondary equations - i.e. if we already have an equation CD=E,
   and later on add the secondary BCD=AF we do not necessarily force BCD to be
   a checked prefix immediately.

   Since we record the length of the "defining word" of a node (the word you
   get by following the prefix chain back to the root), we can tell whether
   we have taken a prefix change at any point in our travels by checking the
   length of the node we are at against the number of symols read so far.
   (This length information is stored because it is used frequently and
   calculating it explicitly each time it is needed would be inefficient.
   In fact data alignment requirements make it cost-free to store this data
   in any case.)

2) In the second place we use the structure as a tree - if we follow the
   State links but stop whenever there is a prefix change, we have a
   conventional tree in which all our equations appear exactly once.

   If the transition from a prefix is reducible then it only "needs" an
   equation if the same transition from the suffix was not already reducible.
   However, if the final reduction does not have a common prefix with the LHS
   then it is desirable to create the equation anyway, because it may have a
   word-difference. Also, the reduction will be faster - in the case of
   recursive word-orderings possibly much faster.

   At the time an equation is set up, the RHS will always point to an
   irreducible node. However, that node might become reducible. In the case
   where it joins language L1 to L3, then this is usually not too serious,
   because we simply need to follow the chain of reduction.reduced_word
   members back until we get to a language_L0 value, and update our current
   value with the word we arrive at. If our RHS has however joined language_A
   and been eliminated altogether, we need to perform a full reduction on
   the old rhs to find the new RHS. For recursive orderings this may not be
   possible immediately.

   Nodes can exist for reducible words without being equations. This happens
   when a node that was irreducible is being removed because its prefix is now
   reducible. But any such node is not in the tree, so we do not usually need
   to worry about it.

3) In the third place we have a dictionary of words that are equal as
   elements. At a reduced word we can find the start of a list of words that
   reduce to that word. MAF uses this to find new equations that KB has not
   found yet. If a word becomes reducible this list becomes invalid without
   clean up (though the words that reduced to that word must still detach
   themselves from the word), but if a node for an equation is about to be
   destroyed when its RHS is still a reduced word, then it must remove itself
   from the list properly.

   The life cycle of a node is very complex, and we may find ourselves looking
   at a node that is dying or dead. This can easily happen, because inserting a
   new equation can have a huge effect on the Rewriter_Machine, in extreme
   cases even causing the immediate destruction of the equation we are
   inserting. It necessary to "attach" to nodes that we are interested
   in and which are possibly subject to deletion. Generally this happens
   automatically, because typically we work on nodes that have come from some
   list structure, and the nodes are attached and detached by the list.
   But if we look at a relative of such a node we may need to manage the
   attach/detach explicitly.

   We use reference counts to work out when it is safe to delete a node that
   has been removed. However, this is not done through so-called "smart
   pointers", partly because some of the pointers that need to be counted are
   within a union, so cannot be in a class with a destructor, and partly
   because not all pointers to nodes are of equal status. For example the
   lhs_first, lhs_next pointers are not reference counted.
   What we are counting is not the number of references to an object as such,
   but the number of references that we either cannot readily clean up, or
   which cannot be cleaned up without forcing destruction of the object
   containing the reference.

4) We can use the inverse links to multiply words on the left by following
   the link to the inverse,following the link for the inverse of the
   generator we want to multiply by, and then inverting again. This
   hopefully speeds up calculation of the word-differences. The calculation
   of inverses typically increases the size of the tree a lot, and inverses
   are not normally calculated unless word-differences are being computed.
*/

#if MAF_USE_LOOKUP
class Block_Manager
{
  /* MAF used to refer to nodes using pointers. However, since there
     needs to be a very large number of references to nodes but there
     are unlikely to be more than 2^32 nodes using pointers wastes a lot
     of memory in the 64 bit world. Therefore we use a scheme that allows
     us to refer to nodes using an integer sized handle (Node_ID).

     When a node is created its handle is placed in reduction.reduced_node.
     Upon construction this value will be set into the prefix pointer but
     not otherwise remembered. When a node is removed from the tree

     Nodes are created and freed in blocks. Individual nodes are managed
     using a free list, but we count the number of nodes used from each
     block, and free entire blocks if possible.
  */

  private:
    Array_Of<Node *> blocks;
    Array_Of<unsigned short> nodes_used;
    Node_ID free_node;
    Element_List block_holes;
    Element_Count nr_blocks;

  public:
    Block_Manager() :
      free_node(0),
      nr_blocks(0)
    {}

    Node * pointer(Node_ID id)
    {
      return id ? blocks[id/BLOCK_SIZE] + (id % BLOCK_SIZE) : 0;
    }

    Node * fast_pointer(Node_ID id)
    {
      return blocks[id/BLOCK_SIZE] + (id % BLOCK_SIZE);
    }

    Node_Reference node_get(Nodes & nodes)
    {
      /* Once created nodes are not returned to the regular heap until the
         entire block of nodes
         Node_Manager instance is destroyed, or its purge() function is called.
         This is because nodes that are removed are almost always replaced
         by new nodes somewhere else, so it pays to avoid the more expensive
         allocation/deallocation built into the heap manager.
         More importantly, several functions work on nodes that might get
         destroyed before the function has finished with them.
         Such functions inspect the language field periodically and break out
         if they notice it has changed to a value that indicates a node is
         in limbo or actually back on the free list (though the latter should
         never happen).
      */

      Element_ID block_nr = 0;

      if (!free_node)
      {
        if (!block_holes.pop(&block_nr))
        {
          block_nr = nr_blocks++;
          if (nr_blocks >= blocks.capacity())
          {
            blocks.set_capacity(blocks.capacity() + 64);
            nodes_used.set_capacity(blocks.capacity());
            nodes.block_table = blocks.buffer();
          }
        }
        Node * node = blocks[block_nr] = new Node[BLOCK_SIZE];
        Node_ID id = block_nr * BLOCK_SIZE;
        for (int i = 0 ; i < BLOCK_SIZE;i++,node++)
        {
          node->reduction.node_id = id;
          node->reduction.queue_prev = free_node;
          free_node = id;
          node->reduction.queue_next = ++id;
        }
        node[-1].reduction.queue_next = 0;
        free_node = block_nr*BLOCK_SIZE;
        if (!free_node)
          free_node++;
      }

      block_nr = free_node/BLOCK_SIZE;
      Node_Reference nr(blocks[block_nr] + (free_node % BLOCK_SIZE),free_node);
      nodes_used[block_nr]++;
      free_node = nr->reduction.queue_next;
      if (free_node)
        fast_pointer(free_node)->reduction.queue_prev = 0;
      return nr;
    }

    void node_free(Node * node)
    {
      Node_ID id = node->reduction.node_id;
      Element_ID block_nr = id/BLOCK_SIZE;
      if (free_node)
        fast_pointer(free_node)->reduction.queue_prev = id;
      node->reduction.queue_next = free_node;
      node->reduction.queue_prev = 0;
      free_node = id;
      if (!--nodes_used[block_nr])
      {
        unsigned i = block_nr ? 0 : 1;
        node = blocks[block_nr] + i;
        for (; i < BLOCK_SIZE;i++,node++)
        {
          if (node->reduction.queue_prev)
          {
            fast_pointer(node->reduction.queue_prev)->reduction.queue_next =
              node->reduction.queue_next;
          }
          else
            free_node = node->reduction.queue_next;
          if (node->reduction.queue_next)
            fast_pointer(node->reduction.queue_next)->reduction.queue_prev =
              node->reduction.queue_prev;
        }
        block_holes.append_one(block_nr);
        delete [] blocks[block_nr];
        blocks[block_nr] = 0;
      }
    }
};
#else

class Block_Manager
{
  private:
    Node * free_node;

  public:
    Block_Manager() :
      free_node(0)
    {}

    Node_Reference node_get(Nodes &)
    {
      Node * node;
      if (!free_node)
      {
        node = free_node = new Node;
        for (int i = 0;i < 128;i++)
        {
          node->prefix = new Node;
          node = (Node *) node->prefix;
        }
        node->prefix = 0;
      }

      node = free_node;
      free_node = (Node *) node->prefix;
      return Node_Reference(node,node);
    }

    void node_free(Node * node)
    {
      node->rvalue = INVALID_SYMBOL;
      node->prefix = free_node;
      free_node = node;
    }

    void purge()
    {
      while (free_node)
      {
        Node * next = (Node *) free_node->prefix;
        delete free_node;
        free_node = next;
      }
    }

    ~Block_Manager()
    {
      purge();
    }
};
#endif
/**/

Node_Manager::Node_Manager(Rewriter_Machine & rm_) :
  rm(rm_),
  maf(rm_.maf),
  pd(rm_.pd),
  nr_generators(rm_.maf.generator_count()),
  last_id(0),
  last_removed_id(0),
  bm(* new Block_Manager),
  confluent(false),
  in_destructor(false)
{
  memset(&stats,0,sizeof(stats));
  if (nr_generators <= 8 || pd.is_coset_system && pd.coset_symbol <= 8 &&
      nr_generators - pd.coset_symbol <= 8)
    maf.options.dense_rm = true;
  root = node_get();
  root->node(*this).construct(*this,Node_Reference(0,0),-1,-1,0);
}

/**/

void Node_Manager::destroy_tree()
{
  /* This needs to be called before the destructor, to allow Job_Manager
     to work properly */
  in_destructor = true;
  root->node(*this).remove_from_tree(*this,root);
  /* When the root was created its reference count was artificially
     incremented to count the root pointer into the tree */
  root->detach(*this,root);
}

/**/

Node_Manager::~Node_Manager()
{
  delete &bm;
}

/**/

Node_Reference Node_Manager::get_state(const Ordinal *values,Word_Length end,
                                       Word_Length start) const
{
  // get_state() is only safe if word is currently in the L3 language
  Node_Reference nr(root,(Node_ID)1);
  while (start < end)
    nr = nr->transition(*this,values[start++]);
  return nr;
}

/**/

Word_Length Node_Manager::height() const
{
  return root->reduced.max_height;
}

/**/

Word_Length Node_Manager::g_height() const
{
  if (!pd.is_coset_system)
    return height();

  Word_Length answer = 0;
  for (Ordinal g = 0; g < pd.coset_symbol;g++)
  {
    State s = root->dense_transition(*this,g);
    if (s != State(root))
      if (s->is_final())
      {
        if (answer < 1)
          answer = 1;
      }
      else if (answer < s->reduced.max_height)
        answer = s->reduced.max_height;
  }
  return answer;
}

/**/

void Node_Manager::validate_tree() const
{
  Ordinal_Word test(maf.alphabet,height());
  Node_Reference s;
  Node_Count found[5] = {0,0,0,0,0};
  unsigned errors = 0;
  Container & container = maf.container;
  Node_Iterator ni(*this);
  while (ni.scan(&s))
  {
    Language lang_s = s->language(*this);
    found[lang_s]++;
    s->read_word(&test,*this);
    Node_Reference ns = get_state(test,test.length(),1);
    Language lang_ns;
    if (lang_s == language_A ||  (lang_ns = ns->language(*this)) == language_A ||
        (s->is_final() ? lang_ns >= lang_s && lang_s != language_L3 :
                         lang_ns > lang_s))
    {
      container.error_output("Node_Manager has bad state : %d %d\n",
                             s->language(*this),ns->language(*this));
      s->print(container.get_stderr_stream(),*this,s);
      errors++;
    }
    if (!s->is_final())
    {
      Word_Length max_height = s->length(*this)+1;
      Ordinal child_start,child_end;
      valid_children(&child_start,&child_end,s->last_letter());
      for (Ordinal g = child_start; g < child_end;g++)
      {
        State child = s->fast_child(*this,s,g);
        if (child && !child->is_final() &&
            child->reduced.max_height > max_height)
          max_height = child->reduced.max_height;
        if (s->transition(*this,g)->flagged(NF_REMOVED))
          MAF_INTERNAL_ERROR(container,("Bad Transition! %p %d\n",State(s),g));
      }
      if (max_height != s->reduced.max_height)
        MAF_INTERNAL_ERROR(container,("Bad height! %p %d %d\n",State(s),
                           max_height,s->reduced.max_height));
      if (s->reduced.inverse && Node::fast_find_node(*this,s->reduced.inverse)->flagged(NF_REMOVED))
        MAF_INTERNAL_ERROR(container,("Bad Inverse! %p %p\n",
                                      State(s),Node::fast_find_node(*this,s->reduced.inverse)));
    }
    else
    {
      State rhs = s->raw_reduced_node(*this);
      if (rhs->flagged(NF_REMOVED))
        MAF_INTERNAL_ERROR(container,("Bad Equation! %p %p\n",State(s),State(rhs)));
      State ts = ns;
      for (Word_Length i = 1; i <= test.length();i++) /* <= is correct here. we may need to go to root */
      {
        ts = get_state(test,test.length(),i);
        if (ts->is_prefix())
          break;
      }

      if (s->fast_is_primary() &&
          ts->length(*this) > s->overlap_suffix(*this)->length(*this))
        MAF_INTERNAL_ERROR(container,("Bad trailing subword! %p %p %p\n",
                           State(s), State(s->overlap_suffix(*this)),ts));
    }
  }
#ifdef DEBUG
  rm.container_status(0,0,"Expected counts "
                       FMT_NC " " FMT_NC " " FMT_NC " " FMT_NC " " FMT_NC "."
                       " Actual counts "
                       FMT_NC " " FMT_NC " " FMT_NC " " FMT_NC " " FMT_NC "\n",
                   stats.nc[0],stats.nc[1],stats.nc[2],stats.nc[3],
                   stats.nc[4],
                   found[0],found[1],found[2],found[3],found[4]);
#endif
}

class Transition_Check
{
  friend class Node_Manager;
  private:
    const Node_ID *state;
    Node_Reference primary;
    Node_Reference lhs_node;
    const Ordinal *value;
    Word_Length new_prefix_length;
    Word_Length length;
    Word_Length check_length;
    Word_Length primary_length;
    Word_Length primary_offset;
    bool final;
    bool rhs_changed;
    bool do_special_overlaps;
    bool do_total_overlaps;
    bool new_reduction;
    Transition_Check(const Equation_Word & lhs,
                     Word_Length new_prefix_length_,
                     bool rhs_changed_,
                     bool publish_total_overlaps,
                     bool publish_special_overlaps,
                     bool new_reduction_) :
      state(lhs.states()),
      value(lhs.buffer()),
      length(primary_length = lhs.fast_length()),
      new_prefix_length(new_prefix_length_),
      rhs_changed(rhs_changed_),
      new_reduction(new_reduction_)
    {
      primary = lhs_node = Node_Reference(lhs.nm,state[length-1]);
      final = primary->is_final()!=0;
      do_total_overlaps = final && rhs_changed && publish_total_overlaps;
      do_special_overlaps = do_total_overlaps && publish_special_overlaps;
      if (final && !primary->fast_is_primary())
      {
        primary = primary->fast_primary(lhs.nm);
        primary_length = primary->length(lhs.nm);
      }
      primary_offset = length - primary_length;
      check_length = length - (final ? 1 : 0);
#ifdef DEBUG
      for (Word_Length i = 0; i < length;i++)
      {
        Ordinal g = Node::fast_find_node(lhs.nm,state[i])->last_letter();
        if (g != value[i] && g != PADDING_SYMBOL)
          MAF_INTERNAL_ERROR(lhs.nm.maf.container,
                             ("Improper lhs %p passed to"
                              " Transition_Check::Transition_Check()\n",&lhs));
      }
#endif
    }
};

/**/

void Node_Manager::check_transitions(const Equation_Word & lhs,
                                     Word_Length new_prefix_length,
                                     bool rhs_changed,
                                     bool publish_overlaps,
                                     bool new_reduction)
{
  /* When a rule a=b has been added to the MAF, look for
     all previous rules of the form xay=c, and schedule them for
     elimination. Also update prefix changes:

     As well as looking for a full match for the new LHS we need to
     examine any prefix changes that occur after a partial match to ensure
     that the prefix change does not cause any matched characters to be
     accepted yet.

     Consider nodes ABCDEFG,ABCDEJK and imagine we are inserting
     a new LHS beginning DEFGY, when the longest prefix beginning DEF was
     previously for a LHS beginning DEFXY, and assume the tree was correct
     for all prior equations. Given input ABCDEFGY the automaton might
     previously have accepted the D when the Y was encountered, since DEFGY
     was not a prefix. (It might not have done so, because CDEFGY might
     have been a prefix). Therefore at node ABCDEFG the Y link is suspect.
     But since there was already an LHS beginning DEF the F link at ABCDE
     cannot be shorter than DEF, and is therefore bound to be OK.

     So in this function we need to examine each branch until we
     are sure it cannot contain a bad prefix change, and not just until
     we are sure it does not contain a complete match.

     This part of the code can come to totally dominate performance if
     page faulting becomes significant, or if there are very many generators
  */
  if (maf.aborting || maf.options.no_equations)
    publish_overlaps = false;
  Transition_Check tc(lhs,new_prefix_length,rhs_changed,
                      publish_overlaps,
                      publish_overlaps && maf.options.special_overlaps!=0,
                      new_reduction);
  Node_Iterator ni(*this);
  bool inside = true;
  Node_Reference nr;
  ni.scan(&nr,true); // skip the root

  if (pd.is_coset_system)
  {
    while (ni.next(&nr,inside))
    {
      inside = false;
      if (!nr->is_final())
      {
        Word_Length offset = ni.word_length();
        if ((offset > nr->reduced.max_accepted ?
            nr->reduced.max_height >= offset + tc.length :
            nr->reduced.max_height >= offset + tc.new_prefix_length) &&
            (tc.value[0] < pd.coset_symbol /* g subword can be anywhere */ ||
             nr->rvalue > pd.coset_symbol /* h node can be followed by anything */))
          inside = true;
        if (inside)
        {
          /* We may be somewhere which could have a simplification or incorrect
             prefix change, but we may not be far enough in yet */
          if (tc.value[0] >= pd.coset_symbol /* we must be at an h node so can look */ ||
              nr->rvalue <= pd.coset_symbol /* we must want to read a g word so can look */)
            check_transitions_inner(tc,nr,offset);
        }
        if (!nr->flagged(NF_PARENT))
          inside = false;
      }
    }
  }
  else
  {
    while (ni.next(&nr,inside))
    {
      inside = false;
      if (!nr->is_final())
      {
        Word_Length offset = ni.word_length();
        if (offset > nr->reduced.max_accepted ?
            nr->reduced.max_height >= offset + tc.length :
            nr->reduced.max_height >= offset + tc.new_prefix_length)
        {
          inside = true;
          /* We are somewhere which could have a simplification or incorrect
             prefix change */
          check_transitions_inner(tc,nr,offset);
        }
        if (!nr->flagged(NF_PARENT))
          inside = false;
      }
    }
  }
}

/**/

void Node_Manager::check_transitions_inner(Transition_Check & tc,
                                           Node_Reference node,Word_Length offset)
{
  /* See Node_Manager::check_transitions() for more information about
     this method.

     That method ensures this is only called with a word that can occur as a
     suffix of this node.
  */

  Node_Reference ns(node);
  Word_Length i;
  /* assuming we are looking at a LHS ug, we are  going to stop just before
     reading g, but if we are called with
     a currently irrreducible word we will check the whole word */

  for (i = 0;i < tc.check_length;i++)
  {
    /* This loop is slightly ineffecient, because we don't need to
       check that prefix changes are long enough until we get to
       i = new_prefix_length-1. I experimented with splitting the loop
       up, but it made no difference to performance.
    */
    if (!(node->flags & NF_SPARSE))
    {
      ns = node->dense_transition(*this,tc.value[i]);
      if (ns->prefix != node)
      {
        /* There was a prefix change here, make sure the change does
           not miss the new prefix.
           Since we are replacing one prefix change by another there
           is no need to check heights.
           Note that i is one less than the length of the node in tc.state[i],
           hence we test <= and not < */
        if (ns->length(*this) <= i)
        {
          node->node(*this).link_replace(*this,node,tc.value[i],Node_Reference(*this,tc.state[i]),
                                          ns,LCF_Accept_Only);
        }
        return;
      }
    }
    else
    {
      /* If we get to a sparse node, we would really rather not call
         sparse_transition(), because it is potentially very slow, and
         we can work out what to do without knowing what the transition
        from node is at the moment */
      ns = node->sparse_child(*this,tc.value[i]);
      if (ns.is_null())
      {
        /* We need to work out whether the suffix needs to change.
           At this point we have read i+1 symbols of the transition
           being checked. So the word we must not miss has length
           i+1, so the suffix should have length at least i.
           If it does not we will replace the suffix. This means we save
           calls to transition() to look up the old link, which is slow for
           sparse nodes.
        */
        if (node->fast_find_node(*this,node->reduced.child.sparse->suffix)->reduced.word_length < i)
        {
          node->reduced.child.sparse->suffix = tc.state[i-1];
          inspect(node,false);
        }
        return;
      }
    }
    if (ns->is_final())
    {
      if (ns->fast_is_primary())
      {
        State tsw = ns->fast_find_node(*this,ns->reduction.trailing_subword);
        if (tsw->reduced.word_length <= i)
        {
          Word_Length was_accepted = node->reduced.word_length - tsw->reduced.word_length;
          tsw->detach(*this,ns->reduction.trailing_subword);
          ns->node(*this).reduction.trailing_subword = tc.state[i];
          ns->fast_find_node(*this,tc.state[i])->attach(*this);
          if (was_accepted == node->reduced.max_accepted)
            inspect(node,false);
        }
      }
      // at this point ns has length offset+i+1
      if (tc.do_special_overlaps && tc.primary_offset < i+1)
        rm.jm.schedule_overlap(ns,tc.lhs_node,offset,false);
      return;
    }
    node = ns;
    if (offset > node->reduced.max_accepted ?
        node->reduced.max_height < offset + tc.length :
        node->reduced.max_height < offset + tc.new_prefix_length)
      return;
  }

  if (tc.final)
  {
    /* There is state beginning xa. Get rid of it with a prefix change
       to the new rule. This is where any equations will get eliminated.
       Whatever was linked here will get detached and then destroyed, so
       any equations in this part of the tree will be eliminated.
       There are two exceptions to this:
         An equation is not replaced by a shorter equation. In this case the
         equation currently linked deserves to be kept on as a secondary
         equation, even if we do not really want secondary equations.
         The reason for doing this is that it seems to improve stability
         when a collapse takes place. These equations are always marked
         as being dubious, and will get thrown away later if they are not
         wanted.

         If a node has been turned into an equation, then the link may
         already be correct. This also happens if we resimplify using
         the same LHS later on in order to get rid of EQ_REDUNDANT equations.
    */
    Ordinal rvalue = tc.value[i];
    if (!node->flagged(NF_SPARSE))
      ns = node->dense_transition(*this,rvalue);
    else
    {
      ns = node->sparse_child(*this,rvalue);
      if (ns.is_null())
      {
        /* Check the suffix in the sparse node is long enough */
        if (node->fast_find_node(*this,node->reduced.child.sparse->suffix)->reduced.word_length < tc.check_length)
        {
          node->reduced.child.sparse->suffix = tc.state[tc.check_length-1];
          inspect(node,false);
        }
        return;
      }
    }
    bool keep = ns == tc.state[i] ||
                ns->is_final() && !(ns->flags & EQ_REDUNDANT+NF_REMOVED) &&
                ns->lhs_length(*this) > tc.length;

    if (!keep)
      node->node(*this).link_replace(*this,node,rvalue,tc.lhs_node,ns);
    else if (ns->prefix == node)
    {
      Word_Length old_length = offset+tc.length;

      Language old_language = ns->language(*this);
      Language new_language = tc.primary_length+1 == old_length ?
                              language_L2 : language_L3;
      if (new_language != old_language)
        ns->node(*this).make_reducible(*this,ns,new_language);
      bool inspect_needed = old_language == language_L1;
      if (tc.rhs_changed)
      {
        if (tc.lhs_node != tc.primary)
        {
          if (tc.do_total_overlaps)
            rm.jm.schedule_overlap(ns,tc.lhs_node,offset,true);
        }
        else
          publish_dubious_rhs(ns);
      }
      Node_Reference tsw(*this,ns->reduction.trailing_subword);
      if (tsw != tc.primary)
      {
        tsw->detach(*this,tsw);
        ns->node(*this).reduction.trailing_subword = tc.primary;
        tc.primary->attach(*this);
      }
      if (inspect_needed)
        inspect(node,false);
    }
    else if (tc.new_reduction)
      if (node->reduced.word_length+1 - tc.length == node->reduced.max_accepted)
        inspect(node,false);
    if (!keep ||
        ns->prefix != node && tc.length > 1 && tc.rhs_changed)
      rm.jm.schedule_partial_reduction_check(node,rvalue);
  }
}

/**/

void Node_Manager::inspect(Node_Reference nr,bool check_height)
{

  /* inspect() is used to correct the  max_accepted,min_height and max_height
     members in prefix nodes after a change to the tree.
     This used to be a recursive member function of Node but is now
     non-recursive since the old version could cause stack overflow.
     It is questionable whether the check_height parameter is useful. It
     might be better to dispense with it and always check the height. However,
     it may just about be worth having as when check_height is false we will
     save on CPU jump target mispredictions.
  */
  for (;;)
  {
    Node &node = nr->node(*this);
    Word_Length old_max_height = node.reduced.max_height;
    Word_Length old_max_accepted = node.reduced.max_accepted;
    int is_new = node.flagged(NF_NEW);
    if (check_height)
      node.reduced.max_height = node.reduced.word_length+1;
    node.reduced.max_accepted = 0;
    node.clear_flags(NF_NEW|NF_PARENT);
    if (!node.flagged(NF_SPARSE))
    {
      Ordinal child_end;
      Ordinal child_start;
      valid_children(&child_start,&child_end,node.rvalue);
      for (Ordinal g = child_start; g < child_end;g++)
      {
        const Node &ns = *node.fast_find_node(*this,node.reduced.child.dense[g]);
        if (ns.prefix == Node_ID(nr)) /* Cast needed for MSVC C12 compatibility */
          ns.rebind(&node,*this,check_height);
        else if (!ns.is_final())
        {
          Word_Length accepted = node.reduced.word_length+1-ns.reduced.word_length;
          if (accepted > node.reduced.max_accepted)
            node.reduced.max_accepted = accepted;
        }
      }
    }
    else
    {
      Node::Sparse_Node &sn = *node.reduced.child.sparse;
      node.reduced.max_accepted = node.reduced.word_length - node.fast_find_node(*this,sn.suffix)->reduced.word_length;
      for (Ordinal i = 0; i < sn.nr_children;i++)
        node.fast_find_node(*this,sn.children[i])->rebind(&node,*this,check_height);
    }
    if (node.reduced.max_height == old_max_height)
      check_height = false;
    if (is_new ||
        node.reduced.max_height != old_max_height ||
        node.reduced.max_accepted != old_max_accepted)
    {
      if (node.prefix)
      {
        nr = node.parent(*this);
        continue;
      }
    }
    break;
  }
}

/**/

bool Node_Manager::in_tree_as_subword(const Equation_Word & lhs) const
{
  /* Check whether a potential new LHS will cause some
     existing nodes or equations to be eliminated.
     This is very similar to check_transitions()
  */
  Node_Iterator ni(*this);
  bool inside = true;
  Node_Reference nr;
  ni.scan(&nr,true); // skip the root
  Word_Length lhs_length = lhs.fast_length();

  if (pd.is_coset_system)
  {
    Ordinal first_letter = lhs.first_letter();
    while (ni.next(&nr,inside))
    {
      inside = false;
      if (!nr->is_final())
      {
        Word_Length offset = ni.word_length();
        if (nr->reduced.max_height >= offset + lhs_length &&
            (first_letter < pd.coset_symbol /* g subword can be anywhere */ ||
             nr->rvalue > pd.coset_symbol /* h node can be followed by anything */))
          inside = true;
        if (inside)
        {
          /* We may be somewhere which could have a simplification or incorrect
             prefix change, but we may not be far enough in yet */
          if (first_letter >= pd.coset_symbol /* we must be at an h node so can look */ ||
              nr->rvalue <= pd.coset_symbol /* we must want to read a g word so can look */)
            if (in_tree_as_subword_inner(lhs,nr))
              return true;
        }
        if (!nr->flagged(NF_PARENT))
          inside = false;
      }
    }
  }
  else
  {
    while (ni.next(&nr,inside))
    {
      inside = false;
      if (!nr->is_final())
      {
        Word_Length offset = ni.word_length();
        if (nr->reduced.max_height >= offset + lhs_length)
        {
          inside = true;
          if (in_tree_as_subword_inner(lhs,nr))
            return true;
        }
        if (!nr->flagged(NF_PARENT))
          inside = false;
      }
    }
  }
  return false;
}

/**/

bool Node_Manager::in_tree_as_subword_inner(const Equation_Word & word,Node_Reference nr) const
{
  /* Before deciding to pool a rule a=b, see if a occurs as
     a subword of an existing LHS or RHS.
     Node_Manager::in_tree_as_subword() ensures this method is only called
     with a word that can occur as a suffix of this node.
  */
  Word_Length length = word.fast_length();
  const Ordinal * values = word.buffer();
  Word_Length offset = nr->reduced.word_length;

  Word_Length i;
  for (i = 0;i < length;)
  {
    nr = nr->fast_child(*this,nr,values[i]);
    if (nr.is_null())
      break;
    i++;
    if (nr->is_final())
      break;
    if (nr->reduced.max_height < offset + length)
      break;
  }

  return i == length;
}

/**/

Node_Reference Node_Manager::construct_equation(Node_Handle prefix,
                                                Ordinal rvalue,
                                                unsigned short eq_flags,
                                                Node_Handle rhs_node,
                                                Node_Handle trailing_subword)

{
  MAF_ASSERT(!prefix->is_final(),maf.container,("Attempt to create equation with reducible prefix\n"));
  Node_Reference e = prefix->fast_child(*this,prefix,rvalue);
  bool new_node(e.is_null());
  bool new_equation(new_node);
  if (new_node)
    e = node_get();
  else
  {
    if (!e->is_final())
    {
      e->node(*this).make_reducible(*this,e,language_L1);
      new_equation = true;
    }
  }

  eq_flags &= EQ_AXIOM;
  if (!trailing_subword->is_final())
  {
    eq_flags |= EQ_PRIMARY;
    if (new_equation)
      current.reduction_added++;
  }
  MAF_ASSERT(!rhs_node->is_final(),maf.container,("Attempt to create equation with reducible RHS\n"));

  e->node(*this).construct_equation(*this,e,prefix,rvalue,eq_flags,rhs_node,trailing_subword,new_node);
  Language language = e->language(*this);
  if (new_node)
  {
    prefix->node(*this).link(*this,prefix,rvalue,e,LCF_All);
    stats.nc[language]++;
  }
  else
    inspect(prefix,true);
  return e;
}

/**/

void Node_Manager::equate_nodes(Node_Handle first,Node_Handle second,
                                const Derivation &d,Ordinal difference,
                                bool create_now,bool force_insert)
{
  if (!maf.options.no_deductions)
    if (create_now)
    {
      Working_Equation we(*this,first,second,d,difference);
      unsigned ae_flags = AE_TIGHT;
      if (force_insert)
       ae_flags |= AE_INSERT;
      rm.add_equation(&we,ae_flags);
    }
    else
      rm.jm.equate_nodes(first,second,d,difference,force_insert || pd.is_short);
}

/**/

void Node_Manager::update_machine()
{
  rm.update_machine();
}

/**/

void Node_Manager::node_free(Node * node)
{
  rm.kill_node(node);
  node->rvalue = INVALID_SYMBOL;
  bm.node_free(node);
  current.node_freed++; // increment the timestamp for freed nodes in Certificate
  stats.nc[language_A]--;
}

/**/

Node_Reference Node_Manager::node_get()
{
  return bm.node_get(*this);
}

void Node_Manager::purge()
{
#ifndef MAF_USE_LOOKUP
  bm.purge();
#endif
}

/**/

void Node_Manager::publish_bad_difference(Node_Handle node)
{
  if (!in_destructor)
    rm.jm.schedule_difference_correction(node);
}

/**/

void Node_Manager::publish_bad_inverse(Node_Handle node)
{
  if (!in_destructor)
    rm.jm.schedule_inverse_check(node);
}

/**/

void Node_Manager::publish_bad_rhs(Node_Handle e)
{
  if (!in_destructor &&
      !e->flagged(NF_REMOVED)) /* This test is needed because equations
                                  don't detach from their rhs until they
                                  are completely destroyed */
  {
    if (e->fast_is_primary() || rm.status().want_secondaries)
    {
      rm.jm.cancel_jobs(e);
      rm.jm.schedule_jobs(e,NS_Uncorrected_Equation);
    }
    else
      publish_dubious_rhs(e,true);
  }
}

/**/

void Node_Manager::publish_dubious_rhs(Node_Handle e,bool rhs_bad)
{
  Node_Status new_status;
  if (rhs_bad || e->raw_reduced_node(*this)->is_final())
    new_status = NS_Dubious_Uncorrected_Equation;
  else
    switch (e->status())
    {
      case NS_Expanded_Equation:
      case NS_Adopted_Equation:
      case NS_Weak_Secondary_Equation:
        new_status = NS_Dubious_Secondary_Equation;
        break;
      case NS_Unconjugated_Equation:
        new_status = NS_Dubious_Unconjugated_Equation;
        break;
      case NS_Oversized_Equation:
      case NS_Undifferenced_Equation:
        new_status = NS_Dubious_Undifferenced_Equation;
        break;
      case NS_Dubious_Secondary_Equation:
      case NS_Dubious_Unconjugated_Equation:
      case NS_Dubious_Undifferenced_Equation:
        return;
      default:
      case NS_Invalid:      // List the other cases explicitly to show we
      case NS_Removed_Node: //have thought about them
      case NS_Removed_Equation:
      case NS_Uncorrected_Equation:
      case NS_Irreducible:
      case NS_Correction_Pending_Equation:
      case NS_Dubious_Uncorrected_Equation:
        MAF_INTERNAL_ERROR(maf.container,("Unexpected status %d in Node_Manager::publish_dubious_rhs()\n",e->status()));
        return;
    }
  rm.jm.cancel_jobs(e);
  rm.jm.schedule_jobs(e,new_status);
}

/**/

void Node_Manager::publish_partial_reduction(Node_Handle node,Ordinal g)
{
  rm.jm.schedule_partial_reduction_check(node,g);
}

/**/

void Node_Manager::remove_node(Node_Handle nh)
{
  Node & node = nh->node(*this);
  if (node.flagged(NF_REDUCIBLE))
  {
    rm.jm.cancel_jobs(nh);
    if (!in_destructor)
      rm.jm.schedule_jobs(nh,NS_Removed_Equation);
#ifdef DEBUG
    else
      node.flags |= NF_REMOVED; // otherwise destroy() will assert
#endif
    Node_ID tsw = node.reduction.trailing_subword;
    node.fast_find_node(*this,tsw)->detach(*this,tsw);
  }
  else
  {
    node.set_status(*this,NS_Removed_Node);
    /* There is no need to schedule anything for a node that was previously
       irreducible.
       If it was a word-difference it has already been placed on the
       bad_differences list.
    */
  }
#if MAF_USE_LOOKUP
  node.reduction.node_id = nh;
#endif
}

