/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2003 Tobias Achterberg                              */
/*                            Thorsten Koch                                  */
/*                  2002-2003 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic Licence.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   nodesel.c
 * @brief  methods and datastructures for node selectors
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include <string.h>

#include "nodesel.h"



/** node priority queue data structure;
 *  the fields lowerboundnode, lowerbound, nlowerbounds and validlowerbound are only used for node selection rules,
 *  that don't store the lowest bound node in the first slot of the queue
 */
struct NodePQ
{
   int              len;                /**< number of used element slots */
   int              size;               /**< total number of available element slots */
   NODE**           slots;              /**< array of element slots */
   NODE*            lowerboundnode;     /**< node with minimal lower bound, or NULL if not available */
   Real             lowerboundsum;      /**< sum of lower bounds of all nodes in the queue */
   Real             lowerbound;         /**< minimal lower bound value of all nodes in the queue */
   int              nlowerbounds;       /**< number of nodes in the queue with minimal lower bound (0 if invalid) */
   unsigned int     validlowerbound:1;  /**< is lower bound value valid? */
};

/** node selector */
struct Nodesel
{
   char*            name;               /**< name of node selector */
   char*            desc;               /**< description of node selector */
   DECL_NODESELFREE ((*nodeselfree));   /**< destructor of node selector */
   DECL_NODESELINIT ((*nodeselinit));   /**< initialize node selector */
   DECL_NODESELEXIT ((*nodeselexit));   /**< deinitialize node selector */
   DECL_NODESELSELECT((*nodeselselect));/**< node selection method */
   DECL_NODESELCOMP ((*nodeselcomp));   /**< node comparison method */
   NODESELDATA*     nodeseldata;        /**< node selector data */
   unsigned int     lowestboundfirst:1; /**< does node comparison sorts w.r.t. lower bound as primal criterion? */
   unsigned int     initialized:1;      /**< is node selector initialized? */
};




/* 
 * node priority queue methods
 */

#define PQ_PARENT(q) (((q)+1)/2-1)
#define PQ_LEFTCHILD(p) (2*(p)+1)
#define PQ_RIGHTCHILD(p) (2*(p)+2)

/** resizes node memory to hold at least the given number of nodes */
static
RETCODE nodepqResize(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set,                /**< global SCIP settings */
   int              minsize             /**< minimal number of storeable nodes */
   )
{
   assert(nodepq != NULL);
   
   if( minsize <= nodepq->size )
      return SCIP_OKAY;

   nodepq->size = SCIPsetCalcTreeGrowSize(set, minsize);
   ALLOC_OKAY( reallocMemoryArray(&nodepq->slots, nodepq->size) );

   return SCIP_OKAY;
}

/** updates the cached minimal lower bound of all nodes in the queue (used for node selection rules, that don't store
 *  the lowest bound node in the first slot of the queue)
 */
static
void nodepqUpdateLowerbound(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set,                /**< global SCIP settings */
   NODE*            node                /**< node to be inserted */
   )
{
   assert(nodepq != NULL);
   assert(set != NULL);
   assert(set->nodesel != NULL);
   assert(!set->nodesel->lowestboundfirst);
   assert(node != NULL);

   assert(nodepq->nlowerbounds > 0 || nodepq->lowerboundnode == NULL);
   debugMessage("update queue's lower bound after adding node %p: nodebound=%g, queuebound=%g, nlowerbounds=%d, lowerboundnode=%p (bound=%g)\n",
      node, SCIPnodeGetLowerbound(node), nodepq->lowerbound, nodepq->nlowerbounds,
      nodepq->lowerboundnode, nodepq->lowerboundnode != NULL ? SCIPnodeGetLowerbound(nodepq->lowerboundnode) : 0.0);
   if( nodepq->validlowerbound )
   {
      assert(nodepq->lowerbound < SCIP_INVALID);
      if( SCIPsetIsLE(set, SCIPnodeGetLowerbound(node), nodepq->lowerbound) )
      {
         if( SCIPsetIsEQ(set, SCIPnodeGetLowerbound(node), nodepq->lowerbound) )
         {
            assert(nodepq->nlowerbounds >= 1);
            nodepq->nlowerbounds++;
         }
         else
         {
            nodepq->lowerboundnode = node;
            nodepq->lowerbound = SCIPnodeGetLowerbound(node);
            nodepq->nlowerbounds = 1;
         }
      }
   }
   debugMessage(" -> new queuebound=%g, nlowerbounds=%d, lowerboundnode=%p\n",
      nodepq->lowerbound, nodepq->nlowerbounds, nodepq->lowerboundnode);

   assert(nodepq->nlowerbounds > 0 || nodepq->lowerboundnode == NULL);
}

/** calculates the minimal lower bound of all nodes in the queue (used for node selection rules, that don't store
 *  the lowest bound node in the first slot of the queue)
 */
static
void nodepqCalcLowerbound(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set                 /**< global SCIP settings */
   )
{
   int i;

   assert(nodepq != NULL);
   assert(set != NULL);
   assert(set->nodesel != NULL);
   assert(!set->nodesel->lowestboundfirst);

   nodepq->validlowerbound = TRUE;
   nodepq->lowerboundnode = NULL;
   nodepq->lowerbound = set->infinity;
   nodepq->nlowerbounds = 0;

   for( i = 0; i < nodepq->len; ++i )
      nodepqUpdateLowerbound(nodepq, set, nodepq->slots[i]);
}

/** creates node priority queue */
RETCODE SCIPnodepqCreate(
   NODEPQ**         nodepq,             /**< pointer to a node priority queue */
   const SET*       set                 /**< global SCIP settings */
   )
{
   assert(nodepq != NULL);

   ALLOC_OKAY( allocMemory(nodepq) );
   (*nodepq)->len = 0;
   (*nodepq)->size = 0;
   (*nodepq)->slots = NULL;
   (*nodepq)->lowerboundnode = NULL;
   (*nodepq)->lowerboundsum = 0.0;
   (*nodepq)->lowerbound = set->infinity;
   (*nodepq)->nlowerbounds = 0;
   (*nodepq)->validlowerbound = TRUE;

   return SCIP_OKAY;
}

/** frees node priority queue, but not the data nodes themselves */
void SCIPnodepqDestroy(
   NODEPQ**         nodepq              /**< pointer to a node priority queue */
   )
{
   assert(nodepq != NULL);
   assert(*nodepq != NULL);

   freeMemoryArrayNull(&(*nodepq)->slots);
   freeMemory(nodepq);
}

/** frees node priority queue and all nodes in the queue */
RETCODE SCIPnodepqFree(
   NODEPQ**         nodepq,             /**< pointer to a node priority queue */
   MEMHDR*          memhdr,             /**< block memory buffers */
   const SET*       set,                /**< global SCIP settings */
   TREE*            tree,               /**< branch-and-bound tree */
   LP*              lp                  /**< actual LP data */
   )
{
   int i;

   assert(nodepq != NULL);
   assert(*nodepq != NULL);

   /* free the nodes of the queue */
   for( i = 0; i < (*nodepq)->len; ++i )
   {
      assert((*nodepq)->slots[i] != NULL);
      assert(SCIPnodeGetType((*nodepq)->slots[i]) == SCIP_NODETYPE_LEAF);
      CHECK_OKAY( SCIPnodeFree(&(*nodepq)->slots[i], memhdr, set, tree, lp) );
   }
   
   /* free the queue data structure */
   SCIPnodepqDestroy(nodepq);

   return SCIP_OKAY;
}

/** inserts node into node priority queue */
RETCODE SCIPnodepqInsert(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set,                /**< global SCIP settings */
   NODE*            node                /**< node to be inserted */
   )
{
   SCIP* scip;
   NODESEL* nodesel;
   int pos;

   assert(nodepq != NULL);
   assert(nodepq->len >= 0);
   assert(set != NULL);
   assert(node != NULL);

   scip = set->scip;
   nodesel = set->nodesel;
   assert(nodesel != NULL);
   assert(nodesel->nodeselcomp != NULL);

   CHECK_OKAY( nodepqResize(nodepq, set, nodepq->len+1) );

   /* insert node as leaf in the tree, move it towards the root as long it is better than its parent */
   pos = nodepq->len;
   nodepq->len++;
   nodepq->lowerboundsum += SCIPnodeGetLowerbound(node);
   while( pos > 0 && nodesel->nodeselcomp(scip, nodesel, node, nodepq->slots[PQ_PARENT(pos)]) < 0 )
   {
      nodepq->slots[pos] = nodepq->slots[PQ_PARENT(pos)];
      pos = PQ_PARENT(pos);
   }
   nodepq->slots[pos] = node;

   if( !nodesel->lowestboundfirst )
   {
      /* update the minimal lower bound */
      nodepqUpdateLowerbound(nodepq, set, node);
   }

   return SCIP_OKAY;
}

/** deletes node at given position from the node priority queue; returns TRUE, if the parent fell down to the
 *  free position
 */
static
Bool nodepqDelPos(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set,                /**< global SCIP settings */
   int              rempos              /**< queue position of node to remove */
   )
{
   SCIP* scip;
   NODESEL* nodesel;
   NODE* lastnode;
   int freepos;
   int childpos;
   int parentpos;
   int brotherpos;
   Bool parentfelldown;

   assert(nodepq != NULL);
   assert(nodepq->len > 0);
   assert(set != NULL);
   assert(0 <= rempos && rempos < nodepq->len);

   scip = set->scip;
   nodesel = set->nodesel;
   assert(nodesel != NULL);
   assert(nodesel->nodeselcomp != NULL);

   if( !nodesel->lowestboundfirst )
   {
      assert(nodepq->nlowerbounds > 0 || nodepq->lowerboundnode == NULL);

      /* update the minimal lower bound */
      if( nodepq->nlowerbounds > 0 )
      {
         NODE* node;
         
         node = nodepq->slots[rempos];
         assert(SCIPsetIsGE(set, SCIPnodeGetLowerbound(node), nodepq->lowerbound));
         
         debugMessage("update queue's lower bound after removal of node %p: nodebound=%g, queuebound=%g, nlowerbounds=%d, lowerboundnode=%p (bound=%g)\n",
            node, SCIPnodeGetLowerbound(node), nodepq->lowerbound, nodepq->nlowerbounds, 
            nodepq->lowerboundnode, nodepq->lowerboundnode != NULL ? SCIPnodeGetLowerbound(nodepq->lowerboundnode) : 0.0);
         if( SCIPsetIsEQ(set, SCIPnodeGetLowerbound(node), nodepq->lowerbound) )
         {
            nodepq->nlowerbounds--;
            if( nodepq->nlowerbounds == 0 )
            {
               nodepq->validlowerbound = FALSE;
               nodepq->lowerbound = SCIP_INVALID;
            }
         }
         if( node == nodepq->lowerboundnode )
            nodepq->lowerboundnode = NULL;
         debugMessage(" -> new queuebound=%g, nlowerbounds=%d, lowerboundnode=%p\n",
            nodepq->lowerbound, nodepq->nlowerbounds, nodepq->lowerboundnode);
      }
      assert(nodepq->nlowerbounds > 0 || nodepq->lowerboundnode == NULL);
   }

   /* remove node of the tree and get a free slot,
    * if the removed node was the last node of the queue
    *  - do nothing
    * if the last node of the queue is better than the parent of the removed node:
    *  - move the parent to the free slot, until the last node can be placed in the free slot
    * if the last node of the queue is not better than the parent of the free slot:
    *  - move the better child to the free slot until the last node can be placed in the free slot
    */
   nodepq->lowerboundsum -= SCIPnodeGetLowerbound(nodepq->slots[rempos]);
   freepos = rempos;
   lastnode = nodepq->slots[nodepq->len-1];
   nodepq->len--;

   if( freepos == nodepq->len )
      return FALSE;
   assert(freepos < nodepq->len);

   /* try to move parents downwards to insert last node */
   parentfelldown = FALSE;
   parentpos = PQ_PARENT(freepos);
   while( freepos > 0 && nodesel->nodeselcomp(scip, nodesel, lastnode, nodepq->slots[parentpos]) < 0 )
   {
      nodepq->slots[freepos] = nodepq->slots[parentpos];
      freepos = parentpos;
      parentpos = PQ_PARENT(freepos);
      parentfelldown = TRUE;
   }
   if( !parentfelldown )
   {
      /* downward moving of parents was not successful -> move children upwards */
      while( freepos <= PQ_PARENT(nodepq->len-1) ) /* as long as free slot has children... */
      {
         /* select the better child of free slot */
         childpos = PQ_LEFTCHILD(freepos);
         assert(childpos < nodepq->len);
         brotherpos = PQ_RIGHTCHILD(freepos);
         if( brotherpos < nodepq->len
            && nodesel->nodeselcomp(scip, nodesel, nodepq->slots[brotherpos], nodepq->slots[childpos]) < 0 )
            childpos = brotherpos;
         /* exit search loop if better child is not better than last node */
         if( nodesel->nodeselcomp(scip, nodesel, lastnode, nodepq->slots[childpos]) <= 0 )
            break;
         /* move better child upwards, free slot is now the better child's slot */
         nodepq->slots[freepos] = nodepq->slots[childpos];
         freepos = childpos;
      }
   }
   assert(0 <= freepos && freepos < nodepq->len);
   assert(!parentfelldown || PQ_LEFTCHILD(freepos) < nodepq->len);
   nodepq->slots[freepos] = lastnode;

   return parentfelldown;
}

/** returns the position of given node in the priority queue, or -1 if not existing */
static
int nodepqFindNode(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set,                /**< global SCIP settings */
   NODE*            node                /**< node to find */
   )
{
   int pos;

   assert(nodepq != NULL);
   assert(nodepq->len >= 0);
   assert(set != NULL);
   assert(node != NULL);

   /* search the node in the queue */
   for( pos = 0; pos < nodepq->len && node != nodepq->slots[pos]; ++pos )
   {}

   if( pos == nodepq->len )
      pos = -1;
   
   return pos;
}

/** removes node from the node priority queue */
RETCODE SCIPnodepqRemove(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set,                /**< global SCIP settings */
   NODE*            node                /**< node to remove */
   )
{
   int pos;

   pos = nodepqFindNode(nodepq, set, node);
   if( pos == -1 )
   {
      errorMessage("node doesn't exist in node priority queue");
      return SCIP_INVALIDDATA;
   }

   (void)nodepqDelPos(nodepq, set, pos);

   return SCIP_OKAY;
}

/** returns the best node of the queue without removing it */
NODE* SCIPnodepqFirst(
   const NODEPQ*    nodepq              /**< node priority queue */
   )
{
   assert(nodepq != NULL);
   assert(nodepq->len >= 0);

   if( nodepq->len == 0 )
      return NULL;

   assert(nodepq->slots[0] != NULL);

   return nodepq->slots[0];
}

/** returns the nodes array of the queue */
NODE** SCIPnodepqNodes(
   const NODEPQ*    nodepq              /**< node priority queue */
   )
{
   assert(nodepq != NULL);

   return nodepq->slots;
}

/** returns the number of nodes stored in the node priority queue */
int SCIPnodepqLen(
   const NODEPQ*    nodepq              /**< node priority queue */
   )
{
   assert(nodepq != NULL);
   assert(nodepq->len >= 0);

   return nodepq->len;
}

/** gets the minimal lower bound of all nodes in the queue */
Real SCIPnodepqGetLowerbound(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set                 /**< global SCIP settings */
   )
{
   assert(nodepq != NULL);
   assert(set != NULL);
   assert(set->nodesel != NULL);

   if( set->nodesel->lowestboundfirst )
   {
      /* the node selector's compare method sorts the minimal lower bound to the front */
      if( nodepq->len > 0 )
      {
         assert(nodepq->slots[0] != NULL);
         return SCIPnodeGetLowerbound(nodepq->slots[0]);
      }
      else
         return set->infinity;
   }
   else
   {
      /* we use bookkeeping to remember the lowest bound */

      /* if the cached lower bound is invalid, calculate it */
      if( !nodepq->validlowerbound )
         nodepqCalcLowerbound(nodepq, set);

      assert(nodepq->validlowerbound);
      assert(nodepq->lowerbound < SCIP_INVALID);

      return nodepq->lowerbound;
   }
}

/** gets the node with minimal lower bound of all nodes in the queue */
NODE* SCIPnodepqGetLowerboundNode(
   NODEPQ*          nodepq,             /**< node priority queue */
   const SET*       set                 /**< global SCIP settings */
   )
{
   assert(nodepq != NULL);
   assert(set != NULL);
   assert(set->nodesel != NULL);

   if( set->nodesel->lowestboundfirst )
   {
      /* the node selector's compare method sorts the minimal lower bound to the front */
      if( nodepq->len > 0 )
      {
         assert(nodepq->slots[0] != NULL);
         return nodepq->slots[0];
      }
      else
         return NULL;
   }
   else
   {
      /* we use bookkeeping to remember the lowest bound */

      /* if the cached lower bound node is invalid, calculate it */
      if( !nodepq->validlowerbound || nodepq->lowerboundnode == NULL )
         nodepqCalcLowerbound(nodepq, set);
      
      assert(nodepq->validlowerbound);
      assert(nodepq->lowerbound < SCIP_INVALID);
      assert(nodepq->lowerbound == set->infinity || nodepq->lowerboundnode != NULL); /*lint !e777*/

      return nodepq->lowerboundnode;
   }
}

/** gets the sum of lower bounds of all nodes in the queue */
Real SCIPnodepqGetLowerboundSum(
   NODEPQ*          nodepq              /**< node priority queue */
   )
{
   assert(nodepq != NULL);

   return nodepq->lowerboundsum;
}

/** free all nodes from the queue that are cut off by the given upper bound */
RETCODE SCIPnodepqBound(
   NODEPQ*          nodepq,             /**< node priority queue */
   MEMHDR*          memhdr,             /**< block memory buffer */
   const SET*       set,                /**< global SCIP settings */
   TREE*            tree,               /**< branch-and-bound tree */
   LP*              lp,                 /**< actual LP data */
   Real             upperbound          /**< upper bound: all nodes with lowerbound >= upperbound are cut off */
   )
{
   NODE* node;
   int pos;
   Bool parentfelldown;

   assert(nodepq != NULL);

   debugMessage("bounding node queue of length %d with upperbound=%g\n", nodepq->len, upperbound);
   pos = nodepq->len-1;
   while( pos >= 0 )
   {
      assert(pos < nodepq->len);
      node = nodepq->slots[pos];
      assert(node != NULL);
      assert(SCIPnodeGetType(node) == SCIP_NODETYPE_LEAF);
      if( SCIPsetIsGE(set, SCIPnodeGetLowerbound(node), upperbound) )
      {
         debugMessage("free node in slot %d (len=%d) at depth %d with lowerbound=%g\n",
            pos, nodepq->len, SCIPnodeGetDepth(node), SCIPnodeGetLowerbound(node));
         /* cut off node; because we looped from back to front, the existing children of the node must have a smaller
          * lower bound than the cut off value
          */
         assert(PQ_LEFTCHILD(pos) >= nodepq->len
            || SCIPsetIsLT(set, SCIPnodeGetLowerbound(nodepq->slots[PQ_LEFTCHILD(pos)]), upperbound));
         assert(PQ_RIGHTCHILD(pos) >= nodepq->len
            || SCIPsetIsLT(set, SCIPnodeGetLowerbound(nodepq->slots[PQ_RIGHTCHILD(pos)]), upperbound));

         /* free the slot in the node PQ */
         parentfelldown = nodepqDelPos(nodepq, set, pos);

         /* - if the slot was occupied by the parent, we have to check this slot (the parent) again; unfortunately,
          *   we will check the node which occupied the parent's slot again, even though it cannot be cut off;
          * - otherwise, the slot was the last slot or it was occupied by a node with a position greater than
          *   the actual position; this node was already checked and we can decrease the position
          */
         if( !parentfelldown )
            pos--;

         /* free memory of the node */
         CHECK_OKAY( SCIPnodeFree(&node, memhdr, set, tree, lp) );
      }
      else
         pos--;
   }
   debugMessage(" -> bounded node queue has length %d\n", nodepq->len);

   return SCIP_OKAY;
}

/** resorts the priority queue (necessary for changes in node selector) */
RETCODE SCIPnodepqResort(
   NODEPQ**         nodepq,             /**< pointer to a node priority queue */
   const SET*       set                 /**< global SCIP settings */
   )
{
   NODEPQ* newnodepq;
   NODESEL* nodesel;
   int i;

   assert(nodepq != NULL);
   assert(*nodepq != NULL);
   assert((*nodepq)->len >= 0);
   assert(set != NULL);

   nodesel = set->nodesel;
   assert(nodesel != NULL);
   assert(nodesel->nodeselcomp != NULL);

   /* create new node priority queue */
   CHECK_OKAY( SCIPnodepqCreate(&newnodepq, set) );

   /* resize the new node priority queue to be able to store all nodes */
   CHECK_OKAY( nodepqResize(newnodepq, set, (*nodepq)->len) );

   /* insert all nodes in the new node priority queue */
   for( i = 0; i < (*nodepq)->len; ++i )
   {
      CHECK_OKAY( SCIPnodepqInsert(newnodepq, set, (*nodepq)->slots[i]) );
   }

   /* destroy the old node priority queue without freeing the nodes */
   SCIPnodepqDestroy(nodepq);

   /* use the new node priority queue */
   *nodepq = newnodepq;

   return SCIP_OKAY;
}




/*
 * node selector methods 
 */

/** creates a node selector */
RETCODE SCIPnodeselCreate(
   NODESEL**        nodesel,            /**< pointer to store node selector */
   const char*      name,               /**< name of node selector */
   const char*      desc,               /**< description of node selector */
   DECL_NODESELFREE ((*nodeselfree)),   /**< destructor of node selector */
   DECL_NODESELINIT ((*nodeselinit)),   /**< initialize node selector */
   DECL_NODESELEXIT ((*nodeselexit)),   /**< deinitialize node selector */
   DECL_NODESELSELECT((*nodeselselect)),/**< node selection method */
   DECL_NODESELCOMP ((*nodeselcomp)),   /**< node comparison method */
   NODESELDATA*     nodeseldata,        /**< node selector data */
   Bool             lowestboundfirst    /**< does node comparison sorts w.r.t. lower bound as primal criterion? */
   )
{
   assert(nodesel != NULL);
   assert(name != NULL);
   assert(desc != NULL);
   assert(nodeselselect != NULL);
   assert(nodeselcomp != NULL);

   ALLOC_OKAY( allocMemory(nodesel) );
   ALLOC_OKAY( duplicateMemoryArray(&(*nodesel)->name, name, strlen(name)+1) );
   ALLOC_OKAY( duplicateMemoryArray(&(*nodesel)->desc, desc, strlen(desc)+1) );
   (*nodesel)->nodeselfree = nodeselfree;
   (*nodesel)->nodeselinit = nodeselinit;
   (*nodesel)->nodeselexit = nodeselexit;
   (*nodesel)->nodeselselect = nodeselselect;
   (*nodesel)->nodeselcomp = nodeselcomp;
   (*nodesel)->nodeseldata = nodeseldata;
   (*nodesel)->lowestboundfirst = lowestboundfirst;
   (*nodesel)->initialized = FALSE;

   return SCIP_OKAY;
}
   
/** frees memory of node selector */
RETCODE SCIPnodeselFree(
   NODESEL**        nodesel,            /**< pointer to node selector data structure */
   SCIP*            scip                /**< SCIP data structure */   
   )
{
   assert(nodesel != NULL);
   assert(*nodesel != NULL);
   assert(!(*nodesel)->initialized);

   /* call destructor of node selector */
   if( (*nodesel)->nodeselfree != NULL )
   {
      CHECK_OKAY( (*nodesel)->nodeselfree(scip, *nodesel) );
   }

   freeMemoryArray(&(*nodesel)->name);
   freeMemoryArray(&(*nodesel)->desc);
   freeMemory(nodesel);

   return SCIP_OKAY;
}

/** initializes node selector */
RETCODE SCIPnodeselInit(
   NODESEL*         nodesel,            /**< node selector */
   SCIP*            scip                /**< SCIP data structure */   
   )
{
   assert(nodesel != NULL);
   assert(scip != NULL);

   if( nodesel->initialized )
   {
      char s[MAXSTRLEN];
      sprintf(s, "Node selector <%s> already initialized", nodesel->name);
      errorMessage(s);
      return SCIP_INVALIDCALL;
   }

   if( nodesel->nodeselinit != NULL )
   {
      CHECK_OKAY( nodesel->nodeselinit(scip, nodesel) );
   }
   nodesel->initialized = TRUE;

   return SCIP_OKAY;
}

/** deinitializes node selector */
RETCODE SCIPnodeselExit(
   NODESEL*         nodesel,            /**< node selector */
   SCIP*            scip                /**< SCIP data structure */   
   )
{
   assert(nodesel != NULL);
   assert(scip != NULL);

   if( !nodesel->initialized )
   {
      char s[MAXSTRLEN];
      sprintf(s, "Node selector <%s> not initialized", nodesel->name);
      errorMessage(s);
      return SCIP_INVALIDCALL;
   }

   if( nodesel->nodeselexit != NULL )
   {
      CHECK_OKAY( nodesel->nodeselexit(scip, nodesel) );
   }
   nodesel->initialized = FALSE;

   return SCIP_OKAY;
}

/** select next node to be processed */
RETCODE SCIPnodeselSelect(
   NODESEL*         nodesel,            /**< node selector */
   const SET*       set,                /**< global SCIP settings */
   NODE**           selnode             /**< pointer to store node to be processed next */
   )
{
   assert(nodesel != NULL);
   assert(nodesel->nodeselselect != NULL);
   assert(set != NULL);
   assert(selnode != NULL);

   CHECK_OKAY( nodesel->nodeselselect(set->scip, nodesel, selnode) );

   return SCIP_OKAY;
}

/** compares two nodes; returns -1/0/+1 if node1 better/equal/worse than node2 */
int SCIPnodeselCompare(
   NODESEL*         nodesel,            /**< node selector */
   const SET*       set,                /**< global SCIP settings */
   NODE*            node1,              /**< first node to compare */
   NODE*            node2               /**< second node to compare */
   )
{
   assert(nodesel != NULL);
   assert(nodesel->nodeselcomp != NULL);
   assert(set != NULL);
   assert(node1 != NULL);
   assert(node2 != NULL);

   return nodesel->nodeselcomp(set->scip, nodesel, node1, node2);
}

/** gets name of node selector */
const char* SCIPnodeselGetName(
   NODESEL*         nodesel             /**< node selector */
   )
{
   assert(nodesel != NULL);

   return nodesel->name;
}

/** gets user data of node selector */
NODESELDATA* SCIPnodeselGetData(
   NODESEL*         nodesel             /**< node selector */
   )
{
   assert(nodesel != NULL);

   return nodesel->nodeseldata;
}

/** sets user data of node selector; user has to free old data in advance! */
void SCIPnodeselSetData(
   NODESEL*         nodesel,            /**< node selector */
   NODESELDATA*     nodeseldata         /**< new node selector user data */
   )
{
   assert(nodesel != NULL);

   nodesel->nodeseldata = nodeseldata;
}

/** is node selector initialized? */
Bool SCIPnodeselIsInitialized(
   NODESEL*         nodesel             /**< node selector */
   )
{
   assert(nodesel != NULL);

   return nodesel->initialized;
}

