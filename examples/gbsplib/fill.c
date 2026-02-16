/****************************************************************************************/
/*  fill.c - Flood fill algorithm (RemoveHiddenLeafs)                                  */
/*  Converted from Genesis3D Fill.Cpp to C99/raylib                                    */
/****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <raylib.h>
#include "bsp.h"
#include "mathlib.h"
#include "portals.h"
#include "poly.h"
#include <raymath.h>

// External declarations
extern GBSP_Node *FindLeaf(GBSP_Node *Node, Vector3 *Origin);
extern GBSP_Node *OutsideNode;
extern void PolyCenter(GBSP_Poly *Poly, Vector3 *Center);

/****************************************************************************************/
/*  Fill.cpp                                                                            */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Does the flood filling of the leafs, and removes untouchable leafs     */
/*                                                                                      */
/****************************************************************************************/


int32_t		CurrentFill;
bool	HitEntity = false;
int32_t		EntityHit = 0;
GBSP_Node	*HitNode;

int32_t		NumRemovedLeafs;

//=====================================================================================
//	PlaceEntities
//=====================================================================================
bool PlaceEntities(GBSP_Node *RootNode)
{				
	int32_t		i;
	GBSP_Node	*Node;
	bool	Empty;

	Empty = false;

	for (i=1; i< NumEntities; i++)
	{
		if (!(Entities[i].Flags & ENTITY_HAS_ORIGIN))		// No "Origin" in entity
			continue;

		Node = FindLeaf(RootNode, &Entities[i].Origin);
		if (!Node)
			return false;

		if (!(Node->Contents & BSP_CONTENTS_SOLID2))
		{
			Node->Entity = i;
			Empty = true;
		}
	}
	
	if (!Empty)
	{
		fprintf(stderr, "ERROR: PlaceEntities:  No valid entities for operation %i.\n", NumEntities);
		return false;
	}
	
	return true;
}

//=====================================================================================
//	FillUnTouchedLeafs_r
//=====================================================================================
void FillUnTouchedLeafs_r(GBSP_Node *Node)
{
	if (Node->PlaneNum != PLANENUM_LEAF)
	{
		FillUnTouchedLeafs_r(Node->Children[0]);
		FillUnTouchedLeafs_r(Node->Children[1]);
		return;
	}

	if ((Node->Contents & BSP_CONTENTS_SOLID2))
		return;		//allready solid or removed...

	if (Node->CurrentFill != CurrentFill)
	{
		// Fill er in with solid so it does not show up...(Preserve user contents)
		Node->Contents &= (0xffff0000);
		Node->Contents |= BSP_CONTENTS_SOLID2;
		NumRemovedLeafs++;
	}
}

//=====================================================================================
//	FillLeafs2_r
//=====================================================================================
bool FillLeafs2_r(GBSP_Node *Node)
{
	GBSP_Portal		*Portal;
	int32_t			Side;
	
	if (Node->Contents & BSP_CONTENTS_SOLID2)
		return true;

	if (Node->CurrentFill == CurrentFill)
		return true;

	Node->CurrentFill = CurrentFill;

	for (Portal = Node->Portals; Portal; Portal = Portal->Next[Side])
	{
		if (Portal->Nodes[0] == Node)
			Side = 0;
		else if (Portal->Nodes[1] == Node)
			Side = 1;
		else
		{
			fprintf(stderr, "ERROR: RemoveOutside2_r:  Portal does not look at either node.\n");
			return false;
		}

		// Go though the portal to the node on the other side (!side)
		if (!FillLeafs2_r(Portal->Nodes[!Side]))
			return false;
	}

	return true;
}

//=====================================================================================
//	CanPassPortal
//	See if a portal can be passed through or not
//=====================================================================================
bool CanPassPortal(GBSP_Portal *Portal)
{
	if (Portal->Nodes[0]->PlaneNum != PLANENUM_LEAF)
		printf("*WARNING* CanPassPortal:  Portal does not seperate leafs.\n");

	if (Portal->Nodes[1]->PlaneNum != PLANENUM_LEAF)
		printf("*WARNING* CanPassPortal:  Portal does not seperate leafs.\n");

	if (Portal->Nodes[0]->Contents & BSP_CONTENTS_SOLID2)
		return false;

	if (Portal->Nodes[1]->Contents & BSP_CONTENTS_SOLID2)
		return false;

	return true;
}

//=====================================================================================
//	FillLeafs_r
//=====================================================================================
bool FillLeafs_r(GBSP_Node *Node, bool Fill, int32_t Dist)
{
	GBSP_Portal		*Portal;
	int32_t			Side;
	
	//if (HitEntity)
	//	return true;
	
	if (Node->Contents & BSP_CONTENTS_SOLID2)
		return true;

	if (Node->CurrentFill == CurrentFill)
		return true;

	Node->CurrentFill = CurrentFill;

	Node->Occupied = Dist;

	if (Fill)
	{
		// Preserve user contents
		Node->Contents &= 0xffff0000;
		Node->Contents |= BSP_CONTENTS_SOLID2;
	}
	else 
	{
		if (Node->Entity)
		{
			HitEntity = true;
			EntityHit = Node->Entity;
			HitNode = Node;
			return true;
		}
	}

	for (Portal = Node->Portals; Portal; Portal = Portal->Next[Side])
	{
		if (Portal->Nodes[0] == Node)
			Side = 0;
		else if (Portal->Nodes[1] == Node)
			Side = 1;
		else
		{
			fprintf(stderr, "ERROR: FillLeafs_r:  Portal does not look at either node.\n");
			return false;
		}
		
		//if (!CanPassPortal(Portal))
		//	continue;

		if (!FillLeafs_r(Portal->Nodes[!Side], Fill, Dist+1))
			return false;
	}

	return true;
}

//=====================================================================================
//	FillFromEntities
//=====================================================================================
bool FillFromEntities(GBSP_Node *RootNode)
{
	int32_t		i;
	GBSP_Node	*Node;
	bool	Empty;

	Empty = false;
	
	for (i=1; i< NumEntities; i++)	// Don't use the world as an entity (skip 0)!!
	{
		if (!(Entities[i].Flags & ENTITY_HAS_ORIGIN))		// No "Origin" in entity
			continue;

		Node = FindLeaf(RootNode, &Entities[i].Origin);

		if (Node->Contents & BSP_CONTENTS_SOLID2)
			continue;
		
		// There is at least one entity in empty space...
		Empty = true;

		if (!FillLeafs2_r(Node))
			return false;
	}

	if (!Empty)
	{
		fprintf(stderr, "ERROR: FillFromEntities:  No valid entities for operation.\n");
		return false;
	}

	return true;
}

//=====================================================================================
//	LeafCenter
//=====================================================================================
void LeafCenter(GBSP_Node *Node, Vector3 *PortalMid)
{
	int32_t		NumPortals, Side;
	Vector3		Mid;
	GBSP_Portal	*Portal;

	NumPortals = 0;
	*PortalMid = (Vector3){0.0f, 0.0f, 0.0f};

	for (Portal = Node->Portals; Portal; Portal = Portal->Next[Side])
	{
		Side = Portal->Nodes[1] == Node;

		PolyCenter(Portal->Poly, &Mid);
		*PortalMid = Vector3Add(*PortalMid, Mid);
		NumPortals++;
	}

	*PortalMid = Vector3Scale(*PortalMid, 1.0f/(float)NumPortals);
}

//=====================================================================================
//	WriteLeakFile
//=====================================================================================
bool WriteLeakFile (char *FileName, GBSP_Node *Start, GBSP_Node *Outside)
{
	Vector3		Mid;
	FILE		*PntFile;
	char		FileName2[1024];
	int32_t		Count;
	GBSP_Node	*Node;
	int			Pos1, Pos2;
	char		TAG[5];
	bool	Stop;

	printf("--- WriteLeakFile ---\n");

	//
	// Write the points to the file
	//
	sprintf (FileName2, "%s.Pnt", FileName);
	PntFile = fopen (FileName2, "wb");

	if (!PntFile)
		fprintf(stderr, "WriteLeakFile:  Couldn't open %s\n", FileName);

	Count = 0;

	strcpy(TAG, "LEAK");
	fwrite(TAG, sizeof(char), 4, PntFile);

	Pos1 = ftell(PntFile);

	fwrite(&Count, sizeof(int32_t), 1, PntFile);
	
	Node = Start;

	fwrite(&Entities[EntityHit].Origin, sizeof(Vector3), 1, PntFile);
	Count++;

	Stop = false;

	while (Node && Node->Occupied > 1)
	{
		int32_t		Next;
		GBSP_Portal	*p, *NextPortal;
		GBSP_Node	*NextNode;
		int32_t		s;

		// find the best portal exit
		Next = Node->Occupied;
		NextNode = NULL;
		NextPortal = NULL;

		printf("Occupied   : %5i\n", Node->Occupied);

		for (p=Node->Portals ; p ; p = p->Next[!s])
		{
			s = (p->Nodes[0] == Node);

			if (p->Nodes[s] == Outside)
			{
				Stop = true;
				break;
			}

			if (p->Nodes[s]->Occupied && p->Nodes[s]->Occupied < Next)
			{
				NextPortal = p;
				NextNode = p->Nodes[s];
				Next = NextNode->Occupied;
			}
		}

		Node = NextNode;
		
		if (Stop)
			break;

		if (NextPortal && Node && Node != Outside)
		{
			// Write out the center of the portal
			PolyCenter(NextPortal->Poly, &Mid);
			fwrite(&Mid, sizeof(Vector3), 1, PntFile);
			Count++;
			// Then writer out the center of the leaf it goes too
			LeafCenter(Node, &Mid);
			fwrite(&Mid, sizeof(Vector3), 1, PntFile);
			Count++;
		}
	}

	printf("Num Points    : %5i\n", Count);

	// HACK!!!!
	Pos2 = ftell(PntFile);

	printf("Pos1 = %5i\n", Pos1);
	printf("Pos2 = %5i\n", Pos2);

	fseek(PntFile, Pos1, SEEK_SET);
	//Count-=2;
	fwrite(&Count, sizeof(int32_t), 1, PntFile);

	fseek(PntFile, Pos2, SEEK_SET);

	fclose (PntFile);

	return true;
}

//=====================================================================================
//	RemoveHiddenLeafs
//=====================================================================================
int32_t RemoveHiddenLeafs(GBSP_Node *RootNode, GBSP_Node *ONode)
{
	int32_t	Side;

	printf(" --- Remove Hidden Leafs --- \n");

	OutsideNode = ONode;
	
	Side = OutsideNode->Portals->Nodes[0] == OutsideNode;

	NumRemovedLeafs = 0;

	if (!PlaceEntities(RootNode))
		return -1;

	HitEntity = false;
	HitNode = NULL;

	CurrentFill = 1;
	if (!FillLeafs_r(OutsideNode->Portals->Nodes[Side], false, 1))
		return -1;

	if (HitEntity)
	{
		printf("*****************************************\n");
		printf("*           *** LEAK ***                *\n");
		printf("* Level is NOT sealed.                  *\n");
		printf("* Optimal removal will not be performed.*\n");
		printf("*****************************************\n");

		WriteLeakFile("Test", HitNode, ONode);
		return -1;
	}

	CurrentFill = 2;
	
	if (!FillFromEntities(RootNode))
		return -1;
	
	FillUnTouchedLeafs_r(RootNode);

	if (Verbose)
		printf("Removed Leafs          : %5i\n", NumRemovedLeafs);

	return NumRemovedLeafs;
}
