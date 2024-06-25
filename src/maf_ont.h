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
$Log: maf_ont.h $
Revision 1.8  2010/06/10 13:58:09Z  Alun
All tabs removed again
Revision 1.7  2010/05/27 07:37:23Z  Alun
Changes required by new style Node_Manager interface and changes to AVL stuff.
removed equations are now ordered with H equations first and coset equations
last in the probably forlorn hope of reducing the number of times coset
equations get changed because of changes to the H-word part.
Revision 1.6  2009/09/12 18:48:37Z  Alun
printf() style functions cleaned up considerably
code now compiles cleanly in g++ both 32-bit and 64-bit
all tabs removed from source files
Revision 1.5  2008/09/04 09:26:52Z  Alun
"Early Sep 2008 snapshot"
Revision 1.4  2007/10/24 21:15:36Z  Alun
*/

#pragma once
#ifndef MAF_ONT_INCLUDED
#define MAF_ONT_INCLUDED 1
#ifndef MAF_AVL_INCLUDED
#include "maf_avl.h"
#endif

class Alphabet;

#ifndef MAFNODE_INCLUDED
#include "mafnode.h"
#endif

#include "maf_nm.h"
#include "container.h"

/* NB. When using these classes one must be certain that the key of the
equation does not change */

class Ordered_Node_Tree : public AVL_Tree<Node_ID>
{
  public:
    Node_Manager & nm;
  public:
    Ordered_Node_Tree(Node_Manager  &nm_) :
      nm(nm_)
    {}
    virtual void extract_key(State data) = 0;
    void delete_state(Node_Handle data)
    {
      Node_ID nid(data);
      extract_key(data);
      AVL_Tree<Node_ID>::remove(nid);
    }
    void remove(Node_Handle e)
    {
      delete_state(e);
      e->node(nm).clear_flags(EQ_QUEUED);
      e->detach(nm,e);
    }
    void insert_state(Node_Handle data)
    {
      Node_ID nid(data);
      extract_key(data);
      insert(nid);
    }
    void add_state(Node_Handle data)
    {
      insert_state(data);
    }
    void add(Node_Handle e)
    {
      MAF_ASSERT(!e.is_null() && !e->flagged(EQ_QUEUED),
                 nm.maf.container,("Bad attempt to queue equation\n"));
      insert_state(e);
      e->attach(nm)->set_flags(EQ_QUEUED);
    }
    bool use(Node_Reference *e)
    {
      *e = Node_Reference(nm,(Node_ID)pluck_first());
      return !e->is_null();
    }
    State_Count length() const
    {
      return node_count();
    }
};

class By_Left_Size_Node_Tree : public Ordered_Node_Tree
{
  private:
    Ordinal_Word test_word;
    Word_Length test_length;
    bool reversed;
  public:
    By_Left_Size_Node_Tree(Node_Manager  &nm_,bool reversed_ = false) :
      Ordered_Node_Tree(nm_),
      test_word(nm_.alphabet()),
      reversed(reversed_)
    {}
    void extract_key(State data)
    {
      data->read_word(&test_word,nm);
      test_length = data->length(nm);
    }
    // virtual method we have to implement
    int compare(Node_ID,Node_ID d2) const
    {
      /* AVL_Tree guarantees first parameter is pointer to data being inserted
         or removed. So we can compare the other word against the current
         test_word.
         We rank equations as follows:
         1) Shorter KHS length is better
         3) If equations are still equal lesser LHS is better
      */
      State e2 = Node::fast_find_node(nm,(Node_ID)d2);

      Word_Length l2 = e2->length(nm);
      if (l2 != test_length)
        if (reversed)
          return test_length > l2  ? -1 : 1;
        else
          return test_length > l2  ? 1 : -1;

      return -e2->compare(nm,test_word);
    }
};

class By_Left_Size_Node_Tree_Queued : public By_Left_Size_Node_Tree
{
  public:
    By_Left_Size_Node_Tree_Queued(Node_Manager  &nm_,bool reversed_ = false) :
      By_Left_Size_Node_Tree(nm_,reversed_)
      {}
    bool use(Node_Reference *e)
    {
      if (By_Left_Size_Node_Tree::use(e))
      {
        (*e)->node(nm).clear_flags(EQ_QUEUED);
        return true;
      }
      return false;
    }
};

class By_Size_Node_Tree : public Ordered_Node_Tree
{
  private:
    Ordinal_Word test_word;
    Word_Length test_length;
    Word_Length test_rlength;
  public:
    By_Size_Node_Tree(Node_Manager  &nm_) :
      Ordered_Node_Tree(nm_),
      test_word(nm_.alphabet())
    {}
    void extract_key(State data)
    {
      data->read_word(&test_word,nm);
      test_rlength = data->raw_reduced_length(nm);
      test_length = data->length(nm);
    }
    // virtual method we have to implement
    int compare(Node_ID,Node_ID d2) const
    {
      /* AVL_Tree guarantees first parameter is pointer to data being inserted
         or removed. So we can compare the other word against the current
         test_word.
         We rank equations as follows:
         1) G equations are better than H equations, H equations are better
            than coset equations
         2) Shorter RHS length is better
         3) Amongst equations of equal RHS length shorter LHS is better
         4) If equations are still equal lesser LHS is better
      */
      State e2 = Node::fast_find_node(nm,(Node_ID) d2);

      Word_Length l2 = e2->raw_reduced_length(nm);
      if (l2 != test_rlength)
        return test_rlength > l2 ? 1 : -1;

      l2 = e2->length(nm);
      if (l2 != test_length)
        return test_length > l2  ? 1 : -1;

      return -e2->compare(nm,test_word);
    }
};


class By_Size_Removed_Time_Node_Tree : public Ordered_Node_Tree
{
  private:
    Word_Length test_length;
    Word_Length test_rlength;
    Total_Length test_tl;
    int equation_type;
    ID timestamp;
  public:
    By_Size_Removed_Time_Node_Tree(Node_Manager  &nm_) :
      Ordered_Node_Tree(nm_)
    {}
    void extract_key(State data)
    {
      test_rlength = data->raw_reduced_length(nm);
      test_length = data->length(nm);
      test_tl = test_rlength + test_length;
      if (nm.pd.is_coset_system && data->first_letter(nm) >= nm.pd.coset_symbol)
        equation_type = data->last_letter() > nm.pd.coset_symbol ? 1 : 2;
      else
        equation_type = 0;
      timestamp = data->removed_id();
    }
    // virtual method we have to implement
    int compare(Node_ID,Node_ID d2) const
    {
      /* AVL_Tree guarantees first parameter is pointer to data being inserted
         or removed. So we can compare the other word against the current
         test_word.
         We rank equations as follows:
         1) Shorter LHS length is better
         2) Amongst equations of equal LHS length shorter RHS is better
         (The opposite rule seems just as sensible and is what I used to
          do. But there seems to be a slight amount of evidence that
          this way round works a bit better on average, though it is
          frequently worse. This should be controlled by an option)
         3) If equations are still equal equation removed first is better
      */
      State e2 = Node::fast_find_node(nm,(Node_ID) d2);

      int e2_type = 0;

      if (nm.pd.is_coset_system)
      {
        if (e2->last_letter() > nm.pd.coset_symbol)
          e2_type = 1;
        else if (e2->first_letter(nm) >= nm.pd.coset_symbol)
          e2_type = 2;
      }

      if (e2_type != equation_type)
        return equation_type > e2_type ? 1 : -1;

      Word_Length l2 = e2->length(nm);
      if (l2 != test_length)
        return test_length > l2 ? 1 : -1;

      if (equation_type != 2)
      {
        l2 = e2->raw_reduced_length(nm);
        if (l2 != test_rlength)
          return test_rlength > l2  ? 1 : -1;
      }

      return timestamp < e2->removed_id() ? 1 : -1;
    }
};

class By_Size_Node_Tree_Queued : public By_Size_Removed_Time_Node_Tree
{
  public:
    By_Size_Node_Tree_Queued(Node_Manager  &nm_) :
      By_Size_Removed_Time_Node_Tree(nm_)
      {}
    bool use(Node_Reference *e)
    {
      if (By_Size_Removed_Time_Node_Tree::use(e))
      {
        (*e)->node(nm).clear_flags(EQ_QUEUED);
        return true;
      }
      return false;
    }
};

class By_SizeTime_Node_Tree : public Ordered_Node_Tree
{
  private:
    ID expand_timestamp;
    Ordinal_Word test_word;
    Word_Length test_length;
    Word_Length test_rlength;
    bool expanded;
  public:
    By_SizeTime_Node_Tree(Node_Manager  &nm_) :
      Ordered_Node_Tree(nm_),
      test_word(nm_.alphabet())
    {}
    void extract_key(State data)
    {
      expand_timestamp = data->expanded_timestamp();
      expanded = expand_timestamp != 0;
      data->read_word(&test_word,nm);
      test_rlength = data->raw_reduced_length(nm);
      test_length = data->length(nm);
    }
    // virtual method we have to implement
    int compare(Node_ID ,Node_ID d2) const
    {
      /* AVL_Tree guarantees first parameter is pointer to data being inserted
         or removed. So we can compare the other word against the current
         test_word.
         We rank equations as follows:
         1) Equations that have not yet been expanded are better than
            those that have.
         2) Less recently expanded equations are better
         3) Shorter RHS length is better
         4) Amongst equations of equal RHS length shorter LHS is better
         5) If equations are still equal lesser LHS is better
      */
      State e2 = Node::fast_find_node(nm,(Node_ID) d2);

      ID time = e2->expanded_timestamp();
      bool e2_expanded = time != 0;

      if (e2_expanded != expanded)
        return expanded ? 1 : -1;

      if (time != expand_timestamp)
        return expand_timestamp > time ? 1 : -1;

      Word_Length l2 = e2->raw_reduced_length(nm);
      if (l2 != test_rlength)
        return test_rlength > l2 ? 1 : -1;

      l2 = e2->length(nm);
      if (l2 != test_length)
        return test_length > l2  ? 1 : -1;

      return -e2->compare(nm,test_word);
    }
};

class By_Time_Node_Tree : public Ordered_Node_Tree
{
  private:
    ID expand_timestamp;
    ID timestamp;
    bool expanded;
  public:
    By_Time_Node_Tree(Node_Manager  &nm_) :
      Ordered_Node_Tree(nm_)
    {}
    void extract_key(State data)
    {
      expand_timestamp = data->expanded_timestamp();
      expanded = expand_timestamp != 0;
      timestamp = data->id();
    }
    // virtual method we have to implement
    int compare(Node_ID ,Node_ID d2) const
    {
      /* AVL_Tree guarantees first parameter is pointer to data being inserted
         or removed. So we can compare the other node against the extracted key.
         We rank equations as follows:
         1) Equations that have not yet been expanded are better than
            those that have.
         2) Less recently expanded equations are better
         3) Older equation is better
      */
      State e2 = Node::fast_find_node(nm,(Node_ID) d2);

      ID time = e2->expanded_timestamp();
      bool e2_expanded = time != 0;

      if (e2_expanded != expanded)
        return expanded ? 1 : -1;

      if (time != expand_timestamp)
        return expand_timestamp > time ? 1 : -1;

      time = e2->id();
      if (time != timestamp)
        return timestamp > time ? 1 : -1;

      return 0;
    }
};

#endif
