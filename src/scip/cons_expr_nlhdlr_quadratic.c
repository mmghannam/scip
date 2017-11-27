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

/**@file   cons_expr_nlhdlr_quadratic.c
 * @brief  nonlinear handler to handle quadratic constraints
 * @author Felipe Serrano
 *
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <string.h>

#include "scip/cons_expr_nlhdlr_quadratic.h"
#include "scip/cons_expr_pow.h"
#include "scip/cons_expr_sum.h"
#include "scip/cons_expr_var.h"
#include "scip/cons_expr_product.h"
#include "scip/cons_quadratic.h"

/* fundamental nonlinear handler properties */
#define NLHDLR_NAME         "quadratic"
#define NLHDLR_DESC         "handler for quadratic expressions"
#define NLHDLR_PRIORITY     100

/*
 * Data structures
 */

/** nonlinear handler data */
struct SCIP_ConsExpr_NlhdlrData
{
   SCIP_Bool             initialized;        /**< whether handler has been initialized and not yet de-initialized */
};

/** nonlinear handler expression data */
struct SCIP_ConsExpr_NlhdlrExprData
{
   int                   nlinvars;           /**< number of linear variables */
   int                   linvarssize;        /**< size of linvars and lincoefs arrays */
   SCIP_VAR**            linvars;            /**< linear variables */
   SCIP_Real*            lincoefs;           /**< coefficients of linear variables */

   int                   nquadvars;          /**< number of variables in quadratic terms */
   int                   quadvarssize;       /**< size of quadvarterms array */
   SCIP_QUADVARTERM*     quadvarterms;       /**< array with quadratic variable terms */

   int                   nbilinterms;        /**< number of bilinear terms */
   int                   bilintermssize;     /**< size of bilinterms array */
   SCIP_BILINTERM*       bilinterms;         /**< bilinear terms array */
};

/*
 * static methods
 */

/** ensures, that linear vars and coefs arrays can store at least num entries */
#ifdef bla
static
SCIP_RETCODE consdataEnsureLinearVarsSize(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSDATA*        consdata,           /**< quadratic constraint data */
   int                   num                 /**< minimum number of entries to store */
   )
{
   assert(scip != NULL);
   assert(consdata != NULL);
   assert(consdata->nlinvars <= consdata->linvarssize);

   if( num > consdata->linvarssize )
   {
      int newsize;

      newsize = SCIPcalcMemGrowSize(scip, num);
      SCIP_CALL( SCIPreallocBlockMemoryArray(scip, &consdata->linvars,  consdata->linvarssize, newsize) );
      SCIP_CALL( SCIPreallocBlockMemoryArray(scip, &consdata->lincoefs, consdata->linvarssize, newsize) );
      if( consdata->lineventdata != NULL )
      {
         SCIP_CALL( SCIPreallocBlockMemoryArray(scip, &consdata->lineventdata, consdata->linvarssize, newsize) );
      }
      consdata->linvarssize = newsize;
   }
   assert(num <= consdata->linvarssize);

   return SCIP_OKAY;
}
#endif

static
SCIP_RETCODE addVarToQuadterms(SCIP_VAR* var, SCIP_CONSEXPR_NLHDLREXPRDATA* exprdata, SCIP_HASHMAP* varidx)
{

   SCIP_QUADVARTERM* quadterm;

   /* if var is in varidx --> it could be because it is linear or quadratic; if linear, remove it from varidx and
    * add a quadterm with square coef equal 0: because of the ordering, we cannot see var nor var^2 anymore;
    * if quadratic, just ignore it.
    * if it is not in varidx --> add a quadterm linear and square coef equal 0 and add it to varidx
    */
   if( SCIPhashmapExists(varidx, var) )
   {
      int idx;

      idx = (int)(size_t)SCIPhashmapGetImage(varidx, var);

      if( idx >= 0 ) /* var has been seen before as a linear variable */
      {
         assert(var == exprdata->linvars[idx]);

         quadterm = &exprdata->quadvarterms[exprdata->nquadvars];

         quadterm->var = var;
         quadterm->sqrcoef = 0.0;
         quadterm->lincoef = exprdata->lincoefs[idx];

         /* var is no longer linear var --> remove it from exprdata->linvars */
         exprdata->nlinvars--;
         if( idx < exprdata->nlinvars )
         {
            exprdata->linvars[idx] = exprdata->linvars[exprdata->nlinvars];
            exprdata->lincoefs[idx] = exprdata->lincoefs[exprdata->nlinvars];
            SCIP_CALL( SCIPhashmapSetImage(varidx, (void *)exprdata->linvars[idx], (void *)(size_t)idx) );
         }

         /* update index of var */
         SCIP_CALL( SCIPhashmapSetImage(varidx, (void *)var, (void *)(size_t)(-exprdata->nquadvars - 1)) );

         /* update number of quadvars */
         exprdata->nquadvars++;
      }
   }
   else /* first time seeing var */
   {
      quadterm = &exprdata->quadvarterms[exprdata->nquadvars];

      quadterm->var = var;
      quadterm->sqrcoef = 0.0;
      quadterm->lincoef = 0.0;

      /* add var to varidx */
      SCIP_CALL( SCIPhashmapInsert(varidx, (void *)var, (void *)(size_t)(-exprdata->nquadvars - 1)) );

      /* update number of quadvars */
      exprdata->nquadvars++;
   }
   return SCIP_OKAY;
}

/*
 * Callback methods of nonlinear handler
 */

/** callback to free data of handler */
static
SCIP_DECL_CONSEXPR_NLHDLRFREEHDLRDATA(freeHdlrDataQuadratic)
{
   return SCIP_OKAY;
}

/** callback to free expression specific data */
static
SCIP_DECL_CONSEXPR_NLHDLRFREEEXPRDATA(freeExprDataQuadratic)
{
   SCIPfreeBlockMemoryArray(scip, &((*nlhdlrexprdata)->linvars), (*nlhdlrexprdata)->linvarssize);
   SCIPfreeBlockMemoryArray(scip, &((*nlhdlrexprdata)->lincoefs), (*nlhdlrexprdata)->linvarssize);
   SCIPfreeBlockMemoryArray(scip, &((*nlhdlrexprdata)->quadvarterms), (*nlhdlrexprdata)->quadvarssize);
   SCIPfreeBlockMemoryArray(scip, &((*nlhdlrexprdata)->bilinterms), (*nlhdlrexprdata)->bilintermssize);

   SCIPfreeBlockMemory(scip, nlhdlrexprdata);

   return SCIP_OKAY;
}

/** callback to be called in initialization */
static
SCIP_DECL_CONSEXPR_NLHDLRINIT(initHdlrQuadratic)
{
   return SCIP_OKAY;
}

/** callback to be called in deinitialization */
static
SCIP_DECL_CONSEXPR_NLHDLREXIT(exitHldrQuadratic)
{
   return SCIP_OKAY;
}


static
SCIP_Bool isTwoVarsProduct(SCIP_CONSHDLR* conshdlr, SCIP_CONSEXPR_EXPR* expr, SCIP_VAR** var1, SCIP_VAR** var2)
{
   assert(var1 != NULL && var2 != NULL);

   if( SCIPgetConsExprExprHdlr(expr) != SCIPgetConsExprExprHdlrProduct(conshdlr) )
      return FALSE;

   if( SCIPgetConsExprExprNChildren(expr) != 2 )
      return FALSE;

   /* linearization var returns var if expr is var */
   *var1 = SCIPgetConsExprExprLinearizationVar(SCIPgetConsExprExprChildren(expr)[0]);
   *var2 = SCIPgetConsExprExprLinearizationVar(SCIPgetConsExprExprChildren(expr)[1]);
   assert(*var1 != NULL && *var2 != NULL);

   return TRUE;
}

static
SCIP_Bool isVarSquare(SCIP_CONSHDLR* conshdlr, SCIP_CONSEXPR_EXPR* expr, SCIP_VAR** var)
{
   if( strcmp("pow", SCIPgetConsExprExprHdlrName(SCIPgetConsExprExprHdlr(expr))) != 0 )
      return FALSE;

   assert(SCIPgetConsExprExprNChildren(expr) == 1);

   if( SCIPgetConsExprExprPowExponent(expr) != 2.0 )
      return FALSE;

   /* linearization var returns var if expr is var */
   *var = SCIPgetConsExprExprLinearizationVar(SCIPgetConsExprExprChildren(expr)[0]);
   assert(*var != NULL);

   return TRUE;
}

/** callback to detect structure in expression tree */
/*
 * - scip SCIP data structure
 * - conshdlr expr-constraint handler
 * - nlhdlr nonlinear handler
 * - expr expression to analyze
 * - success buffer to return whether a nlhdlr specific structure has been found
 * - nlhdlrexprdata nlhdlr's expr data to be stored in expr, can only be set to non-NULL if success is set to TRUE
 *
 * An expression is quadratic if:
 * - It is a product expression of two var expressions
 * - It is power expression of a var expression with exponent 2.0
 * - It is a sum expression where each of its chlidren is of the type of one of the above or a simple variable
 *
 * @note: the expression needs to be simplified (in particular, it is assumed to be sorted)
 * Sorted implies that:
 *  - expr < expr^2: bases are the same, but exponent 1 < 2
 *  - expr < expr * other_expr: u*v < w holds if and only if v < w (OR8), but here w = u < v, since expr comes before
 *  other_expr in the product
 *  - expr < other_expr * expr: u*v < w holds if and only if v < w (OR8), but here v = w
 *
 * It also implies that
 *  - expr^2 < expr * other_expr
 *  - other_expr * expr < expr^2
 *
 * It also implies that x^-2 < x^-1, but since, so far, we do not interpret x^-2 as (x^-1)^2, it is not a problem.
 */
/* TODO: capture variables? */
static
SCIP_DECL_CONSEXPR_NLHDLRDETECT(detectHdlrQuadratic)
{
   SCIP_CONSEXPR_NLHDLREXPRDATA* exprdata;
   SCIP_HASHMAP*  varidx;
   SCIP_VAR* var1 = NULL;
   SCIP_VAR* var2 = NULL;
   int c;

   assert(scip != NULL);
   assert(nlhdlr != NULL);
   assert(expr != NULL);
   assert(success != NULL);
   assert(nlhdlrexprdata != NULL);

   *success = FALSE;

   /* TODO: handle simple cases */
   if( isVarSquare(conshdlr, expr, &var1) || isTwoVarsProduct(conshdlr, expr, &var1, &var2) )
   {
      *success = TRUE;
      return SCIP_OKAY;
   }

   /* the only case left is sum expressions */
   if( SCIPgetConsExprExprHdlr(expr) != SCIPgetConsExprExprHdlrSum(conshdlr) )
      return SCIP_OKAY;

   /* varidx maps vars to indices; if index > 0, it is its index in the linvars array, otherwise -index-1 is its
    * index in the quadterms array
    */
   SCIP_CALL( SCIPhashmapCreate(&varidx, SCIPblkmem(scip), SCIPgetConsExprExprNChildren(expr)) );

   /* TODO: too much memory? should resize afterwards? */
   SCIP_CALL( SCIPallocBlockMemory(scip, nlhdlrexprdata) );
   exprdata = *nlhdlrexprdata;
   SCIP_CALL( SCIPallocClearBlockMemoryArray(scip, &exprdata->linvars, SCIPgetConsExprExprNChildren(expr)) );
   SCIP_CALL( SCIPallocClearBlockMemoryArray(scip, &exprdata->lincoefs, SCIPgetConsExprExprNChildren(expr)) );
   SCIP_CALL( SCIPallocClearBlockMemoryArray(scip, &exprdata->quadvarterms, SCIPgetConsExprExprNChildren(expr)) );
   SCIP_CALL( SCIPallocClearBlockMemoryArray(scip, &exprdata->bilinterms, SCIPgetConsExprExprNChildren(expr)) );
   exprdata->linvarssize = SCIPgetConsExprExprNChildren(expr);
   exprdata->quadvarssize = SCIPgetConsExprExprNChildren(expr);
   exprdata->bilintermssize = SCIPgetConsExprExprNChildren(expr);

   /* TODO comment */
   exprdata->nlinvars = 0;
   exprdata->nquadvars = 0;
   exprdata->nbilinterms = 0;
   for( c = 0; c < SCIPgetConsExprExprNChildren(expr); ++c )
   {
      SCIP_CONSEXPR_EXPR* child;
      SCIP_Real coef;

      child = SCIPgetConsExprExprChildren(expr)[c];
      coef = SCIPgetConsExprExprSumCoefs(expr)[c];

      assert(child != NULL);
      assert(! SCIPisZero(scip, coef));

      var1 = NULL;
      var2 = NULL;
      if( isVarSquare(conshdlr, child, &var1) ) /* quadratic term */
      {
         int idx;

         assert(var1 != NULL);

         /* if var1 has already been seen in a bilinear term --> var is in a quadterm no need to create a new one */
         if( SCIPhashmapExists(varidx, var1) &&
               (idx = (int)(size_t)SCIPhashmapGetImage(varidx, var1)) < 0 )
         {
            SCIP_QUADVARTERM* quadterm = &exprdata->quadvarterms[-idx-1];

            assert(quadterm->var == var1);
            assert(quadterm->sqrcoef == 0.0);

            quadterm->sqrcoef = coef;
         }
         else
         {
            /* add a new quadterm sqrcoef = 0 and with the lincoef of var1 */
            SCIP_CALL( addVarToQuadterms(var1, exprdata, varidx) );

            /* change sqr coef of newly created quadterm */
            exprdata->quadvarterms[exprdata->nquadvars - 1].sqrcoef = coef;
         }
      }
      else if( isTwoVarsProduct(conshdlr, child, &var1, &var2) ) /* bilinear term */
      {
         SCIP_BILINTERM* bilinterm;
         assert(SCIPgetConsExprExprProductCoef(child) == 1.0);
         assert(var1 != NULL && var2 != NULL);

         bilinterm = &exprdata->bilinterms[exprdata->nbilinterms];
         exprdata->nbilinterms++;

         bilinterm->coef = coef;
         bilinterm->var1 = var1;
         bilinterm->var2 = var2;

         /* variables involved in a bilinear term that are not in a quadterm, need to be added to a quadterm and removed
          * from exprdata->linvars */
         SCIP_CALL( addVarToQuadterms(var1, exprdata, varidx) );
         SCIP_CALL( addVarToQuadterms(var2, exprdata, varidx) );
      }
      else
      {
         /* not a product of exprs nor square of an expr --> use aux var (which is var itself when expr is var expr) */
         var1 = SCIPgetConsExprExprLinearizationVar(child);
         assert(var1 != NULL);

         /* store its index in linvars in case we see this var again later in a bilinear or quadratic term */
         SCIP_CALL( SCIPhashmapInsert(varidx, var1, (void *)(size_t)exprdata->nlinvars) );
         exprdata->linvars[exprdata->nlinvars] = var1;
         exprdata->lincoefs[exprdata->nlinvars] = coef;
         exprdata->nlinvars++;
      }
   }

   /* check whether identified quadratic expression is actually quadratic */
   if( exprdata->nbilinterms + exprdata->nquadvars == 0 )
   {
      SCIP_CALL( freeExprDataQuadratic(scip, nlhdlr, nlhdlrexprdata) );
   }

   SCIPhashmapFree(&varidx);


   /* TODO: check if convex or concave */
   *success = TRUE;
   return SCIP_OKAY;
}

/** nonlinear handler separation callback
 *
 * The method tries to separate a given point by means of the nonlinear handler.
 */
static
SCIP_DECL_CONSEXPR_NLHDLRSEPA(sepaHdlrQuadratic)
{
   return SCIP_OKAY;
}

/** nonlinear handler copy callback
 *
 * the method includes the nonlinear handler into a expression constraint handler
 *
 * This method is usually called when doing a copy of an expression constraint handler.
 */
static
SCIP_DECL_CONSEXPR_NLHDLRCOPYHDLR(copyHdlrQuadratic)
{
   SCIP_CONSEXPR_NLHDLR* targetnlhdlr;
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;

   assert(targetscip != NULL);
   assert(targetconsexprhdlr != NULL);
   assert(sourceconsexprhdlr != NULL);
   assert(sourcenlhdlr != NULL);
   assert(strcmp(SCIPgetConsExprNlhdlrName(sourcenlhdlr), "testhdlr") == 0);

   SCIP_CALL( SCIPallocClearMemory(targetscip, &nlhdlrdata) );

   SCIP_CALL( SCIPincludeConsExprNlhdlrBasic(targetscip, targetconsexprhdlr, &targetnlhdlr,
            SCIPgetConsExprNlhdlrName(sourcenlhdlr), SCIPgetConsExprNlhdlrDesc(sourcenlhdlr),
            SCIPgetConsExprNlhdlrPriority(sourcenlhdlr), detectHdlrQuadratic, nlhdlrdata) );

   SCIPsetConsExprNlhdlrFreeHdlrData(targetscip, targetnlhdlr, freeHdlrDataQuadratic);
   SCIPsetConsExprNlhdlrFreeExprData(targetscip, targetnlhdlr, freeExprDataQuadratic);
   SCIPsetConsExprNlhdlrCopyHdlr(targetscip, targetnlhdlr, copyHdlrQuadratic);
   SCIPsetConsExprNlhdlrInitExit(targetscip, targetnlhdlr, initHdlrQuadratic, exitHldrQuadratic);
   SCIPsetConsExprNlhdlrSepa(targetscip, targetnlhdlr, sepaHdlrQuadratic);

   return SCIP_OKAY;
}

/** includes quadratic nonlinear handler to consexpr */
SCIP_RETCODE SCIPincludeConsExprNlhdlrQuadratic(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_CONSHDLR*        consexprhdlr        /**< expression constraint handler */
   )
{
   SCIP_CONSEXPR_NLHDLR* nlhdlr;
   SCIP_CONSEXPR_NLHDLRDATA* nlhdlrdata;

   assert(scip != NULL);
   assert(consexprhdlr != NULL);

   /* SCIP_CALL( SCIPallocClearMemory(scip, &nlhdlrdata) ); */
   nlhdlrdata = NULL;

   SCIP_CALL( SCIPincludeConsExprNlhdlrBasic(scip, consexprhdlr, &nlhdlr, NLHDLR_NAME, NLHDLR_DESC, NLHDLR_PRIORITY,
            detectHdlrQuadratic, nlhdlrdata) );

   SCIPsetConsExprNlhdlrFreeExprData(scip, nlhdlr, freeExprDataQuadratic);


   /* TODO: create and store expression specific data here */

   return SCIP_OKAY;
}
