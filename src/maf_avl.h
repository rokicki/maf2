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
$Log: maf_avl.h $
Revision 1.7  2010/06/10 16:39:52Z  Alun
Fixes to ensure clean compilation with g++
Revision 1.6  2010/06/10 13:58:05Z  Alun
All tabs removed again
Revision 1.5  2010/05/06 14:47:31Z  Alun
Now uses templates and abstraction so that we can construct both pointer and
ID based AVL trees
Revision 1.4  2009/07/04 10:32:06Z  Alun
delete_data() now bool instead of void. Integrity checking method added
Revision 1.3  2008/09/14 15:57:04Z  Alun
"Mid Sep 2008 snapshot"
Revision 1.2  2007/09/07 20:20:19Z  Alun_Williams
*/
#pragma once
#ifndef MAF_AVL_INCLUDED
#define MAF_AVL_INCLUDED 1
#ifndef MAFBASE_INCLUDED
#include "mafbase.h"
#endif

/**
The AVL_Tree class implements the algorithms for building AVL binary trees
(These are almost balanced binary tree for which the comparison count is no
worse than 4% of the optimal and the average count is virtually optimal.)

To use the class you must derive a class from AVL_Tree<class T> which implements
the compare() method.

To insert data into the tree you call insert(T) passing the data to be
inserted. If T is some kind of reference it is the caller's responsibility to
ensure the relevant object exists will exist for as long as it is in the tree,
and that the key of the data won't change.

To remove data you call remove(T). This will remove an equal item of
data if there is one.

The space overhead for this class is considerable since for every item in
the tree we construct a node of 16 bytes (assuming 32 bit pointers, 8 bit
bytes and typical alignment restrictions). If instead we were to create a
contiguous array of pointers we would save 12 bytes for every item, and lookup
would be just as fast. On the other hand insertion and deletion of items
would be much more expensive.

As this class is used in MAF it would be OK to replace it with a class that
implemented red-black trees instead. But I am much more familiar with AVL
trees than red-black trees and already had code to hand which could easily
be adapted to implement this class.
**/

class AVL_Node
{
  friend class Abstract_AVL_Tree;
  private:
    AVL_Node *left;
    AVL_Node *right;
    /* bf takes following values -1 left subtree is taller,
                                  0 subtrees are same height
                                  1 right subtree taller
       It is used to prevent height of left and right sub trees differing by
       more than 1.
    */
    signed char bf;
  protected:
    AVL_Node() :
      left(0),
      right(0),
      bf(0)
    {}
  public:
    State_Count depth() const;
    void check() const;
};

class Abstract_AVL_Tree
{
  private:
    AVL_Node * root;
    short taller;
    short shorter;
    State_Count nr_nodes;
  public:
    State_Count node_count() const
    {
      return nr_nodes;
    }
    State_Count depth() const;
  protected:
    Abstract_AVL_Tree();
    virtual ~Abstract_AVL_Tree();
#if 0
    void visit_in_order() const
    {
      visit_in_order(root);
    }
    void visit_in_post_order() const
    {
      visit_in_post_order(root);
    }
    void visit_in_pre_order() const
    {
      visit_in_pre_order(root);
    }
    void visit_in_reverse_order() const
    {
      visit_in_reverse_order(root);
    }
#endif
  protected:
    AVL_Node * pluck_first();
//    // queries
//    const AVL_Node * find_first() const;
//    const AVL_Node * find_last() const;
    void empty();
    AVL_Node * start()
    {
      return root;
    }
    void insert()
    {
      insert(root);
    }
    void remove()
    {
      remove(root);
    }
  private:
    virtual int compare(const AVL_Node &d2) const = 0;
    virtual AVL_Node * create() const = 0;
    virtual void destroy(AVL_Node &) const = 0;
//    virtual void visit(const AVL_Node &) const {};
    // commands
    void detach_first_node(AVL_Node * &root,AVL_Node * * deleted_node);
    void remove(AVL_Node * &root);
    void fix_left_shortening(AVL_Node * &root);
    void fix_right_shortening(AVL_Node * &root);
    void insert(AVL_Node * &root);
    static void rotate_left(AVL_Node *&node);
    static void rotate_right(AVL_Node *&node);
#if 0
    // const commands
    void visit_in_order(const AVL_Node *root) const;
    void visit_in_post_order(const AVL_Node *root) const;
    void visit_in_pre_order(const AVL_Node *root) const;
    void visit_in_reverse_order(const AVL_Node *root) const;
#endif
    void check() const;
};

template<class T> class AVL_Tree;

template<class T> class Actual_AVL_Node : public AVL_Node
{
  friend class AVL_Tree<T>;
  private:
    T data;
    Actual_AVL_Node(T t) :
      AVL_Node(),
      data(t)
    {}
};

/* This is the implementation of a type safe abstract AVL_Tree.
   It is still necessary to derive another class from this that
   implements the compare() function, because we expect the data
   being managed to support multiple collate sequences.
   T should normally be either a pointer or an ID type.
*/

template<class T> class AVL_Tree : public Abstract_AVL_Tree
{
  private:
    T work;
    typedef Actual_AVL_Node<T> My_Node;

    /* compare must return 1,0,-1 according to whether
       d1 > d2, d1 == d2, d1 < d2 */
    virtual int compare(T d1, T d2) const = 0;

    AVL_Node * create() const
    {
      return new My_Node(work);
    }
    void destroy(AVL_Node & anode) const
    {
      delete (My_Node *) &anode;
    }
    virtual int compare(const AVL_Node & d2) const
    {
      return compare(work,((My_Node &) d2).data);
    }
  public:
    ~AVL_Tree()
    {
      empty();
    }
    bool insert(T t)
    {
      work = t;
      State_Count was_nr_nodes = node_count();
      Abstract_AVL_Tree::insert();
      return node_count() > was_nr_nodes;
    }
    bool remove(T t)
    {
      work = t;
      State_Count was_nr_nodes = node_count();
      Abstract_AVL_Tree::remove();
      return node_count() < was_nr_nodes;
    }
    T pluck_first()
    {
      My_Node * node = (My_Node *) Abstract_AVL_Tree::pluck_first();
      if (node)
      {
        work = node->data;
        delete node;
      }
      else
        work = T(0);
      return work;
    }

};

#endif
