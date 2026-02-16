/****************************************************************************************/
/*  tjunct.c - T-junction fixing                                                       */
/*  Converted from Genesis3D TJunct.cpp to C99/raylib                                  */
/****************************************************************************************/
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp.h"
#include "common.h"
#include "mathlib.h"
#include "poly.h"
#include <raymath.h>

/****************************************************************************************/
/*  TJunct.cpp                                                                          */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Removes T-Juncts                                                       */
/*                                                                                      */
/****************************************************************************************/



#define OFF_EPSILON					0.05f

bool	FixTJuncts = true;

#define MAX_TEMP_INDEX_VERTS		1024

int32_t	NumTempIndexVerts;
int32_t	TempIndexVerts[MAX_TEMP_INDEX_VERTS];

int32_t	NumWeldedVerts;
int32_t	TotalIndexVerts;
Vector3	WeldedVerts[MAX_WELDED_VERTS];

int32_t	EdgeVerts[MAX_WELDED_VERTS];
int32_t	NumEdgeVerts;

int32_t	NumFixedFaces;
int32_t	NumTJunctions;

#define USE_HASHING

#define	HASH_SIZE	128							// Must be power of 2
#define	HASH_SIZE2	HASH_SIZE*HASH_SIZE			// Squared(HASH_SIZE)
#define HASH_SHIFT	8							// Log2(HASH_SIZE)+1

int32_t	VertexChain[MAX_WELDED_VERTS];			// the next vertex in a hash chain
int32_t	HashVerts[HASH_SIZE*HASH_SIZE];			// a vertex number, or 0 for no verts

//=====================================================================================
//	FinalizeFace
//=====================================================================================
bool FinalizeFace(GBSP_Face *Face, int32_t Base)
{
	int32_t	i;

	TotalIndexVerts += NumTempIndexVerts;

	if (NumTempIndexVerts == Face->NumIndexVerts)
		return true;
/*
	if (TexInfo[Face->TexInfo].Flags & TEXINFO_MIRROR)
		return true;

	if (TexInfo[Face->TexInfo].Flags & TEXINFO_SKY)
		return true;
//*/
	if (Face->IndexVerts)
		free(Face->IndexVerts); Face->IndexVerts = NULL;

	Face->IndexVerts = malloc(sizeof(int32_t) * NumTempIndexVerts);
		
	for (i=0; i< NumTempIndexVerts; i++)
		Face->IndexVerts[i] = TempIndexVerts[(i+Base)%NumTempIndexVerts];

	Face->NumIndexVerts = NumTempIndexVerts;

	NumFixedFaces++;

	return true;
}

Vector3	EdgeStart;
Vector3	EdgeDir;

bool TestEdge_r(float Start, float End, int32_t p1, int32_t p2, int32_t StartVert)
{
	int32_t	j, k;
	float	Dist;
	Vector3	Delta;
	Vector3	Exact;
	Vector3	Off;
	float	Error;
	Vector3	p;

	if (p1 == p2)
	{
		//printf("TestEdge_r:  Degenerate Edge.\n");
		return true;		// degenerate edge
	}

	for (k=StartVert ; k<NumEdgeVerts ; k++)
	{
		j = EdgeVerts[k];

		if (j==p1 || j == p2)
			continue;

		p = WeldedVerts[j];

		Delta = Vector3Subtract(p, EdgeStart);
		Dist = Vector3DotProduct(Delta, EdgeDir);
		
		if (Dist <= Start || Dist >= End)
			continue;	
		
		geVec3d_AddScaled(&EdgeStart, &EdgeDir, Dist, &Exact);
		Off = Vector3Subtract(p, Exact);
		Error = geVec3d_Length(&Off);

		if (fabs(Error) > OFF_EPSILON)
			continue;		

		// break the edge
		NumTJunctions++;

		TestEdge_r (Start, Dist, p1, j, k+1);
		TestEdge_r (Dist, End, j, p2, k+1);

		return true;
	}

	if (NumTempIndexVerts >= MAX_TEMP_INDEX_VERTS)
	{
		fprintf(stderr, "ERROR: Max Temp Index Verts.\n");
		return false;
	}

	TempIndexVerts[NumTempIndexVerts] = p1;
	NumTempIndexVerts++;

	return true;
}

void FindEdgeVerts(Vector3 *V1, Vector3 *V2)
{
#ifdef USE_HASHING
	int32_t	x1, y1, x2, y2;
	int32_t	t, x, y, Index;

	x1 = (HASH_SIZE2 + (int32_t)(V1->x+0.5)) >> HASH_SHIFT;
	y1 = (HASH_SIZE2 + (int32_t)(V1->y+0.5)) >> HASH_SHIFT;
	x2 = (HASH_SIZE2 + (int32_t)(V2->x+0.5)) >> HASH_SHIFT;
	y2 = (HASH_SIZE2 + (int32_t)(V2->y+0.5)) >> HASH_SHIFT;

	if (x1 > x2)
	{
		t = x1;
		x1 = x2;
		x2 = t;
	}
	if (y1 > y2)
	{
		t = y1;
		y1 = y2;
		y2 = t;
	}

	NumEdgeVerts = 0;
	for (x=x1 ; x <= x2 ; x++)
	{
		for (y=y1 ; y <= y2 ; y++)
		{
			for (Index = HashVerts[y*HASH_SIZE+x] ; Index ; Index = VertexChain[Index])
			{
				EdgeVerts[NumEdgeVerts++] = Index;
			}
		}
	}

#else
	int32_t		i;

	NumEdgeVerts = NumWeldedVerts-1;

	for (i=0; i< NumEdgeVerts; i++)
	{
		EdgeVerts[i] = i+1;
	}
#endif

}

//=====================================================================================
//	FixFaceTJunctions
//=====================================================================================
bool FixFaceTJunctions(GBSP_Node *Node, GBSP_Face *Face)
{
	int32_t	i, P1, P2;
	int32_t	Start[MAX_TEMP_INDEX_VERTS];
	int32_t	Count[MAX_TEMP_INDEX_VERTS];
	Vector3	Edge2;
	float	Len;
	int32_t	Base;

	NumTempIndexVerts = 0;
	
	for (i=0; i< Face->NumIndexVerts; i++)
	{
		P1 = Face->IndexVerts[i];
		P2 = Face->IndexVerts[(i+1)%Face->NumIndexVerts];

		EdgeStart = WeldedVerts[P1];
		Edge2 = WeldedVerts[P2];

		FindEdgeVerts(&EdgeStart, &Edge2);

		EdgeDir = Vector3Subtract(Edge2, EdgeStart);
		Len     = Vector3Length(EdgeDir);
		EdgeDir = Vector3Normalize(EdgeDir);

		Start[i] = NumTempIndexVerts;

		TestEdge_r(0.0f, Len, P1, P2, 0);

		Count[i] = NumTempIndexVerts - Start[i];
	}

	if (NumTempIndexVerts < 3)
	{
		Face->NumIndexVerts = 0;

		//printf("FixFaceTJunctions:  Face collapsed.\n");
		return true;
	}

	for (i=0 ; i<Face->NumIndexVerts; i++)
	{
		if (Count[i] == 1 && Count[(i+Face->NumIndexVerts-1)%Face->NumIndexVerts] == 1)
			break;
	}

	if (i == Face->NumIndexVerts)
	{
		Base = 0;
	}
	else
	{	// rotate the vertex order
		Base = Start[i];
	}

	if (!FinalizeFace(Face, Base))
		return false;

	return true;
}

//=====================================================================================
//	FixTJunctions_r
//=====================================================================================
bool FixTJunctions_r(GBSP_Node *Node)
{
	GBSP_Face	*Face, *Next;

	if (Node->PlaneNum == PLANENUM_LEAF)
		return true;

	for (Face = Node->Faces; Face; Face = Next)
	{
		Next = Face->Next;

		if (Face->Merged || Face->Split[0] || Face->Split[1])
			continue;

		FixFaceTJunctions(Node, Face);
	}
	
	if (!FixTJunctions_r(Node->Children[0]))
		return false;

	if (!FixTJunctions_r(Node->Children[1]))
		return false;

	return true;
}

//=====================================================================================
//	HashVert
//=====================================================================================
int32_t HashVert(Vector3 *Vert)
{
	int32_t	x, y;

	x = (HASH_SIZE2 + (int32_t)(Vert->x + 0.5)) >> HASH_SHIFT;
	y = (HASH_SIZE2 + (int32_t)(Vert->y + 0.5)) >> HASH_SHIFT;

	if ( x < 0 || x >= HASH_SIZE || y < 0 || y >= HASH_SIZE )
	{
		ERR_OUT("HashVert: Vert outside valid range");
		return -1;
	}
	
	return y*HASH_SIZE + x;
}

#define INTEGRAL_EPSILON	0.01f

//=====================================================================================
//	WeldVert
//=====================================================================================
#ifdef USE_HASHING
int32_t WeldVert(Vector3 *Vert)
{
	int32_t		i, h;

	for (i=0; i<3; i++)
		if (fabs(VectorToSUB(*Vert, i) - (int32_t)(VectorToSUB(*Vert, i)+0.5)) < INTEGRAL_EPSILON )
			VectorToSUB(*Vert, i) = (float)((int32_t)(VectorToSUB(*Vert, i)+0.5));

	h = HashVert(Vert);

	if (h == -1)
		return -1;

	for (i=HashVerts[h]; i; i = VertexChain[i])
	{
		if (i >= MAX_WELDED_VERTS)
		{
			fprintf(stderr, "ERROR: WeldVert:  Invalid hash vert.\n");
			return -1;
		}

		if (geVec3d_Compare(Vert, &WeldedVerts[i], VCOMPARE_EPSILON))
			return i;
	}

	if (NumWeldedVerts >= MAX_WELDED_VERTS)
	{
		fprintf(stderr, "ERROR: WeldVert:  Max welded verts.\n");
		return -1;
	}

	WeldedVerts[NumWeldedVerts] = *Vert;

	VertexChain[NumWeldedVerts] = HashVerts[h];
	HashVerts[h] = NumWeldedVerts;

	NumWeldedVerts++;

	return NumWeldedVerts-1;
}
#else
int32_t WeldVert(Vector3 *Vert)
{
	int32_t	i;
	Vector3	*pWeldedVerts;

	pWeldedVerts = WeldedVerts;

	for (i=0; i< NumWeldedVerts; i++)
	{
		if (geVec3d_Compare(Vert, pWeldedVerts, VCOMPARE_EPSILON))
			return i;

		pWeldedVerts++;
	}

	if (i >= MAX_WELDED_VERTS)
	{
		fprintf(stderr, "ERROR: WeldVert:  Max welded verts.\n");
		return -1;
	}

	WeldedVerts[i] = *Vert;

	NumWeldedVerts++;

	return i;
}
#endif
//=====================================================================================
//	GetFaceVertIndexNumbers
//=====================================================================================
bool GetFaceVertIndexNumbers(GBSP_Face *Face)
{
	int32_t	i, Index;
	Vector3	*Verts;

	NumTempIndexVerts = 0;
	
	Verts = Face->Poly->Verts;

	for (i=0; i< Face->Poly->NumVerts; i++)
	{
		if (NumTempIndexVerts >= MAX_TEMP_INDEX_VERTS)
		{
			fprintf(stderr, "ERROR: GetFaceVertIndexNumbers:  Max Temp Index Verts.\n");
			return false;
		}

		Index = WeldVert(&Verts[i]);

		if (Index == -1)
		{
			fprintf(stderr, "ERROR: GetFaceVertIndexNumbers:  Could not FindVert.\n");
			return false;
		}

		TempIndexVerts[NumTempIndexVerts] = Index;
		NumTempIndexVerts++;

		TotalIndexVerts++;
	}

	Face->NumIndexVerts = NumTempIndexVerts;
	Face->IndexVerts = malloc(sizeof(int32_t) * NumTempIndexVerts);

	if (!Face->IndexVerts)
	{
		fprintf(stderr, "ERROR: GetFaceVertIndexNumbers:  Out of memory for index list.\n");
		return false;
	}

	for (i=0; i < NumTempIndexVerts; i++)
		Face->IndexVerts[i] = TempIndexVerts[i];

	return true;
}

//=====================================================================================
//	GetFaceVertIndexNumbers_r
//=====================================================================================
bool GetFaceVertIndexNumbers_r(GBSP_Node *Node)
{
	GBSP_Face	*Face, *Next;

	if (Node->PlaneNum == PLANENUM_LEAF)
		return true;

	for (Face = Node->Faces; Face; Face = Next)
	{
		Next = Face->Next;

		if (Face->Merged || Face->Split[0] || Face->Split[1])
			continue;

		if (!GetFaceVertIndexNumbers(Face))
			return false;
	}
	
	if (!GetFaceVertIndexNumbers_r(Node->Children[0]))
		return false;

	if (!GetFaceVertIndexNumbers_r(Node->Children[1]))
		return false;

	return true;
}

//=====================================================================================
//	FixModelTJunctions
//=====================================================================================
bool FixModelTJunctions(void)
{
	int32_t		i;

	printf(" --- Weld Model Verts --- \n");

	NumWeldedVerts = 0;
	TotalIndexVerts = 0;
	
	for (i=0; i< NumBSPModels; i++)
	{

		if (!GetFaceVertIndexNumbers_r(BSPModels[i].RootNode[0]))
			return false;
	}

	if (!FixTJuncts)		// Skip if asked to do so...
		return true;

	printf(" --- Fix Model TJunctions --- \n");

	TotalIndexVerts = 0;

	NumTJunctions = 0;
	NumFixedFaces = 0;

	for (i=0; i< NumBSPModels; i++)
	{

		if (!FixTJunctions_r(BSPModels[i].RootNode[0]))
			return false;
	}

	if (Verbose)
	{
		printf(" Num TJunctions        : %5i\n", NumTJunctions);
		printf(" Num Fixed Faces       : %5i\n", NumFixedFaces);
	}

	return true;
}
