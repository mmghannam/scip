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
#pragma ident "@(#) $Id: prop.h,v 1.9 2005/02/14 13:35:47 bzfpfend Exp $"

/**@file   prop.h
 * @brief  internal methods for propagators
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __PROP_H__
#define __PROP_H__


#include "scip/def.h"
#include "scip/memory.h"
#include "scip/type_retcode.h"
#include "scip/type_result.h"
#include "scip/type_set.h"
#include "scip/type_stat.h"
#include "scip/type_lp.h"
#include "scip/type_var.h"
#include "scip/type_prop.h"
#include "scip/pub_prop.h"



/** creates a propagator */
extern
RETCODE SCIPpropCreate(
   PROP**           prop,               /**< pointer to propagator data structure */
   SET*             set,                /**< global SCIP settings */
   BLKMEM*          blkmem,             /**< block memory for parameter settings */
   const char*      name,               /**< name of propagator */
   const char*      desc,               /**< description of propagator */
   int              priority,           /**< priority of propagator (>= 0: before, < 0: after constraint handlers) */
   int              freq,               /**< frequency for calling propagator */
   Bool             delay,              /**< should propagator be delayed, if other propagators found reductions? */
   DECL_PROPFREE    ((*propfree)),      /**< destructor of propagator */
   DECL_PROPINIT    ((*propinit)),      /**< initialize propagator */
   DECL_PROPEXIT    ((*propexit)),      /**< deinitialize propagator */
   DECL_PROPINITSOL ((*propinitsol)),   /**< solving process initialization method of propagator */
   DECL_PROPEXITSOL ((*propexitsol)),   /**< solving process deinitialization method of propagator */
   DECL_PROPEXEC    ((*propexec)),      /**< execution method of propagator */
   DECL_PROPRESPROP ((*propresprop)),   /**< propagation conflict resolving method */
   PROPDATA*        propdata            /**< propagator data */
   );

/** calls destructor and frees memory of propagator */
extern
RETCODE SCIPpropFree(
   PROP**           prop,               /**< pointer to propagator data structure */
   SET*             set                 /**< global SCIP settings */
   );

/** initializes propagator */
extern
RETCODE SCIPpropInit(
   PROP*            prop,               /**< propagator */
   SET*             set                 /**< global SCIP settings */
   );

/** calls exit method of propagator */
extern
RETCODE SCIPpropExit(
   PROP*            prop,               /**< propagator */
   SET*             set                 /**< global SCIP settings */
   );

/** informs propagator that the branch and bound process is being started */
extern
RETCODE SCIPpropInitsol(
   PROP*            prop,               /**< propagator */
   SET*             set                 /**< global SCIP settings */
   );

/** informs propagator that the branch and bound process data is being freed */
extern
RETCODE SCIPpropExitsol(
   PROP*            prop,               /**< propagator */
   SET*             set                 /**< global SCIP settings */
   );

/** calls execution method of propagator */
extern
RETCODE SCIPpropExec(
   PROP*            prop,               /**< propagator */
   SET*             set,                /**< global SCIP settings */
   STAT*            stat,               /**< dynamic problem statistics */
   int              depth,              /**< depth of current node */
   Bool             execdelayed,        /**< execute propagator even if it is marked to be delayed */
   RESULT*          result              /**< pointer to store the result of the callback method */
   );

/** resolves the given conflicting bound, that was deduced by the given propagator, by putting all "reason" bounds
 *  leading to the deduction into the conflict queue with calls to SCIPaddConflictLb() and SCIPaddConflictUb()
 */
extern
RETCODE SCIPpropResolvePropagation(
   PROP*            prop,               /**< propagator */
   SET*             set,                /**< global SCIP settings */
   VAR*             infervar,           /**< variable whose bound was deduced by the constraint */
   int              inferinfo,          /**< user inference information attached to the bound change */
   BOUNDTYPE        inferboundtype,     /**< bound that was deduced (lower or upper bound) */
   BDCHGIDX*        bdchgidx,           /**< bound change index, representing the point of time where change took place */
   RESULT*          result              /**< pointer to store the result of the callback method */
   );

/** sets priority of propagator */
extern
void SCIPpropSetPriority(
   PROP*            prop,               /**< propagator */
   SET*             set,                /**< global SCIP settings */
   int              priority            /**< new priority of the propagator */
   );

#endif
