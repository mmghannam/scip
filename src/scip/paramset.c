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

/**@file   paramset.c
 * @brief  methods and datastructures for handling parameter settings
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#include <assert.h>
#include <string.h>

#include "memory.h"
#include "misc.h"
#include "message.h"
#include "paramset.h"



/** data for Bool parameters */
struct BoolParam
{
   Bool*            valueptr;           /**< pointer to store the current parameter value, or NULL */
   Bool             actvalue;           /**< stores the actual parameter value if it is not stored in *valueptr */
   Bool             defaultvalue;       /**< default value of the parameter */
};
typedef struct BoolParam BOOLPARAM;

/** data for int parameters */
struct IntParam
{
   int*             valueptr;           /**< pointer to store the current parameter value, or NULL */
   int              actvalue;           /**< stores the actual parameter value if it is not stored in *valueptr */
   int              defaultvalue;       /**< default value of the parameter */
   int              minvalue;           /**< minimum value for parameter */
   int              maxvalue;           /**< maximum value for parameter */
};
typedef struct IntParam INTPARAM;

/** data for Longint parameters */
struct LongintParam
{
   Longint*         valueptr;           /**< pointer to store the current parameter value, or NULL */
   Longint          actvalue;           /**< stores the actual parameter value if it is not stored in *valueptr */
   Longint          defaultvalue;       /**< default value of the parameter */
   Longint          minvalue;           /**< minimum value for parameter */
   Longint          maxvalue;           /**< maximum value for parameter */
};
typedef struct LongintParam LONGINTPARAM;

/** data for Real parameters */
struct RealParam
{
   Real*            valueptr;           /**< pointer to store the current parameter value, or NULL */
   Real             actvalue;           /**< stores the actual parameter value if it is not stored in *valueptr */
   Real             defaultvalue;       /**< default value of the parameter */
   Real             minvalue;           /**< minimum value for parameter */
   Real             maxvalue;           /**< maximum value for parameter */
};
typedef struct RealParam REALPARAM;

/** data for char parameters */
struct CharParam
{
   char*            valueptr;           /**< pointer to store the current parameter value, or NULL */
   char             actvalue;           /**< stores the actual parameter value if it is not stored in *valueptr */
   char             defaultvalue;       /**< default value of the parameter */
   char*            allowedvalues;      /**< array with possible parameter values, or NULL if not restricted */
};
typedef struct CharParam CHARPARAM;

/** data for char* parameters */
struct StringParam
{
   char**           valueptr;           /**< pointer to store the current parameter value, or NULL */
   char*            actvalue;           /**< stores the actual parameter value if it is not stored in *valueptr */
   char*            defaultvalue;       /**< default value of the parameter */
};
typedef struct StringParam STRINGPARAM;

/** single parameter */
struct Param
{
   union
   {
      BOOLPARAM     boolparam;          /**< data for Bool parameters */
      INTPARAM      intparam;           /**< data for int parameters */
      LONGINTPARAM  longintparam;       /**< data for Longint parameters */
      REALPARAM     realparam;          /**< data for Real parameters */
      CHARPARAM     charparam;          /**< data for char parameters */
      STRINGPARAM   stringparam;        /**< data for char* parameters */
   } data;
   char*            name;               /**< name of the parameter */
   char*            desc;               /**< description of the parameter */
   DECL_PARAMCHGD   ((*paramchgd));     /**< change information method of parameter */
   PARAMDATA*       paramdata;          /**< locally defined parameter specific data */
   PARAMTYPE        paramtype;          /**< type of this parameter */
};

/** set of parameters */
struct ParamSet
{
   HASHTABLE*       hashtable;          /**< hash table to store the parameters */
   PARAM**          params;             /**< array with parameters */
   int              nparams;            /**< number of parameters */
   int              paramssize;         /**< size of params array */
};




/*
 * Parameter methods
 */

/** hash key retrieval function for parameters */
static
DECL_HASHGETKEY(hashGetKeyParam)
{
   PARAM* param;

   param = (PARAM*)elem;
   assert(param != NULL);

   return param->name;
}

/** checks parameter value according to the given feasible domain; issues a warning message if value was invalid */
static
RETCODE paramCheckBool(
   PARAM*           param,              /**< parameter */
   Bool             value               /**< value to check */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_BOOL);

   if( value != TRUE && value != FALSE )
   {
      char s[MAXSTRLEN];
      sprintf(s, "Invalid value <%d> for bool parameter <%s>. Must be <0> (FALSE) or <1> (TRUE).",
         value, param->name);
      warningMessage(s);
      return SCIP_PARAMETERWRONGVAL;
   }

   return SCIP_OKAY;
}

/** checks parameter value according to the given feasible domain; issues a warning message if value was invalid */
static
RETCODE paramCheckInt(
   PARAM*           param,              /**< parameter */
   int              value               /**< value to check */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_INT);

   if( value < param->data.intparam.minvalue || value > param->data.intparam.maxvalue )
   {
      char s[MAXSTRLEN];
      sprintf(s, "Invalid value <%d> for int parameter <%s>. Must be in range [%d,%d].",
         value, param->name, param->data.intparam.minvalue, param->data.intparam.maxvalue);
      warningMessage(s);
      return SCIP_PARAMETERWRONGVAL;
   }
   
   return SCIP_OKAY;
}

/** checks parameter value according to the given feasible domain; issues a warning message if value was invalid */
static
RETCODE paramCheckLongint(
   PARAM*           param,              /**< parameter */
   Longint          value               /**< value to check */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_LONGINT);

   if( value < param->data.longintparam.minvalue || value > param->data.longintparam.maxvalue )
   {
      char s[MAXSTRLEN];
      sprintf(s, "Invalid value <%lld> for longint parameter <%s>. Must be in range [%lld,%lld].",
         value, param->name, param->data.longintparam.minvalue, param->data.longintparam.maxvalue);
      warningMessage(s);
      return SCIP_PARAMETERWRONGVAL;
   }
   
   return SCIP_OKAY;
}

/** checks parameter value according to the given feasible domain; issues a warning message if value was invalid */
static
RETCODE paramCheckReal(
   PARAM*           param,              /**< parameter */
   Real             value               /**< value to check */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_REAL);

   if( value < param->data.realparam.minvalue || value > param->data.realparam.maxvalue )
   {
      char s[MAXSTRLEN];
      sprintf(s, "Invalid real parameter value <%g>. Must be in range [%g,%g].",
         value, param->data.realparam.minvalue, param->data.realparam.maxvalue);
      warningMessage(s);
      return SCIP_PARAMETERWRONGVAL;
   }
   
   return SCIP_OKAY;
}

/** checks parameter value according to the given feasible domain; issues a warning message if value was invalid */
static
RETCODE paramCheckChar(
   PARAM*           param,              /**< parameter */
   char             value               /**< value to check */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_CHAR);

   if( value == '\b' || value == '\f' || value == '\n' || value == '\r' || value == '\v' )
   {
      char s[MAXSTRLEN];
      sprintf(s, "Invalid char parameter value <%x>.", (int)value);
      warningMessage(s);
      return SCIP_PARAMETERWRONGVAL;
   }

   if( param->data.charparam.allowedvalues != NULL )
   {
      char* c;

      c = param->data.charparam.allowedvalues;
      while( *c != '\0' && *c != value )
         c++;

      if( *c != value )
      {
         char s[MAXSTRLEN];
         sprintf(s, "Invalid char parameter value <%c>. Must be in set {%s}.",
            value, param->data.charparam.allowedvalues);
         warningMessage(s);
         return SCIP_PARAMETERWRONGVAL;
      }
   }

   return SCIP_OKAY;
}

/** checks parameter value according to the given feasible domain; issues a warning message if value was invalid */
static
RETCODE paramCheckString(
   PARAM*           param,              /**< parameter */
   const char*      value               /**< value to check */
   )
{
   unsigned int i;

   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_STRING);

   if( value == NULL )
   {
      warningMessage("Cannot assign a NULL string to a string parameter.");
      return SCIP_PARAMETERWRONGVAL;
   }

   for( i = 0; i < strlen(value); ++i )
   {
      if( value[i] == '\b' || value[i] == '\f' || value[i] == '\n' || value[i] == '\r' || value[i] == '\v' )
      {
         char s[MAXSTRLEN];
         sprintf(s, "Invalid character <%x> in string parameter at position %d.", (int)value[i], i);
         warningMessage(s);
         return SCIP_PARAMETERWRONGVAL;
      }
   }

   return SCIP_OKAY;
}

/** returns type of parameter */
PARAMTYPE SCIPparamGetType(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);

   return param->paramtype;
}

/** returns name of parameter */
const char* SCIPparamGetName(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);

   return param->name;
}

/** returns description of parameter */
const char* SCIPparamGetDesc(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);

   return param->desc;
}

/** returns locally defined parameter specific data */
PARAMDATA* SCIPparamGetData(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);

   return param->paramdata;
}

/** returns value of Bool parameter */
Bool SCIPparamGetBool(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_BOOL);

   if( param->data.boolparam.valueptr != NULL )
      return *param->data.boolparam.valueptr;
   else
      return param->data.boolparam.actvalue;
}

/** returns value of int parameter */
int SCIPparamGetInt(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_INT);

   if( param->data.intparam.valueptr != NULL )
      return *param->data.intparam.valueptr;
   else
      return param->data.intparam.actvalue;
}

/** returns value of Longint parameter */
Longint SCIPparamGetLongint(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_LONGINT);

   if( param->data.longintparam.valueptr != NULL )
      return *param->data.longintparam.valueptr;
   else
      return param->data.longintparam.actvalue;
}

/** returns value of Real parameter */
Real SCIPparamGetReal(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_REAL);

   if( param->data.realparam.valueptr != NULL )
      return *param->data.realparam.valueptr;
   else
      return param->data.realparam.actvalue;
}

/** returns value of char parameter */
char SCIPparamGetChar(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_CHAR);

   if( param->data.charparam.valueptr != NULL )
      return *param->data.charparam.valueptr;
   else
      return param->data.charparam.actvalue;
}

/** returns value of string parameter */
char* SCIPparamGetString(
   PARAM*           param               /**< parameter */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_STRING);

   if( param->data.stringparam.valueptr != NULL )
      return *param->data.stringparam.valueptr;
   else
      return param->data.stringparam.actvalue;
}

/** sets value of Bool parameter */
static
RETCODE paramSetBool(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure, or NULL if paramchgd method should not be called */   
   Bool             value               /**< new value of the parameter */
   )
{
   assert(param != NULL);

   /* check, if value is possible for the parameter */
   CHECK_OKAY( paramCheckBool(param, value) );

   /* set the actual parameter's value */
   if( param->data.boolparam.valueptr != NULL )
      *param->data.boolparam.valueptr = value;
   else
      param->data.boolparam.actvalue = value;

   /* call the parameter's change information method */
   if( param->paramchgd != NULL && scip != NULL )
   {
      CHECK_OKAY( param->paramchgd(scip, param) );
   }

   return SCIP_OKAY;
}

/** sets value of int parameter */
static
RETCODE paramSetInt(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure, or NULL if paramchgd method should not be called */   
   int              value               /**< new value of the parameter */
   )
{
   assert(param != NULL);

   /* check, if value is possible for the parameter */
   CHECK_OKAY( paramCheckInt(param, value) );

   /* set the actual parameter's value */
   if( param->data.intparam.valueptr != NULL )
      *param->data.intparam.valueptr = value;
   else
      param->data.intparam.actvalue = value;

   /* call the parameter's change information method */
   if( param->paramchgd != NULL && scip != NULL )
   {
      CHECK_OKAY( param->paramchgd(scip, param) );
   }

   return SCIP_OKAY;
}

/** sets value of Longint parameter */
static
RETCODE paramSetLongint(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure, or NULL if paramchgd method should not be called */   
   Longint          value               /**< new value of the parameter */
   )
{
   assert(param != NULL);

   /* check, if value is possible for the parameter */
   CHECK_OKAY( paramCheckLongint(param, value) );

   /* set the actual parameter's value */
   if( param->data.longintparam.valueptr != NULL )
      *param->data.longintparam.valueptr = value;
   else
      param->data.longintparam.actvalue = value;

   /* call the parameter's change information method */
   if( param->paramchgd != NULL && scip != NULL )
   {
      CHECK_OKAY( param->paramchgd(scip, param) );
   }

   return SCIP_OKAY;
}

/** sets value of Real parameter */
static
RETCODE paramSetReal(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure, or NULL if paramchgd method should not be called */   
   Real             value               /**< new value of the parameter */
   )
{
   assert(param != NULL);

   /* check, if value is possible for the parameter */
   CHECK_OKAY( paramCheckReal(param, value) );

   /* set the actual parameter's value */
   if( param->data.realparam.valueptr != NULL )
      *param->data.realparam.valueptr = value;
   else
      param->data.realparam.actvalue = value;

   /* call the parameter's change information method */
   if( param->paramchgd != NULL && scip != NULL )
   {
      CHECK_OKAY( param->paramchgd(scip, param) );
   }

   return SCIP_OKAY;
}

/** sets value of char parameter */
static
RETCODE paramSetChar(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure, or NULL if paramchgd method should not be called */   
   char             value               /**< new value of the parameter */
   )
{
   assert(param != NULL);

   /* check, if value is possible for the parameter */
   CHECK_OKAY( paramCheckChar(param, value) );

   /* set the actual parameter's value */
   if( param->data.charparam.valueptr != NULL )
      *param->data.charparam.valueptr = value;
   else
      param->data.charparam.actvalue = value;

   /* call the parameter's change information method */
   if( param->paramchgd != NULL && scip != NULL )
   {
      CHECK_OKAY( param->paramchgd(scip, param) );
   }

   return SCIP_OKAY;
}

/** sets value of string parameter */
static
RETCODE paramSetString(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure, or NULL if paramchgd method should not be called */   
   const char*      value               /**< new value of the parameter */
   )
{
   assert(param != NULL);

   /* check, if value is possible for the parameter */
   CHECK_OKAY( paramCheckString(param, value) );

   /* set the actual parameter's value */
   if( param->data.stringparam.valueptr != NULL )
   {
      freeMemoryArrayNull(param->data.stringparam.valueptr);
      duplicateMemoryArray(param->data.stringparam.valueptr, value, strlen(value)+1);
   }
   else
   {
      freeMemoryArrayNull(&param->data.stringparam.actvalue);
      duplicateMemoryArray(&param->data.stringparam.actvalue, value, strlen(value)+1);
   }

   /* call the parameter's change information method */
   if( param->paramchgd != NULL && scip != NULL )
   {
      CHECK_OKAY( param->paramchgd(scip, param) );
   }

   return SCIP_OKAY;
}

/** creates a parameter with name and description, does not set the type specific parameter values themselves */
static
RETCODE paramCreate(
   PARAM**          param,              /**< pointer to the parameter */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   assert(param != NULL);
   assert(name != NULL);
   assert(desc != NULL);

   ALLOC_OKAY( allocBlockMemory(memhdr, param) );
   
   ALLOC_OKAY( duplicateMemoryArray(&(*param)->name, name, strlen(name)+1) );
   ALLOC_OKAY( duplicateMemoryArray(&(*param)->desc, desc, strlen(desc)+1) );

   (*param)->paramchgd = paramchgd;
   (*param)->paramdata = paramdata;

   return SCIP_OKAY;
}

/** creates a Bool parameter, and sets its value to default */
static
RETCODE paramCreateBool(
   PARAM**          param,              /**< pointer to the parameter */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   Bool*            valueptr,           /**< pointer to store the current parameter value, or NULL */
   Bool             defaultvalue,       /**< default value of the parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   assert(param != NULL);
   assert(name != NULL);

   CHECK_OKAY( paramCreate(param, memhdr, name, desc, paramchgd, paramdata) );

   (*param)->paramtype = SCIP_PARAMTYPE_BOOL;
   (*param)->data.boolparam.valueptr = valueptr;
   (*param)->data.boolparam.defaultvalue = defaultvalue;

   CHECK_OKAY( paramSetBool(*param, NULL, defaultvalue) );

   return SCIP_OKAY;
}

/** creates a int parameter, and sets its value to default */
static
RETCODE paramCreateInt(
   PARAM**          param,              /**< pointer to the parameter */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   int*             valueptr,           /**< pointer to store the current parameter value, or NULL */
   int              defaultvalue,       /**< default value of the parameter */
   int              minvalue,           /**< minimum value for parameter */
   int              maxvalue,           /**< maximum value for parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   assert(param != NULL);
   assert(name != NULL);

   CHECK_OKAY( paramCreate(param, memhdr, name, desc, paramchgd, paramdata) );

   (*param)->paramtype = SCIP_PARAMTYPE_INT;
   (*param)->data.intparam.valueptr = valueptr;
   (*param)->data.intparam.defaultvalue = defaultvalue;
   (*param)->data.intparam.minvalue = minvalue;
   (*param)->data.intparam.maxvalue = maxvalue;

   CHECK_OKAY( paramSetInt(*param, NULL, defaultvalue) );

   return SCIP_OKAY;
}

/** creates a Longint parameter, and sets its value to default */
static
RETCODE paramCreateLongint(
   PARAM**          param,              /**< pointer to the parameter */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   Longint*         valueptr,           /**< pointer to store the current parameter value, or NULL */
   Longint          defaultvalue,       /**< default value of the parameter */
   Longint          minvalue,           /**< minimum value for parameter */
   Longint          maxvalue,           /**< maximum value for parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   assert(param != NULL);
   assert(name != NULL);

   CHECK_OKAY( paramCreate(param, memhdr, name, desc, paramchgd, paramdata) );

   (*param)->paramtype = SCIP_PARAMTYPE_LONGINT;
   (*param)->data.longintparam.valueptr = valueptr;
   (*param)->data.longintparam.defaultvalue = defaultvalue;
   (*param)->data.longintparam.minvalue = minvalue;
   (*param)->data.longintparam.maxvalue = maxvalue;

   CHECK_OKAY( paramSetLongint(*param, NULL, defaultvalue) );

   return SCIP_OKAY;
}

/** creates a Real parameter, and sets its value to default */
static
RETCODE paramCreateReal(
   PARAM**          param,              /**< pointer to the parameter */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   Real*            valueptr,           /**< pointer to store the current parameter value, or NULL */
   Real             defaultvalue,       /**< default value of the parameter */
   Real             minvalue,           /**< minimum value for parameter */
   Real             maxvalue,           /**< maximum value for parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   assert(param != NULL);
   assert(name != NULL);

   CHECK_OKAY( paramCreate(param, memhdr, name, desc, paramchgd, paramdata) );

   (*param)->paramtype = SCIP_PARAMTYPE_REAL;
   (*param)->data.realparam.valueptr = valueptr;
   (*param)->data.realparam.defaultvalue = defaultvalue;
   (*param)->data.realparam.minvalue = minvalue;
   (*param)->data.realparam.maxvalue = maxvalue;

   CHECK_OKAY( paramSetReal(*param, NULL, defaultvalue) );

   return SCIP_OKAY;
}

/** creates a char parameter, and sets its value to default */
static
RETCODE paramCreateChar(
   PARAM**          param,              /**< pointer to the parameter */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   char*            valueptr,           /**< pointer to store the current parameter value, or NULL */
   char             defaultvalue,       /**< default value of the parameter */
   const char*      allowedvalues,      /**< array with possible parameter values, or NULL if not restricted */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   assert(param != NULL);
   assert(name != NULL);

   CHECK_OKAY( paramCreate(param, memhdr, name, desc, paramchgd, paramdata) );

   (*param)->paramtype = SCIP_PARAMTYPE_CHAR;
   (*param)->data.charparam.valueptr = valueptr;
   (*param)->data.charparam.defaultvalue = defaultvalue;
   if( allowedvalues != NULL )
   {
      ALLOC_OKAY( duplicateMemoryArray(&(*param)->data.charparam.allowedvalues, allowedvalues, strlen(allowedvalues)+1) );
   }
   else
      (*param)->data.charparam.allowedvalues = NULL;

   CHECK_OKAY( paramSetChar(*param, NULL, defaultvalue) );

   return SCIP_OKAY;
}

/** creates a string parameter, and sets its value to default */
static
RETCODE paramCreateString(
   PARAM**          param,              /**< pointer to the parameter */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   char**           valueptr,           /**< pointer to store the current parameter value, or NULL */
   const char*      defaultvalue,       /**< default value of the parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   assert(param != NULL);
   assert(name != NULL);
   assert(valueptr == NULL || *valueptr == NULL);
   assert(defaultvalue != NULL);

   CHECK_OKAY( paramCreate(param, memhdr, name, desc, paramchgd, paramdata) );

   (*param)->paramtype = SCIP_PARAMTYPE_STRING;
   (*param)->data.stringparam.valueptr = valueptr;
   ALLOC_OKAY( duplicateMemoryArray(&(*param)->data.stringparam.defaultvalue, defaultvalue, strlen(defaultvalue)+1) );
   (*param)->data.stringparam.actvalue = NULL;

   CHECK_OKAY( paramSetString(*param, NULL, defaultvalue) );

   return SCIP_OKAY;
}

/** frees a single parameter */
static
void paramFree(
   PARAM**          param,              /**< pointer to the parameter */
   MEMHDR*          memhdr              /**< block memory */
   )
{
   assert(param != NULL);
   assert(*param != NULL);

   switch( (*param)->paramtype )
   {
   case SCIP_PARAMTYPE_BOOL:
   case SCIP_PARAMTYPE_INT:
   case SCIP_PARAMTYPE_LONGINT:
   case SCIP_PARAMTYPE_REAL:
      break;
   case SCIP_PARAMTYPE_CHAR:
      freeMemoryArrayNull(&(*param)->data.charparam.allowedvalues);
      break;
   case SCIP_PARAMTYPE_STRING:
      freeMemoryArray(&(*param)->data.stringparam.defaultvalue);
      if( (*param)->data.stringparam.valueptr == NULL )
      {
         freeMemoryArray(&(*param)->data.stringparam.actvalue);
      }
      else
      {
         freeMemoryArray(&(*param)->data.stringparam.valueptr);
      }
      break;
   default:
      errorMessage("invalid parameter type");
      abort();
   }

   freeMemoryArray(&(*param)->name);
   freeMemoryArray(&(*param)->desc);
   freeBlockMemory(memhdr, param);
}

/** sets Bool parameter according to the value of the given string */
static
RETCODE paramParseBool(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure */   
   char*            valuestr            /**< value in string format (may be modified during parse) */
   )
{
   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_BOOL);
   assert(valuestr != NULL);

   if( strcasecmp(valuestr, "TRUE") == 0 )
   {
      CHECK_OKAY( paramSetBool(param, scip, TRUE) );
   }
   else if( strcasecmp(valuestr, "FALSE") == 0 )
   {
      CHECK_OKAY( paramSetBool(param, scip, FALSE) );
   }
   else
   {
      char s[2*MAXSTRLEN];
      sprintf(s, "invalid parameter value <%s> for Bool parameter <%s>", valuestr, param->name);
      errorMessage(s);
      return SCIP_PARSEERROR;
   }
   
   return SCIP_OKAY;
}

/** sets int parameter according to the value of the given string */
static
RETCODE paramParseInt(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure */   
   char*            valuestr            /**< value in string format (may be modified during parse) */
   )
{
   int value;

   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_INT);
   assert(valuestr != NULL);

   if( sscanf(valuestr, "%d", &value) == 1 )
   {
      CHECK_OKAY( paramSetInt(param, scip, value) );
   }
   else
   {
      char s[2*MAXSTRLEN];
      sprintf(s, "invalid parameter value <%s> for int parameter <%s>", valuestr, param->name);
      errorMessage(s);
      return SCIP_PARSEERROR;
   }
   
   return SCIP_OKAY;
}

/** sets Longint parameter according to the value of the given string */
static
RETCODE paramParseLongint(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure */   
   char*            valuestr            /**< value in string format (may be modified during parse) */
   )
{
   Longint value;

   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_LONGINT);
   assert(valuestr != NULL);

   if( sscanf(valuestr, LONGINT_FORMAT, &value) == 1 )
   {
      CHECK_OKAY( paramSetLongint(param, scip, value) );
   }
   else
   {
      char s[2*MAXSTRLEN];
      sprintf(s, "invalid parameter value <%s> for Longint parameter <%s>", valuestr, param->name);
      errorMessage(s);
      return SCIP_PARSEERROR;
   }
   
   return SCIP_OKAY;
}

/** sets Real parameter according to the value of the given string */
static
RETCODE paramParseReal(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure */   
   char*            valuestr            /**< value in string format (may be modified during parse) */
   )
{
   Real value;

   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_REAL);
   assert(valuestr != NULL);

   if( sscanf(valuestr, REAL_FORMAT, &value) == 1 )
   {
      CHECK_OKAY( paramSetReal(param, scip, value) );
   }
   else
   {
      char s[2*MAXSTRLEN];
      sprintf(s, "invalid parameter value <%s> for Real parameter <%s>", valuestr, param->name);
      errorMessage(s);
      return SCIP_PARSEERROR;
   }
   
   return SCIP_OKAY;
}

/** sets Char parameter according to the value of the given string */
static
RETCODE paramParseChar(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure */   
   char*            valuestr            /**< value in string format (may be modified during parse) */
   )
{
   char value;

   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_CHAR);
   assert(valuestr != NULL);

   if( sscanf(valuestr, "%c", &value) == 1 )
   {
      CHECK_OKAY( paramSetChar(param, scip, value) );
   }
   else
   {
      char s[2*MAXSTRLEN];
      sprintf(s, "invalid parameter value <%s> for char parameter <%s>", valuestr, param->name);
      errorMessage(s);
      return SCIP_PARSEERROR;
   }
   
   return SCIP_OKAY;
}

/** sets String parameter according to the value of the given string */
static
RETCODE paramParseString(
   PARAM*           param,              /**< parameter */
   SCIP*            scip,               /**< SCIP data structure */   
   char*            valuestr            /**< value in string format (may be modified during parse) */
   )
{
   unsigned int len;

   assert(param != NULL);
   assert(param->paramtype == SCIP_PARAMTYPE_STRING);
   assert(valuestr != NULL);

   /* check for quotes */
   len = strlen(valuestr);
   if( len <= 1 || valuestr[0] != '"' || valuestr[len-1] != '"' )
   {
      char s[2*MAXSTRLEN];
      sprintf(s, "invalid parameter value <%s> for string parameter <%s> (string has to be in double quotes)",
         valuestr, param->name);
      errorMessage(s);
      return SCIP_PARSEERROR;
   }

   /* remove the quotes */
   valuestr[len-1] = '\0';
   valuestr++;
   CHECK_OKAY( paramSetString(param, scip, valuestr) );
   
   return SCIP_OKAY;
}

/** writes the parameter to a file */
static
RETCODE paramWrite(
   PARAM*           param,              /**< parameter */
   FILE*            file,               /**< file to write parameter to */
   Bool             comments            /**< should parameter descriptions be written as comments? */
   )
{
   assert(param != NULL);
   assert(file != NULL);

   if( comments )
   {
      fprintf(file, "# %s\n", param->desc);
      switch( param->paramtype )
      {
      case SCIP_PARAMTYPE_BOOL:
         fprintf(file, "# [type: bool, range: {TRUE,FALSE}, default: %s]\n",
            param->data.boolparam.defaultvalue ? "TRUE" : "FALSE");
         break;
      case SCIP_PARAMTYPE_INT:
         fprintf(file, "# [type: int, range: [%d,%d], default: %d]\n", 
            param->data.intparam.minvalue, param->data.intparam.maxvalue, param->data.intparam.defaultvalue);
         break;
      case SCIP_PARAMTYPE_LONGINT:
         fprintf(file, "# [type: longint, range: [%lld,%lld], default: %lld]\n", 
            param->data.longintparam.minvalue, param->data.longintparam.maxvalue, param->data.longintparam.defaultvalue);
         break;
      case SCIP_PARAMTYPE_REAL:
         fprintf(file, "# [type: real, range: [%.15g,%.15g], default: %.15g]\n",
            param->data.realparam.minvalue, param->data.realparam.maxvalue, param->data.realparam.defaultvalue);
         break;
      case SCIP_PARAMTYPE_CHAR:
         fprintf(file, "# [type: char, range: {%s}, default: %c]\n",
            param->data.charparam.allowedvalues != NULL ? param->data.charparam.allowedvalues : "all chars",
            param->data.charparam.defaultvalue);
         break;
      case SCIP_PARAMTYPE_STRING:
         fprintf(file, "# [type: string, default: \"%s\"]\n", param->data.stringparam.defaultvalue);
         break;
      default:
         errorMessage("unknown parameter type");
         return SCIP_INVALIDDATA;
      }
   }
   fprintf(file, "%s = ", param->name);
   switch( param->paramtype )
   {
   case SCIP_PARAMTYPE_BOOL:
      fprintf(file, "%s", SCIPparamGetBool(param) ? "TRUE" : "FALSE");
      break;
   case SCIP_PARAMTYPE_INT:
      fprintf(file, "%d", SCIPparamGetInt(param));
      break;
   case SCIP_PARAMTYPE_LONGINT:
      fprintf(file, "%lld", SCIPparamGetLongint(param));
      break;
   case SCIP_PARAMTYPE_REAL:
      fprintf(file, "%.15g", SCIPparamGetReal(param));
      break;
   case SCIP_PARAMTYPE_CHAR:
      fprintf(file, "%c", SCIPparamGetChar(param));
      break;
   case SCIP_PARAMTYPE_STRING:
      fprintf(file, "\"%s\"", SCIPparamGetString(param));
      break;
   default:
      errorMessage("unknown parameter type");
      return SCIP_INVALIDDATA;
   }

   fprintf(file, "\n");

   return SCIP_OKAY;
}




/*
 * Parameter set methods
 */

/** creates parameter set */
RETCODE SCIPparamsetCreate(
   PARAMSET**       paramset            /**< pointer to store the parameter set */
   )
{
   assert(paramset != NULL);

   ALLOC_OKAY( allocMemory(paramset) );

   CHECK_OKAY( SCIPhashtableCreate(&(*paramset)->hashtable, SCIP_HASHSIZE_PARAMS,
                  hashGetKeyParam, SCIPhashKeyEqString, SCIPhashKeyValString) );

   (*paramset)->params = NULL;
   (*paramset)->nparams = 0;
   (*paramset)->paramssize = 0;

   return SCIP_OKAY;
}

/** frees parameter set */
void SCIPparamsetFree(
   PARAMSET**       paramset,           /**< pointer to the parameter set */
   MEMHDR*          memhdr              /**< block memory */
   )
{
   int i;

   assert(paramset != NULL);
   assert(*paramset != NULL);
   assert((*paramset)->paramssize == 0 || (*paramset)->params != NULL);
   assert((*paramset)->paramssize >= (*paramset)->nparams);

   for( i = 0; i < (*paramset)->nparams; ++i )
   {
      paramFree(&(*paramset)->params[i], memhdr);
   }

   SCIPhashtableFree(&(*paramset)->hashtable, memhdr);

   freeMemoryArrayNull(&(*paramset)->params);
   freeMemory(paramset);
}

/** adds parameter to the parameter set */
static
RETCODE paramsetAdd(
   PARAMSET*        paramset,           /**< parameter set */
   MEMHDR*          memhdr,             /**< block memory */
   PARAM*           param               /**< parameter to add */
   )
{
   assert(paramset != NULL);
   assert(param != NULL);

   /* insert the parameter name to the hash table */
   CHECK_OKAY( SCIPhashtableSafeInsert(paramset->hashtable, memhdr, (void*)param) );

   /* ensure, that there is enough space in the params array */
   if( paramset->nparams >= paramset->paramssize )
   {
      paramset->paramssize *= 2;
      paramset->paramssize = MAX(paramset->paramssize, paramset->nparams+1);
      ALLOC_OKAY( reallocMemoryArray(&paramset->params, paramset->paramssize) );
   }
   assert(paramset->nparams < paramset->paramssize);
   
   /* insert parameter in the params array */
   paramset->params[paramset->nparams] = param;
   paramset->nparams++;
   
   return SCIP_OKAY;
}

/** creates a Bool parameter, sets it to its default value, and adds it to the parameter set */
RETCODE SCIPparamsetAddBool(
   PARAMSET*        paramset,           /**< parameter set */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   Bool*            valueptr,           /**< pointer to store the current parameter value, or NULL */
   Bool             defaultvalue,       /**< default value of the parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* create the parameter */
   CHECK_OKAY( paramCreateBool(&param, memhdr, name, desc, valueptr, defaultvalue, paramchgd, paramdata) );

   /* add parameter to the parameter set */
   CHECK_OKAY( paramsetAdd(paramset, memhdr, param) );
   
   return SCIP_OKAY;
}

/** creates a int parameter, sets it to its default value, and adds it to the parameter set */
RETCODE SCIPparamsetAddInt(
   PARAMSET*        paramset,           /**< parameter set */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   int*             valueptr,           /**< pointer to store the current parameter value, or NULL */
   int              defaultvalue,       /**< default value of the parameter */
   int              minvalue,           /**< minimum value for parameter */
   int              maxvalue,           /**< maximum value for parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* create the parameter */
   CHECK_OKAY( paramCreateInt(&param, memhdr, name, desc, valueptr, defaultvalue, minvalue, maxvalue, 
                  paramchgd, paramdata) );

   /* add parameter to the parameter set */
   CHECK_OKAY( paramsetAdd(paramset, memhdr, param) );
   
   return SCIP_OKAY;
}

/** creates a Longint parameter, sets it to its default value, and adds it to the parameter set */
RETCODE SCIPparamsetAddLongint(
   PARAMSET*        paramset,           /**< parameter set */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   Longint*         valueptr,           /**< pointer to store the current parameter value, or NULL */
   Longint          defaultvalue,       /**< default value of the parameter */
   Longint          minvalue,           /**< minimum value for parameter */
   Longint          maxvalue,           /**< maximum value for parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* create the parameter */
   CHECK_OKAY( paramCreateLongint(&param, memhdr, name, desc, valueptr, defaultvalue, minvalue, maxvalue, 
                  paramchgd, paramdata) );

   /* add parameter to the parameter set */
   CHECK_OKAY( paramsetAdd(paramset, memhdr, param) );
   
   return SCIP_OKAY;
}

/** creates a Real parameter, sets it to its default value, and adds it to the parameter set */
RETCODE SCIPparamsetAddReal(
   PARAMSET*        paramset,           /**< parameter set */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   Real*            valueptr,           /**< pointer to store the current parameter value, or NULL */
   Real             defaultvalue,       /**< default value of the parameter */
   Real             minvalue,           /**< minimum value for parameter */
   Real             maxvalue,           /**< maximum value for parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* create the parameter */
   CHECK_OKAY( paramCreateReal(&param, memhdr, name, desc, valueptr, defaultvalue, minvalue, maxvalue, 
                  paramchgd, paramdata) );

   /* add parameter to the parameter set */
   CHECK_OKAY( paramsetAdd(paramset, memhdr, param) );
   
   return SCIP_OKAY;
}

/** creates a char parameter, sets it to its default value, and adds it to the parameter set */
RETCODE SCIPparamsetAddChar(
   PARAMSET*        paramset,           /**< parameter set */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   char*            valueptr,           /**< pointer to store the current parameter value, or NULL */
   char             defaultvalue,       /**< default value of the parameter */
   const char*      allowedvalues,      /**< array with possible parameter values, or NULL if not restricted */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* create the parameter */
   CHECK_OKAY( paramCreateChar(&param, memhdr, name, desc, valueptr, defaultvalue, allowedvalues, paramchgd, paramdata) );

   /* add parameter to the parameter set */
   CHECK_OKAY( paramsetAdd(paramset, memhdr, param) );
   
   return SCIP_OKAY;
}

/** creates a string parameter, sets it to its default value, and adds it to the parameter set */
RETCODE SCIPparamsetAddString(
   PARAMSET*        paramset,           /**< parameter set */
   MEMHDR*          memhdr,             /**< block memory */
   const char*      name,               /**< name of the parameter */
   const char*      desc,               /**< description of the parameter */
   char**           valueptr,           /**< pointer to store the current parameter value, or NULL */
   const char*      defaultvalue,       /**< default value of the parameter */
   DECL_PARAMCHGD   ((*paramchgd)),     /**< change information method of parameter */
   PARAMDATA*       paramdata           /**< locally defined parameter specific data */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* create the parameter */
   CHECK_OKAY( paramCreateString(&param, memhdr, name, desc, valueptr, defaultvalue, paramchgd, paramdata) );

   /* add parameter to the parameter set */
   CHECK_OKAY( paramsetAdd(paramset, memhdr, param) );
   
   return SCIP_OKAY;
}

/** gets the value of an existing Bool parameter */
RETCODE SCIPparamsetGetBool(
   PARAMSET*        paramset,           /**< parameter set */
   const char*      name,               /**< name of the parameter */
   Bool*            value               /**< pointer to store the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);
   assert(value != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_BOOL )
      return SCIP_PARAMETERWRONGTYPE;

   /* get the actual parameter's value */
   *value = SCIPparamGetBool(param);

   return SCIP_OKAY;
}

/** gets the value of an existing int parameter */
RETCODE SCIPparamsetGetInt(
   PARAMSET*        paramset,           /**< parameter set */
   const char*      name,               /**< name of the parameter */
   int*             value               /**< pointer to store the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);
   assert(value != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_INT )
      return SCIP_PARAMETERWRONGTYPE;

   /* get the actual parameter's value */
   *value = SCIPparamGetInt(param);

   return SCIP_OKAY;
}

/** gets the value of an existing Longint parameter */
RETCODE SCIPparamsetGetLongint(
   PARAMSET*        paramset,           /**< parameter set */
   const char*      name,               /**< name of the parameter */
   Longint*         value               /**< pointer to store the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);
   assert(value != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_LONGINT )
      return SCIP_PARAMETERWRONGTYPE;

   /* get the actual parameter's value */
   *value = SCIPparamGetLongint(param);

   return SCIP_OKAY;
}

/** gets the value of an existing Real parameter */
RETCODE SCIPparamsetGetReal(
   PARAMSET*        paramset,           /**< parameter set */
   const char*      name,               /**< name of the parameter */
   Real*            value               /**< pointer to store the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);
   assert(value != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_REAL )
      return SCIP_PARAMETERWRONGTYPE;

   /* get the actual parameter's value */
   *value = SCIPparamGetReal(param);

   return SCIP_OKAY;
}

/** gets the value of an existing char parameter */
RETCODE SCIPparamsetGetChar(
   PARAMSET*        paramset,           /**< parameter set */
   const char*      name,               /**< name of the parameter */
   char*            value               /**< pointer to store the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);
   assert(value != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_CHAR )
      return SCIP_PARAMETERWRONGTYPE;

   /* get the actual parameter's value */
   *value = SCIPparamGetChar(param);

   return SCIP_OKAY;
}

/** gets the value of an existing string parameter */
RETCODE SCIPparamsetGetString(
   PARAMSET*        paramset,           /**< parameter set */
   const char*      name,               /**< name of the parameter */
   char**           value               /**< pointer to store the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);
   assert(value != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_STRING )
      return SCIP_PARAMETERWRONGTYPE;

   /* get the actual parameter's value */
   *value = SCIPparamGetString(param);

   return SCIP_OKAY;
}

/** changes the value of an existing Bool parameter */
RETCODE SCIPparamsetSetBool(
   PARAMSET*        paramset,           /**< parameter set */
   SCIP*            scip,               /**< SCIP data structure */   
   const char*      name,               /**< name of the parameter */
   Bool             value               /**< new value of the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_BOOL )
      return SCIP_PARAMETERWRONGTYPE;

   /* set the actual parameter's value */
   CHECK_OKAY( paramSetBool(param, scip, value) );

   return SCIP_OKAY;
}

/** changes the value of an existing int parameter */
RETCODE SCIPparamsetSetInt(
   PARAMSET*        paramset,           /**< parameter set */
   SCIP*            scip,               /**< SCIP data structure */   
   const char*      name,               /**< name of the parameter */
   int              value               /**< new value of the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_INT )
      return SCIP_PARAMETERWRONGTYPE;

   /* set the actual parameter's value */
   CHECK_OKAY( paramSetInt(param, scip, value) );

   return SCIP_OKAY;
}

/** changes the value of an existing Longint parameter */
RETCODE SCIPparamsetSetLongint(
   PARAMSET*        paramset,           /**< parameter set */
   SCIP*            scip,               /**< SCIP data structure */   
   const char*      name,               /**< name of the parameter */
   Longint          value               /**< new value of the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_LONGINT )
      return SCIP_PARAMETERWRONGTYPE;

   /* set the actual parameter's value */
   CHECK_OKAY( paramSetLongint(param, scip, value) );

   return SCIP_OKAY;
}

/** changes the value of an existing Real parameter */
RETCODE SCIPparamsetSetReal(
   PARAMSET*        paramset,           /**< parameter set */
   SCIP*            scip,               /**< SCIP data structure */   
   const char*      name,               /**< name of the parameter */
   Real             value               /**< new value of the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_REAL )
      return SCIP_PARAMETERWRONGTYPE;

   /* set the actual parameter's value */
   CHECK_OKAY( paramSetReal(param, scip, value) );

   return SCIP_OKAY;
}

/** changes the value of an existing char parameter */
RETCODE SCIPparamsetSetChar(
   PARAMSET*        paramset,           /**< parameter set */
   SCIP*            scip,               /**< SCIP data structure */   
   const char*      name,               /**< name of the parameter */
   char             value               /**< new value of the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_CHAR )
      return SCIP_PARAMETERWRONGTYPE;

   /* set the actual parameter's value */
   CHECK_OKAY( paramSetChar(param, scip, value) );

   return SCIP_OKAY;
}

/** changes the value of an existing string parameter */
RETCODE SCIPparamsetSetString(
   PARAMSET*        paramset,           /**< parameter set */
   SCIP*            scip,               /**< SCIP data structure */   
   const char*      name,               /**< name of the parameter */
   const char*      value               /**< new value of the parameter */
   )
{
   PARAM* param;

   assert(paramset != NULL);

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)name);
   if( param == NULL )
      return SCIP_PARAMETERUNKNOWN;
   if( param->paramtype != SCIP_PARAMTYPE_STRING )
      return SCIP_PARAMETERWRONGTYPE;

   /* set the actual parameter's value */
   CHECK_OKAY( paramSetString(param, scip, value) );

   return SCIP_OKAY;
}

/** parses a parameter file line "paramname = paramvalue" and sets parameter accordingly */
static
RETCODE paramsetParse(
   PARAMSET*        paramset,           /**< parameter set */
   SCIP*            scip,               /**< SCIP data structure */   
   char*            line                /**< line to parse (is modified during parse, but not freed) */
   )
{
   PARAM* param;
   char* paramname;
   char* paramvaluestr;
   char* lastquote;
   Bool quoted;

   assert(paramset != NULL);
   assert(line != NULL);

   paramname = NULL;
   paramvaluestr = NULL;

   /* find the start of the parameter name */
   while( *line == ' ' || *line == '\t' || *line == '\r' )
      line++;
   if( *line == '\0' || *line == '\n' || *line == '#' )
      return SCIP_OKAY;
   paramname = line;

   /* find the end of the parameter name */
   while( *line != ' ' && *line != '\t' && *line != '\r' && *line != '\n' && *line != '#' && *line != '\0' && *line != '=' )
      line++;
   if( *line == '=' )
   {
      *line = '\0';
      line++;
   }
   else
   {
      *line = '\0';
      line++;

      /* search for the '=' char in the line */
      while( *line == ' ' || *line == '\t' || *line == '\r' )
         line++;
      if( *line != '=' )
      {
         errorMessage("character '=' was expected after the parameter name");
         return SCIP_PARSEERROR;
      }
      line++;
   }

   /* find the start of the parameter value string */
   while( *line == ' ' || *line == '\t' || *line == '\r' )
      line++;
   if( *line == '\0' || *line == '\n' || *line == '#' )
   {
      errorMessage("parameter value is missing");
      return SCIP_PARSEERROR;
   }
   paramvaluestr = line;

   /* find the end of the parameter value string */
   quoted = (*paramvaluestr == '"');
   lastquote = NULL;
   while( (quoted || (*line != ' ' && *line != '\t' && *line != '\r' && *line != '\n' && *line != '#')) && *line != '\0' )
   {
      if( *line == '"' )
         lastquote = line;
      line++;
   }
   if( lastquote != NULL )
      line = lastquote+1;
   if( *line == '#' )
      *line = '\0';
   else if( *line != '\0' )
   {
      /* check, if the rest of the line is clean */
      *line = '\0';
      line++;
      while( *line == ' ' || *line == '\t' || *line == '\r' )
         line++;
      if( *line != '\0' && *line != '\n' && *line != '#' )
      {
         errorMessage("additional characters after parameter value");
         return SCIP_PARSEERROR;
      }
   }

   /* retrieve parameter from hash table */
   param = SCIPhashtableRetrieve(paramset->hashtable, (void*)paramname);
   if( param == NULL )
   {
      char s[2*MAXSTRLEN];
      sprintf(s, "unknown parameter <%s>", paramname);
      warningMessage(s);
      return SCIP_OKAY;
   }

   /* set parameter's value */
   switch( param->paramtype )
   {
   case SCIP_PARAMTYPE_BOOL:
      CHECK_OKAY( paramParseBool(param, scip, paramvaluestr) );
      break;
   case SCIP_PARAMTYPE_INT:
      CHECK_OKAY( paramParseInt(param, scip, paramvaluestr) );
      break;
   case SCIP_PARAMTYPE_LONGINT:
      CHECK_OKAY( paramParseLongint(param, scip, paramvaluestr) );
      break;
   case SCIP_PARAMTYPE_REAL:
      CHECK_OKAY( paramParseReal(param, scip, paramvaluestr) );
      break;
   case SCIP_PARAMTYPE_CHAR:
      CHECK_OKAY( paramParseChar(param, scip, paramvaluestr) );
      break;
   case SCIP_PARAMTYPE_STRING:
      CHECK_OKAY( paramParseString(param, scip, paramvaluestr) );
      break;
   default:
      errorMessage("unknown parameter type");
      return SCIP_INVALIDDATA;
   }
   
   return SCIP_OKAY;
}

/** reads parameters from a file */
RETCODE SCIPparamsetRead(
   PARAMSET*        paramset,           /**< parameter set */
   SCIP*            scip,               /**< SCIP data structure */   
   const char*      filename            /**< file name */
   )
{
   RETCODE retcode;
   FILE* file;
   char line[1024];
   char s[MAXSTRLEN];
   int lineno;

   assert(paramset != NULL);
   assert(filename != NULL);

   /* open the file for reading */
   file = fopen(filename, "r");
   if( file == NULL )
   {
      sprintf(s, "cannot open file <%s> for reading", filename);
      errorMessage(s);
      perror(filename);
      return SCIP_NOFILE;
   }

   /* read the parameters from the file */
   lineno = 0;
   while( fgets(line, sizeof(line), file) != NULL )
   {
      lineno++;
      retcode = paramsetParse(paramset, scip, line);
      if( retcode == SCIP_PARSEERROR )
      {
         sprintf(s, "input error in file <%s> line %d", filename, lineno);
         errorMessage(s);
      }
      CHECK_OKAY( retcode );
   }

   /* close input file */
   if( filename != NULL )
      fclose(file);

   return SCIP_OKAY;
}

/** writes all parameters in the parameter set to a file */
RETCODE SCIPparamsetWrite(
   PARAMSET*        paramset,           /**< parameter set */
   const char*      filename,           /**< file name, or NULL for stdout */
   Bool             comments            /**< should parameter descriptions be written as comments? */
   )
{
   FILE* file;
   int i;

   assert(paramset != NULL);

   /* open the file for writing */
   if( filename != NULL )
   {
      file = fopen(filename, "w");
      if( file == NULL )
      {
         char s[MAXSTRLEN];
         sprintf(s, "cannot open file <%s> for writing", filename);
         errorMessage(s);
         perror(filename);
         return SCIP_FILECREATEERROR;
      }
   }
   else
      file = stdout;

   /* write the parameters to the file */
   for( i = 0; i < paramset->nparams; ++i )
   {
      CHECK_OKAY( paramWrite(paramset->params[i], file, comments) );
      if( comments )
         fprintf(file, "\n");
   }

   /* close output file */
   if( filename != NULL )
      fclose(file);

   return SCIP_OKAY;
}

