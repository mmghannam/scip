/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2005 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2005 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic License.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: nodesel_dfs.c,v 1.23 2005/02/14 13:35:46 bzfpfend Exp $"

/**@file   nodesel_dfs.c
 * @brief  node selector for depth first search
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include <string.h>

#include "scip/nodesel_dfs.h"


#define NODESEL_NAME             "dfs"
#define NODESEL_DESC             "depth first search"
#define NODESEL_STDPRIORITY           0
#define NODESEL_MEMSAVEPRIORITY  100000
#define NODESEL_LOWESTFIRST       FALSE   /**< are the nodes sorted such that the lowest bound node comes first? */




/*
 * Callback methods
 */

/** destructor of node selector to free user data (called when SCIP is exiting) */
#define nodeselFreeDfs NULL


/** initialization method of node selector (called after problem was transformed) */
#define nodeselInitDfs NULL


/** deinitialization method of node selector (called before transformed problem is freed) */
#define nodeselExitDfs NULL


/** solving process initialization method of node selector (called when branch and bound process is about to begin) */
#define nodeselInitsolDfs NULL


/** solving process deinitialization method of node selector (called before branch and bound process data is freed) */
#define nodeselExitsolDfs NULL


/** node selection method of node selector */
static
DECL_NODESELSELECT(nodeselSelectDfs)
{  /*lint --e{715}*/
   assert(nodesel != NULL);
   assert(strcmp(SCIPnodeselGetName(nodesel), NODESEL_NAME) == 0);
   assert(scip != NULL);
   assert(selnode != NULL);

   *selnode = SCIPgetPrioChild(scip);
   if( *selnode == NULL )
   {
      *selnode = SCIPgetPrioSibling(scip);
      if( *selnode == NULL )
      {
         *selnode = SCIPgetBestLeaf(scip);
      }
   }

   return SCIP_OKAY;
}


/** node comparison method of node selector */
static
DECL_NODESELCOMP(nodeselCompDfs)
{  /*lint --e{715}*/
   int depth1;
   int depth2;

   assert(nodesel != NULL);
   assert(strcmp(SCIPnodeselGetName(nodesel), NODESEL_NAME) == 0);
   assert(scip != NULL);

   depth1 = SCIPnodeGetDepth(node1);
   depth2 = SCIPnodeGetDepth(node2);
   if( depth1 > depth2 )
      return -1;
   else if( depth1 < depth2 )
      return +1;
   else
   {
      Real lowerbound1;
      Real lowerbound2;

      lowerbound1 = SCIPnodeGetLowerbound(node1);
      lowerbound2 = SCIPnodeGetLowerbound(node2);
      if( lowerbound1 < lowerbound2 )
         return -1;
      else if( lowerbound1 > lowerbound2 )
         return +1;
      else
         return 0;
   }
}





/*
 * dfs specific interface methods
 */

/** creates the node selector for depth first search and includes it in SCIP */
RETCODE SCIPincludeNodeselDfs(
   SCIP*            scip                /**< SCIP data structure */
   )
{
   NODESELDATA* nodeseldata;

   /* create dfs node selector data */
   nodeseldata = NULL;

   /* include node selector */
   CHECK_OKAY( SCIPincludeNodesel(scip, NODESEL_NAME, NODESEL_DESC, NODESEL_STDPRIORITY, NODESEL_MEMSAVEPRIORITY,
         NODESEL_LOWESTFIRST,
         nodeselFreeDfs, nodeselInitDfs, nodeselExitDfs, 
         nodeselInitsolDfs, nodeselExitsolDfs, nodeselSelectDfs, nodeselCompDfs,
         nodeseldata) );

   return SCIP_OKAY;
}

