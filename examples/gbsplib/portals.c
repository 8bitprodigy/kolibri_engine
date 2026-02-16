/****************************************************************************************/
/*  portals.c - Portal creation and management                                         */
/*  Converted from Genesis3D PORTALS.CPP to C99/raylib                                 */
/****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <raymath.h>
#include "bsp.h"
#include "mathlib.h"
#include "poly.h"

/****************************************************************************************/
/*  Portals.cpp                                                                         */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Creates and manages portals (passages from leaf-to-leaf)               */
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



GBSP_Node	*OutsideNode;
Vector3		NodeMins, NodeMaxs;
bool	VisPortals;

bool CreateAllOutsidePortals(GBSP_Node *Node);
void GetNodeMinsMaxs(GBSP_Node *Node);
bool AddPortalToNodes(GBSP_Portal *Portal, GBSP_Node *Front, GBSP_Node *Back);
bool RemovePortalFromNode(GBSP_Portal *Portal, GBSP_Node *Node);
GBSP_Portal *AllocPortal(void);

//=====================================================================================
//	CreatePolyOnNode
//=====================================================================================
bool CreatePolyOnNode (GBSP_Node *Node, GBSP_Poly **Out)
{
	GBSP_Poly	*Poly;
	GBSP_Plane	*Plane;
	GBSP_Node	*Parent;

	Poly = CreatePolyFromPlane(&Planes[Node->PlaneNum]);

	if (!Poly)
	{
		fprintf(stderr, "ERROR: "CreatePolyOnNode:  Could not create poly.\n");
		return false;
	}

	// Clip this portal by all the parents of this node
	for (Parent = Node->Parent ; Parent && Poly ; )
	{
		bool	Side;

		Plane = &Planes[Parent->PlaneNum];

		Side = (Parent->Children[0] == Node) ? false : true;
			
		if (!ClipPolyEpsilon(Poly, 0.001f, Plane, Side, &Poly))
			return false;

		Node = Parent;
		Parent = Parent->Parent;
	}

	*Out = Poly;

	return true;
}

//=====================================================================================
//	CreatePortals
//=====================================================================================
bool CreatePortals(GBSP_Node *RootNode, GBSP_Model *Model, bool Vis)
{
	if (Verbose)
		printf(" --- Create Portals --- \n");

	VisPortals = Vis;

	OutsideNode = &Model->OutsideNode;

	NodeMins = Model->Mins;
	NodeMaxs = Model->Maxs;

	if (!CreateAllOutsidePortals(RootNode))
	{
		fprintf(stderr, "ERROR: "CreatePortals:  Could not create bbox portals.\n");
		return false;
	}

	if (!PartitionPortals_r(RootNode))
	{
		fprintf(stderr, "ERROR: "CreatePortals:  Could not partition portals.\n");
		return false;
	}

	return true;
}

//=====================================================================================
//	CreateOutsidePortal
//=====================================================================================
GBSP_Portal *CreateOutsidePortal(GBSP_Plane *Plane, GBSP_Node *Node)
{
	GBSP_Portal	*NewPortal;
	int32_t		Side;

	NewPortal = AllocPortal();
	if (!NewPortal)
		return NULL;

	NewPortal->Poly = CreatePolyFromPlane(Plane);
	if (!NewPortal->Poly)
	{
		return NULL;
	}
	NewPortal->PlaneNum = FindPlane(Plane, &Side);

	if (NewPortal->PlaneNum == -1)
	{
		fprintf(stderr, "ERROR: "CreateOutsidePortal:  Could not create plane.\n");
		return NULL;
	}

	if (!Side)
	{
		if (!AddPortalToNodes(NewPortal, Node, OutsideNode))
			return NULL;
	}
	else
	{
		if (!AddPortalToNodes(NewPortal, OutsideNode, Node))
			return NULL;
	}

	return NewPortal;
}

//=====================================================================================
//	CreateAllOutsidePortals
//=====================================================================================
bool CreateAllOutsidePortals(GBSP_Node *Node)
{
	int32_t		k, i, Index;
	GBSP_Plane	PPlanes[6];
	GBSP_Portal	*Portals[6];

	memset(OutsideNode, 0, sizeof(GBSP_Node));

	memset(PPlanes, 0, 6*sizeof(GBSP_Plane));

	OutsideNode->PlaneNum = PLANENUM_LEAF;
	OutsideNode->Contents = BSP_CONTENTS_SOLID2;
	OutsideNode->Portals = NULL;
	OutsideNode->BrushList = NULL;

	// So there won't be NULL volume leafs when we create the outside portals
	for (k=0; k< 3; k++)
	{
		if (VectorToSUB(NodeMins, k)-128 <= -MIN_MAX_BOUNDS || VectorToSUB(NodeMaxs, k)+128 >= MIN_MAX_BOUNDS)
		{
			fprintf(stderr, "ERROR: "CreateAllOutsidePortals:  World BOX out of range...\n");
			return false;
		}

		VectorToSUB(NodeMins, k) -= (float)128;
		VectorToSUB(NodeMaxs, k) += (float)128;
	}

	// Create 6 portals, and point to the outside and the RootNode
	for (i=0; i<3; i++)
	{
		for (k=0; k<2; k++)
		{
			Index = k*3 + i;
			PPlanes[Index].Normal = (Vector3){0.0f, 0.0f, 0.0f};

			if (k == 0)
			{
				VectorToSUB(PPlanes[Index].Normal, i) = (float)1;
				PPlanes[Index].Dist = VectorToSUB(NodeMins, i);
			}
			else
			{
				VectorToSUB(PPlanes[Index].Normal, i) = (float)-1;
				PPlanes[Index].Dist = -VectorToSUB(NodeMaxs, i);
			}
			
			Portals[Index] = CreateOutsidePortal(&PPlanes[Index], Node);
			if (!Portals[Index])
				return false;
		}
	}
							  
	for (i=0; i< 6; i++)
	{
		for (k=0; k< 6; k++)
		{
			if (k == i)
				continue;

			if (!ClipPoly(Portals[i]->Poly, &PPlanes[k], false, &Portals[i]->Poly))
			{
				fprintf(stderr, "ERROR: "CreateAllOutsidePortals:  There was an error clipping the portal.\n");
				return false;
			}

			if (!Portals[i]->Poly)
			{
				fprintf(stderr, "ERROR: "CreateAllOutsidePortals:  Portal was clipped away.\n");
				return false;
			}
		}
	}

	return true;
}

//=====================================================================================
//	CheckPortal
//=====================================================================================
bool CheckPortal(GBSP_Portal *Portal)
{
	GBSP_Poly	*Poly;
	Vector3		*Verts;
	int32_t		i, k;
	float		Val;

	Poly = Portal->Poly;
	Verts = Poly->Verts;

	if (Poly->NumVerts < 3)
	{
		fprintf(stderr, "ERROR: "CheckPortal:  NumVerts < 3.\n");
		return false;
	}

	for (i=0; i< Poly->NumVerts; i++)
	{
		for (k=0; k<3; k++)
		{
			Val = VectorToSUB(Verts[i], k);

			if (Val == MIN_MAX_BOUNDS)
			{
				fprintf(stderr, "ERROR: "CheckPortal:  Portal was not clipped on all sides!!!\n");
				return false;
			}
		
			if (Val == -MIN_MAX_BOUNDS)
			{
				fprintf(stderr, "ERROR: "CheckPortal:  Portal was not clipped on all sides!!!\n");
				return false;
			}
		}
	}

	return true;
}

//=======================================================================================
//	CalcNodeBoundsFromPortals
//	Calcs bounds for nodes, and leafs
//=======================================================================================
void CalcNodeBoundsFromPortals(GBSP_Node *Node)
{
	GBSP_Portal		*p;
	int32_t			s, i;

	ClearBounds(&Node->Mins, &Node->Maxs);

	for (p=Node->Portals; p; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		for (i=0; i<p->Poly->NumVerts; i++)
			AddPointToBounds(&p->Poly->Verts[i], &Node->Mins, &Node->Maxs);
	}
}

//=====================================================================================
//	PartitionPortals_r
//=====================================================================================
bool PartitionPortals_r(GBSP_Node *Node)
{
	GBSP_Poly		*NewPoly, *FPoly, *BPoly;
	GBSP_Plane		*pPlane, *pPlane2;
	GBSP_Portal		*Portal, *NewPortal, *Next;
	GBSP_Node		*Front, *Back, *OtherNode;
	int32_t			Side;

	CalcNodeBoundsFromPortals(Node);

	if (Node->PlaneNum == PLANENUM_LEAF)
		return true;

	if (VisPortals && Node->Detail)			// We can stop at detail seperators for the vis tree
		return true;

	//if (!InitializeNodePortal(Node))
	//	return false;
	//if (!DistributeNodePortalsToChildren(Node))
	//	return false;

	Front = Node->Children[0];
	Back = Node->Children[1];

	pPlane = &Planes[Node->PlaneNum];

	// Create a new portal
	if (!CreatePolyOnNode (Node, &NewPoly))
	{
		fprintf(stderr, "ERROR: "PartitionPortals_r:  CreatePolyOnNode failed.\n");
		return false;
	}

	// Clip it against all other portals attached to this node
	for (Portal = Node->Portals; Portal && NewPoly; Portal = Portal->Next[Side])
	{
		if (Portal->Nodes[0] == Node)
			Side = 0;
		else if (Portal->Nodes[1] == Node)
			Side = 1;
		else
		{
			fprintf(stderr, "ERROR: "PartitionPortals_r:  Portal does not look at either node.\n");
			return false;
		}

		pPlane2 = &Planes[Portal->PlaneNum];

		if (!ClipPolyEpsilon(NewPoly, 0.001f, pPlane2, Side, &NewPoly))
		{
			fprintf(stderr, "ERROR: "PartitionPortals_r:  There was an error clipping the poly.\n");
			return false;
		}

		if (!NewPoly)
		{
			printf("PartitionPortals_r:  Portal was cut away.\n");
			break;
		}
	}
	
	if (NewPoly && PolyIsTiny (NewPoly))
	{
		FreePoly(NewPoly);
		NewPoly = NULL;
	}

	if (NewPoly)
	{
		NewPortal = AllocPortal();
		if (!NewPortal)
		{
			fprintf(stderr, "ERROR: "PartitionPortals_r:  Out of memory for portal.\n");
			return false;
		}
		NewPortal->Poly = NewPoly;
		NewPortal->PlaneNum = Node->PlaneNum;
		NewPortal->OnNode = Node;

		if (!CheckPortal(NewPortal))
		{
			fprintf(stderr, "ERROR: "PartiionPortals_r:  Check Portal failed.\n");
			return false;
		}
		else
			AddPortalToNodes(NewPortal, Front, Back);

	}
	
	// Partition all portals by this node
	for (Portal = Node->Portals; Portal; Portal = Next)
	{
		if (Portal->Nodes[0] == Node)
			Side = 0;
		else if (Portal->Nodes[1] == Node)
			Side = 1;
		else
		{
			fprintf(stderr, "ERROR: "PartitionPortals_r:  Portal does not look at either node.\n");
			return false;
		}

		Next = Portal->Next[Side];
		
		// Remember the node on the back side
		OtherNode = Portal->Nodes[!Side];
		RemovePortalFromNode(Portal, Portal->Nodes[0]);
		RemovePortalFromNode(Portal, Portal->Nodes[1]);

		if (!SplitPolyEpsilon(Portal->Poly, 0.001f, pPlane, &FPoly, &BPoly, false))
		{
			fprintf(stderr, "ERROR: "PartitionPortals_r:  Could not split portal.\n");
			return false;
		}
		
		if (FPoly && PolyIsTiny(FPoly))
		{
			FreePoly(FPoly);
			FPoly = NULL;
		}

		if (BPoly && PolyIsTiny(BPoly))
		{
			FreePoly(BPoly);
			BPoly = NULL;
		}
		
		if (!FPoly && !BPoly)
			continue;
		
		if (!FPoly)
		{
			Portal->Poly = BPoly;
			if (Side)
				AddPortalToNodes(Portal, OtherNode, Back);
			else
				AddPortalToNodes(Portal, Back, OtherNode);
			continue;
		}

		if (!BPoly)
		{
			Portal->Poly = FPoly;
			if (Side)
				AddPortalToNodes(Portal, OtherNode, Front);
			else
				AddPortalToNodes(Portal, Front, OtherNode);
			continue;
		}

		// Portal was split
		NewPortal = AllocPortal();
		if (!NewPortal)
		{
			fprintf(stderr, "ERROR: "PartitionPortals_r:  Out of memory for portal.\n");
			return false;
		}
		Portal->Poly = FPoly;
		*NewPortal = *Portal;
		NewPortal->Poly = BPoly;
		
		if (Side)
		{
			AddPortalToNodes(Portal, OtherNode, Front);
			AddPortalToNodes(NewPortal, OtherNode, Back);
		}
		else
		{
			AddPortalToNodes(Portal, Front, OtherNode);
			AddPortalToNodes(NewPortal, Back, OtherNode);
		}
	}

	if (Node->Portals != NULL)
	{
		printf("*WARNING* PartitionPortals_r:  Portals still on node after distribution...\n");
	}
	
	if (!PartitionPortals_r(Front))
		return false;

	if (!PartitionPortals_r(Back))
		return false;

	return true;
}

//=====================================================================================
//	AddPortalToNodes
//=====================================================================================
bool AddPortalToNodes(GBSP_Portal *Portal, GBSP_Node *Front, GBSP_Node *Back)
{
	if (Portal->Nodes[0] || Portal->Nodes[1])
	{
		fprintf(stderr, "ERROR: "LinkPortal:  Portal allready looks at one of the nodes.\n");
		return false;
	}

	Portal->Nodes[0] = Front;
	Portal->Next[0] = Front->Portals;
	Front->Portals = Portal;

	Portal->Nodes[1] = Back;
	Portal->Next[1] = Back->Portals;
	Back->Portals = Portal;

	return true;
}

//=====================================================================================
//	RemovePortalFromNode
//=====================================================================================
bool RemovePortalFromNode(GBSP_Portal *Portal, GBSP_Node *Node)
{
	GBSP_Portal	*p, **p2;
	int32_t		Side;

	assert(Node->Portals);		// Better have some portals on this node
	assert(!(Portal->Nodes[0] == Node && Portal->Nodes[1] == Node));
	assert(Portal->Nodes[0] == Node || Portal->Nodes[1] == Node);	

	// Find the portal on this node
	for (p2 = &Node->Portals, p = *p2; p; p2 = &p->Next[Side], p = *p2)
	{
		assert(!(p->Nodes[0] == Node && p->Nodes[1] == Node));
		assert(p->Nodes[0] == Node || p->Nodes[1] == Node);

		Side = (p->Nodes[1] == Node);	// Get the side of the portal that this node is on

		if (p == Portal)
			break;			// Got it
	}
	 
	assert(p && p2 && *p2);

	Side = (Portal->Nodes[1] == Node);	// Get the side of the portal that the node was on
	
	*p2 = Portal->Next[Side];
	Portal->Nodes[Side] = NULL;

	return true;
}

//=====================================================================================
//	AllocPortal
//=====================================================================================
GBSP_Portal *AllocPortal(void)
{
	GBSP_Portal	*NewPortal;

	NewPortal = malloc(sizeof(GBSP_Portal));

	if (!NewPortal)
	{
		fprintf(stderr, "ERROR: "AllocPortal:  Out of memory!\n");
		return NULL;
	}

	memset(NewPortal, 0, sizeof(GBSP_Portal));

	return NewPortal;
}

//=====================================================================================
//	FreePortal
//=====================================================================================
bool FreePortal(GBSP_Portal *Portal)
{
	if (!Portal->Poly)
	{
		fprintf(stderr, "ERROR: "FreePortal:  Portal with NULL Poly.\n");
		return false;
	}

	FreePoly(Portal->Poly);

	free(Portal); Portal = NULL;

	return true;
}

//=====================================================================================
//	FreePOrtals_r
//=====================================================================================
bool FreePortals_r(GBSP_Node *Node)
{
	GBSP_Portal *Portal, *Next;
	int32_t		Side;

	if (!Node)
		return true;
	
	for (Portal = Node->Portals; Portal; Portal = Next)
	{
		if (Portal->Nodes[0] == Node)
			Side = 0;
		else if (Portal->Nodes[1] == Node)
			Side = 1;
		else
		{
			fprintf(stderr, "ERROR: "FreePortals_r:  Portal does not look at either node.\n");
			return false;
		}

		Next = Portal->Next[Side];

		if (!RemovePortalFromNode(Portal, Portal->Nodes[0]))
			return false;

		if (!RemovePortalFromNode(Portal, Portal->Nodes[1]))
			return false;

		if (!FreePortal(Portal))
			return false;
	}

	Node->Portals = NULL;

	if (Node->PlaneNum == PLANENUM_LEAF)
		return true;

	if (!FreePortals_r(Node->Children[0]))
		return false;

	if (!FreePortals_r(Node->Children[1]))
		return false;

	return true;
}

//=====================================================================================
//	FreePortals
//=====================================================================================
bool FreePortals(GBSP_Node *RootNode)
{
	return(FreePortals_r(RootNode));
}

void AddPointToBounds(Vector3 *v, Vector3 *Mins, Vector3 *Maxs);

//=====================================================================================
//	GetNodeMinsMaxs
//=====================================================================================
void GetNodeMinsMaxs(GBSP_Node *Node)
{
	ClearBounds(&NodeMins, &NodeMaxs);

	NodeMins = Node->Mins;
	NodeMaxs = Node->Maxs;
}

//====================================================================================
//	GetLeafBBoxFromPortals
//====================================================================================
bool GetLeafBBoxFromPortals(GBSP_Node *Node, Vector3 *Mins, Vector3 *Maxs)
{
	GBSP_Poly	*Poly;
	Vector3		*Verts;
	GBSP_Portal	*Portal;
	int32_t		i, k, Side;

	if (Node->PlaneNum != PLANENUM_LEAF)
	{
		fprintf(stderr, "ERROR: "GetLeafBBoxFromPortals:  Not a leaf.\n");
		return false;
	}

	ClearBounds(Mins, Maxs);

	for (Portal = Node->Portals; Portal; Portal = Portal->Next[Side])
	{
		Side = (Portal->Nodes[1] == Node);

		Poly = Portal->Poly;

		Verts = Poly->Verts;
		for (i=0; i< Poly->NumVerts; i++)
		{
			for (k=0; k<3; k++)
			{
				if (VectorToSUB(Verts[i], k) < VectorToSUB(*Mins, k))
					VectorToSUB(*Mins, k) = VectorToSUB(Verts[i], k);
				if (VectorToSUB(Verts[i], k) > VectorToSUB(*Maxs, k))
					VectorToSUB(*Maxs, k) = VectorToSUB(Verts[i], k);
			}
		}
	}

	return true;
}
