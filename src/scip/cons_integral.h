/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2002 Tobias Achterberg                              */
/*                            Thorsten Koch                                  */
/*                            Alexander Martin                               */
/*                  2002-2002 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic Licence.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   cons_integral.h
 * @brief  constraint handler for the integrality constraint
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __CONS_INTEGRAL_H__
#define __CONS_INTEGRAL_H__


#include "scip.h"


DECL_CONSINIT(SCIPconsInitIntegral);
DECL_CONSEXIT(SCIPconsExitIntegral);
DECL_CONSFREE(SCIPconsFreeIntegral);
DECL_CONSTRAN(SCIPconsTranIntegral);
DECL_CONSSEPA(SCIPconsSepaIntegral);
DECL_CONSENFO(SCIPconsEnfoIntegral);
DECL_CONSCHCK(SCIPconsChckIntegral);
DECL_CONSPROP(SCIPconsPropIntegral);


extern
RETCODE SCIPincludeConsHdlrIntegral(    /**< creates the handler for the integrality constraint and includes it in SCIP */
   SCIP*            scip                /**< SCIP data structure */
   );

#endif
