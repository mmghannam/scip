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
#pragma ident "@(#) $Id: heur_rootsoldiving.h,v 1.4 2005/02/14 13:35:44 bzfpfend Exp $"

/**@file   heur_rootsoldiving.h
 * @brief  LP diving heuristic that changes variable's objective values using root LP solution as guide
 * @author Kati Wolter
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __HEUR_ROOTSOLDIVING_H__
#define __HEUR_ROOTSOLDIVING_H__


#include "scip/scip.h"


/** creates the rootsoldiving heuristic and includes it in SCIP */
extern
RETCODE SCIPincludeHeurRootsoldiving(
   SCIP*            scip                /**< SCIP data structure */
   );

#endif
