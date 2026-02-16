/****************************************************************************************/
/*  bsp.c - Core BSP functions                                                         */
/*  Converted from Genesis3D BSP.CPP to C99/raylib                                      */
/****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "bsp.h"
#include "mathlib.h"
#include "brush2.h"

// BspParms structure
typedef struct BspParms
{
    bool Verbose;
    bool EntityVerbose;
} BspParms;

// External declarations
extern bool gCountVerts;
extern int32_t NumLeafSides;
extern int32_t NumLeafClusters;
extern int32_t NumLeafBevels;
extern void *GFXEntData;
extern int32_t NumGFXEntData;

// Stub function declarations
extern bool LoadBrushFile(const char *filename);
extern void BeginGBSPModels(void);
extern bool ProcessEntities(void);
extern void FreeAllEntities(void);
extern bool LoadGBSPFile(const char *filename);
extern bool SaveGBSPFile(const char *filename);
extern bool ConvertEntitiesToGFXEntData(void);
extern void FreeGBSPFile(void);
extern void FreeFace(GBSP_Face *face);
extern bool FreePortals(GBSP_Node *node);
extern void FreeBSP_r(GBSP_Node *Node);

/****************************************************************************************/
/*  BSP.cpp                                                                             */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Module distributes code to all the other modules                       */
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
/*Genesis3D Version 1.1 released November 15, 1999                            */
/*  Copyright (C) 1999 WildTangent, Inc. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/



// Globals
GBSP_Model	BSPModels[MAX_BSP_MODELS];
int32_t		NumBSPModels;

bool	Verbose = true;
bool	OriginalVerbose;
bool	EntityVerbose = false;

//
// BSP2.cpp defs
//
bool	ProcessEntities(void);				
void		FreeBSP_r(GBSP_Node *Node);	

//=======================================================================================
//	InsertModelNumbers
//=======================================================================================
bool InsertModelNumbers(void)
{
	int32_t	i, NumModels;
	char	ModelNum[128];

	NumModels = 0;

	for (i=0; i< NumEntities; i++)
	{
		if (CancelRequest)
			return false;
		
		if (!Entities[i].Brushes2)		// No model if no brushes
			continue;
		
		Entities[i].ModelNum = NumModels;

		if (i != 0)
		{
			sprintf(ModelNum, "%i", NumModels);
			SetKeyValue(&Entities[i], "Model", ModelNum);
		}
		NumModels++;
	}
	return true;
}

//========================================================================================
//	CreateBSP
//========================================================================================
bool CreateBSP(char *FileName, BspParms *Parms)
{
	OriginalVerbose = Verbose = Parms->Verbose;
	EntityVerbose = Parms->EntityVerbose;
		
	gCountVerts = true;

	NumLeafSides = 0;
	NumLeafClusters = 0;
	NumLeafBevels = 0;
	NumPlanes = 0;

	if (!LoadBrushFile(FileName))
	{
		FreeAllEntities();
		return false;
	}
	
	InsertModelNumbers();

	BeginGBSPModels();
	
	if (!ProcessEntities())
	{
		FreeAllGBSPData();
		FreeAllEntities();
		return false;
	}

	return true;
}

//========================================================================================
//	UpdateEntities
//	Updates the entities only...
//========================================================================================
bool UpdateEntities(char *MapName, char *BSPName)
{
	printf("--- Update Entities --- \n");

	if (!LoadBrushFile(MapName))
		goto ExitWithError;

	InsertModelNumbers();

	// Load the old .BSP
	if (!LoadGBSPFile(BSPName))
	{
		fprintf(stderr, "ERROR: UpdateEntities:  Could not load .bsp file.\n");
		goto ExitWithError;
	}

	// Destroy any old GFXEntData in the .BSP file
	if (GFXEntData)
	{
		free(GFXEntData); GFXEntData = NULL;
		GFXEntData = NULL;
	}
	NumGFXEntData = 0;

	if (!ConvertEntitiesToGFXEntData())
	{
		fprintf(stderr, "ERROR: UpdateEntities:  ConvertEntitiesToGFXEntData failed.\n");
		return false;
	}

	// Save it!!!
	if (!SaveGBSPFile(BSPName))
	{
		fprintf(stderr, "ERROR: UpdateEntities:  SaveGBSPFile failed.\n");
		goto ExitWithError;
	}

	FreeAllEntities();					// Free any entities that might be left behind
	FreeGBSPFile();						// Free any GFX BSP data left over
	return true;
	
	ExitWithError:
		FreeAllEntities();				// Free any entities that might be left behind
		FreeGBSPFile();					// Free any GFX BSP data left over
		return false;
}

//========================================================================================
//	AllocNode
//========================================================================================
GBSP_Node *AllocNode(void)
{
	GBSP_Node	*Node;

	Node = malloc(sizeof(GBSP_Node));

	if (Node == NULL)
	{
		fprintf(stderr, "ERROR: AllocNode:  Out of memory!\n");
		return NULL;
	}

	memset(Node, 0, sizeof(GBSP_Node));

	return Node;
}

//========================================================================================
//	FreeNode
//========================================================================================
void FreeNode(GBSP_Node *Node)
{
	GBSP_Face	*Face, *Next;
	GBSP_Brush	*Brush, *NextB;

	if (Node->LeafFaces)
		free(Node->LeafFaces); Node->LeafFaces = NULL;

	for (Face = Node->Faces; Face; Face = Next)
	{
		Next = Face->Next;
		FreeFace(Face);
	}

	for (Brush = Node->BrushList; Brush; Brush = NextB)
	{
		NextB = Brush->Next;

		FreeBrush(Brush);
	}

	free(Node); Node = NULL;
}

//========================================================================================
//	FreeAllGBSPData
//========================================================================================
bool FreeAllGBSPData(void)
{
	int32_t	i;

	for (i=0; i< NumBSPModels; i++)
	{
		if (!FreePortals(BSPModels[i].RootNode[0]))
		{
			fprintf(stderr, "ERROR: FreeAllGBSPData:  Could not free portals.\n");
			return false;
		}
		FreeBSP_r(BSPModels[i].RootNode[0]);
		
		memset(&BSPModels[i], 0, sizeof(GBSP_Model));
	}

	return true;
}

//========================================================================================
//	CleanupGBSP
//========================================================================================
void CleanupGBSP(void)
{
	FreeAllGBSPData();
	FreeAllEntities();
}

//
//	Planes
//

GBSP_Plane	Planes[MAX_BSP_PLANES];
int32_t		NumPlanes = 0;

//====================================================================================
//	PlaneFromVerts
//	Expects at least 3 verts
//====================================================================================
void PlaneFromVerts(Vector3 *Verts, GBSP_Plane *Plane)
{
	Vector3		Vect1, Vect2;
	
	Vect1 = Vector3Subtract(Verts[0], Verts[1]);
	Vect2 = Vector3Subtract(Verts[2], Verts[1]);
	
	Plane->Normal = Vector3CrossProduct(Vect1, Vect2);
	Plane->Normal = Vector3Normalize(Plane->Normal);

	Plane->Dist = Vector3DotProduct(Verts[0], Plane->Normal);

	Plane->Type = geVec3d_PlaneType(&Plane->Normal);
}

//====================================================================================
//	SidePlane
//====================================================================================
void SidePlane(GBSP_Plane *Plane, int32_t *Side)
{
	int32_t		Type;

	*Side = 0;					// Default to same direction

	Plane->Type = geVec3d_PlaneType(&Plane->Normal);

	Type = Plane->Type % PLANE_ANYX;

	// Point the major axis in the positive direction
	if (VectorToSUB(Plane->Normal, Type) < 0)
	{
		Plane->Normal = Vector3Negate(Plane->Normal);
		Plane->Dist = -Plane->Dist;
		*Side = 1;									// Flip if direction is opposite match
	}
}

#define NORMAL_EPSILON		(float)0.00001

//====================================================================================
//	PlaneEqual
//====================================================================================
bool PlaneEqual(GBSP_Plane *Plane1, GBSP_Plane *Plane2)
{
	if (fabs(Plane1->Normal.x - Plane2->Normal.x) < NORMAL_EPSILON && 
		fabs(Plane1->Normal.y - Plane2->Normal.y) < NORMAL_EPSILON && 
		fabs(Plane1->Normal.z - Plane2->Normal.z) < NORMAL_EPSILON && 
		fabs(Plane1->Dist - Plane2->Dist) < DIST_EPSILON )
			return true;

	return false;
}

//====================================================================================
//	SnapVector
//====================================================================================
void SnapVector(Vector3 *Normal)
{
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if ( fabs(VectorToSUB(*Normal,i) - (float)1) < ANGLE_EPSILON )
		{
			geVec3d_Clear(Normal);
			VectorToSUB(*Normal,i) = (float)1;
			break;
		}

		if ( fabs(VectorToSUB(*Normal,i) - (float)-1) < ANGLE_EPSILON )
		{
			geVec3d_Clear(Normal);
			VectorToSUB(*Normal,i) = (float)-1;
			break;
		}
	}
}

//====================================================================================
//	RoundInt
//====================================================================================
float RoundInt(float In)
{
	return (float)floor(In + (float)0.5);
}

//====================================================================================
//	SnapPlane
//====================================================================================
void SnapPlane(Vector3 *Normal, float *Dist)
{
	SnapVector (Normal);

	if (fabs(*Dist-RoundInt(*Dist)) < DIST_EPSILON)
		*Dist = RoundInt(*Dist);
}


//=======================================================================================
//	PlaneInverse
//=======================================================================================
void PlaneInverse(GBSP_Plane *Plane)
{
	Plane->Normal = Vector3Negate(Plane->Normal);
	Plane->Dist = -Plane->Dist;

}

//====================================================================================
//	FindPlane
//====================================================================================
int32_t FindPlane(GBSP_Plane *Plane, int32_t *Side)
{
	GBSP_Plane	Plane1;
	GBSP_Plane	*Plane2;
	//float		Dot;
	int32_t		i;

	SnapPlane(&Plane->Normal, &Plane->Dist);

	Plane1 = *Plane;

	SidePlane(&Plane1, Side);		// Find axis, and flip if necessary, to make major axis positive

	Plane2 = Planes;	

	for (i=0; i< NumPlanes; i++)	// Try to return a plane allready in the list
	{
		if (PlaneEqual(&Plane1, Plane2))
			return i;

		Plane2++;
	}

	if (NumPlanes >= MAX_BSP_PLANES)
	{
		fprintf(stderr, "ERROR: Max BSP Planes.\n");
		return -1;
	}
	
	Planes[NumPlanes++] = Plane1;

	return i;
}
