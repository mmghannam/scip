/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2012 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   sepa_gomory.c
 * @brief  Gomory MIR Cuts
 * @author Tobias Achterberg
 * @author Stefan Heinz
 */

/**@todo try k-Gomory-cuts (s. Cornuejols: K-Cuts: A Variation of Gomory Mixed Integer Cuts from the LP Tableau)
 *
 * @todo Try cuts on the objective tableau row.
 *
 * @todo Also try negative basis inverse row?
 *
 * @todo It happends that the SICPcalcMir() function returns with the same cut for different calls. Check if this is a
 *       bug or do not use it for the MIP below and turn off presolving and all heuristics:
 *
 *  Max y
 *  Subject to
 *  c1: -x + y <= 1
 *  c2: 2x + 3y <= 12
 *  c3: 3x + 2y <= 12
 *  Bounds
 *  0 <= x
 *  0 <= y
 *  General
 *  x
 *  y
 *  END
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include <string.h>

#include "scip/sepa_gomory.h"
#include "scip/pub_misc.h"


#define SEPA_NAME              "gomory"
#define SEPA_DESC              "Gomory MIR cuts separator"
#define SEPA_PRIORITY             -1000
#define SEPA_FREQ                     0
#define SEPA_MAXBOUNDDIST           0.0
#define SEPA_USESSUBSCIP          FALSE /**< does the separator use a secondary SCIP instance? */
#define SEPA_DELAY                FALSE /**< should separation method be delayed, if other separators found cuts? */

#define DEFAULT_MAXROUNDS             5 /**< maximal number of gomory separation rounds per node (-1: unlimited) */
#define DEFAULT_MAXROUNDSROOT        10 /**< maximal number of gomory separation rounds in the root node (-1: unlimited) */
#define DEFAULT_MAXSEPACUTS          50 /**< maximal number of gomory cuts separated per separation round */
#define DEFAULT_MAXSEPACUTSROOT      50 /**< maximal number of gomory cuts separated per separation round in root node */
#define DEFAULT_DYNAMICCUTS        TRUE /**< should generated cuts be removed from the LP if they are no longer tight? */
#define DEFAULT_MAXWEIGHTRANGE    1e+04 /**< maximal valid range max(|weights|)/min(|weights|) of row weights */
#define DEFAULT_MAKEINTEGRAL       TRUE /**< try to scale all cuts to integral coefficients */
#define DEFAULT_FORCECUTS         FALSE /**< if conversion to integral coefficients failed still use the cut */
#define DEFAULT_SEPARATEROWS       TRUE /**< separate rows with integral slack */

#define BOUNDSWITCH              0.9999 /**< threshold for bound switching - see SCIPcalcMIR() */
#define USEVBDS                    TRUE /**< use variable bounds - see SCIPcalcMIR() */
#define ALLOWLOCAL                 TRUE /**< allow to generate local cuts - see SCIPcalcMIR() */
#define FIXINTEGRALRHS            FALSE /**< try to generate an integral rhs - see SCIPcalcMIR() */
#define MAKECONTINTEGRAL          FALSE /**< convert continuous variable to integral variables in SCIPmakeRowIntegral() */
#define MINFRAC                    0.05 /**< minimal fractionality of a basis variable in order to try Gomory cut */
#define MAXFRAC                    0.95 /**< maximal fractionality of a basis variable in order to try Gomory cut */

#define MAXAGGRLEN(nvars)          (0.1*(nvars)+1000) /**< maximal length of base inequality */


/** separator data */
struct SCIP_SepaData
{
   SCIP_Real             maxweightrange;     /**< maximal valid range max(|weights|)/min(|weights|) of row weights */
   int                   maxrounds;          /**< maximal number of gomory separation rounds per node (-1: unlimited) */
   int                   maxroundsroot;      /**< maximal number of gomory separation rounds in the root node (-1: unlimited) */
   int                   maxsepacuts;        /**< maximal number of gomory cuts separated per separation round */
   int                   maxsepacutsroot;    /**< maximal number of gomory cuts separated per separation round in root node */
   int                   lastncutsfound;     /**< total number of cuts found after last call of separator */
   SCIP_Bool             dynamiccuts;        /**< should generated cuts be removed from the LP if they are no longer tight? */
   SCIP_Bool             makeintegral;       /**< try to scale all cuts to integral coefficients */
   SCIP_Bool             forcecuts;          /**< if conversion to integral coefficients failed still use the cut */
   SCIP_Bool             separaterows;       /**< separate rows with integral slack */
};


/*
 * Callback methods
 */

/** copy method for separator plugins (called when SCIP copies plugins) */
static
SCIP_DECL_SEPACOPY(sepaCopyGomory)
{  /*lint --e{715}*/
   assert(scip != NULL);
   assert(sepa != NULL);
   assert(strcmp(SCIPsepaGetName(sepa), SEPA_NAME) == 0);

   /* call inclusion method of constraint handler */
   SCIP_CALL( SCIPincludeSepaGomory(scip) );

   return SCIP_OKAY;
}

/** destructor of separator to free user data (called when SCIP is exiting) */
static
SCIP_DECL_SEPAFREE(sepaFreeGomory)
{  /*lint --e{715}*/
   SCIP_SEPADATA* sepadata;

   assert(strcmp(SCIPsepaGetName(sepa), SEPA_NAME) == 0);

   /* free separator data */
   sepadata = SCIPsepaGetData(sepa);
   assert(sepadata != NULL);

   SCIPfreeMemory(scip, &sepadata);

   SCIPsepaSetData(sepa, NULL);

   return SCIP_OKAY;
}










/** LP solution separation method of separator */
static
SCIP_DECL_SEPAEXECLP(sepaExeclpGomory)
{  /*lint --e{715}*/
   SCIP_SEPADATA* sepadata;
   SCIP_VAR** vars;
   SCIP_COL** cols;
   SCIP_ROW** rows;
   SCIP_Real* binvrow;
   SCIP_Real* cutcoefs;
   SCIP_Real maxscale;
   SCIP_Longint maxdnom;
   int* basisind;
   int nvars;
   int ncols;
   int nrows;
   int ncalls;
   int depth;
   int maxdepth;
   int maxsepacuts;
   int ncuts;
   int c;
   int i;

   assert(sepa != NULL);
   assert(strcmp(SCIPsepaGetName(sepa), SEPA_NAME) == 0);
   assert(scip != NULL);
   assert(result != NULL);

   *result = SCIP_DIDNOTRUN;

   sepadata = SCIPsepaGetData(sepa);
   assert(sepadata != NULL);

   depth = SCIPgetDepth(scip);
   ncalls = SCIPsepaGetNCallsAtNode(sepa);

   /* only call separator, if we are not close to terminating */
   if( SCIPisStopped(scip) )
      return SCIP_OKAY;

   /* only call the gomory cut separator a given number of times at each node */
   if( (depth == 0 && sepadata->maxroundsroot >= 0 && ncalls >= sepadata->maxroundsroot)
      || (depth > 0 && sepadata->maxrounds >= 0 && ncalls >= sepadata->maxrounds) )
      return SCIP_OKAY;

   /* only call separator, if an optimal LP solution is at hand */
   if( SCIPgetLPSolstat(scip) != SCIP_LPSOLSTAT_OPTIMAL )
      return SCIP_OKAY;

   /* only call separator, if the LP solution is basic */
   if( !SCIPisLPSolBasic(scip) )
      return SCIP_OKAY;

   /* only call separator, if there are fractional variables */
   if( SCIPgetNLPBranchCands(scip) == 0 )
      return SCIP_OKAY;

   /* get variables data */
   SCIP_CALL( SCIPgetVarsData(scip, &vars, &nvars, NULL, NULL, NULL, NULL) );

   /* get LP data */
   SCIP_CALL( SCIPgetLPColsData(scip, &cols, &ncols) );
   SCIP_CALL( SCIPgetLPRowsData(scip, &rows, &nrows) );
   if( ncols == 0 || nrows == 0 )
      return SCIP_OKAY;

#if 0 /* if too many columns, separator is usually very slow: delay it until no other cuts have been found */
   if( ncols >= 50*nrows )
      return SCIP_OKAY;

   if( ncols >= 5*nrows )
   {
      int ncutsfound;

      ncutsfound = SCIPgetNCutsFound(scip);
      if( ncutsfound > sepadata->lastncutsfound || !SCIPsepaWasLPDelayed(sepa) )
      {
         sepadata->lastncutsfound = ncutsfound;
         *result = SCIP_DELAYED;
         return SCIP_OKAY;
      }
   }
#endif

   /* set the maximal denominator in rational representation of gomory cut and the maximal scale factor to
    * scale resulting cut to integral values to avoid numerical instabilities
    */
   /**@todo find better but still stable gomory cut settings: look at dcmulti, gesa3, khb0525, misc06, p2756 */
   maxdepth = SCIPgetMaxDepth(scip);
   if( depth == 0 )
   {
      maxdnom = 1000;
      maxscale = 1000.0;
   }
   else if( depth <= maxdepth/4 )
   {
      maxdnom = 1000;
      maxscale = 1000.0;
   }
   else if( depth <= maxdepth/2 )
   {
      maxdnom = 100;
      maxscale = 100.0;
   }
   else
   {
      maxdnom = 10;
      maxscale = 10.0;
   }

   *result = SCIP_DIDNOTFIND;

   /* allocate temporary memory */
   SCIP_CALL( SCIPallocBufferArray(scip, &cutcoefs, nvars) );
   SCIP_CALL( SCIPallocBufferArray(scip, &basisind, nrows) );
   SCIP_CALL( SCIPallocBufferArray(scip, &binvrow, nrows) );

   /* get basis indices */
   SCIP_CALL( SCIPgetLPBasisInd(scip, basisind) );

   /* get the maximal number of cuts allowed in a separation round */
   if( depth == 0 )
      maxsepacuts = sepadata->maxsepacutsroot;
   else
      maxsepacuts = sepadata->maxsepacuts;

   SCIPdebugMessage("searching gomory cuts: %d cols, %d rows, maxdnom=%"SCIP_LONGINT_FORMAT", maxscale=%g, maxcuts=%d\n",
      ncols, nrows, maxdnom, maxscale, maxsepacuts);

   /* for all basic columns belonging to integer variables, try to generate a gomory cut */
   ncuts = 0;
   for( i = 0; i < nrows && ncuts < maxsepacuts && !SCIPisStopped(scip); ++i )
   {
      SCIP_Bool tryrow;

      tryrow = FALSE;
      c = basisind[i];
      if( c >= 0 )
      {
         SCIP_VAR* var;

         assert(c < ncols);
         var = SCIPcolGetVar(cols[c]);
         if( SCIPvarGetType(var) != SCIP_VARTYPE_CONTINUOUS )
         {
            SCIP_Real primsol;

            primsol = SCIPcolGetPrimsol(cols[c]);
            assert(SCIPgetVarSol(scip, var) == primsol); /*lint !e777*/

            if( SCIPfeasFrac(scip, primsol) >= MINFRAC )
            {
               SCIPdebugMessage("trying gomory cut for col <%s> [%g]\n", SCIPvarGetName(var), primsol);
               tryrow = TRUE;
            }
         }
      }
      else if( sepadata->separaterows )
      {
         SCIP_ROW* row;

         assert(0 <= -c-1 && -c-1 < nrows);
         row = rows[-c-1];
         if( SCIProwIsIntegral(row) && !SCIProwIsModifiable(row) )
         {
            SCIP_Real primsol;

            primsol = SCIPgetRowActivity(scip, row);
            if( SCIPfeasFrac(scip, primsol) >= MINFRAC )
            {
               SCIPdebugMessage("trying gomory cut for row <%s> [%g]\n", SCIProwGetName(row), primsol);
               tryrow = TRUE;
            }
         }
      }

      if( tryrow )
      {
         SCIP_Real cutrhs;
         SCIP_Real cutact;
         SCIP_Bool success;
         SCIP_Bool cutislocal;

         /* get the row of B^-1 for this basic integer variable with fractional solution value */
         SCIP_CALL( SCIPgetLPBInvRow(scip, i, binvrow) );

         cutact = 0.0;
         cutrhs = SCIPinfinity(scip);

         /* create a MIR cut out of the weighted LP rows using the B^-1 row as weights */
         SCIP_CALL( SCIPcalcMIR(scip, NULL, BOUNDSWITCH, USEVBDS, ALLOWLOCAL, FIXINTEGRALRHS, NULL, NULL,
               (int) MAXAGGRLEN(nvars), sepadata->maxweightrange, MINFRAC, MAXFRAC,
               binvrow, 1.0, NULL, NULL, cutcoefs, &cutrhs, &cutact, &success, &cutislocal) );
         assert(ALLOWLOCAL || !cutislocal);

         /* @todo Currently we are using the SCIPcalcMIR() function to compute the coefficients of the Gomory
          *       cut. Alternatively, we could use the direct version (see thesis of Achterberg formula (8.4)) which
          *       leads to cut a of the form \sum a_i x_i \geq 1. Rumor has it that these cuts are better.
          */

         SCIPdebugMessage(" -> success=%u: %g <= %g\n", success, cutact, cutrhs);

         /* if successful, convert dense cut into sparse row, and add the row as a cut */
         if( success && SCIPisFeasGT(scip, cutact, cutrhs) )
         {
            SCIP_ROW* cut;
            char cutname[SCIP_MAXSTRLEN];
            int v;

            /* construct cut name */
            if( c >= 0 )
               (void) SCIPsnprintf(cutname, SCIP_MAXSTRLEN, "gom%d_x%d", SCIPgetNLPs(scip), c);
            else
               (void) SCIPsnprintf(cutname, SCIP_MAXSTRLEN, "gom%d_s%d", SCIPgetNLPs(scip), -c-1);

            /* create empty cut */
            SCIP_CALL( SCIPcreateEmptyRow(scip, &cut, cutname, -SCIPinfinity(scip), cutrhs,
                  cutislocal, FALSE, sepadata->dynamiccuts) );

            /* cache the row extension and only flush them if the cut gets added */
            SCIP_CALL( SCIPcacheRowExtensions(scip, cut) );

            /* collect all non-zero coefficients */
            for( v = 0; v < nvars; ++v )
            {
               if( !SCIPisZero(scip, cutcoefs[v]) )
               {
                  SCIP_CALL( SCIPaddVarToRow(scip, cut, vars[v], cutcoefs[v]) );
               }
            }
            assert(SCIProwGetNNonz(cut) > 0);

            /* Only take efficacious cuts, except for cuts with one non-zero coefficients (= bound
               changes); the latter cuts will be handeled internally in sepastore. */
            if( SCIProwGetNNonz(cut) == 1 || SCIPisCutEfficacious(scip, NULL, cut) )
            {
               SCIPdebugMessage(" -> gomory cut for <%s>: act=%f, rhs=%f, eff=%f\n",
                  c >= 0 ? SCIPvarGetName(SCIPcolGetVar(cols[c])) : SCIProwGetName(rows[-c-1]),
                  cutact, cutrhs, SCIPgetCutEfficacy(scip, NULL, cut));

               if( SCIProwGetNNonz(cut) > 1 || sepadata->makeintegral )
               {
                  /* try to scale the cut to integral values */
                  SCIP_CALL( SCIPmakeRowIntegral(scip, cut, -SCIPepsilon(scip), SCIPsumepsilon(scip),
                        maxdnom, maxscale, MAKECONTINTEGRAL, &success) );

                  /* only take cuts which were successfully transformed to integral coefficients except the force flag
                   * is set to TRUE
                   */
                  if( (SCIPisInfinity(scip, -SCIProwGetLhs(cut)) && SCIPisInfinity(scip, SCIProwGetRhs(cut))) ||
                     (!sepadata->forcecuts && !success) )
                  {
                     SCIPdebugMessage(" -> gomory cut <%s> couldn't be scaled to integral coefficients: act=%f, rhs=%f, eff=%f\n",
                        cutname, cutact, cutrhs, SCIPgetCutEfficacy(scip, NULL, cut));

		     /* release the row */
		     SCIP_CALL( SCIPreleaseRow(scip, &cut) );

                     continue;
                  }

                  /* @todo Trying to make the Gomory cut integral might fail. Due to numerical reasons/arguments we
                   *       currently ignore such cuts. If the cut, however, has small support (let's say smaller or equal to
                   *       5), we might want to add that cut (even it does not have integral coefficients). To be able to
                   *       do that we need to add a rank to the data structure of a row. The rank of original rows are
                   *       zero and for aggregated rows it is the maximum over all used rows plus one.
                   */
               }

               SCIPdebugMessage(" -> found gomory cut <%s>: act=%f, rhs=%f, norm=%f, eff=%f, min=%f, max=%f (range=%f)\n",
                  cutname, SCIPgetRowLPActivity(scip, cut), SCIProwGetRhs(cut), SCIProwGetNorm(cut),
                  SCIPgetCutEfficacy(scip, NULL, cut),
                  SCIPgetRowMinCoef(scip, cut), SCIPgetRowMaxCoef(scip, cut),
                  SCIPgetRowMaxCoef(scip, cut)/SCIPgetRowMinCoef(scip, cut));

               /* flush all changes before adding the cut */
               SCIP_CALL( SCIPflushRowExtensions(scip, cut) );

               SCIP_CALL( SCIPaddCut(scip, NULL, cut, FALSE) );

               /* add global cuts which are not implicit bound changes to the cut pool */
               if( !cutislocal && SCIProwGetNNonz(cut) > 1 )
               {
                  SCIP_CALL( SCIPaddPoolCut(scip, cut) );
               }

               *result = SCIP_SEPARATED;
               ncuts++;
            }

            /* release the row */
            SCIP_CALL( SCIPreleaseRow(scip, &cut) );
         }
      }
   }

   /* free temporary memory */
   SCIPfreeBufferArray(scip, &binvrow);
   SCIPfreeBufferArray(scip, &basisind);
   SCIPfreeBufferArray(scip, &cutcoefs);

   SCIPdebugMessage("end searching gomory cuts: found %d cuts\n", ncuts);

   sepadata->lastncutsfound = SCIPgetNCutsFound(scip);

   return SCIP_OKAY;
}






/*
 * separator specific interface methods
 */

/** creates the Gomory MIR cut separator and includes it in SCIP */
SCIP_RETCODE SCIPincludeSepaGomory(
   SCIP*                 scip                /**< SCIP data structure */
   )
{
   SCIP_SEPADATA* sepadata;
   SCIP_SEPA* sepa;

   /* create separator data */
   SCIP_CALL( SCIPallocMemory(scip, &sepadata) );
   sepadata->lastncutsfound = 0;

   /* include separator */
   SCIP_CALL( SCIPincludeSepaBasic(scip, &sepa, SEPA_NAME, SEPA_DESC, SEPA_PRIORITY, SEPA_FREQ, SEPA_MAXBOUNDDIST,
         SEPA_USESSUBSCIP, SEPA_DELAY,
         sepaExeclpGomory, NULL,
         sepadata) );

   assert(sepa != NULL);

   /* set non-NULL pointers to callback methods */
   SCIP_CALL( SCIPsetSepaCopy(scip, sepa, sepaCopyGomory) );
   SCIP_CALL( SCIPsetSepaFree(scip, sepa, sepaFreeGomory) );

   /* add separator parameters */
   SCIP_CALL( SCIPaddIntParam(scip,
         "separating/gomory/maxrounds",
         "maximal number of gomory separation rounds per node (-1: unlimited)",
         &sepadata->maxrounds, FALSE, DEFAULT_MAXROUNDS, -1, INT_MAX, NULL, NULL) );
   SCIP_CALL( SCIPaddIntParam(scip,
         "separating/gomory/maxroundsroot",
         "maximal number of gomory separation rounds in the root node (-1: unlimited)",
         &sepadata->maxroundsroot, FALSE, DEFAULT_MAXROUNDSROOT, -1, INT_MAX, NULL, NULL) );
   SCIP_CALL( SCIPaddIntParam(scip,
         "separating/gomory/maxsepacuts",
         "maximal number of gomory cuts separated per separation round",
         &sepadata->maxsepacuts, FALSE, DEFAULT_MAXSEPACUTS, 0, INT_MAX, NULL, NULL) );
   SCIP_CALL( SCIPaddIntParam(scip,
         "separating/gomory/maxsepacutsroot",
         "maximal number of gomory cuts separated per separation round in the root node",
         &sepadata->maxsepacutsroot, FALSE, DEFAULT_MAXSEPACUTSROOT, 0, INT_MAX, NULL, NULL) );
   SCIP_CALL( SCIPaddRealParam(scip,
         "separating/gomory/maxweightrange",
         "maximal valid range max(|weights|)/min(|weights|) of row weights",
         &sepadata->maxweightrange, TRUE, DEFAULT_MAXWEIGHTRANGE, 1.0, SCIP_REAL_MAX, NULL, NULL) );
   SCIP_CALL( SCIPaddBoolParam(scip,
         "separating/gomory/dynamiccuts",
         "should generated cuts be removed from the LP if they are no longer tight?",
         &sepadata->dynamiccuts, FALSE, DEFAULT_DYNAMICCUTS, NULL, NULL) );
   SCIP_CALL( SCIPaddBoolParam(scip,
         "separating/gomory/makeintegral",
         "try to scale cuts to integral coefficients",
         &sepadata->makeintegral, TRUE, DEFAULT_MAKEINTEGRAL, NULL, NULL) );
   SCIP_CALL( SCIPaddBoolParam(scip,
         "separating/gomory/forcecuts",
         "if conversion to integral coefficients failed still use the cut",
         &sepadata->forcecuts, TRUE, DEFAULT_FORCECUTS, NULL, NULL) );
   SCIP_CALL( SCIPaddBoolParam(scip,
         "separating/gomory/separaterows",
         "separate rows with integral slack",
         &sepadata->separaterows, TRUE, DEFAULT_SEPARATEROWS, NULL, NULL) );

   return SCIP_OKAY;
}

