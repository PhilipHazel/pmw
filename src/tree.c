/*************************************************
*   PMW binary balanced tree handling functions  *
*************************************************/

/* Copyright (c) Philip Hazel 2021 */
/* This file created: December 2020 */
/* This file last modified: January 2021 */

#include "pmw.h"


#define tree_lbal      1         /* left subtree is longer */
#define tree_rbal      2         /* right subtree is longer */
#define tree_bmask     3         /* mask for flipping bits */



/*************************************************
*         Insert a new node into a tree          *
*************************************************/

/* This function is used for a number of different binary trees, which remember
things that need to be looked up by name.

Arguments:
  treebase      pointer to the root of the tree
  node          the node to insert, with data fields filled in

Returns:        TRUE if the node is successfully inserted,
                FALSE otherwise (duplicate node found)
*/

BOOL
tree_insert(tree_node **treebase, tree_node *node)
{
tree_node *p = *treebase;
tree_node **q, *r, *s, **t;
int a;

/* Initialize the tree fields of the node */

node->left = NULL;
node->right = NULL;
node->balance = 0;

/* Deal with an empty tree */

if (p == NULL)
  {
  *treebase = node;
  return TRUE;
  }

/* The tree is not empty. While finding the insertion point, q points to the
pointer to p, and t points to the pointer to the potential re-balancing point.
*/

q = treebase;
t = q;

/* Loop to search tree for place to insert new node */

for (;;)
  {
  int c = Ustrcmp(node->name, p->name);
  if (c == 0) return FALSE;             /* Duplicate found */

  /* Deal with climbing down the tree, exiting from the loop when we reach a
  leaf. */

  q = (c > 0)? &(p->right) : &(p->left);
  p = *q;
  if (p == NULL) break;

  /* Save the address of the pointer to the last node en route which has a
  non-zero balance factor. */

  if (p->balance != 0) t = q;
  }

/* When the above loop completes, q points to the pointer to NULL; that is the
place at which the new node must be inserted. */

*q = node;

/* Set up s as the potential re-balancing point, and r as the next node after
it along the route*/

s = *t;
r = (Ustrcmp(node->name, s->name) > 0)? s->right : s->left;

/* Adjust balance factors along the route from s to node. */

p = r;
while (p != node)
  if (Ustrcmp(node->name, p->name) < 0)
    {
    p->balance = tree_lbal;
    p = p->left;
    }
  else
    {
    p->balance = tree_rbal;
    p = p->right;
    }

/* Now the World-Famous Balancing Act */

a = (Ustrcmp(node->name, s->name) < 0)? tree_lbal : tree_rbal;

if (s->balance == 0)       /* The tree has grown higher */
  s->balance = a;
else if (s->balance != a)  /* The tree has become more balanced */
  s->balance = 0;

else                       /* The tree has got out of balance */
  {
  if (r->balance == a)     /* Perform a single rotation */
    {
    p = r;
    if (a == tree_rbal)
      {
      s->right = r->left;
      r->left = s;
      }
    else
      {
      s->left = r->right;
      r->right = s;
      }
    s->balance = 0;
    r->balance = 0;
    }

  else                          /* Perform a double rotation */
    {
    if (a == tree_rbal)
      {
      p = r->left;
      r->left = p->right;
      p->right = r;
      s->right = p->left;
      p->left = s;
      }
    else
      {
      p = r->right;
      r->right = p->left;
      p->left = r;
      s->left = p->right;
      p->right = s;
      }

    s->balance = (p->balance == a)? (a^tree_bmask) : 0;
    r->balance = (p->balance == (a^tree_bmask))? a : 0;
    p->balance = 0;
    }

  /* Finishing touch */

  *t = p;
  }

return TRUE;     /* Successful insertion */
}


/*************************************************
*          Search tree for node by name          *
*************************************************/

/*
Arguments:
  p          the root node of the tree
  name       the name of the required node

Returns:     pointer to the found node, or NULL
*/

tree_node *
tree_search(tree_node *p, uschar *name)
{
while (p != NULL)
  {
  int c = Ustrcmp(name, p->name);
  if (c == 0) return p;
  p = (c < 0)? p->left : p->right;
  }
return NULL;
}

/* End of tree.c */
