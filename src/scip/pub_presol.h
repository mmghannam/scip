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
#pragma ident "@(#) $Id: pub_presol.h,v 1.8 2005/02/14 13:35:48 bzfpfend Exp $"

/**@file   pub_presol.h
 * @brief  public methods for presolvers
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __PUB_PRESOL_H__
#define __PUB_PRESOL_H__


#include "scip/def.h"
#include "scip/type_misc.h"
#include "scip/type_presol.h"



/** compares two presolvers w. r. to their priority */
extern
DECL_SORTPTRCOMP(SCIPpresolComp);

/** gets user data of presolver */
extern
PRESOLDATA* SCIPpresolGetData(
   PRESOL*          presol              /**< presolver */
   );

/** sets user data of presolver; user has to free old data in advance! */
extern
void SCIPpresolSetData(
   PRESOL*          presol,             /**< presolver */
   PRESOLDATA*      presoldata          /**< new presolver user data */
   );

/** gets name of presolver */
extern
const char* SCIPpresolGetName(
   PRESOL*          presol              /**< presolver */
   );

/** gets description of presolver */
extern
const char* SCIPpresolGetDesc(
   PRESOL*          presol              /**< presolver */
   );

/** gets priority of presolver */
extern
int SCIPpresolGetPriority(
   PRESOL*          presol              /**< presolver */
   );

/** should presolver be delayed, if other presolvers found reductions? */
extern
Bool SCIPpresolIsDelayed(
   PRESOL*          presol              /**< presolver */
   );

/** was presolver delayed at the last call? */
extern
Bool SCIPpresolWasDelayed(
   PRESOL*          presol              /**< presolver */
   );

/** is presolver initialized? */
extern
Bool SCIPpresolIsInitialized(
   PRESOL*          presol              /**< presolver */
   );

/** gets time in seconds used in this presolver */
extern
Real SCIPpresolGetTime(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of variables fixed in presolver */
extern
int SCIPpresolGetNFixedVars(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of variables aggregated in presolver */
extern
int SCIPpresolGetNAggrVars(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of variable types changed in presolver */
extern
int SCIPpresolGetNVarTypes(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of bounds changed in presolver */
extern
int SCIPpresolGetNChgBds(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of holes added to domains of variables in presolver */
extern
int SCIPpresolGetNAddHoles(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of constraints deleted in presolver */
extern
int SCIPpresolGetNDelConss(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of constraints upgraded in presolver */
extern
int SCIPpresolGetNUpgdConss(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of coefficients changed in presolver */
extern
int SCIPpresolGetNChgCoefs(
   PRESOL*          presol              /**< presolver */
   );

/** gets number of constraint sides changed in presolver */
extern
int SCIPpresolGetNChgSides(
   PRESOL*          presol              /**< presolver */
   );


#endif
