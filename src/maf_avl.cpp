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


// $Log: maf_avl.cpp $
// Revision 1.6  2010/05/05 23:52:04Z  Alun
// Code is now more abstract so we can support both ID and pointer based
// data structures
// Revision 1.5  2009/09/12 18:47:47Z  Alun
// printf() style functions cleaned up considerably
// code now compiles cleanly in g++ both 32-bit and 64-bit
// all tabs removed from source files
// Revision 1.4  2009/04/30 12:54:32Z  Alun
// Integrity check method added. Fixed bugs that could cause corruption when
// duplicate item was inserted or after unsuccessful deletion
// Revision 1.3  2007/09/07 20:20:16Z  Alun_Williams
//

#include "awcc.h"
#include "awdefs.h"
#include "maf_avl.h"

State_Count AVL_Node::depth() const
{
  if (!this)
    return 0;
  return max(left->depth(),right->depth())+1;
}

void AVL_Node::check() const
{
  if (this)
  {
    left->check();
    right->check();
    int answer = right->depth() - left->depth();
    if (answer != bf)
      * (char *) 0 = 0;
  }
}


Abstract_AVL_Tree::Abstract_AVL_Tree() :
  root(0),
  taller(false),
  shorter(false),
  nr_nodes(0)
{
}

Abstract_AVL_Tree::~Abstract_AVL_Tree()
{
  empty();
}

/**/

void Abstract_AVL_Tree::empty()
{
  AVL_Node * anode;
  while ((anode = pluck_first())!=0)
    destroy(*anode);
}

void Abstract_AVL_Tree::check() const
{
  root->check();
}

State_Count Abstract_AVL_Tree::depth() const
{
  return root->depth();
}

void Abstract_AVL_Tree::remove(AVL_Node * &root)
{
  if (!root)
  {
    shorter = 0;
    return;
  }
  int cmp = compare(*root);

  if (cmp == 0)
  {
    AVL_Node * unwanted = root;
    if (!root->left)
    {
      /* No left tree so we can replace ourself with our right subtree */
      shorter = 1;
      root = root->right;
    }
    else if (!root->right)
    {
      /* No right tree so we can replace ourself with our left subtree */
      shorter = 1;
      root = root->left;
    }
    else /* Hard case! */
    {
      AVL_Node * next;
      /* Get the immediate succesor of this item - left most item in right subtree */
      detach_first_node(root->right,&next);

      /* Replace ourself with that node */
      next->left = root->left;
      next->right = root->right;
      next->bf = root->bf;
      root = next;
      if (shorter)
        fix_right_shortening(root);
    }
    destroy(*unwanted);
    nr_nodes--;
  }
  else if (cmp > 0)
  {
    remove(root->right);
    if (shorter)
      fix_right_shortening(root);
  }
  else
  {
    remove(root->left);
    if (shorter)
      fix_left_shortening(root);
  }
}

/**/

void Abstract_AVL_Tree::detach_first_node(AVL_Node * &root,AVL_Node * * deleted_node)
{
  /* pluck the left most element of the avl tree */

  if (root->left == 0)
  {
    shorter = 1;
    *deleted_node = root;
    root = root->right;
  }
  else
  {
    detach_first_node(root->left,deleted_node);
    if (shorter)
      fix_left_shortening(root);
  }
}

/**/
#if 0

const AVL_Node * Abstract_AVL_Tree::find_first() const
{
  AVL_Node * answer = root;
  if (!answer)
    return 0;
  while (answer->left)
    answer = answer->left;
  return answer;
}

/**/

const AVL_Node * Abstract_AVL_Tree::find_last() const
{
  AVL_Node * answer = root;
  if (!answer)
    return 0;
  while (answer->right)
    answer = answer->right;
  return answer;
}

#endif

/**/

void Abstract_AVL_Tree::fix_left_shortening(AVL_Node * &root)
{
  /* Performs necessary rotations after our left subtree got shorter */
  if (root->bf <= 0)
  {
    /* 0:  1  ->   1   -1:  1   -> 1    */
    /*    / \     /  \     / \    /  \  */
    /*   m   m   m-1  m   m+1 m  m    m */
    shorter = root->bf++;
  }
  else if (root->right->bf >= 0)
  {
    /* in diagram below rhs */
    /* shows tree after deletion */
    /* case 1 */
    /*   1  ->       2     */
    /*  / \         / \    */
    /* m   2       1   m+1 */
    /*    / \     / \      */
    /*   m   m+1 m   m     */
    /* case 0 */
    /*   1  ->       2     */
    /*  / \         / \    */
    /* m   2       1   m+1 */
    /*    / \     / \      */
    /*   m+1 m+1  m   m+1   */
    shorter = root->right->bf!=0;
    /* root becomes root->left */
    root->bf = !shorter;
    /* root right becomes root */
    root->right->bf = -!shorter;
    rotate_left(root);
  }
  else
  {
    /*   1           ->     3      */
    /*  / \                /  \    */
    /* m   2 ->   3       1     2  */
    /*    / \    / \     / \   / \ */
    /*   3   m  a    2  m   a b   m*/
    /*  / \        /  \            */
    /* a   b      b    m           */
    /* a and b are both of height  */
    /*  m or m-1 and not both m-1  */
    rotate_right(root->right);
    rotate_left(root);
    root->left->bf = -(root->bf == 1);
    root->right->bf = root->bf == -1;
    root->bf = 0;
  }
}

/**/

void Abstract_AVL_Tree::fix_right_shortening(AVL_Node * &root)
{
  /* Performs necessary rotations after our right subtree got shorter.
     Should be "opposite" of previous function i.e. replace
     left by right,right by left,and change sign of all comparisons and
     assignments to bf */
  if (root->bf >= 0)
  {
    /* 0:  1  ->   1   1:   1   -> 0   */
    /*    / \     /  \     / \    / \  */
    /*   m   m   m    m-1 m  m+1 m   m */
    shorter = root->bf--;
  }
  else if (root->left->bf <= 0)
  {
    /* in diagram below rhs */
    /* shows tree after deletion */
    /* case -1 */
    /*     1  ->       2      */
    /*    / \         / \     */
    /*   2   m       m+1 1    */
    /*  / \             / \   */
    /* m+1 m           m   m  */
    /* case 0 */
    /*    1  ->       2       */
    /*   / \         / \      */
    /*  2   m     m+1   1     */
    /* / \             / \    */
    /*m+1 m+1        m+1  m   */
    /* root becomes root->right */
    shorter = root->left->bf !=0;
    root->bf = -!shorter;
    /* root->left becomes root */
    root->left->bf = !shorter ;
    rotate_right(root);
  }
  else
  {
    /*    1           ->     3      */
    /*   / \                /  \    */
    /*  2   m   2->3       2    1   */
    /* / \        / \     / \  / \  */
    /* m   3     2   b   m   a b  m */
    /*    / \   / \                 */
    /*   a   b m   a                */
    /* a and b are both of height   */
    /*  m or m-1 and not both m-1 */
    rotate_left(root->left);
    rotate_right(root);
    root->right->bf = root->bf == -1;
    root->left->bf = -(root->bf ==1);
    root->bf = 0;
  }
}

/**/

void Abstract_AVL_Tree::insert(AVL_Node * &root)
{
  if (root == 0)
  {
    root = create();
    taller = 1;
    nr_nodes++;
  }
  else
  {
    int cmp = compare(*root);
    if (cmp > 0)
    {
      insert(root->right);
      if (taller) /* right subtree increased in height */
      {
        if (root->bf <= 0)
        {
          /* -1 : 1  ->  1     0: 1  -> 1    */
          /*     / \    / \      / \   / \   */
          /*    m+1 m  m+1 m+1  m   m m   m+1*/
          taller = ++root->bf;
        }
        else
        {
          taller = 0;
          if (root->right->bf > 0)
          {
            /* case 0 can't happen because if right subtree increased in
               height so its left and right must have different heights */

             /* in diagram below lhs*/
             /* shows tree after insertion */
             /*   1     ->   2       */
             /*  / \        / \      */
             /* m   2      1   m+1   */
             /*    / \    /  \       */
             /*   m  m+1 m    m      */
             rotate_left(root);
             root->bf = root->left->bf = 0;
          }
#ifdef DEBUG
          else if (root->right->bf == 0)
            * (char *) 0 = 0; /* We really can't get here! */
#endif
          else /* -1 */
          {
            /*   1           ->     3       */
            /*  / \                /  \     */
            /* m   2 ->   3       1     2   */
            /*    / \    / \     / \   / \  */
            /*   3   m  a    2  m   a  b  m */
            /*  / \        /  \             */
            /* a   b      b    m            */
            /* a and b are both of height   */
            /* m or m-1 and not both m-1    */
            rotate_right(root->right);
            rotate_left(root);
            root->left->bf = -(root->bf == 1);
            root->right->bf = root->bf == -1;
            root->bf = 0;
          }
        }
      }
    }
    else if (cmp < 0)
    {
      insert(root->left);
      if (taller) /* left subtree increased in height */
      {
        if (root->bf >= 0)
          taller = --root->bf;
        else
        {
          taller = 0;
#ifdef DEBUG
          if (root->left->bf==0)
            * (char *) 0 = 0;
#endif
          if (root->left->bf < 0)
          {
            /* case 0 can't happen because if right subtree increased in
               height so its left and right must have different heights */
            rotate_right(root);
            root->bf = root->right->bf = 0;
          }
          else /* 1 */
          {
            rotate_left(root->left);
            rotate_right(root);
            root->left->bf = -(root->bf == 1);
            root->right->bf = (root->bf == -1);
            root->bf = 0;
          }
        }
      }
    }
    else
      taller = 0;
  }
}

/**/

AVL_Node * Abstract_AVL_Tree::pluck_first()
{
  AVL_Node * plucked = 0;
  if (root)
    detach_first_node(root,&plucked);
  if (plucked)
    nr_nodes--;
  return plucked;
}

/**/

void Abstract_AVL_Tree::rotate_left(AVL_Node *&node)
{
  /* Effect of rotate_left():

            node            1
           / \             / \
          a   1         node  c
             / \         / \
            b   c       a   b */

  AVL_Node * temp = node->right;
  node->right = temp->left;
  temp->left = node;
  node = temp;
}

/**/

void Abstract_AVL_Tree::rotate_right(AVL_Node *&node)
{
  /* Effect of rotate_right():

            node            1
           / \             / \
          1   c           a   node
         / \                 / \
        a   b               b   c
  */
  AVL_Node *temp = node->left;
  node->left = temp->right;
  temp->right = node;
  node = temp;
}

/**/

#if 0

void Abstract_AVL_Tree::visit_in_order(const AVL_Node *root) const
{
  while (root)
  {
    visit_in_order(root->left);
    visit(*root);
    root = root->right;
  }
}

/**/

void Abstract_AVL_Tree::visit_in_post_order(const AVL_Node *root) const
{
  if (root)
  {
    visit_in_post_order(root->left);
    visit_in_post_order(root->right);
    visit(*root);
  }
}

/**/

void Abstract_AVL_Tree::visit_in_pre_order(const AVL_Node *root) const
{
  while (root)
  {
    visit(*root);
    visit_in_pre_order(root->left);
    root = root->right;
  }
}

/**/

void Abstract_AVL_Tree::visit_in_reverse_order(const AVL_Node *root) const
{
  while (root)
  {
    visit_in_reverse_order(root->right);
    visit(*root);
    root = root->left;
  }
}

#endif
