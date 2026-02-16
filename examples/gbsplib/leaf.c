/****************************************************************************************/
/*  leaf.c - Leaf operations                                                           */
/*  Converted from Genesis3D Leaf.cpp to C99/raylib                                    */
/****************************************************************************************/
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <raylib.h>

#include "bsp.h"
#include "leaf.h"
#include "mathlib.h"
#include "portals.h"
#include "brush2.h"
#include <raymath.h>


#define MAX_LEAF_SIDES 65536
#define MAX_AREAS 256

// External declarations
extern int32_t         NumGFXAreas;
extern int32_t         NumGFXAreaPortals;
extern GFX_Area       *GFXAreas;          // GFX_Area array
extern GFX_AreaPortal *GFXAreaPortals;    // GFX_AreaPortal array


/****************************************************************************************/
/*  Leaf.cpp                                                                            */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Creates leaf collision hulls, areas, etc...                            */
/*                                                                                      */
/****************************************************************************************/



int32_t		 NumLeafClusters;

//======================================================================================
//	FillLeafClusters_r
//======================================================================================
bool FillLeafClusters_r(GBSP_Node *Node, int32_t Cluster)
{
	if (Node->PlaneNum == PLANENUM_LEAF)
	{
		if ((Node->Contents & BSP_CONTENTS_SOLID2))
			Node->Cluster = -1;
		else
			Node->Cluster = Cluster;

		return true;
	}
	
	Node->Cluster = Cluster;

	FillLeafClusters_r(Node->Children[0], Cluster);
	FillLeafClusters_r(Node->Children[1], Cluster);
	
	return true;	
}

//======================================================================================
//	FindLeafClusters_r
//======================================================================================
bool CreateLeafClusters_r(GBSP_Node *Node)
{
	if (Node->PlaneNum != PLANENUM_LEAF && !Node->Detail)
	{
		CreateLeafClusters_r(Node->Children[0]);
		CreateLeafClusters_r(Node->Children[1]);
		return true;
	}
	
	// Either a leaf or detail node
	if (Node->Contents & BSP_CONTENTS_SOLID2)
	{
		Node->Cluster = -1;
		return true;
	}
	
	if (!FillLeafClusters_r(Node, NumLeafClusters))
		return false;

	NumLeafClusters++;

	return true;
}

//======================================================================================
//	CreateLeafClusters
//======================================================================================
bool CreateLeafClusters(GBSP_Node *RootNode)
{
	printf(" --- CreateLeafClusters --- \n");

	if (!CreateLeafClusters_r(RootNode))
	{
		fprintf(stderr, "ERROR: CreateLeafClusters:  Failed to find leaf clusters.\n");
		return false;
	}

	if (Verbose)
		printf("Num Clusters       : %5i\n", NumLeafClusters);

	return true;
}

//======================================================================================
//	Leaf Sides (Used for expandable collision HULL)
//======================================================================================

int32_t NumLeafSides;
int32_t NumLeafBevels;

GBSP_LeafSide LeafSides[MAX_LEAF_SIDES];

#define MAX_TEMP_LEAF_SIDES	100

int32_t CNumLeafSides;
int32_t LPlaneNumbers[MAX_TEMP_LEAF_SIDES];
int32_t	LPlaneSides[MAX_TEMP_LEAF_SIDES];

//====================================================================================
//	FinishLeafSides
//====================================================================================
bool FinishLeafSides(GBSP_Node *Node)
{
	Vector3		Mins, Maxs;
	GBSP_Plane	Plane;
	int32_t		Axis, i, Dir;

	if (!GetLeafBBoxFromPortals(Node, &Mins, &Maxs))
	{
		fprintf(stderr, "ERROR: FinishLeafSides:  Could not get leaf portal BBox.\n");
		return false;
	}
	
	if (CNumLeafSides < 4)
		printf("*WARNING*  FinishLeafSides:  Incomplete leaf volume.\n");

	// Add any bevel planes to the sides so we can expand them for axial box collisions
	else for (Axis=0 ; Axis <3 ; Axis++)
	{
		for (Dir=-1 ; Dir <= 1 ; Dir+=2)
		{
			// See if the plane is allready in the sides
			for (i=0; i< CNumLeafSides; i++)
			{
				Plane = Planes[LPlaneNumbers[i]];
				if (LPlaneSides[i])
				{
					Plane.Normal = Vector3Negate(Plane.Normal);
					Plane.Dist = -Plane.Dist;
				}

				if (VectorToSUB(Plane.Normal, Axis) == (float)Dir)
					break;
			}

			if (i >= CNumLeafSides)
			{	
				// Add a new axial aligned side

				Plane.Normal = Vector3Zero();

				VectorToSUB(Plane.Normal, Axis) = (float)Dir;
				
				// get the mins/maxs from the gbsp brush
				if (Dir == 1)
					Plane.Dist = VectorToSUB(Maxs, Axis);
				else
					Plane.Dist = -VectorToSUB(Mins, Axis);
				
				LPlaneNumbers[i] = FindPlane(&Plane, &LPlaneSides[i]);

				if (LPlaneNumbers[i] == -1)
				{
					fprintf(stderr, "ERROR: FinishLeafSides:  Could not create the plane.\n");
					return false;
				}

				CNumLeafSides++;
				
				NumLeafBevels++;
			}
		}
	}		
	
	Node->FirstSide = NumLeafSides;
	Node->NumSides = CNumLeafSides;
	
	for (i=0; i< CNumLeafSides; i++)
	{
		if (NumLeafSides >= MAX_LEAF_SIDES)
		{
			fprintf(stderr, "ERROR: FinishLeafSides:  Max Leaf Sides.\n");
			return false;
		}
		
		LeafSides[NumLeafSides].PlaneNum = LPlaneNumbers[i];
		LeafSides[NumLeafSides].PlaneSide = LPlaneSides[i];

		NumLeafSides++;
	}

	return true;
}
		
//====================================================================================
//	CreateLeafSides_r
//====================================================================================
bool CreateLeafSides_r(GBSP_Node *Node)
{
	GBSP_Portal	*Portal, *Next;
	int32_t		Side, i;

	Node->FirstSide = -1;
	Node->NumSides = 0;

	if (Node->PlaneNum == PLANENUM_LEAF)	// At a leaf, convert portals to leaf sides...
	{
		if (!(Node->Contents & BSP_CONTENTS_SOLID_CLIP))		// Don't convert empty leafs
			return true;				

		if (!Node->Portals)
		{
			printf("*WARNING* CreateLeafSides:  Contents leaf with no portals!\n");
			return true;
		}

		// Reset number of sides for this solid leaf (should we save out other contents?)
		//	(this is just for a collision hull for now...)
		CNumLeafSides = 0;

		for (Portal = Node->Portals; Portal; Portal = Next)
		{
			Side = (Portal->Nodes[0] == Node);
			Next = Portal->Next[!Side];

			for (i=0; i< CNumLeafSides; i++)
			{
				if (LPlaneNumbers[i] == Portal->PlaneNum && LPlaneSides[i] == Side)
					break;
			}

			// Make sure we don't duplicate planes (this happens with portals)
			if (i >= MAX_TEMP_LEAF_SIDES)
			{
				fprintf(stderr, "ERROR: CreateLeafSides_r:  Max portal leaf sides.\n");
				return false;
			}

			if (i >= CNumLeafSides)
			{
				LPlaneNumbers[i] = Portal->PlaneNum;
				LPlaneSides[i] = Side;
				CNumLeafSides++;
			}
			
		}
		
		if (!FinishLeafSides(Node))
		{
			return false;
		}

		return true;
	}

	if (!CreateLeafSides_r(Node->Children[0]))
		return false;

	if (!CreateLeafSides_r(Node->Children[1]))
		return false;

	return true;
}

//====================================================================================
//	CreateLeafSides
//	Creates leaf sides with bevel edges for axial bounding box clipping hull
//====================================================================================
bool CreateLeafSides(GBSP_Node *RootNode)
{
	if (Verbose)
		printf(" --- Create Leaf Sides --- \n");
	
	if (!CreateLeafSides_r(RootNode))
		return false;

	if (Verbose)
	{
		printf("Num Leaf Sides       : %5i\n", NumLeafSides);
		printf("Num Leaf Bevels      : %5i\n", NumLeafBevels);
	}

	return true;
}

//=====================================================================================
//			 *** AREA LEAFS ***
//=====================================================================================

#define	MAX_AREAS			256
#define	MAX_AREA_PORTALS	1024

//=====================================================================================
//	ModelForLeafNode
//=====================================================================================
GBSP_Model *ModelForLeafNode(GBSP_Node *Node)
{
	GBSP_Brush		*Brush;

	if (Node->PlaneNum != PLANENUM_LEAF)
	{
		fprintf(stderr, "ERROR: ModelForLeafNode:  Node not a leaf!\n");
		return NULL;
	}

	for (Brush = Node->BrushList; Brush; Brush = Brush->Next)
	{
		if (Brush->Original->EntityNum)
			break;
	}

	if (!Brush)
		return NULL;

	return &BSPModels[Entities[Brush->Original->EntityNum].ModelNum];
}

//=====================================================================================
//	FillAreas_r
//=====================================================================================
bool FillAreas_r(GBSP_Node *Node, int32_t Area)
{
	GBSP_Portal		*Portal;
	int32_t			Side;

	if ((Node->Contents & BSP_CONTENTS_SOLID2))
		return true;			// Stop at solid leafs

	if ((Node->Contents & BSP_CONTENTS_AREA2))
	{
		GBSP_Model		*Model;

		Model = ModelForLeafNode(Node);
		
		if (!Model)
		{
			fprintf(stderr, "ERROR: FillAreas_r:  No model for leaf.\n");
			return false;
		}

		if (Model->Areas[0] == Area || Model->Areas[1] == Area)
			return true;	// Already flooded into this portal from this area

		if (!Model->Areas[0])
			Model->Areas[0] = Area;
		else if (!Model->Areas[1])
			Model->Areas[1] = Area;
		else
			printf("*WARNING* FillAreas_r:  Area Portal touched more than 2 areas.\n");

		return true;		
	}

	if (Node->Area != 0)		// Already set
		return true;

	// Mark it
	Node->Area = Area;

	// Flood through all of this leafs portals
	for (Portal = Node->Portals; Portal; Portal = Portal->Next[Side])
	{
		Side = (Portal->Nodes[1] == Node);
		
		if (!FillAreas_r(Portal->Nodes[!Side], Area))
			return false;
	}

	return true;
}

//=====================================================================================
//	CreateAreas_r
//=====================================================================================
bool CreateAreas_r(GBSP_Node *Node)
{
	if (Node->PlaneNum == PLANENUM_LEAF)
	{
		if (Node->Contents & BSP_CONTENTS_SOLID2)	// Stop at solid
			return true;

		if (Node->Contents & BSP_CONTENTS_AREA2)	// Don't start at area portals
			return true;

		if (Node->Area != 0)						// Already set
			return true;

		if (NumGFXAreas >= MAX_AREAS)
		{
			fprintf(stderr, "ERROR: CreateAreas_r:  Max Areas.\n");
			return false;
		}

		// Once we find a normal leaf, flood out marking the current area
		// stopping at other areas leafs, and solid leafs (unpassable leafs)
		if (!FillAreas_r(Node, NumGFXAreas++))
			return false;

		return true;
	}

	if (!CreateAreas_r(Node->Children[0]))
		return false;
	if (!CreateAreas_r(Node->Children[1]))
		return false;

	return true;
}

//=====================================================================================
//	FinishAreaPortals_r
//=====================================================================================
bool FinishAreaPortals_r(GBSP_Node *Node)
{
	GBSP_Model		*Model;

	if (Node->PlaneNum != PLANENUM_LEAF)
	{
		if (!FinishAreaPortals_r(Node->Children[0]))
			return false;
		if (!FinishAreaPortals_r(Node->Children[1]))
			return false;
	}

	if (!(Node->Contents & BSP_CONTENTS_AREA2))
		return true;		// Only interested in area portals

	if (Node->Area)
		return true;		// Already set...

	Model = ModelForLeafNode(Node);

	if (!Model)
	{
		fprintf(stderr, "ERROR: FinishAreaPortals_r:  No model for leaf.\n");
		return false;
	}

	Node->Area = Model->Areas[0];		// Set to first area that flooded into portal
	Model->IsAreaPortal = true;
	
	return true;
}

//=====================================================================================
//	FinishAreas
//=====================================================================================
bool FinishAreas(void)
{
	int32_t		i;
	GFX_Area	*a;
	GBSP_Model	*pModel;
	int32_t		m;

	// First, go through and print out all errors pertaining to model areas
	for (pModel = BSPModels+1, m=1; m < NumBSPModels; m++, pModel++)	// Skip world model
	{
		if (!pModel->IsAreaPortal)
			continue;		// Not an area portal model

		if (!pModel->Areas[0])
			printf("*WARNING* FinishAreas:  AreaPortal did not touch any areas!\n");
		else if (!pModel->Areas[1])
			printf("*WARNING* FinishAreas:  AreaPortal only touched one area.\n");
	}

	NumGFXAreaPortals = 0;		// Clear the Portals var

	// Area 0 is the invalid area, set it here, and skip it in the loop below
	GFXAreas[0].FirstAreaPortal = 0;
	GFXAreas[0].NumAreaPortals = 0;

	for (a = GFXAreas+1, i = 1; i < NumGFXAreas; i++, a++)		// Skip invalid area (area 0)
	{
		a->FirstAreaPortal = NumGFXAreaPortals;

		// Go through all the area portals (models), and create all portals that belong to this area
		for (pModel = BSPModels+1, m=1; m < NumBSPModels; m++, pModel++)	// Skip world model
		{
			int32_t			a0, a1;
			GFX_AreaPortal	*p;

			a0 = pModel->Areas[0];
			a1 = pModel->Areas[1];

			if (!a0 || !a1)
				continue;				// Model didn't seperate 2 areas

			if (a0 == a1)				// Portal seperates same area
				continue;

			if (a0 != i && a1 != i)
				continue;				// Portal is not a part of this area

			if (NumGFXAreaPortals >= MAX_AREA_PORTALS)
			{
				fprintf(stderr, "ERROR: FinishAreas:  Max area portals.\n");
				return false;		// Oops
			}
			
			p = &GFXAreaPortals[NumGFXAreaPortals];

			if (a0 == i)		// Grab the area on the opposite side of the portal
				p->Area = a1;
			else if (a1 == i)
				p->Area = a0;

			p->ModelNum = m;	// Set the portals model number
			NumGFXAreaPortals++;
		}
		a->NumAreaPortals = NumGFXAreaPortals - a->FirstAreaPortal;
	}

	return true;
}

//=====================================================================================
//	CreateAreas
//=====================================================================================
bool CreateAreas(GBSP_Node *RootNode)
{
	GBSP_Model	*pModel;
	int32_t		m;

	printf(" --- Create Area Leafs --- \n");

	// Clear all model area info
	for (pModel = BSPModels+1, m=1; m < NumBSPModels; m++, pModel++)	// Skip world model
	{
		pModel->Areas[0] = pModel->Areas[1] = 0;
		pModel->IsAreaPortal = false;
	}

	if (GFXAreas)
		free(GFXAreas); GFXAreas = NULL;
	if (GFXAreaPortals)
		free(GFXAreaPortals); GFXAreaPortals = NULL;

	GFXAreas = malloc(sizeof(GFX_Area) *  MAX_AREAS);

	if (!GFXAreas)
		return false;

	GFXAreaPortals = malloc(sizeof(GFX_AreaPortal) *  MAX_AREA_PORTALS);

	if (!GFXAreaPortals)
		return false;
	
	NumGFXAreas = 1;			// 0 is invalid 
	NumGFXAreaPortals = 0;

	if (!CreateAreas_r(RootNode))
	{
		fprintf(stderr, "ERROR: Could not create model areas.\n");
		return false;
	}

	if (!FinishAreaPortals_r(RootNode))
	{
		fprintf(stderr, "ERROR: CreateAreas: FinishAreaPortals_r failed.\n");
		return false;
	}

	if (!FinishAreas())
	{
		fprintf(stderr, "ERROR: Could not finalize model areas.\n");
		return false;
	}

	//printf("Num Areas            : %5i\n", NumGFXAreas-1);

	return true;
}

//=====================================================================================
//	FindLeaf
//=====================================================================================
GBSP_Node *FindLeaf(GBSP_Node *Node, Vector3 *Origin)
{
	GBSP_Plane	*Plane;
	float		Dist;

	while(Node && Node->PlaneNum != PLANENUM_LEAF)
	{
		Plane = &Planes[Node->PlaneNum];
		Dist = Vector3DotProduct(*Origin, Plane->Normal) - Plane->Dist;
		if (Dist > 0)
			Node = Node->Children[0];
		else
			Node = Node->Children[1];
	}

	if (!Node)
	{
		fprintf(stderr, "ERROR: FindLeaf:  NULL Node/Leaf.\n");
		return NULL;
	}

	return Node;
}
