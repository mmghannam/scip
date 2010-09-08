/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2010 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License.             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: objcloneable.h,v 1.3 2010/09/08 19:14:52 bzfhende Exp $"

/**@file   objcloneable.h
 * @author Michael Winkler
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_OBJCLONEABLE_H__
#define __SCIP_OBJCLONEABLE_H__

#include "scip/def.h"
#include "scip/scip.h"

/** all C++ wrapper object plugins should extend this class */
namespace scip
{
   struct ObjCloneable {
      virtual ~ObjCloneable() {}
      virtual ObjCloneable* clone(SCIP* scip, SCIP_Bool* valid) const { return 0; }
      virtual SCIP_Bool iscloneable() const { return false; }
   };
}

#endif
