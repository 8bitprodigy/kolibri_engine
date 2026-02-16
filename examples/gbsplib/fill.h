/****************************************************************************************/
/*  Fill.h                                                                              */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Does the flood filling of the leafs, and removes untouchable leafs     */
/*                                                                                      */
/*  The contents of this file are subject to the Genesis3D Public License               */
/*  Version 1.01 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.genesis3d.com                                                            */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Genesis3D, released March 25, 1999.                            */
/*  Genesis3D Version 1.1 released November 15, 1999                                 */
/*  Copyright (C) 1999 WildTangent, Inc. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/
#ifndef FILL_H
#define FILL_H


#include "bsp.h"

int32_t	RemoveHiddenLeafs(GBSP_Node *RootNode, GBSP_Node *ONode);

#endif
