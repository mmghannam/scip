/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2017 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   Binpacking/src/cmain.c
 * @brief  Main file for binpacking pricing example
 * @author Timo Berthold
 * @author Stefan Heinz
 *
 *  This the file contains the \ref main() main function of the projects. This includes all the default plugins of
 *  \SCIP and the once which belong to that projects. After that is starts the interactive shell of \SCIP or processes
 *  the shell arguments if given.
 */
#include <stdio.h>

#include "scip/scip.h"
#include "scip/scipshell.h"
#include "scip/scipdefplugins.h"

#include "cons_rpa.h"
#include "reader_rpa.h"
#include "pricer_rpa.h"

/* parameters */
#define DEFAULT_NLP_NODELIMIT                10000L    /**< node limit for verification NLP */
#define DEFAULT_NLP_TIMELIMIT                SCIP_REAL_MAX /**< time limit for verification NLP */
#define DEFAULT_HEUR_ITERLIMIT               1000      /**< iteration limit for heuristic verification */
#define DEFAULT_HEUR_TIMELIMIT               SCIP_REAL_MAX /**< time limit for heuristic verification */
#define DEFAULT_NLP_NODELIMITSOFT            1000L     /**< soft node limit for verification NLP */
#define DEFAULT_NLP_TIMELIMITSOFT            SCIP_REAL_MAX /**< soft time limit for verification NLP */
#define DEFAULT_HEUR_ITERLIMITSOFT           100       /**< soft iteration limit for heuristic verification */
#define DEFAULT_HEUR_TIMELIMITSOFT           SCIP_REAL_MAX /**< soft time limit for heuristic verification */
#define DEFAULT_TEXOUTFILENAME               ""        /**< tex output filename for the best found solution */

/** creates a SCIP instance with default plugins, evaluates command line parameters, runs SCIP appropriately,
 *  and frees the SCIP instance
 */
static
SCIP_RETCODE runShell(
   int                   argc,               /**< number of shell parameters */
   char**                argv,               /**< array with shell parameters */
   const char*           defaultsetname      /**< name of default settings file */
   )
{
   SCIP* scip = NULL;

   /*********
    * Setup *
    *********/

   /* initialize SCIP */
   SCIP_CALL( SCIPcreate(&scip) );

   /* we explicitly enable the use of a debug solution for this main SCIP instance */
   SCIPenableDebugSol(scip);

   /* include default SCIP plugins */
   SCIP_CALL( SCIPincludeDefaultPlugins(scip) );

   /* include reader for ringpacking instances */
   SCIP_CALL( SCIPincludeReaderRpa(scip) );

   /* include ringpacking constraint handler */
   SCIP_CALL( SCIPincludeConshdlrRpa(scip) );

   /* include ringpacking pricer  */
   SCIP_CALL( SCIPincludePricerRingpacking(scip) );

   /* for column generation instances, disable restarts */
   SCIP_CALL( SCIPsetIntParam(scip,"presolving/maxrestarts",0) );

   /* turn off all separation algorithms */
   SCIP_CALL( SCIPsetSeparating(scip, SCIP_PARAMSETTING_OFF, TRUE) );

   /* add parameters */
   SCIP_CALL( SCIPaddRealParam(scip,
         "ringpacking/nlptimelimit",
         "time limit for verification NLP",
         NULL, FALSE, DEFAULT_NLP_TIMELIMIT, -1.0, SCIP_REAL_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddLongintParam(scip,
         "ringpacking/nlpnodelimit",
         "node limit for verification NLP",
         NULL, FALSE, DEFAULT_NLP_NODELIMIT, 0L, SCIP_LONGINT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip,
         "ringpacking/heurtimelimit",
         "time limit for heuristic verification",
         NULL, FALSE, DEFAULT_HEUR_TIMELIMIT, -1.0, SCIP_REAL_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
         "ringpacking/heuriterlimit",
         "iteration limit for heuristic verification",
         NULL, FALSE, DEFAULT_HEUR_ITERLIMIT, 0, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip,
         "ringpacking/nlptimelimitsoft",
         "soft time limit for verification NLP",
         NULL, FALSE, DEFAULT_NLP_TIMELIMITSOFT, -1.0, SCIP_REAL_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddLongintParam(scip,
         "ringpacking/nlpnodelimitsoft",
         "soft node limit for verification NLP",
         NULL, FALSE, DEFAULT_NLP_NODELIMITSOFT, 0L, SCIP_LONGINT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddRealParam(scip,
         "ringpacking/heurtimelimitsoft",
         "soft time limit for heuristic verification",
         NULL, FALSE, DEFAULT_HEUR_TIMELIMITSOFT, -1.0, SCIP_REAL_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddIntParam(scip,
         "ringpacking/heuriterlimitsoft",
         "soft iteration limit for heuristic verification",
         NULL, FALSE, DEFAULT_HEUR_ITERLIMITSOFT, 0, INT_MAX, NULL, NULL) );

   SCIP_CALL( SCIPaddStringParam(scip,
         "ringpacking/texoutfilename",
         "tex output filename for the best found solution (\"\": disable)",
         NULL, FALSE, DEFAULT_TEXOUTFILENAME, NULL, NULL) );

   /**********************************
    * Process command line arguments *
    **********************************/
   SCIP_CALL( SCIPprocessShellArguments(scip, argc, argv, defaultsetname) );

   /********************
    * Deinitialization *
    ********************/

   SCIP_CALL( SCIPfree(&scip) );

   BMScheckEmptyMemory();

   return SCIP_OKAY;
}

int
main(
   int                        argc,
   char**                     argv
   )
{
   SCIP_RETCODE retcode;

   retcode = runShell(argc, argv, "scip.set");
   if( retcode != SCIP_OKAY )
   {
      SCIPprintError(retcode);
      return -1;
   }

   return 0;
}
