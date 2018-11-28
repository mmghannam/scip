/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2018 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   benderscut_feas.c
 * @brief  Standard feasibility cuts for Benders' decomposition
 * @author Stephen J. Maher
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include "nlpi/exprinterpret.h"
#include "nlpi/pub_expr.h"
#include "scip/benderscut_feas.h"
#include "scip/benderscut_opt.h"
#include "scip/cons_linear.h"
#include "scip/pub_benderscut.h"
#include "scip/pub_benders.h"
#include "scip/pub_lp.h"
#include "scip/pub_message.h"
#include "scip/pub_misc.h"
#include "scip/pub_misc_linear.h"
#include "scip/pub_nlp.h"
#include "scip/pub_var.h"
#include "scip/scip_benders.h"
#include "scip/scip_cons.h"
#include "scip/scip_general.h"
#include "scip/scip_lp.h"
#include "scip/scip_mem.h"
#include "scip/scip_message.h"
#include "scip/scip_nlp.h"
#include "scip/scip_numerics.h"
#include "scip/scip_prob.h"
#include "scip/scip_solvingstats.h"
#include "scip/scip_var.h"

#define BENDERSCUT_NAME             "feas"
#define BENDERSCUT_DESC             "Standard feasibility cuts for Benders' decomposition"
#define BENDERSCUT_PRIORITY     10000
#define BENDERSCUT_LPCUT         TRUE

/*
 * Local methods
 */

/** computing as standard Benders' feasibility cut from the dual solutions of the LP
 *
 *  NOTE: The cut must be created before being passed to this function
 */
static
SCIP_RETCODE computeStandardFeasibilityCut(
   SCIP*                 masterprob,         /**< the SCIP instance of the master problem */
   SCIP*                 subproblem,         /**< the SCIP instance of the pricing problem */
   SCIP_BENDERS*         benders,            /**< the benders' decomposition structure */
   SCIP_SOL*             sol,                /**< primal CIP solution */
   SCIP_CONS*            cut,                /**< the cut that is generated from the pricing problem */
   SCIP_Bool*            success             /**< was the cut generation successful? */
   )
{
   SCIP_VAR** vars;
   int nvars;
   int nrows;
   SCIP_Real dualsol;
   SCIP_Real lhs;       /* the left hand side of the cut */
   SCIP_Real addval;    /* the value that must be added to the lhs */
   SCIP_Real activity;
   int i;

   assert(masterprob != NULL);
   assert(subproblem != NULL);
   assert(benders != NULL);
   assert(cut != NULL);
   assert(SCIPgetStatus(subproblem) == SCIP_STATUS_INFEASIBLE
      || SCIPgetLPSolstat(subproblem) == SCIP_LPSOLSTAT_INFEASIBLE);

   (*success) = FALSE;

   /* looping over all LP rows and setting the coefficients of the cut */
   nrows = SCIPgetNLPRows(subproblem);
   for( i = 0; i < nrows; i++ )
   {
      SCIP_ROW* lprow;
      addval = 0;

      lprow = SCIPgetLPRows(subproblem)[i];
      assert(lprow != NULL);

      dualsol = SCIProwGetDualfarkas(lprow);
      assert( !SCIPisInfinity(subproblem, dualsol) && !SCIPisInfinity(subproblem, -dualsol) );

      if( SCIPisDualfeasZero(subproblem, dualsol) )
         continue;

      lhs = SCIPgetLhsLinear(masterprob, cut);

      if( dualsol > 0.0 )
         addval = dualsol*SCIProwGetLhs(lprow);
      else
         addval = dualsol*SCIProwGetRhs(lprow);

      lhs += addval;

      /* if the bound becomes infinite, then the cut generation terminates. */
      if( SCIPisInfinity(masterprob, lhs) || SCIPisInfinity(masterprob, -lhs)
         || SCIPisInfinity(masterprob, addval) || SCIPisInfinity(masterprob, -addval))
      {
         (*success) = FALSE;
         SCIPdebugMsg(masterprob, "Infinite bound when generating feasibility cut.\n");
         return SCIP_OKAY;
      }

      /* Update the lhs of the cut */
      SCIP_CALL( SCIPchgLhsLinear(masterprob, cut, lhs) );
   }

   nvars = SCIPgetNVars(subproblem);
   vars = SCIPgetVars(subproblem);

   /* looping over all variables to update the coefficients in the computed cut. */
   for( i = 0; i < nvars; i++ )
   {
      SCIP_VAR* var;
      SCIP_VAR* mastervar;

      var = vars[i];

      /* retreiving the master problem variable for the given subproblem variable. */
      SCIP_CALL( SCIPgetBendersMasterVar(masterprob, benders, var, &mastervar) );

      dualsol = SCIPgetVarFarkasCoef(subproblem, var);

      if( SCIPisZero(subproblem, dualsol) )
         continue;

      /* checking whether the original variable is a linking variable.
       * If this is the case, then the corresponding master variable is added to the generated cut.
       * If the pricing variable is not a linking variable, then the farkas dual value is added to the lhs
       */
      if( mastervar != NULL )
      {
         SCIPdebugMsg(masterprob ,"Adding coeffs to feasibility cut: <%s> dualsol %g\n", SCIPvarGetName(mastervar), dualsol);

         SCIP_CALL( SCIPaddCoefLinear(masterprob, cut, mastervar, dualsol) );
      }
      else
      {
         addval = 0;

         /* get current lhs of the subproblem cut */
         lhs = SCIPgetLhsLinear(masterprob, cut);

         if( SCIPisPositive(subproblem, dualsol) )
            addval = dualsol*SCIPvarGetUbGlobal(var);
         else if( SCIPisNegative(subproblem, dualsol) )
            addval = dualsol*SCIPvarGetLbGlobal(var);

         lhs -= addval;

         /* if the bound becomes infinite, then the cut generation terminates. */
         if( SCIPisInfinity(masterprob, lhs) || SCIPisInfinity(masterprob, -lhs)
            || SCIPisInfinity(masterprob, addval) || SCIPisInfinity(masterprob, -addval))
         {
            (*success) = FALSE;
            SCIPdebugMsg(masterprob, "Infinite bound when generating feasibility cut.\n");
            return SCIP_OKAY;
         }

         /* Update lhs */
         SCIP_CALL( SCIPchgLhsLinear(masterprob, cut, lhs) );
      }
   }

   assert(SCIPisInfinity(masterprob, SCIPgetRhsLinear(masterprob, cut)));

   /* the activity of the cut should be less than the lhs. This will ensure that the evaluated solution will be cut off.
    * It is possible that the activity is greater than the lhs. This could be caused by numerical difficulties. In this
    * case, no cut will be generated.
    */
   lhs = SCIPgetLhsLinear(masterprob, cut);
   activity = SCIPgetActivityLinear(masterprob, cut, sol);
   if( SCIPisGE(masterprob, activity, lhs) )
   {
      (*success) = FALSE;
      SCIPdebugMsg(masterprob ,"Invalid feasibility cut - activity is greater than lhs %g >= %g.\n", activity, lhs);
      return SCIP_OKAY;
   }

   assert(cut != NULL);

   SCIPdebugPrintCons(masterprob, cut, NULL);

   (*success) = TRUE;

   return SCIP_OKAY;
}


/** computing as standard Benders' feasibility cut from the dual solutions of the NLP
 *
 *  NOTE: The cut must be created before being passed to this function
 */
static
SCIP_RETCODE computeStandardFeasibilityCutNL(
   SCIP*                 masterprob,         /**< the SCIP instance of the master problem */
   SCIP*                 subproblem,         /**< the SCIP instance of the pricing problem */
   SCIP_BENDERS*         benders,            /**< the benders' decomposition structure */
   SCIP_SOL*             sol,                /**< primal CIP solution */
   SCIP_CONS*            cut,                /**< the cut that is generated from the pricing problem */
   SCIP_Bool*            success             /**< was the cut generation successful? */
   )
{
   SCIP_EXPRINT* exprinterpreter;
   SCIP_VAR** vars;
   int nrows;
   int nvars;
   SCIP_Real dirderiv;
   SCIP_Real dualsol;
   SCIP_Real lhs;       /* the left hand side of the cut */
   SCIP_Real activity;
   int i;

   assert(masterprob != NULL);
   assert(subproblem != NULL);
   assert(benders != NULL);
   assert(cut != NULL);
   assert(SCIPisNLPConstructed(subproblem));
   assert(SCIPgetNLPSolstat(subproblem) == SCIP_NLPSOLSTAT_LOCINFEASIBLE || SCIPgetNLPSolstat(subproblem) == SCIP_NLPSOLSTAT_GLOBINFEASIBLE);

   (*success) = FALSE;

   nvars = SCIPgetNNLPVars(subproblem);
   vars = SCIPgetNLPVars(subproblem);

   lhs = 0.0;
   dirderiv = 0.0;

   SCIP_CALL( SCIPexprintCreate(SCIPblkmem(subproblem), &exprinterpreter) );

   /* looping over all NLP rows and setting the corresponding coefficients of the cut */
   nrows = SCIPgetNNLPNlRows(subproblem);
   for( i = 0; i < nrows; i++ )
   {
      SCIP_NLROW* nlrow;

      nlrow = SCIPgetNLPNlRows(subproblem)[i];
      assert(nlrow != NULL);

      dualsol = SCIPnlrowGetDualsol(nlrow);
      assert( !SCIPisInfinity(subproblem, dualsol) && !SCIPisInfinity(subproblem, -dualsol) );

      if( SCIPisZero(subproblem, dualsol) )
         continue;

      SCIP_CALL( SCIPaddNlRowGradientBenderscutOpt(masterprob, subproblem, benders, NULL, cut, nlrow, exprinterpreter, -dualsol, &dirderiv) );

      SCIP_CALL( SCIPgetNlRowActivity(subproblem, nlrow, &activity) );

      if( dualsol > 0.0 )
      {
         assert(!SCIPisInfinity(subproblem, SCIPnlrowGetRhs(nlrow)));
         lhs += dualsol * (activity - SCIPnlrowGetRhs(nlrow));
      }
      else
      {
         assert(!SCIPisInfinity(subproblem, -SCIPnlrowGetLhs(nlrow)));
         lhs += dualsol * (activity - SCIPnlrowGetLhs(nlrow));
      }
   }

   SCIPexprintFree(&exprinterpreter);

   /* looping over all variable bounds and updating the corresponding coefficients of the cut; compute checkobj */
   for( i = 0; i < nvars; i++ )
   {
      SCIP_VAR* var;
      SCIP_VAR* mastervar;
      SCIP_Real coef;

      var = vars[i];

      /* retrieving the master problem variable for the given subproblem variable. */
      SCIP_CALL( SCIPgetBendersMasterVar(masterprob, benders, var, &mastervar) );

      dualsol = SCIPgetNLPVarsUbDualsol(subproblem)[i] - SCIPgetNLPVarsLbDualsol(subproblem)[i];

      /* checking whether the subproblem variable has a corresponding master variable. */
      if( mastervar == NULL || dualsol == 0.0 )
         continue;

      coef = -dualsol;

      SCIP_CALL( SCIPaddCoefLinear(masterprob, cut, mastervar, coef) );

      dirderiv += coef * SCIPvarGetNLPSol(var);
   }

   lhs += dirderiv;

   /* if the side became infinite or dirderiv was infinite, then the cut generation terminates. */
   if( SCIPisInfinity(masterprob, lhs) || SCIPisInfinity(masterprob, -lhs)
      || SCIPisInfinity(masterprob, dirderiv) || SCIPisInfinity(masterprob, -dirderiv))
   {
      (*success) = FALSE;
      SCIPdebugMsg(masterprob, "Infinite bound when generating feasibility cut. lhs = %g dirderiv = %g.\n", lhs, dirderiv);
      return SCIP_OKAY;
   }

   /* Update the lhs of the cut */
   lhs += SCIPgetLhsLinear(masterprob, cut);
   SCIP_CALL( SCIPchgLhsLinear(masterprob, cut, lhs) );

   /* the activity of the cut should be less than the lhs. This will ensure that the evaluated solution will be cut off.
    * It is possible that the activity is greater than the lhs. This could be caused by numerical difficulties. In this
    * case, no cut will be generated.
    */
   activity = SCIPgetActivityLinear(masterprob, cut, sol);
   if( SCIPisGE(masterprob, activity, lhs) )
   {
      (*success) = FALSE;
      SCIPdebugMsg(masterprob ,"Invalid feasibility cut - activity is greater than lhs %g >= %g.\n", activity, lhs);
      return SCIP_OKAY;
   }

   assert(cut != NULL);

   SCIPdebugPrintCons(masterprob, cut, NULL);

   (*success) = TRUE;

   return SCIP_OKAY;
}

/** generates and applies Benders' cuts */
static
SCIP_RETCODE generateAndApplyBendersCuts(
   SCIP*                 masterprob,         /**< the SCIP instance of the master problem */
   SCIP*                 subproblem,         /**< the SCIP instance of the pricing problem */
   SCIP_BENDERS*         benders,            /**< the benders' decomposition */
   SCIP_BENDERSCUT*      benderscut,         /**< the benders' decomposition cut method */
   SCIP_SOL*             sol,                /**< primal CIP solution */
   int                   probnumber,         /**< the number of the pricing problem */
   SCIP_RESULT*          result              /**< the result from solving the subproblems */
   )
{
   SCIP_CONS* cut;
   char cutname[SCIP_MAXSTRLEN];
   SCIP_Bool success;

   assert(masterprob != NULL);
   assert(subproblem != NULL);
   assert(benders != NULL);
   assert(result != NULL);

   /* setting the name of the generated cut */
   (void) SCIPsnprintf(cutname, SCIP_MAXSTRLEN, "feasibilitycut_%d_%d", probnumber,
      SCIPbenderscutGetNFound(benderscut) );

   /* creating the constraint for the cut */
   SCIP_CALL( SCIPcreateConsBasicLinear(masterprob, &cut, cutname, 0, NULL, NULL, 0.0, SCIPinfinity(masterprob)) );

   if( SCIPisNLPConstructed(subproblem) )
   {
      /* computing the coefficients of the feasibility cut from the NLP */
      SCIP_CALL( computeStandardFeasibilityCutNL(masterprob, subproblem, benders, sol, cut, &success) );
   }
   else
   {
      if( SCIPgetNLPIterations(subproblem) == 0 )
      {
         SCIPverbMessage(masterprob, SCIP_VERBLEVEL_FULL, NULL, "There were no iterations in pricing problem %d. "
           "A Benders' decomposition feasibility cut will be generated from the presolved LP data.\n", probnumber);
      }

      /* computing the coefficients of the feasibility cut from the LP */
      SCIP_CALL( computeStandardFeasibilityCut(masterprob, subproblem, benders, sol, cut, &success) );
   }

   /* if success is FALSE, then there was an error in generating the feasibility cut. No cut will be added to the master
    * problem. Otherwise, the constraint is added to the master problem.
    */
   if( !success )
   {
      (*result) = SCIP_DIDNOTFIND;
      SCIPdebugMsg(masterprob, "Error in generating Benders' feasibility cut for problem %d.\n", probnumber);
   }
   else
   {
      /* adding the constraint to the master problem */
      SCIP_CALL( SCIPaddCons(masterprob, cut) );

      (*result) = SCIP_CONSADDED;
   }

   SCIP_CALL( SCIPreleaseCons(masterprob, &cut) );

   return SCIP_OKAY;
}

/*
 * Callback methods of Benders' decomposition cuts
 */

/** execution method of Benders' decomposition cuts */
static
SCIP_DECL_BENDERSCUTEXEC(benderscutExecFeas)
{  /*lint --e{715}*/
   SCIP* subproblem;

   assert(scip != NULL);
   assert(benders != NULL);
   assert(benderscut != NULL);
   assert(result != NULL);
   assert(probnumber >= 0 && probnumber < SCIPbendersGetNSubproblems(benders));

   subproblem = SCIPbendersSubproblem(benders, probnumber);

   /* only generate feasibility cuts if the subproblem is infeasible */
   if( SCIPgetStatus(subproblem) == SCIP_STATUS_INFEASIBLE ||
      (SCIPgetStage(subproblem) == SCIP_STAGE_SOLVING && !SCIPisNLPConstructed(subproblem) && SCIPgetLPSolstat(subproblem) == SCIP_LPSOLSTAT_INFEASIBLE) ||
      (SCIPgetStage(subproblem) == SCIP_STAGE_SOLVING && SCIPisNLPConstructed(subproblem) && (SCIPgetNLPSolstat(subproblem) == SCIP_NLPSOLSTAT_LOCINFEASIBLE || SCIPgetNLPSolstat(subproblem) == SCIP_NLPSOLSTAT_GLOBINFEASIBLE)) )
   {
      /* generating a cut for a given subproblem */
      SCIP_CALL( generateAndApplyBendersCuts(scip, subproblem, benders, benderscut,
            sol, probnumber, result) );
   }

   return SCIP_OKAY;
}


/*
 * Benders' decomposition cuts specific interface methods
 */

/** creates the Standard Feasibility Benders' decomposition cuts and includes it in SCIP */
SCIP_RETCODE SCIPincludeBenderscutFeas(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_BENDERS*         benders             /**< Benders' decomposition */
   )
{
   SCIP_BENDERSCUT* benderscut;

   assert(benders != NULL);

   benderscut = NULL;

   /* include Benders' decomposition cuts */
   SCIP_CALL( SCIPincludeBenderscutBasic(scip, benders, &benderscut, BENDERSCUT_NAME, BENDERSCUT_DESC,
         BENDERSCUT_PRIORITY, BENDERSCUT_LPCUT, benderscutExecFeas, NULL) );

   assert(benderscut != NULL);

   return SCIP_OKAY;
}
