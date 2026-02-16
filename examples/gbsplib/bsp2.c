/****************************************************************************************/
/*  bsp2.c - BSP tree construction                                                      */
/*  Converted from Genesis3D BSP2.CPP to C99/raylib                                     */
/****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <raylib.h>
#include "mathlib.h"
#include "poly.h"
#include "bsp.h"
#include "portals.h"
#include "fill.h"
#include "brush2.h"

#define USE_VOLUMES

// External declarations
extern int32_t NumTexInfo;
extern int32_t FindTextureIndex(const char *name, int32_t flags);




// Track if entities have been set
static bool entities_initialized = false;

// Set entities from map data before calling ProcessWorldModel
void SetBSPEntities(int num_entities, Vector3 *origins, int32_t *flags) {
    memset(Entities, 0, sizeof(MAP_Entity) * MAX_MAP_ENTITIES);
    NumEntities = num_entities < MAX_MAP_ENTITIES ? num_entities : MAX_MAP_ENTITIES;
    
    for (int i = 0; i < NumEntities; i++) {
        Entities[i].Flags = flags ? flags[i] : 0;
        Entities[i].Origin = origins ? origins[i] : (Vector3){0, 0, 0};
        Entities[i].ModelNum = 0;
        
        if (i > 0) {
            printf("[SetBSPEntities] Entity %d at (%.1f, %.1f, %.1f) flags=0x%x\n",
                   i, Entities[i].Origin.x, Entities[i].Origin.y, Entities[i].Origin.z, Entities[i].Flags);
        }
    }
    
    entities_initialized = true;
}

// Initialize entities for BSP - only runs if SetBSPEntities wasn't called

// Initialize default texture info
GFX_TexInfo *GFXTexInfo = NULL;
int32_t NumGFXTexInfo = 0;

void InitBSPTexInfo(void) {
    if (!GFXTexInfo) {
        GFXTexInfo = (GFX_TexInfo*)malloc(sizeof(GFX_TexInfo) * 1);
        if (!GFXTexInfo) {
            printf("[InitBSPTexInfo] Failed to allocate texture info\n");
            return;
        }
        NumGFXTexInfo = 1;
        NumTexInfo = 1;
        
        // Set up default texinfo
        GFXTexInfo[0].Vecs[0] = (Vector3){1, 0, 0};  // U axis
        GFXTexInfo[0].Vecs[1] = (Vector3){0, 1, 0};  // V axis
        GFXTexInfo[0].Shift[0] = 0;
        GFXTexInfo[0].Shift[1] = 0;
        GFXTexInfo[0].DrawScale[0] = 1;
        GFXTexInfo[0].DrawScale[1] = 1;
        GFXTexInfo[0].Flags = 0;
        GFXTexInfo[0].FaceLight = 0;
        GFXTexInfo[0].ReflectiveScale = 0;
        GFXTexInfo[0].Alpha = 1.0f;
        GFXTexInfo[0].MipMapBias = 0;
        GFXTexInfo[0].Texture = -1;
        
        printf("[InitBSPTexInfo] Initialized default texture info\n");
    }
}
void InitBSPEntities(void) {
    if (entities_initialized) {
        // Already initialized by SetBSPEntities - don't overwrite
        return;
    }
    
    // Fallback: use dummy entities
    printf("[InitBSPEntities] WARNING: No entities set, using dummy\n");
    NumEntities = 2;
    Entities[0].Flags = 0;
    Entities[1].Flags = ENTITY_HAS_ORIGIN;
    Entities[1].Origin = (Vector3){0, 0, 0};
    entities_initialized = true;
}


// Sky data structure - stub for now
typedef struct {
    int32_t Textures[6];
    Vector3 Axis;
    float Dpm;
    float DrawScale;
} GFX_SkyData_t;

extern GFX_SkyData_t GFXSkyData;

#define TEXTURE_SKYBOX 0x0001

Vector3	TreeMins;
Vector3	TreeMaxs;

float	MicroVolume = 0.1f;
int32_t	NumVisNodes, NumNonVisNodes;

extern int32_t	NumMerged;
extern int32_t	NumSubdivided;

int32_t	NumMakeFaces = 0;

#define PLANESIDE_EPSILON 0.001f



//=======================================================================================
//	VisibleContents
//=======================================================================================
uint32_t VisibleContents(uint32_t Contents)
{
	int32_t	j;
	uint32_t	MajorContents;

	if (!Contents)
		return 0;

	// Only check visible contents
	Contents &= BSP_VISIBLE_CONTENTS;
	
	// Return the strongest one, return the first lsb
	for (j=0; j<32; j++)
	{
		MajorContents = (Contents & (1<<j));
		if (MajorContents)
			return MajorContents;
	}

	return 0;
}	

//=======================================================================================
//	FindPortalSide
//=======================================================================================
void FindPortalSide (GBSP_Portal *p, int32_t PSide)
{
	uint32_t		VisContents, MajorContents;
	GBSP_Brush	*Brush;
	int32_t		i,j;
	int32_t		PlaneNum;
	GBSP_Side	*BestSide;
	float		Dot, BestDot;
	GBSP_Plane	*p1, *p2;

	// First, check to see if the contents are intersecting sheets (special case)
	if ((p->Nodes[0]->Contents & BSP_CONTENTS_SHEET) && (p->Nodes[1]->Contents & BSP_CONTENTS_SHEET))
	{
		// The contents are intersecting sheets, so or them together
		VisContents = p->Nodes[0]->Contents | p->Nodes[1]->Contents;
	}
	else
	{
		// Make sure the contents on both sides are not the same
		VisContents = p->Nodes[0]->Contents ^ p->Nodes[1]->Contents;
	}

	// There must be a visible contents on at least one side of the portal...
	MajorContents = VisibleContents(VisContents);

	if (!MajorContents)
		return;

	PlaneNum = p->OnNode->PlaneNum;
	BestSide = NULL;
	BestDot = 0.0f;

	for (j=0 ; j<2 ; j++)
	{
		GBSP_Node	*Node;

		Node = p->Nodes[j];
		p1 = &Planes[p->OnNode->PlaneNum];

		for (Brush=Node->BrushList ; Brush ; Brush=Brush->Next)
		{
			MAP_Brush	*MapBrush;
			GBSP_Side	*pSide;

			MapBrush = Brush->Original;

			// Only use the brush that contains a major contents (solid)
			if (!(MapBrush->Contents & MajorContents))
				continue;

			pSide = MapBrush->OriginalSides;

			for (i=0 ; i<MapBrush->NumSides ; i++, pSide++)
			{
				if (pSide->Flags & SIDE_NODE)
					continue;		// Side not visible (result of a csg'd topbrush)

				// First, Try an exact match
				if (pSide->PlaneNum == PlaneNum)
				{	
					BestSide = pSide;
					goto GotSide;
				}
				// In the mean time, try for the closest match
				p2 = &Planes[pSide->PlaneNum];
				Dot = Vector3DotProduct(p1->Normal, p2->Normal);
				if (Dot > BestDot)
				{
					BestDot = Dot;
					BestSide = pSide;
				}
			}

		}

	}

	GotSide:

	if (!BestSide)
		printf("WARNING: Could not map portal to original brush...\n");

	p->SideFound = true;
	p->Side = BestSide;
}

//=======================================================================================
//	MarkVisibleSides_r
//=======================================================================================
void MarkVisibleSides_r (GBSP_Node *Node)
{
	GBSP_Portal	*p;
	int32_t		s;

	// Recurse to leafs 
	if (Node->PlaneNum != PLANENUM_LEAF)
	{
		MarkVisibleSides_r (Node->Children[0]);
		MarkVisibleSides_r (Node->Children[1]);
		return;
	}

	// Empty (air) leafs don't have faces
	if (!Node->Contents)
		return;

	for (p=Node->Portals ; p ; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		if (!p->OnNode)
			continue;		// Outside node (assert for it here!!!)

		if (!p->SideFound)
			FindPortalSide (p, s);

		if (p->Side)
			p->Side->Flags |= SIDE_VISIBLE;

		#if 1
		if (p->Side)
		{
			if (!(p->Nodes[!s]->Contents & BSP_CONTENTS_SOLID2) && 
				(p->Nodes[s]->Contents & BSP_CONTENTS_SHEET) && 
				!(p->Side->Flags & SIDE_SHEET))
			{ 
				p->Side->Flags &= ~SIDE_VISIBLE;
				p->Side = NULL;
				p->SideFound = true;		// Don't look for this side again!!!
			}
		}
		#endif
	}

}

//=======================================================================================
//	MarkVisibleSides
//	Looks at all the portals, and marks leaf boundrys that seperate different contents
//  into faces.
//=======================================================================================
void MarkVisibleSides(GBSP_Node *Node, MAP_Brush *Brushes)
{
	int32_t		j;
	MAP_Brush	*Brush;
	int32_t		NumSides;

	if (Verbose)
		printf("--- Map Portals to Brushes ---\n");

	// Clear all the visible flags
	for (Brush = Brushes; Brush; Brush = Brush->Next)
	{
		NumSides = Brush->NumSides;

		for (j=0 ; j<NumSides ; j++)
			Brush->OriginalSides[j].Flags &= ~SIDE_VISIBLE;
	}
	
	// Set visible flags on the sides that are used by portals
	MarkVisibleSides_r (Node);
}

//=======================================================================================
//	BoxOnPlaneSide
//=======================================================================================
int32_t BoxOnPlaneSide(Vector3 *Mins, Vector3 *Maxs, GBSP_Plane *Plane)
{
	int32_t	Side;
	int32_t	i;
	Vector3	Corners[2];
	float	Dist1, Dist2;
	
	if (Plane->Type < 3)
	{
		Side = 0;
		if (VectorToSUB(*Maxs, Plane->Type) > Plane->Dist+PLANESIDE_EPSILON)
			Side |= PSIDE_FRONT;
		if (VectorToSUB(*Mins, Plane->Type) < Plane->Dist-PLANESIDE_EPSILON)
			Side |= PSIDE_BACK;
		return Side;
	}
	
	for (i=0 ; i<3 ; i++)
	{
		if (VectorToSUB(Plane->Normal, i) < 0)
		{
			VectorToSUB(Corners[0], i) = VectorToSUB(*Mins, i);
			VectorToSUB(Corners[1], i) = VectorToSUB(*Maxs, i);
		}
		else
		{
			VectorToSUB(Corners[1], i) = VectorToSUB(*Mins, i);
			VectorToSUB(Corners[0], i) = VectorToSUB(*Maxs, i);
		}
	}

	Dist1 = Vector3DotProduct(Plane->Normal, Corners[0]) - Plane->Dist;
	Dist2 = Vector3DotProduct(Plane->Normal, Corners[1]) - Plane->Dist;
	Side = 0;
	if (Dist1 >= PLANESIDE_EPSILON)
		Side = PSIDE_FRONT;
	if (Dist2 < PLANESIDE_EPSILON)
		Side |= PSIDE_BACK;

	return Side;
}

//=======================================================================================
//	MakeBSPBrushes
//	Converts map brushes into useable bsp brushes
//=======================================================================================
GBSP_Brush *MakeBSPBrushes(MAP_Brush *MapBrushes)
{
	MAP_Brush		*MapBrush;
	GBSP_Brush		*NewBrush, *NewBrushes;
	int32_t			j, NumSides;
	int32_t			Vis;

	NewBrushes = NULL;

	for (MapBrush = MapBrushes; MapBrush; MapBrush = MapBrush->Next)
	{
		NumSides = MapBrush->NumSides;

		if (!NumSides)
			continue;

		Vis = 0;
		for (j=0; j< NumSides; j++)
		{
			if (MapBrush->OriginalSides[j].Poly)
				Vis++;
		}

		if (!Vis)
			continue;

		NewBrush = AllocBrush(NumSides);
	
		NewBrush->Original = MapBrush;
		NewBrush->NumSides = NumSides;

		memcpy (NewBrush->Sides, MapBrush->OriginalSides, NumSides*sizeof(GBSP_Side));

		for (j=0 ; j<NumSides ; j++)
		{
			if (MapBrush->OriginalSides[j].Flags & SIDE_HINT)
				NewBrush->Sides[j].Flags |= SIDE_VISIBLE;

			if (NewBrush->Sides[j].Poly)
			{
				CopyPoly(NewBrush->Sides[j].Poly, &NewBrush->Sides[j].Poly);
			}
		}
		
		NewBrush->Mins = MapBrush->Mins;
		NewBrush->Maxs = MapBrush->Maxs;

		BoundBrush (NewBrush);

		if (!CheckBrush(NewBrush))
		{
			fprintf(stderr, "MakeBSPBrushes:  Bad brush.\n");
			continue;
		}

		NewBrush->Next = NewBrushes;
		NewBrushes = NewBrush;
	}

	return NewBrushes;
}

//=======================================================================================
//	TestBrushToPlane
//=======================================================================================
int32_t TestBrushToPlane(	GBSP_Brush *Brush, int32_t PlaneNum, int32_t PSide, 
						int32_t *NumSplits, bool *HintSplit, int32_t *EpsilonBrush)
{
	int32_t		i, j, Num;
	GBSP_Plane	*Plane;
	int32_t		s;
	GBSP_Poly	*p;
	float		d, FrontD, BackD;
	int32_t		Front, Back;
	GBSP_Side	*pSide;

	*NumSplits = 0;
	*HintSplit = false;

	for (i=0 ; i<Brush->NumSides ; i++)
	{
		Num = Brush->Sides[i].PlaneNum;
		
		if (Num == PlaneNum && !Brush->Sides[i].PlaneSide)
			return PSIDE_BACK|PSIDE_FACING;

		if (Num == PlaneNum && Brush->Sides[i].PlaneSide)
			return PSIDE_FRONT|PSIDE_FACING;
	}
	
	// See if it's totally on one side or the other
	Plane = &Planes[PlaneNum];
	s = BoxOnPlaneSide (&Brush->Mins, &Brush->Maxs, Plane);

	if (s != PSIDE_BOTH)
		return s;
	
	// The brush is split, count the number of splits 
	FrontD = BackD = 0.0f;

	for (pSide = Brush->Sides, i=0 ; i<Brush->NumSides ; i++, pSide++)
	{
		Vector3	*pVert;
		uint8	PSide;

		if (pSide->Flags & SIDE_NODE)
			continue;		

		if (!(pSide->Flags & SIDE_VISIBLE))
			continue;		

		p = pSide->Poly;

		if (!p)
			continue;

		PSide = pSide->PlaneSide;

		Front = Back = 0;

		for (pVert = p->Verts, j=0 ; j<p->NumVerts; j++, pVert++)
		{
		#if 1
			d = Plane_PointDistanceFast(Plane, pVert);
		#else
			d = Vector3DotProduct(pVert, &Plane->Normal) - Plane->Dist;
		#endif

			if (d > FrontD)
				FrontD = d;
			else if (d < BackD)
				BackD = d;

			if (d > 0.1) 
				Front = 1;
			else if (d < -0.1) 
				Back = 1;
		}

		if (Front && Back)
		{
			(*NumSplits)++;
			if (pSide->Flags & SIDE_HINT)
				*HintSplit = true;
		}
	}

	// Check to see if this split would produce a tiny brush (would result in tiny leafs, bad for vising)
	if ( (FrontD > 0.0 && FrontD < 1.0) || (BackD < 0.0 && BackD > -1.0) )
		(*EpsilonBrush)++;

	return s;
}

//=======================================================================================
//	CheckPlaneAgainstParents
//	Makes sure no plane gets used twice in the tree, from the children up.  
//	This would screw up the portals
//=======================================================================================
bool CheckPlaneAgainstParents (int32_t PNum, GBSP_Node *Node)
{
	GBSP_Node	*p;

	for (p=Node->Parent ; p ; p=p->Parent)
	{
		if (p->PlaneNum == PNum)
		{
			fprintf(stderr, "Tried parent");
			return false;
		}
	}

	return true;
}

//=======================================================================================
//	CheckPlaneAgainstVolume
//	Makes sure that a potential splitter does not make tiny volumes from the parent volume
//=======================================================================================
bool CheckPlaneAgainstVolume (int32_t PNum, GBSP_Node *Node)
{
	GBSP_Brush	*Front, *Back;
	bool	Good;

	SplitBrush(Node->Volume, PNum, 0, SIDE_NODE, false, &Front, &Back);

	Good = (Front && Back);

	if (Front)
		FreeBrush(Front);
	if (Back)
		FreeBrush(Back);

	return Good;
}

//=======================================================================================
//	SelectSplitSide
//=======================================================================================
GBSP_Side *SelectSplitSide(GBSP_Brush *Brushes, GBSP_Node *Node)
{
	int32_t		Value, BestValue;
	GBSP_Brush	*Brush, *Test;
	GBSP_Side	*Side, *BestSide;
	int32_t		i, j, Pass, NumPasses;
	int32_t		PNum, PSide;
	int32_t		s;
	int32_t		Front, Back, Both, Facing, Splits;
	int32_t		BSplits;
	int32_t		BestSplits;
	int32_t		EpsilonBrush;
	bool	HintSplit;

	if (CancelRequest)
		return NULL;

	BestSide = NULL;
	BestValue = -999999;
	BestSplits = 0;

	NumPasses = 4;
	for (Pass = 0 ; Pass < NumPasses ; Pass++)
	{
		for (Brush = Brushes ; Brush ; Brush=Brush->Next)
		{
			if ( (Pass & 1) && !(Brush->Original->Contents & BSP_CONTENTS_DETAIL2) )
				continue;
			if ( !(Pass & 1) && (Brush->Original->Contents & BSP_CONTENTS_DETAIL2) )
				continue;
			
			for (i=0 ; i<Brush->NumSides ; i++)
			{
				Side = &Brush->Sides[i];

				if (!Side->Poly)
					continue;	
				if (Side->Flags & (SIDE_TESTED|SIDE_NODE))
					continue;	
 				if (!(Side->Flags&SIDE_VISIBLE) && Pass<2)
					continue;	

				PNum = Side->PlaneNum;
				PSide = Side->PlaneSide;
				
				assert(CheckPlaneAgainstParents (PNum, Node) == true);
				
			#ifdef USE_VOLUMES
				if (!CheckPlaneAgainstVolume (PNum, Node))
					continue;	
			#endif
				
				Front = 0;
				Back = 0;
				Both = 0;
				Facing = 0;
				Splits = 0;
				EpsilonBrush = 0;

				for (Test = Brushes ; Test ; Test=Test->Next)
				{
					s = TestBrushToPlane(Test, PNum, PSide, &BSplits, &HintSplit, &EpsilonBrush);

					Splits += BSplits;

					if (BSplits && (s&PSIDE_FACING) )
						fprintf(stderr, "PSIDE_FACING with splits\n");

					Test->TestSide = s;

					if (s & PSIDE_FACING)
					{
						Facing++;
						for (j=0 ; j<Test->NumSides ; j++)
						{
							if (Test->Sides[j].PlaneNum == PNum)
								Test->Sides[j].Flags |= SIDE_TESTED;
						}
					}
					if (s & PSIDE_FRONT)
						Front++;
					if (s & PSIDE_BACK)
						Back++;
					if (s == PSIDE_BOTH)
						Both++;
				}

				Value = 5*Facing - 5*Splits - abs(Front-Back);
				
				if (Planes[PNum].Type < 3)
					Value+=5;				
				
				Value -= EpsilonBrush*1000;	

				if (HintSplit && !(Side->Flags & SIDE_HINT) )
					Value = -999999;

				if (Value > BestValue)
				{
					BestValue = Value;
					BestSide = Side;
					BestSplits = Splits;
					for (Test = Brushes ; Test ; Test=Test->Next)
						Test->Side = Test->TestSide;
				}
			}

		#if 0
			if (BestSide)		// Just take the first side...
				break;
		#endif

		}

		if (BestSide)
		{
			if (Pass > 1)
				NumNonVisNodes++;
			
			if (Pass > 0)
			{
				Node->Detail = true;			// Not needed for vis
				if (BestSide->Flags & SIDE_HINT)
					printf("*** Hint as Detail!!! ***\n");
			}
			
			break;
		}
	}

	for (Brush = Brushes ; Brush ; Brush=Brush->Next)
	{
		for (i=0 ; i<Brush->NumSides ; i++)
			Brush->Sides[i].Flags &= ~SIDE_TESTED;
	}

	return BestSide;
}

//=======================================================================================
//	SplitBrushList
//=======================================================================================
void SplitBrushList (GBSP_Brush *Brushes, GBSP_Node *Node, GBSP_Brush **Front, GBSP_Brush **Back)
{
	GBSP_Brush	*Brush, *NewBrush, *NewBrush2, *Next;
	GBSP_Side	*Side;
	int32_t		Sides;
	int32_t		i;

	*Front = *Back = NULL;

	for (Brush = Brushes ; Brush ; Brush = Next)
	{
		Next = Brush->Next;

		Sides = Brush->Side;

		if (Sides == PSIDE_BOTH)
		{	
			SplitBrush(Brush, Node->PlaneNum, 0, SIDE_NODE, false, &NewBrush, &NewBrush2);
			if (NewBrush)
			{
				NewBrush->Next = *Front;
				*Front = NewBrush;
			}
			if (NewBrush2)
			{
				NewBrush2->Next = *Back;
				*Back = NewBrush2;
			}
			continue;
		}

		NewBrush = CopyBrush(Brush);

		if (Sides & PSIDE_FACING)
		{
			for (i=0 ; i<NewBrush->NumSides ; i++)
			{
				Side = NewBrush->Sides + i;
				if (Side->PlaneNum == Node->PlaneNum)
					Side->Flags |= SIDE_NODE;
			}
		}


		if (Sides & PSIDE_FRONT)
		{
			NewBrush->Next = *Front;
			*Front = NewBrush;
			continue;
		}
		if (Sides & PSIDE_BACK)
		{
			NewBrush->Next = *Back;
			*Back = NewBrush;
			continue;
		}
	}
}

//=======================================================================================
//	LeafNode
//	Converts a node into a leaf, and create the contents
//=======================================================================================
void LeafNode (GBSP_Node *Node, GBSP_Brush *Brushes)
{
	GBSP_Brush	*Brush;
	int32_t		i;

	Node->PlaneNum = PLANENUM_LEAF;
	Node->Contents = 0;

	// Get the contents of this leaf, by examining all the brushes that made this leaf
	for (Brush = Brushes; Brush; Brush = Brush->Next)
	{
		if (Brush->Original->Contents & BSP_CONTENTS_SOLID2)
		{
			for (i=0 ; i < Brush->NumSides ;i++)
			{
				if (!(Brush->Sides[i].Flags & SIDE_NODE))
					break;
			}
		
			// If all the planes in this leaf where caused by splits, then
			// we can force this leaf to be solid...
			if (i == Brush->NumSides)
			{
				//Node->Contents &= 0xffff0000;
				Node->Contents |= BSP_CONTENTS_SOLID2;
				//break;
			}
			
		}
		
		Node->Contents |= Brush->Original->Contents;
	}

	// Once brushes get down to the leafs, we don't need to keep the polys on them anymore...
	// We can free them now...
	for (Brush = Brushes ; Brush ; Brush=Brush->Next)
	{
		// Don't need to keep polygons anymore...
		for (i=0; i< Brush->NumSides; i++)
		{
			if (Brush->Sides[i].Poly)
			{
				FreePoly(Brush->Sides[i].Poly);
				Brush->Sides[i].Poly = NULL;
			}
		}
	}
	
	Node->BrushList = Brushes;
}

//=======================================================================================
//	BuildTree_r
//=======================================================================================
GBSP_Node *BuildTree_r (GBSP_Node *Node, GBSP_Brush *Brushes)
{
	GBSP_Node	*NewNode;
	GBSP_Side	*BestSide;
	int32_t		i;
	GBSP_Brush	*Children[2];

	NumVisNodes++;

	// find the best plane to use as a splitter
	BestSide = SelectSplitSide (Brushes, Node);
	
	if (!BestSide)
	{
		// leaf node
		Node->Side = NULL;
		Node->PlaneNum = PLANENUM_LEAF;
		LeafNode (Node, Brushes);

	#ifdef USE_VOLUMES
		FreeBrush(Node->Volume);
	#endif
		return Node;
	}

	// This is a splitplane node
	Node->Side = BestSide;
	Node->PlaneNum = BestSide->PlaneNum;

	SplitBrushList (Brushes, Node, &Children[0], &Children[1]);
	FreeBrushList(Brushes);
	
	// Allocate children before recursing
	for (i=0 ; i<2 ; i++)
	{
		NewNode = AllocNode ();
		NewNode->Parent = Node;
		Node->Children[i] = NewNode;
	}
	
#ifdef USE_VOLUMES
	// Distribute this nodes volume to its children
	SplitBrush(Node->Volume, Node->PlaneNum, 0, SIDE_NODE, false, &Node->Children[0]->Volume,
		&Node->Children[1]->Volume);

	if (!Node->Children[0]->Volume || !Node->Children[1]->Volume)
		printf("*WARNING* BuildTree_r:  Volume was not split on both sides...\n");
	
	FreeBrush(Node->Volume);
#endif	

	// Recursively process children
	for (i=0 ; i<2 ; i++)
		Node->Children[i] = BuildTree_r (Node->Children[i], Children[i]);

	return Node;
}


//=======================================================================================
//	BuildBSP
//=======================================================================================
GBSP_Node *BuildBSP(GBSP_Brush *BrushList)
{
	GBSP_Node	*Node;
	GBSP_Brush	*b;
	int32_t		NumVisFaces, NumNonVisFaces;
	int32_t		NumVisBrushes;
	int32_t		i;
	float		Volume;
	Vector3		Mins, Maxs;

	if (Verbose)
		printf("--- Build BSP Tree ---\n");

	ClearBounds(&Mins, &Maxs);

	NumVisFaces = 0;
	NumNonVisFaces = 0;
	NumVisBrushes = 0;

	for (b=BrushList ; b ; b=b->Next)
	{
		NumVisBrushes++;

		Volume = BrushVolume (b);

		if (Volume < MicroVolume)
		{
			printf("**WARNING** BuildBSP: Brush with NULL volume\n");
		}
		
		for (i=0 ; i<b->NumSides ; i++)
		{
			if (!b->Sides[i].Poly)
				continue;
			if (b->Sides[i].Flags & SIDE_NODE)
				continue;
			if (b->Sides[i].Flags & SIDE_VISIBLE)
				NumVisFaces++;
			else
				NumNonVisFaces++;
		}

		AddPointToBounds (&b->Mins, &Mins, &Maxs);
		AddPointToBounds (&b->Maxs, &Mins, &Maxs);
		
	}
	
	if (Verbose)
	{
		printf("Total Brushes          : %5i\n", NumVisBrushes);
		printf("Total Faces            : %5i\n", NumVisFaces);
		printf("Faces Removed          : %5i\n", NumNonVisFaces);
	}

	NumVisNodes = 0;
	NumNonVisNodes = 0;
	
	Node = AllocNode();

#ifdef USE_VOLUMES
	Node->Volume = BrushFromBounds (&Mins, &Maxs);

	if (BrushVolume(Node->Volume) < 1.0f)
		printf("**WARNING** BuildBSP: BAD world volume.\n");
#endif

	Node = BuildTree_r (Node, BrushList);

	// Top node is always valid, this way portals can use top node to get box of entire bsp...
	Node->Mins = Mins;
	Node->Maxs = Maxs;
	
	TreeMins = Mins;
	TreeMaxs = Maxs;

	if (Verbose)
	{
		printf("Total Nodes            : %5i\n", NumVisNodes/2 - NumNonVisNodes);
		printf("Nodes Removed          : %5i\n", NumNonVisNodes);
		printf("Total Leafs            : %5i\n", (NumVisNodes+1)/2);
	}

	return Node;
}

//=======================================================================================
//	FaceFromPortal
//=======================================================================================
GBSP_Face *FaceFromPortal (GBSP_Portal *p, int32_t PSide)
{
	GBSP_Face	*f;
	GBSP_Side	*Side;

	Side = p->Side;
	
	if (!Side)
		return NULL;	// Portal does not bridge different visible contents

	if ( (p->Nodes[PSide]->Contents & BSP_CONTENTS_WINDOW2)
		&& VisibleContents(p->Nodes[!PSide]->Contents^p->Nodes[PSide]->Contents) == BSP_CONTENTS_WINDOW2)
		return NULL;

	f = AllocFace (0);

	if (Side->TexInfo >= NumTexInfo || Side->TexInfo < 0)
		printf("*WARNING* FaceFromPortal:  Bad texinfo.\n");

	f->TexInfo = Side->TexInfo;
	f->PlaneNum = Side->PlaneNum;
	f->PlaneSide = PSide;
	f->Portal = p;
	f->Visible = true;

	if (PSide)
		ReversePoly(p->Poly, &f->Poly);
	else
		CopyPoly(p->Poly, &f->Poly);

	return f;
}

void SubdivideNodeFaces (GBSP_Node *Node);

//=======================================================================================
//	CountLeafFaces_r
//=======================================================================================
void CountLeafFaces_r(GBSP_Node *Node, GBSP_Face *Face)
{
	
	while (Face->Merged)
		Face = Face->Merged;

	if (Face->Split[0])
	{
		CountLeafFaces_r(Node, Face->Split[0]);
		CountLeafFaces_r(Node, Face->Split[1]);
		return;
	}

	Node->NumLeafFaces++;
}

//=======================================================================================
//	GetLeafFaces_r
//=======================================================================================
void GetLeafFaces_r(GBSP_Node *Node, GBSP_Face *Face)
{
	
	while (Face->Merged)
		Face = Face->Merged;

	if (Face->Split[0])
	{
		GetLeafFaces_r(Node, Face->Split[0]);
		GetLeafFaces_r(Node, Face->Split[1]);
		return;
	}

	Node->LeafFaces[Node->NumLeafFaces++] = Face;
}

//=======================================================================================
//	MakeLeafFaces_r
//=======================================================================================
void MakeLeafFaces_r(GBSP_Node *Node)
{
	GBSP_Portal	*p;
	int32_t		s;

	// Recurse down to leafs
	if (Node->PlaneNum != PLANENUM_LEAF)
	{
		MakeLeafFaces_r (Node->Children[0]);
		MakeLeafFaces_r (Node->Children[1]);
		return;
	}

	// Solid leafs never have visible faces
	if (Node->Contents & BSP_CONTENTS_SOLID2)
		return;

	// Reset counter
	Node->NumLeafFaces = 0;

	// See which portals are valid
	for (p=Node->Portals ; p ; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		if (!p->Face[s])
			continue;

		CountLeafFaces_r(Node, p->Face[s]);
	}

	Node->LeafFaces = (GBSP_Face**)malloc(sizeof(GBSP_Face*)*(Node->NumLeafFaces+1));
	
	// Reset counter
	Node->NumLeafFaces = 0;
	
	// See which portals are valid
	for (p=Node->Portals ; p ; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		if (!p->Face[s])
			continue;

		GetLeafFaces_r(Node, p->Face[s]);
	}
}

//=======================================================================================
//	MakeLeafFaces
//=======================================================================================
void MakeLeafFaces(GBSP_Node *Root)
{
	MakeLeafFaces_r(Root);
}

bool MergeFaceList2(GBSP_Face *Faces);

//=======================================================================================
//	MakeFaces_r
//=======================================================================================
void MakeFaces_r (GBSP_Node *Node)
{
	GBSP_Portal	*p;
	int32_t		s;

	// Recurse down to leafs
	if (Node->PlaneNum != PLANENUM_LEAF)
	{
		MakeFaces_r (Node->Children[0]);
		MakeFaces_r (Node->Children[1]);
		
		// Marge list
		MergeFaceList2(Node->Faces);
		// Subdivide them for lightmaps
		SubdivideNodeFaces(Node);
		return;
	}

	// Solid leafs never have visible faces
	if (Node->Contents & BSP_CONTENTS_SOLID2)
		return;

	// See which portals are valid
	for (p=Node->Portals ; p ; p = p->Next[s])
	{
		s = (p->Nodes[1] == Node);

		p->Face[s] = FaceFromPortal (p, s);
		if (p->Face[s])
		{
			// Record the contents on each side of the face
			p->Face[s]->Contents[0] = Node->Contents;			// Front side contents is this leaf
			p->Face[s]->Contents[1] = p->Nodes[!s]->Contents;	// Back side contents is the leaf on the other side of this portal

			// Add the face to the list of faces on the node that originaly created the portal
			p->Face[s]->Next = p->OnNode->Faces;
			p->OnNode->Faces = p->Face[s];

			NumMakeFaces++;
		}
	}
}


//=======================================================================================
//	MakeFaces
//=======================================================================================
void MakeFaces (GBSP_Node *Node)
{
	if (Verbose)
		printf("--- Finalize Faces ---\n");
	
	NumMerged = 0;
	NumSubdivided = 0;
	NumMakeFaces = 0;

	MakeFaces_r (Node);

	if (Verbose)
	{
		printf("TotalFaces             : %5i\n", NumMakeFaces);
		printf("Merged Faces           : %5i\n", NumMerged);
		printf("Subdivided Faces       : %5i\n", NumSubdivided);
		printf("FinalFaces             : %5i\n", (NumMakeFaces-NumMerged)+NumSubdivided);
	}
}

int32_t	MergedNodes;

//=======================================================================================
//=======================================================================================
void MergeNodes_r (GBSP_Node *Node)
{
	GBSP_Brush		*b, *Next;

	if (Node->PlaneNum == PLANENUM_LEAF)
		return;

	MergeNodes_r (Node->Children[0]);
	MergeNodes_r (Node->Children[1]);

	if (Node->Children[0]->PlaneNum == PLANENUM_LEAF && Node->Children[1]->PlaneNum == PLANENUM_LEAF)
	//if ((Node->Children[0]->Contents == Node->Children[1]->Contents) ||
	if ((Node->Children[0]->Contents & BSP_CONTENTS_SOLID2) && (Node->Children[1]->Contents & BSP_CONTENTS_SOLID2))
	if ((Node->Children[0]->Contents & 0xffff0000) == (Node->Children[1]->Contents & 0xffff0000))
	{
		if (Node->Faces)
			fprintf(stderr, "Node->Faces seperating BSP_CONTENTS_SOLID!");

		if (Node->Children[0]->Faces || Node->Children[1]->Faces)
			fprintf(stderr, "!Node->faces with children");

		// FIXME: free stuff
		Node->PlaneNum = PLANENUM_LEAF;
		//Node->Contents = BSP_CONTENTS_SOLID2;
		Node->Contents = Node->Children[0]->Contents;
		Node->Contents |= Node->Children[1]->Contents;

		Node->Detail = false;

		if (Node->BrushList)
			fprintf(stderr, "MergeNodes: node->brushlist");

		// combine brush lists
		Node->BrushList = Node->Children[1]->BrushList;

		for (b=Node->Children[0]->BrushList ; b ; b=Next)
		{
			Next = b->Next;
			b->Next = Node->BrushList;
			Node->BrushList = b;
		}

		MergedNodes++;
	}
}


//=======================================================================================
//=======================================================================================
void MergeNodes(GBSP_Node *Node)
{
	if (Verbose)
		printf("--- Merge Nodes ---\n");

	MergedNodes = 0;
	
	MergeNodes_r (Node);

	if (Verbose)
		printf("Num Merged             : %5i\n", MergedNodes);
}

//=======================================================================================
//	FreeBSP_r
//=======================================================================================
void FreeBSP_r(GBSP_Node *Node)
{
	if (!Node)
		return;

	if (Node->PlaneNum == PLANENUM_LEAF)
	{
		FreeNode(Node);
		return;
	}

	FreeBSP_r(Node->Children[0]);
	FreeBSP_r(Node->Children[1]);

	FreeNode(Node);
}

//=======================================================================================
//=======================================================================================
bool ProcessWorldModel(GBSP_Model *Model, MAP_Brush *MapBrushes)
{
	GBSP_Node	*Node;
	InitBSPTexInfo();
	GBSP_Brush		*Brushes;

	// Make the bsp brush list
	Brushes = MakeBSPBrushes(MapBrushes);

	if (!Brushes)
	{
		fprintf(stderr, "ProcessWorldModel:  Could not make real brushes.\n");
		return false;
	}

	// Csg the brushes together so none of them are overlapping
	// (this is legal, but makes alot more brushes that land in leafs in the long run...)
	Brushes = CSGBrushes(Brushes);

#if 0
	OutputBrushes("Test.3dt", Brushes);
#endif

	// Build the bsp
	Node = BuildBSP(Brushes);

	Model->Mins = TreeMins;
	Model->Maxs = TreeMaxs;
	
	if (!CreatePortals(Node, Model, false))	
	{
		fprintf(stderr, "Could not create the portals.\n");
		return false;
	}

	// Remove "unseen" leafs so MarkVisibleSides won't reach unseeen areas
	InitBSPEntities();
	if (RemoveHiddenLeafs(Node, &BSPModels[0].OutsideNode) == -1)
	{
		printf("Failed to remove hidden leafs.\n");
		return false;
	}
						    
	// Mark visible sided on the portals
	MarkVisibleSides (Node, MapBrushes);

	// Free vis portals...
	if (!FreePortals(Node))
	{
		printf("BuildBSP:  Could not free portals.\n");
		return false;
	}

	// Free the tree
	FreeBSP_r(Node);

	// Make the bsp brush list
	Brushes = MakeBSPBrushes(MapBrushes);

	if (!Brushes)
	{
		fprintf(stderr, "ProcessWorldModel:  Could not make real brushes.\n");
		return false;
	}

	// Csg the brushes
	Brushes = CSGBrushes(Brushes);

	// Build the bsp
	Node = BuildBSP(Brushes);

	Model->Mins = TreeMins;
	Model->Maxs = TreeMaxs;

	if (!CreatePortals(Node, Model, false))	
	{
		fprintf(stderr, "Could not create the portals.\n");
		return false;
	}

	// Remove hidden leafs one last time
	InitBSPEntities();
	if (RemoveHiddenLeafs(Node, &BSPModels[0].OutsideNode) == -1)
	{
		printf("Failed to remove hidden leafs.\n");
		return false;
	}

	// Mark visible sides one last time
	MarkVisibleSides (Node, MapBrushes);

	// Finally make the faces on the visible portals
	MakeFaces(Node);

	// Make the leaf LeafFaces (record what faces touch what leafs...)
	MakeLeafFaces(Node);

	// Free portals...
	if (!FreePortals(Node))
	{
		printf("BuildBSP:  Could not free portals.\n");
		return false;
	}

	// Prune the tree
	MergeNodes (Node);

	//FreeBrushList(Brushes);

	// Assign the root node to the model
	Model->RootNode[0] = Node;

	//ShowBrushHeap();

	return true;
}

//=======================================================================================
//=======================================================================================
bool ProcessSubModel(GBSP_Model *Model, MAP_Brush *MapBrushes)
{
	GBSP_Brush		*Brushes;
	GBSP_Node		*Node;

	// Make the bsp brush list
	Brushes = MakeBSPBrushes(MapBrushes);
	Brushes = CSGBrushes(Brushes);

	// Build the bsp
	Node = BuildBSP(Brushes);

	Model->Mins = TreeMins;
	Model->Maxs = TreeMaxs;

	if (!CreatePortals(Node, Model, false))	
	{
		fprintf(stderr, "Could not create the portals.\n");
		return NULL;
	}

	MarkVisibleSides (Node, MapBrushes);
						    
	MakeFaces(Node);

	if (!FreePortals(Node))
	{
		printf("BuildBSP:  Could not free portals.\n");
		return false;
	}

	MergeNodes (Node);

	// Assign the root node to the model
	Model->RootNode[0] = Node;

	return true;
}

char SkyNames[6][32] = {
	"SkyTop",
	"SkyBottom",
	"SkyLeft",
	"SkyRight",
	"SkyFront",
	"SkyBack"
};

//========================================================================================
//	GetSkyBoxInfo - STUBBED (no entity system)
//========================================================================================
bool GetSkyBoxInfo(void)
{
	// Stub: Just initialize sky data to defaults
	for (int i = 0; i < 6; i++)
		GFXSkyData.Textures[i] = -1;
	
	GFXSkyData.Axis = (Vector3){0, 0, 0};
	GFXSkyData.Dpm = 0.0f;
	GFXSkyData.DrawScale = 1.0f;
	
	return true;
}

//=======================================================================================
//	ProcessEntities - Process all brush entities (worldspawn + func_* entities)
//=======================================================================================
bool ProcessEntities(void)
{
	int32_t		i;
	bool		OldVerbose;

	OldVerbose = Verbose;

	for (i = 0; i < NumEntities; i++)
	{
		if (CancelRequest)
		{
			printf("Cancel requested...\n");
			return false;
		}
		
		if (!Entities[i].Brushes2)		// No model if no brushes
			continue;
		
		BSPModels[Entities[i].ModelNum].Origin = Entities[i].Origin;

		if (i == 0)
		{
			// Process worldspawn (entity 0)
			if (!ProcessWorldModel(&BSPModels[0], Entities[i].Brushes2))
				return false;
			
			if (!GetSkyBoxInfo())
			{
				fprintf(stderr, "Could not get SkyBox names from world...\n");
				return false;
			}
		}
		else
		{
			// Process brush entity (func_door, func_wall, etc.)
			if (!EntityVerbose)
				Verbose = false;

			if (!ProcessSubModel(&BSPModels[NumBSPModels], Entities[i].Brushes2))
				return false;
		}
		
		NumBSPModels++;
	}

	Verbose = OldVerbose;

	return true;
}

//=======================================================================================
//	SaveGFXMotionData
//=======================================================================================
// STUB: bool SaveGFXMotionData(geVFile *VFile)
// STUB: {
// STUB: 	int32_t		i, NumMotions;
// STUB: 	GBSP_Chunk	Chunk;
// STUB: 	long		StartPos;
// STUB: 	long		EndPos;
// STUB: 
// STUB: 	geVFile_Tell(VFile, &StartPos);
// STUB: 	Chunk.Type = GBSP_CHUNK_MOTIONS;
// STUB: 	Chunk.Size = 0;
// STUB: 	Chunk.Elements = 1;
// STUB: 
// STUB: 	WriteChunk(&Chunk, NULL, VFile);
// STUB: 
// STUB: 	geVFile_Printf(VFile, "Genesis_Motion_File v1.0\r\n");
// STUB: 
// STUB: 	// Count the motions
// STUB: 	NumMotions = 0;
// STUB: 	for (i=0; i< NumEntities; i++)
// STUB: 	{
// STUB: 		if (!Entities[i].Brushes2)	
// STUB: 			continue;
// STUB: 		
// STUB: 		if (!Entities[i].Motion)			// No motion data for model
// STUB: 			continue;
// STUB: 		
// STUB: 		NumMotions++;
// STUB: 	}
// STUB: 
// STUB: 	geVFile_Printf(VFile, "NumMotions %i\r\n", NumMotions);	// Save out number of motions
// STUB: 
// STUB: 	// For all entities that have motion, save their motion out in a motion file...
// STUB: 	for (i=0; i< NumEntities; i++)
// STUB: 	{
// STUB: 		if (!Entities[i].Brushes2)			// No model if no brushes
// STUB: 			continue;
// STUB: 
// STUB: 		if (!Entities[i].Motion)			// No motion data for model
// STUB: 			continue;
// STUB: 
// STUB: 		geVFile_Printf(VFile, "ModelNum %i\r\n", Entities[i].ModelNum);
// STUB: 			
// STUB: 		// Save out the motion
// STUB: 		if (!geMotion_WriteToFile(Entities[i].Motion, VFile))
// STUB: 		{
// STUB: 			fprintf(stderr, "Error saving motion data.\n");
// STUB: 			return false;
// STUB: 		}
// STUB: 		geMotion_Destroy(&Entities[i].Motion);
// STUB: 		Entities[i].Motion = NULL;
// STUB: 	}
// STUB: 
// STUB: 	geVFile_Tell(VFile, &EndPos);
// STUB: 
// STUB: 	geVFile_Seek(VFile, StartPos, GE_VFILE_SEEKSET);
// STUB: 
// STUB: 	Chunk.Type = GBSP_CHUNK_MOTIONS;
// STUB: 	Chunk.Size = 1;
// STUB: 	Chunk.Elements = EndPos - StartPos - sizeof(Chunk);
// STUB: 	WriteChunk(&Chunk, NULL, VFile);
// STUB: 	geVFile_Seek(VFile, EndPos, GE_VFILE_SEEKSET);
// STUB: 	NumGFXMotionBytes = Chunk.Elements;
// STUB: 
// STUB: 	return true;
// STUB: }
// STUB: 

