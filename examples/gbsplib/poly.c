/****************************************************************************************/
/*  poly.c                                                                              */
/*  Converted from Genesis3D POLY.CPP to C99/raylib                                     */
/****************************************************************************************/
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp.h"
#include "mathlib.h"
#include "poly.h"

extern GFX_TexInfo *GFXTexInfo;
#include <raymath.h>

/****************************************************************************************/
/*  Poly.Cpp                                                                            */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Various Poly routines (clipping, splitting, etc)                       */
/*                                                                                      */
/****************************************************************************************/



int32_t		NumSubdivides;
float		SubdivideSize = 235.0f;
//float		SubdivideSize = ((float)((16*MAX_LMAP_SIZE)-20));
//float		SubdivideSize = ((float)((16*MAX_LMAP_SIZE)-32));

int32_t		NumSubdivided;
int32_t		NumMerged;

//#define FREAKED_OUT

bool	gCountVerts;
int32_t		gTotalVerts;
int32_t		gPeekVerts;	

//#define DEGENERATE_EPSILON		0.05f
#define DEGENERATE_EPSILON			0.001f

//
//	Polys
//

//====================================================================================
//	AllocPoly
//====================================================================================
GBSP_Poly *AllocPoly(int32_t NumVerts)
{
	GBSP_Poly	*NewPoly;

	if (NumVerts > 1000)
	{
		fprintf(stderr, "ERROR: Bogus verts: %i\n", NumVerts);
		return NULL;
	}
	
	NewPoly = malloc(sizeof(GBSP_Poly));

	if (!NewPoly)
	{
		fprintf(stderr, "ERROR: AllocPoly:  Not enough memory.\n");
		return NULL;
	}
	
	NewPoly->Verts = malloc(sizeof(Vector3) * NumVerts);
	
	if (!NewPoly->Verts)
	{
		fprintf(stderr, "ERROR: AllocPoly:  Not enough memory for verts: %i\n", NumVerts);
		free(NewPoly); NewPoly = NULL;
		return NULL;
	}

	NewPoly->NumVerts = NumVerts;

#ifdef SHOW_DEBUG_STATS
	if (gCountVerts)
	{
		gTotalVerts += NumVerts;
		if (gTotalVerts > gPeekVerts)
			gPeekVerts = gTotalVerts;
	}
#endif

	return NewPoly;
}

//====================================================================================
//	FreePoly
//====================================================================================
void FreePoly(GBSP_Poly *Poly)
{
	if (!Poly)
	{
		printf("*WARNING* FreePoly: NULL Poly.\n");
		return;
	}

	if (Poly->Verts)
	{
		free(Poly->Verts); Poly->Verts = NULL;

	#ifdef SHOW_DEBUG_STATS
		if (gCountVerts)
		{
			gTotalVerts -= Poly->NumVerts;
		}
	#endif
	}

	free(Poly); Poly = NULL;
}

//=====================================================================================
//	TextureAxisFromPlane
//=====================================================================================
bool TextureAxisFromPlane(GBSP_Plane *Pln, Vector3 *Xv, Vector3 *Yv)
{
	int32_t	BestAxis;
	float	Dot,Best;
	int32_t		i;
	
	Best = 0.0f;
	BestAxis = -1;
	
	for (i=0 ; i<3 ; i++)
	{
		Dot = (float)fabs(VectorToSUB(Pln->Normal, i));
		if (Dot > Best)
		{
			Best = Dot;
			BestAxis = i;
		}
	}

	switch(BestAxis)
	{
		case 0:						// X
			Xv->x = (float)0;
			Xv->y = (float)0;
			Xv->z = (float)1;

			Yv->x = (float)0;
			Yv->y = (float)-1;
			Yv->z = (float)0;
			break;
		case 1:						// Y
			Xv->x = (float)1;
			Xv->y = (float)0;
			Xv->z = (float)0;

			Yv->x = (float)0;
			Yv->y = (float)0;
			Yv->z = (float)1;
			break;
		case 2:						// Z
			Xv->x = (float)1;
			Xv->y = (float)0;
			Xv->z = (float)0;

			Yv->x = (float)0;
			Yv->y = (float)-1;
			Yv->z = (float)0;
			break;
		default:
			fprintf(stderr, "ERROR: TextureAxisFromPlane: No Axis found.\n");
			return false;
			break;
	}

	return true;
}

//====================================================================================
//	CreatePolyFromPlane	
//	Create a huge poly with normal pointing in same direction as Plane
//====================================================================================
GBSP_Poly *CreatePolyFromPlane(GBSP_Plane *Plane)
{
	Vector3		Normal = Plane->Normal;
	Vector3		UpVect = {0.0f, 0.0f, 0.0f};
	Vector3		RightVect, Org;
	GBSP_Poly	*Poly;
	
	if (!TextureAxisFromPlane(Plane, &RightVect, &UpVect))
		return NULL;

	// Produce some walk vectors
	RightVect = Vector3CrossProduct(Normal, UpVect);		// Get right vector
	UpVect = Vector3CrossProduct(Normal, RightVect);		// Get new Up vector from correct Right vector

	UpVect = Vector3Normalize(UpVect);
	RightVect = Vector3Normalize(RightVect);

	// Create the poly with 4 verts
	Poly = AllocPoly(4);

	if (!Poly)
	{
		fprintf(stderr, "ERROR: CreatePolyFromPlane:  Not enough memory for new poly!\n");
		return NULL;
	}

	Org = Vector3Scale(Normal, Plane->Dist);						// Get the Org
	UpVect = Vector3Scale(UpVect, (float)MIN_MAX_BOUNDS);			// Scale walk vectors
	RightVect = Vector3Scale(RightVect, (float)MIN_MAX_BOUNDS);

	Poly->Verts[0] = Vector3Subtract(Org, RightVect);
	Poly->Verts[0] = Vector3Add(Poly->Verts[0], UpVect);

	Poly->Verts[1] = Vector3Add(Org, RightVect);
	Poly->Verts[1] = Vector3Add(Poly->Verts[1], UpVect);

	Poly->Verts[2] = Vector3Add(Org, RightVect);
	Poly->Verts[2] = Vector3Subtract(Poly->Verts[2], UpVect);

	Poly->Verts[3] = Vector3Subtract(Org, RightVect);
	Poly->Verts[3] = Vector3Subtract(Poly->Verts[3], UpVect);

#ifdef FREAKED_OUT	
	Vector3	Normal2, V1, V2;
	V1 = Vector3Subtract(Poly->Verts[0], Poly->Verts[1]);
	V2 = Vector3Subtract(Poly->Verts[2], Poly->Verts[1]);
	Normal2 = Vector3CrossProduct(V1, V2);
	Normal2 = Vector3Normalize(Normal2);
	
	Normal = Plane->Normal;
	
	if (!(fabsf(Normal.x-Normal2.x)<VCOMPARE_EPSILON && fabsf(Normal.y-Normal2.y)<VCOMPARE_EPSILON && fabsf(Normal.z-Normal2.z)<VCOMPARE_EPSILON))
	{
		fprintf(stderr, "ERROR: Normal1 X:%2.2f, Y:%2.2f, Z:%2.2f\n", Normal.x, Normal.y, Normal.z);
		fprintf(stderr, "ERROR: Normal2 X:%2.2f, Y:%2.2f, Z:%2.2f\n", Normal2.x, Normal2.y, Normal2.z);
		fprintf(stderr, "ERROR: CreatePolyFromPlane:  Normals do not match.\n");
		return NULL;
	}
#endif	

	return Poly;

}

#define MAX_TEMP_VERTS	200

Vector3 TempVerts[MAX_TEMP_VERTS];
Vector3 TempVerts2[MAX_TEMP_VERTS];

#define CLIP_EPSILON	(float)0.001

//====================================================================================
//	ClipPoly
//====================================================================================
bool ClipPoly(GBSP_Poly *InPoly, GBSP_Plane *Plane, bool FlipTest, GBSP_Poly **OutPoly)
{
	GBSP_Poly	*NewPoly = NULL;
	Vector3		*pInVert, Vert1, Vert2;
	Vector3		*pFrontVert, *Verts;
	int32_t		i, NextVert, NewNumVerts = 0;
	float		Scale;

	Vector3		Normal = Plane->Normal;
	float		Dist = Plane->Dist;
	int32_t		NumVerts = InPoly->NumVerts;
	int32_t		VSides[100];
	float		VDist[100];
	int32_t		CountSides[3];

	*OutPoly = NULL;

	if (NumVerts >= 100)
	{
		fprintf(stderr, "ERROR: ClipPoly:  Too many verts.\n");
		return false;
	}
	
	memset(CountSides, 0, sizeof(CountSides));

	Verts = InPoly->Verts;
	pInVert = Verts;
	pFrontVert = TempVerts;
	
	if (FlipTest)		// Flip the normal and dist
	{
		Normal.x = -Normal.x;
		Normal.y = -Normal.y;
		Normal.z = -Normal.z;
		Dist = -Dist;
	}
	
	// See what side of plane each vert is on
	for (i=0; i< NumVerts; i++)
	{
		VDist[i] = Vector3DotProduct(Verts[i], Normal) - Dist;
		if (VDist[i] > ON_EPSILON)
			VSides[i] = 0;
		else if (VDist[i] < -ON_EPSILON)
			VSides[i] = 1;
		else
			VSides[i] = 2;

		CountSides[VSides[i]]++;
	}

	if (!CountSides[0])
	{
		FreePoly(InPoly);
		*OutPoly = NULL;
		return true;
	}

	if (!CountSides[1])
	{
		*OutPoly = InPoly;
		return true;
	}

	// Else we have to split this sucker
	for (i=0; i< NumVerts; i++)
	{
		Vert1 = Verts[i];

		if (VSides[i] == 2)				// On plane, put on both sides
		{
            *pFrontVert++ = Vert1;
			continue;
		}
		
		if (VSides[i] == 0)				// Front side, put on front list
            *pFrontVert++ = Vert1;

		NextVert = (i+1) % NumVerts;
	 	
		if (VSides[NextVert] == 2 || VSides[NextVert] == VSides[i])
			continue;

		Vert2 = Verts[NextVert];
		Scale = VDist[i] / (VDist[i] - VDist[NextVert]);

		pFrontVert->x = Vert1.x + (Vert2.x - Vert1.x) * Scale;
		pFrontVert->y = Vert1.y + (Vert2.y - Vert1.y) * Scale;
		pFrontVert->z = Vert1.z + (Vert2.z - Vert1.z) * Scale;

		pFrontVert++;
	}

	FreePoly(InPoly);		// Don't need this anymore

    NewNumVerts = pFrontVert - TempVerts;

	if (NewNumVerts < 3)
	{
		*OutPoly = NULL;
		return true;
	}
	
	NewPoly = AllocPoly(NewNumVerts);

	if (!NewPoly)
	{
		fprintf(stderr, "ERROR: ClipPoly:  Not enough mem for new poly.\n");
		return false;
	}

	for (i = 0; i< NewNumVerts; i++)
		NewPoly->Verts[i] =	TempVerts[i];

	*OutPoly = NewPoly;
	return true;
}

//====================================================================================
//	ClipPolyEpsilon
//====================================================================================
bool ClipPolyEpsilon(GBSP_Poly *InPoly, float Epsilon, GBSP_Plane *Plane, bool FlipTest, GBSP_Poly **OutPoly)
{
	GBSP_Poly	*NewPoly = NULL;
	Vector3		*pInVert, Vert1, Vert2;
	Vector3		*pFrontVert, *Verts;
	int32_t		i, NextVert, NewNumVerts = 0;
	float		Scale;

	Vector3		Normal = Plane->Normal;
	float		Dist = Plane->Dist;
	int32_t		NumVerts = InPoly->NumVerts;
	int32_t		VSides[100];
	float		VDist[100];
	int32_t		CountSides[3];

	*OutPoly = NULL;

	if (NumVerts >= 100)
	{
		fprintf(stderr, "ERROR: ClipPoly:  Too many verts.\n");
		return false;
	}
	
	memset(CountSides, 0, sizeof(CountSides));

	Verts = InPoly->Verts;
	pInVert = Verts;
	pFrontVert = TempVerts;
	
	if (FlipTest)		// Flip the normal and dist
	{
		Normal.x = -Normal.x;
		Normal.y = -Normal.y;
		Normal.z = -Normal.z;
		Dist = -Dist;
	}
	
	// See what side of plane each vert is on
	for (i=0; i< NumVerts; i++)
	{
		VDist[i] = Vector3DotProduct(Verts[i], Normal) - Dist;
		if (VDist[i] > Epsilon)
			VSides[i] = 0;
		else if (VDist[i] < -Epsilon)
			VSides[i] = 1;
		else
			VSides[i] = 2;

		CountSides[VSides[i]]++;
	}

	if (!CountSides[0])
	{
		FreePoly(InPoly);
		*OutPoly = NULL;
		return true;
	}

	if (!CountSides[1])
	{
		*OutPoly = InPoly;
		return true;
	}

	// Else we have to split this sucker
	for (i=0; i< NumVerts; i++)
	{
		Vert1 = Verts[i];

		if (VSides[i] == 2)				// On plane, put on both sides
		{
            *pFrontVert++ = Vert1;
			continue;
		}
		
		if (VSides[i] == 0)				// Front side, put on front list
            *pFrontVert++ = Vert1;

		NextVert = (i+1) % NumVerts;
	 	
		if (VSides[NextVert] == 2 || VSides[NextVert] == VSides[i])
			continue;

		Vert2 = Verts[NextVert];
		Scale = VDist[i] / (VDist[i] - VDist[NextVert]);

		pFrontVert->x = Vert1.x + (Vert2.x - Vert1.x) * Scale;
		pFrontVert->y = Vert1.y + (Vert2.y - Vert1.y) * Scale;
		pFrontVert->z = Vert1.z + (Vert2.z - Vert1.z) * Scale;

		pFrontVert++;
	}

	FreePoly(InPoly);		// Don't need this anymore

    NewNumVerts = pFrontVert - TempVerts;

	if (NewNumVerts < 3)
	{
		*OutPoly = NULL;
		return true;
	}
	
	NewPoly = AllocPoly(NewNumVerts);

	if (!NewPoly)
	{
		fprintf(stderr, "ERROR: ClipPoly:  Not enough mem for new poly.\n");
		return false;
	}

	for (i = 0; i< NewNumVerts; i++)
		NewPoly->Verts[i] =	TempVerts[i];

	*OutPoly = NewPoly;
	return true;
}

//====================================================================================
//	SplitPoly
//====================================================================================
bool SplitPoly(GBSP_Poly *InPoly, GBSP_Plane *Plane, GBSP_Poly **Front, GBSP_Poly **Back, 
			   bool FlipTest)
{
	GBSP_Poly	*NewPoly = NULL;
	Vector3		*pInVert, Vert1, Vert2;
	Vector3		*pFrontVert, *pBackVert, *Verts;
	int32_t		i, NextVert, NewNumVerts = 0;
	float		Scale;

	Vector3		Normal = Plane->Normal;
	float		Dist = Plane->Dist, *pDist;
	int32_t		NumVerts = InPoly->NumVerts;
	int32_t		VSides[100];
	float		VDist[100];
	int32_t		CountSides[3];

	if (NumVerts >= 100)
	{
		fprintf(stderr, "ERROR: SplitPoly:  Too many verts.\n");
		return false;
	}
	
	memset(CountSides, 0, sizeof(CountSides));

	Verts = InPoly->Verts;
	pInVert = Verts;
	pFrontVert = TempVerts;
	pBackVert = TempVerts2;
	
	if (FlipTest)		// Flip the normal and dist
	{
		Normal.x = -Normal.x;
		Normal.y = -Normal.y;
		Normal.z = -Normal.z;
		Dist = -Dist;
	}
	
	// See what side of plane each vert is on
	if (Plane->Type < PLANE_ANYX)
	{
		pDist = &VectorToSUB(Verts[0], Plane->Type);

		for (i=0; i< NumVerts ; i++)
		{

			//VDist[i] = VectorToSUB(Verts[i], Plane->Type) - Plane->Dist;
			VDist[i] = *pDist - Plane->Dist;
			if (FlipTest)
				VDist[i] = -VDist[i];

			if (VDist[i] > ON_EPSILON)
				VSides[i] = 0;
			else if (VDist[i] < -ON_EPSILON)
				VSides[i] = 1;
			else
				VSides[i] = 2;

			CountSides[VSides[i]]++;

			pDist += 3;
		}
	}
	else
	{
		for (i=0; i< NumVerts; i++)
		{
			VDist[i] = Vector3DotProduct(Verts[i], Normal) - Dist;
			if (VDist[i] > ON_EPSILON)
				VSides[i] = 0;
			else if (VDist[i] < -ON_EPSILON)
				VSides[i] = 1;
			else
				VSides[i] = 2;

			CountSides[VSides[i]]++;
		}
	}

	if (!CountSides[0] && !CountSides[1])
	{
		*Back = NULL;
		*Front = InPoly;
		return true;
	}

	// Get out quick, if no splitting is neccesary
	if (!CountSides[0])
	{
		*Front = NULL;
		*Back = InPoly;
		return true;
	}
	if (!CountSides[1])
	{
		*Back = NULL;
		*Front = InPoly;
		return true;
	}

	// Else we have to split this sucker
	for (i=0; i< NumVerts; i++)
	{
		Vert1 = Verts[i];

		if (VSides[i] == 2)				// On plane, put on both sides
		{
            *pFrontVert++ = Vert1;
            *pBackVert++ = Vert1;
			continue;
		}
		
		if (VSides[i] == 0)				// Front side, put on front list
            *pFrontVert++ = Vert1;
		else if (VSides[i] == 1)		// Back side, put on back list
            *pBackVert++ = Vert1;


		NextVert = (i+1) % NumVerts;
	 	
		if (VSides[NextVert] == 2 || VSides[NextVert] == VSides[i])
			continue;

		Vert2 = Verts[NextVert];
		Scale = VDist[i] / (VDist[i] - VDist[NextVert]);

		pFrontVert->x = Vert1.x + (Vert2.x - Vert1.x) * Scale;
		pFrontVert->y = Vert1.y + (Vert2.y - Vert1.y) * Scale;
		pFrontVert->z = Vert1.z + (Vert2.z - Vert1.z) * Scale;

		*pBackVert = *pFrontVert;
		pFrontVert++;
		pBackVert++;
	}

	FreePoly(InPoly);		// Don't need this anymore

    NewNumVerts = pFrontVert - TempVerts;

	if (NewNumVerts < 3)
		*Front = NULL;
	else
	{
		NewPoly = AllocPoly(NewNumVerts);

		if (!NewPoly)
		{
			fprintf(stderr, "ERROR: SplitPoly:  Not enough mem for new poly.\n");
			return false;
		}

		for (i = 0; i< NewNumVerts; i++)
			NewPoly->Verts[i] =	TempVerts[i];

		*Front = NewPoly;
	}
    
	NewNumVerts = pBackVert - TempVerts2;
	
	if (NewNumVerts < 3)
		*Back = NULL;
	else
	{
		NewPoly = AllocPoly(NewNumVerts);

		if (!NewPoly)
		{
			fprintf(stderr, "ERROR: SplitPoly:  Not enough mem for new poly.\n");
			return false;
		}

		for (i = 0; i< NewNumVerts; i++)
			NewPoly->Verts[i] = TempVerts2[i];

		*Back = NewPoly;
	}

	return true;
}

//====================================================================================
//	SplitPoly
//====================================================================================
bool SplitPolyEpsilon(GBSP_Poly *InPoly, float Epsilon, GBSP_Plane *Plane, GBSP_Poly **Front, GBSP_Poly **Back, 
			   bool FlipTest)
{
	GBSP_Poly	*NewPoly = NULL;
	Vector3		*pInVert, Vert1, Vert2;
	Vector3		*pFrontVert, *pBackVert, *Verts;
	int32_t		i, NextVert, NewNumVerts = 0;
	float		Scale;

	Vector3		Normal = Plane->Normal;
	float		Dist = Plane->Dist, *pDist;
	int32_t		NumVerts = InPoly->NumVerts;
	int32_t		VSides[100];
	float		VDist[100];
	int32_t		CountSides[3];

	if (NumVerts >= 100)
	{
		fprintf(stderr, "ERROR: SplitPoly:  Too many verts.\n");
		return false;
	}
	
	memset(CountSides, 0, sizeof(CountSides));

	Verts = InPoly->Verts;
	pInVert = Verts;
	pFrontVert = TempVerts;
	pBackVert = TempVerts2;
	
	if (FlipTest)		// Flip the normal and dist
	{
		Normal.x = -Normal.x;
		Normal.y = -Normal.y;
		Normal.z = -Normal.z;
		Dist = -Dist;
	}
	
	// See what side of plane each vert is on
	if (Plane->Type < PLANE_ANYX)
	{
		pDist = &VectorToSUB(Verts[0], Plane->Type);

		for (i=0; i< NumVerts ; i++)
		{

			//VDist[i] = VectorToSUB(Verts[i], Plane->Type) - Plane->Dist;
			VDist[i] = *pDist - Plane->Dist;
			if (FlipTest)
				VDist[i] = -VDist[i];

			if (VDist[i] > Epsilon)
				VSides[i] = 0;
			else if (VDist[i] < -Epsilon)
				VSides[i] = 1;
			else
				VSides[i] = 2;

			CountSides[VSides[i]]++;

			pDist += 3;
		}
	}
	else
	{
		for (i=0; i< NumVerts; i++)
		{
			VDist[i] = Vector3DotProduct(Verts[i], Normal) - Dist;
			if (VDist[i] > Epsilon)
				VSides[i] = 0;
			else if (VDist[i] < -Epsilon)
				VSides[i] = 1;
			else
				VSides[i] = 2;

			CountSides[VSides[i]]++;
		}
	}

	// Get out quick, if no splitting is neccesary
	if (!CountSides[0])
	{
		*Front = NULL;
		*Back = InPoly;
		return true;
	}
	if (!CountSides[1])
	{
		*Back = NULL;
		*Front = InPoly;
		return true;
	}

	// Else we have to split this sucker
	for (i=0; i< NumVerts; i++)
	{
		Vert1 = Verts[i];

		if (VSides[i] == 2)				// On plane, put on both sides
		{
            *pFrontVert++ = Vert1;
            *pBackVert++ = Vert1;
			continue;
		}
		
		if (VSides[i] == 0)				// Front side, put on front list
            *pFrontVert++ = Vert1;
		else if (VSides[i] == 1)		// Back side, put on back list
            *pBackVert++ = Vert1;


		NextVert = (i+1) % NumVerts;
	 	
		if (VSides[NextVert] == 2 || VSides[NextVert] == VSides[i])
			continue;

		Vert2 = Verts[NextVert];
		Scale = VDist[i] / (VDist[i] - VDist[NextVert]);

		pFrontVert->x = Vert1.x + (Vert2.x - Vert1.x) * Scale;
		pFrontVert->y = Vert1.y + (Vert2.y - Vert1.y) * Scale;
		pFrontVert->z = Vert1.z + (Vert2.z - Vert1.z) * Scale;

		*pBackVert = *pFrontVert;
		pFrontVert++;
		pBackVert++;
	}

	FreePoly(InPoly);		// Don't need this anymore

    NewNumVerts = pFrontVert - TempVerts;

	if (NewNumVerts < 3)
		*Front = NULL;
	else
	{
		NewPoly = AllocPoly(NewNumVerts);

		if (!NewPoly)
		{
			fprintf(stderr, "ERROR: SplitPoly:  Not enough mem for new poly.\n");
			return false;
		}

		for (i = 0; i< NewNumVerts; i++)
			NewPoly->Verts[i] =	TempVerts[i];

		*Front = NewPoly;
	}
    
	NewNumVerts = pBackVert - TempVerts2;
	
	if (NewNumVerts < 3)
		*Back = NULL;
	else
	{
		NewPoly = AllocPoly(NewNumVerts);

		if (!NewPoly)
		{
			fprintf(stderr, "ERROR: SplitPoly:  Not enough mem for new poly.\n");
			return false;
		}

		for (i = 0; i< NewNumVerts; i++)
			NewPoly->Verts[i] = TempVerts2[i];

		*Back = NewPoly;
	}

	return true;
}

//====================================================================================
//	RemoveDegenerateEdges
//====================================================================================
void RemoveDegenerateEdges(GBSP_Poly *Poly)
{
	int32_t		i, NumVerts;
	Vector3		*Verts, V1, V2, NVerts[1000], Vec;
	bool	Bad = false;
	int32_t		NumNVerts = 0;

	Verts = Poly->Verts;
	NumVerts = Poly->NumVerts;

	for (i=0; i< NumVerts; i++)
	{
		V1 = Verts[i];
		V2 = Verts[(i+1)%NumVerts];

		Vec = Vector3Subtract(V1, V2);

		if (Vector3Length(Vec) > DEGENERATE_EPSILON)
		{
			NVerts[NumNVerts++] = V1;	
		}
		else
			Bad = true;
	}

	if (Bad)
	{
		//Hook.Printf("Removing degenerate edge...\n");

		Poly->NumVerts = NumNVerts;
		for (i=0; i< NumNVerts; i++)
		{
			Verts[i] = NVerts[i];
		}
	}
}

//====================================================================================
//	RemoveDegenerateEdges2
//====================================================================================
bool RemoveDegenerateEdges2(GBSP_Poly *Poly)
{
	int32_t		i, NumVerts;
	Vector3		*Verts, V1, V2, NVerts[1000], Vec;
	bool	Bad = false;
	int32_t		NumNVerts = 0;

	Verts = Poly->Verts;
	NumVerts = Poly->NumVerts;

	for (i=0; i< NumVerts; i++)
	{
		V1 = Verts[i];
		V2 = Verts[(i+1)%NumVerts];

		Vec = Vector3Subtract(V1, V2);

		if (Vector3Length(Vec) > DEGENERATE_EPSILON)
		{
			NVerts[NumNVerts++] = V1;	
		}
		else
			Bad = true;
	}

	if (NumNVerts < 3)
		return false;

	if (Bad)
	{
		Poly->NumVerts = NumNVerts;
		for (i=0; i< NumNVerts; i++)
		{
			Verts[i] = NVerts[i];
		}
	}

	return true;
}

//====================================================================================
//	PolyArea
//====================================================================================
float PolyArea(GBSP_Poly *Poly)
{
	int32_t	i;
	Vector3	Vect1, Vect2, Cross;
	float	Total;

	Total = 0.0f;

	for (i=2 ; i<Poly->NumVerts ; i++)
	{
		Vect1 = Vector3Subtract(Poly->Verts[i-1], Poly->Verts[0]);
		Vect2 = Vector3Subtract(Poly->Verts[i]  , Poly->Verts[0]);
		Cross = Vector3CrossProduct(Vect1, Vect2);
		Total += (float)0.5 * Vector3Length(Cross);
	}

	return (float)Total;
}

#define BOGUS_VERTS		256
//====================================================================================
//	CopyPoly
//====================================================================================
bool CopyPoly(GBSP_Poly *In, GBSP_Poly **Out)
{
	GBSP_Poly	*NewPoly;
	int32_t		i;

	if (!In)
	{
		fprintf(stderr, "ERROR: CopyPoly:  NULL Poly!!!\n");
		return false;
	}

	if (In->NumVerts <= 0 || In->NumVerts >= BOGUS_VERTS)
	{
		fprintf(stderr, "ERROR: CopyPoly:  Bogus number of verts:  %i\n", In->NumVerts);
		return false;
	}
	
	NewPoly = AllocPoly(In->NumVerts);
	if (!NewPoly)
	{
		return false;
	}

	for (i=0; i<In->NumVerts; i++)
		NewPoly->Verts[i] = In->Verts[i];
	
	*Out = NewPoly;

	return true;
}

//====================================================================================
//	CopyPoly2
//====================================================================================
GBSP_Poly *CopyPoly2(GBSP_Poly *In)
{
	GBSP_Poly	*NewPoly;
	int32_t		i;

	if (!In)
	{
		fprintf(stderr, "ERROR: CopyPoly:  NULL Poly!!!\n");
		return NULL;
	}

	if (In->NumVerts <= 0 || In->NumVerts >= BOGUS_VERTS)
	{
		fprintf(stderr, "ERROR: CopyPoly:  Bogus number of verts:  %i\n", In->NumVerts);
		return NULL;
	}
	
	NewPoly = AllocPoly(In->NumVerts);

	if (!NewPoly)
		return NULL;

	for (i=0; i<In->NumVerts; i++)
		NewPoly->Verts[i] = In->Verts[i];
	
	return NewPoly;
}

//====================================================================================
//	ReversePoly
//====================================================================================
bool ReversePoly(GBSP_Poly *In, GBSP_Poly **Out)
{
	GBSP_Poly	*NewPoly;
	int32_t		i;

	NewPoly = AllocPoly(In->NumVerts);
	if (!NewPoly)
	{
		return false;
	}

	for (i=0; i<In->NumVerts; i++)
		NewPoly->Verts[i] = In->Verts[In->NumVerts-i-1];
	
	*Out = NewPoly;

	return true;
}

//====================================================================================
//	PolyCenter
//====================================================================================
void PolyCenter(GBSP_Poly *Poly, Vector3 *Center)
{
	int32_t	i;

	*Center = (Vector3){0.0f, 0.0f, 0.0f};

	for (i=0; i< Poly->NumVerts; i++)
		*Center = Vector3Add(Poly->Verts[i], *Center);

	*Center = Vector3Scale(*Center, 1.0f/(float)Poly->NumVerts);
}

#define EDGE_LENGTH 0.1f

//====================================================================================
//	PolyIsTiny
//====================================================================================
bool PolyIsTiny (GBSP_Poly *Poly)
{
	int32_t	i, j;
	float	Len;
	Vector3	Delta;
	int32_t	Edges;

	Edges = 0;

	//return false;

	for (i=0 ; i<Poly->NumVerts ; i++)
	{
		j = i == Poly->NumVerts - 1 ? 0 : i+1;

		Delta = Vector3Subtract(Poly->Verts[j], Poly->Verts[i]);

		Len = Vector3Length(Delta);

		if (Len > EDGE_LENGTH)
		{
			if (++Edges == 3)
				return false;
		}
	}

	//printf("Poly is tiny...\n");

	return true;
}

//
//	Faces
//

//====================================================================================
//	AllocFace
//====================================================================================
GBSP_Face *AllocFace(int32_t NumVerts)
{
	GBSP_Face *Face;

	Face = malloc(sizeof(GBSP_Face));

	if (!Face)
		return NULL;

	memset(Face, 0, sizeof(GBSP_Face));

	if (NumVerts)
	{
		Face->Poly = AllocPoly(NumVerts);
		if (!Face->Poly)
		{
			FreeFace(Face);
			return NULL;
		}
	}
	else
		Face->Poly = NULL;

	return Face;
}

//====================================================================================
//	FreeFace
//====================================================================================
void FreeFace(GBSP_Face *Face)
{
	if (!Face)
	{
		printf("*WARNING* FreeFace: NULL Face.\n");
		return;
	}

	//if (!Face->Poly)
	//	Hook.Error("FreeFace: NULL Poly.\n");
	if (Face->Poly)
		FreePoly(Face->Poly);
	Face->Poly = NULL;

	if (Face->IndexVerts)
		free(Face->IndexVerts); Face->IndexVerts = NULL;
	Face->IndexVerts = NULL;
	
	free(Face); Face = NULL;
}

//====================================================================================
//	void SplitFace
//====================================================================================
bool SplitFace(GBSP_Face *In, GBSP_Plane *Split, GBSP_Face **Front, GBSP_Face **Back, bool FlipTest)
{
	GBSP_Poly	*F, *B;
	GBSP_Face	*NewFace;

	if (!SplitPoly(In->Poly, Split, &F, &B, FlipTest))
		return false;

	if (!F && !B)
	{
		fprintf(stderr, "ERROR: SplitFace:  Poly was clipped away.\n");
		return false;
	}

	if (!F)
	{
		In->Poly = B;
		*Back = In;
		*Front = NULL;
		return true;
	}
	if (!B)
	{
		In->Poly = F;
		*Front = In;
		*Back = NULL;
		return true;
	}

	NewFace = malloc(sizeof(GBSP_Face));
	if (!NewFace)
	{
		fprintf(stderr, "ERROR: SplitFace:  Out of memory for new face.\n");
		return false;
	}

	*NewFace = *In;

	In->Poly = F;
	NewFace->Poly = B;

	*Front = In;
	*Back = NewFace;

	return true;
}

//====================================================================================
//	CopyFace
//====================================================================================
bool CopyFace(GBSP_Face *In, GBSP_Face **Out)
{
	GBSP_Face	*NewFace;
	GBSP_Poly	*NewPoly;
	int32_t		i;

	NewFace = AllocFace(0);

	if (!NewFace)
	{
		fprintf(stderr, "ERROR: CopyFaceList:  Out of memory for new face.\n");
		return false;
	}

	*NewFace = *In;
	NewPoly = AllocPoly(In->Poly->NumVerts);

	if (!NewPoly)
	{
		fprintf(stderr, "ERROR: CopyFaceList:  Out of memory for newpoly.\n");
		return false;
	}
		
	NewFace->Poly = NewPoly;

	for (i=0; i< In->Poly->NumVerts; i++)
		NewFace->Poly->Verts[i] = In->Poly->Verts[i];

	return true;
}

//====================================================================================
//	MergeFaceList2
//====================================================================================
bool MergeFaceList2(GBSP_Face *Faces)
{
	GBSP_Face	*Face1, *Face2, *End, *Merged;

	for (Face1 = Faces ; Face1 ; Face1 = Face1->Next)
	{
		if (Face1->Poly->NumVerts == -1)
			continue;

		if (Face1->Merged || Face1->Split[0] || Face1->Split[1])
			continue;

		for (Face2 = Faces ; Face2 != Face1 ; Face2 = Face2->Next)
		{
			if (Face2->Poly->NumVerts == -1)
				continue;

			if (Face2->Merged || Face2->Split[0] || Face2->Split[1])
				continue;
			
			Merged = MergeFace(Face1, Face2);

			if (!Merged)
				continue;

			RemoveDegenerateEdges(Merged->Poly);
			
			if (!CheckFace(Merged, false))
			{
				FreeFace(Merged);
				Face1->Merged = NULL;
				Face2->Merged = NULL;
				continue;
			}

			NumMerged++;

			// Add the Merged to the end of the face list 
			// so it will be checked against all the faces again
			for (End = Faces ; End->Next ; End = End->Next);
				
			Merged->Next = NULL;
			End->Next = Merged;
			break;
		}
	}

	return true;
}

//====================================================================================
//	FreeFaceList
//====================================================================================
void FreeFaceList(GBSP_Face *List)
{			
	GBSP_Face	*Next;

	while(List)
	{
		Next = List->Next;
		FreeFace(List);
		List = Next;
	}
}

//====================================================================================
//	EdgeExist
//====================================================================================
int32_t EdgeExist(Vector3 *Edge1, GBSP_Poly *Poly, int32_t *EdgeIndexOut)
{
	int32_t		i;
	Vector3		Edge2[2], *Verts;
	int32_t		NumVerts;

	Verts = Poly->Verts;
	NumVerts = Poly->NumVerts;

	for (i=0; i< NumVerts; i++)
	{
		Edge2[0] = Verts[i];
		Edge2[1] = Verts[(i+1)%NumVerts];

		if ((fabsf(Edge1->x-Edge2->x)<VCOMPARE_EPSILON && fabsf(Edge1->y-Edge2->y)<VCOMPARE_EPSILON && fabsf(Edge1->z-Edge2->z)<VCOMPARE_EPSILON))
		if ((fabsf((Edge1+1)->x-(Edge2+1)->x)<VCOMPARE_EPSILON && fabsf((Edge1+1)->y-(Edge2+1)->y)<VCOMPARE_EPSILON && fabsf((Edge1+1)->z-(Edge2+1)->z)<VCOMPARE_EPSILON))
		{
			EdgeIndexOut[0] = i;
			EdgeIndexOut[1] = (i+1)%NumVerts;
			return true;
		}
	}

	return false;
}

#define COLINEAR_EPSILON		0.0001f

//====================================================================================
//	MergeFace
//====================================================================================
GBSP_Face *MergeFace(GBSP_Face *Face1, GBSP_Face *Face2)
{
	Vector3		Edge1[2], *Verts1, *Verts2;
	int32_t		i, k, NumVerts, NumVerts2, EdgeIndex[2], NumNewVerts;
	GBSP_Poly	*NewPoly = NULL, *Poly1, *Poly2;
	Vector3		Normal1, Normal2, Vec1, Vec2;
	GBSP_Face	*NewFace = NULL;
	float		Dot;
	//int32_t		Start, End;
	bool	Keep1 = true, Keep2 = true;

	//
	// Planes and Sides MUST match before even trying to merge
	//
	if (Face1->PlaneNum != Face2->PlaneNum)
		return NULL;
	if (Face1->PlaneSide != Face2->PlaneSide)
		return NULL;

#if 1
	if ((Face1->Contents[0]&BSP_MERGE_SEP_CONTENTS) != (Face2->Contents[0]&BSP_MERGE_SEP_CONTENTS))
		return NULL;
	if ((Face1->Contents[1]&BSP_MERGE_SEP_CONTENTS) != (Face2->Contents[1]&BSP_MERGE_SEP_CONTENTS))
		return NULL;
#endif

	if (Face1->TexInfo != Face2->TexInfo)
		return NULL;
	
	Poly1 = Face1->Poly;
	Poly2 = Face2->Poly;

	if (Poly1->NumVerts == -1 || Poly2->NumVerts == -1)
		return NULL;

	Verts1 = Poly1->Verts;
	NumVerts = Poly1->NumVerts;

	//
	// Go through each edge of Poly1, and see if the reverse of it exist in Poly2
	//
	for (i=0; i< NumVerts; i++)		
	{
		Edge1[1] = Verts1[i];
		Edge1[0] = Verts1[(i+1)%NumVerts];

		if (EdgeExist(Edge1, Poly2, EdgeIndex))	// Found one, break out
			break;
	}

	if (i >= NumVerts)							// Did'nt find an edge, return nothing
		return NULL;

	Verts2 = Poly2->Verts;
	NumVerts2 = Poly2->NumVerts;

	//
	//	See if the 2 joined make a convex poly, connect them, and return new one
	//
	Normal1 = Planes[Face1->PlaneNum].Normal;	// Get the normal
	if (Face1->PlaneSide)
		Normal1 = Vector3Subtract(VecOrigin, Normal1);

	Vec1 = Verts1[(i+NumVerts-1)%NumVerts];		// Get the normal of the edge just behind edge1
	Vec1 = Vector3Subtract(Vec1, Edge1[1]);
	Normal2 = Vector3CrossProduct(Normal1, Vec1);
	Normal2 = Vector3Normalize(Normal2);
	Vec2 = Vector3Subtract(Verts2[(EdgeIndex[1]+1)%NumVerts2], Verts2[EdgeIndex[1]]);
	Dot = Vector3DotProduct(Vec2, Normal2);
	if (Dot > COLINEAR_EPSILON)
		return NULL;							// Edge makes a non-convex poly
	if (Dot >=-COLINEAR_EPSILON)				// Drop point, on colinear edge
		Keep1 = false;

	Vec1 = Verts1[(i+2)%NumVerts];				// Get the normal of the edge just behind edge1
	Vec1 = Vector3Subtract(Vec1, Edge1[0]);
	Normal2 = Vector3CrossProduct(Normal1, Vec1);
	Normal2 = Vector3Normalize(Normal2);
	Vec2 = Vector3Subtract(Verts2[(EdgeIndex[0]+NumVerts2-1)%NumVerts2], Verts2[EdgeIndex[0]]);
	Dot = Vector3DotProduct(Vec2, Normal2);
	if (Dot > COLINEAR_EPSILON)
		return NULL;							// Edge makes a non-convex poly
	if (Dot >=-COLINEAR_EPSILON)				// Drop point, on colinear edge
		Keep2 = false;

	//if (NumVerts+NumVerts2 > 30)
	//	return NULL;
	
	NewFace = AllocFace(NumVerts+NumVerts2);
	NewPoly = NewFace->Poly;
	if (!NewFace)
	{
		fprintf(stderr, "ERROR: *WARNING* MergeFace:  Out of memory for new face!\n");
		return NULL;
	}

	//
	// Make a new poly, free the old ones...
	//
	NumNewVerts = 0;

	for (k = (i+1)%NumVerts; k != i ; k = (k+1)%NumVerts)
	{
		if (k == (i+1)%NumVerts && ! Keep2)
			continue;
		NewPoly->Verts[NumNewVerts++] = Verts1[k];
	}

	i = EdgeIndex[0];

	for (k = (i+1)%NumVerts2; k != i ; k = (k+1)%NumVerts2)
	{
		if (k == (i+1)%NumVerts2 && ! Keep1)
			continue;
		NewPoly->Verts[NumNewVerts++] = Verts2[k];
	}

	*NewFace = *Face2;

	NewPoly->NumVerts = NumNewVerts;
	NewFace->Poly = NewPoly;

	//Hook.Printf("Merged face: %i\n", NumNewVerts);

	Face1->Merged = NewFace;
	Face2->Merged = NewFace;

	return NewFace;
}

//====================================================================================
//	NewFaceFromFace
//====================================================================================
GBSP_Face *NewFaceFromFace (GBSP_Face *f)
{
	GBSP_Face	*Newf;

	Newf = AllocFace (0);
	*Newf = *f;
	//Newf->merged = NULL;
	Newf->Split[0] = Newf->Split[1] = NULL;
	Newf->Poly = NULL;
	
	return Newf;
}

//====================================================================================
//	FanFace_r
//====================================================================================
void FanFace_r(GBSP_Node *Node, GBSP_Face *Face)
{
	GBSP_Poly	*Poly, *NewPoly;
	Vector3		*pVert;
	int32_t		i;

	Poly = Face->Poly;

	if (Poly->NumVerts == 3)		// Don't need to fan...
		return;			

	// Split this poly at the firsdt fan point into 2 more polys, and fan those...
	NewPoly = AllocPoly(3);

	NewPoly->Verts[0] = Poly->Verts[0];
	NewPoly->Verts[1] = Poly->Verts[1];
	NewPoly->Verts[2] = Poly->Verts[2];

	Face->Split[0] = NewFaceFromFace(Face);	
	Face->Split[0]->Poly = NewPoly;
	Face->Split[0]->Next = Node->Faces;
	Node->Faces = Face->Split[0];
		
	// Now make the rest of the polys
	NewPoly = AllocPoly(Poly->NumVerts-1);

	pVert = NewPoly->Verts;

	for (i=0; i<Poly->NumVerts; i++)
	{
		if (i==1)
			continue;

		*pVert++ = Poly->Verts[i];
	}
	
	Face->Split[1] = NewFaceFromFace(Face);	
	Face->Split[1]->Poly = NewPoly;
	Face->Split[1]->Next = Node->Faces;
	Node->Faces = Face->Split[1];
	
	FanFace_r(Node, Face->Split[1]);
}

//====================================================================================
//	GetLog
//====================================================================================
uint32_t GetLog(uint32_t P2)
{
	uint32_t p = 0;
	int32_t  i = 0;

	if (!P2)
	{
		printf("*WARNING* GetLog:  Bad input.\n");
		return 0;
	}
	
	for (i = P2; i > 0; i>>=1)
		p++;

	return (p-1);
}


//====================================================================================
//	Snaps a number to a power of 2
//====================================================================================
int32_t SnapToPower2(int32_t Width)
{
	int RetWidth;

	for ( RetWidth = 1; RetWidth < Width; RetWidth <<= 1 ) ;
	
	//assert( RetWidth <= 256 );

	return RetWidth;
}

//====================================================================================
//	SubdivideFace
//====================================================================================
void SubdivideFace(GBSP_Node *Node, GBSP_Face *Face)
{
	float		Mins, Maxs;
	float		v;
	int32_t		Axis, i;
	GFX_TexInfo	*Tex;
	Vector3		Temp;
	float		Dist;
	GBSP_Poly	*p, *Frontp, *Backp;
	GBSP_Plane	Plane;

	if (Face->Merged)
		return;

	// Special (non-surface cached) faces don't need subdivision
/* We need to handle this later when we support lightmapping
	Tex = &TexInfo[Face->TexInfo];

	//if ( Tex->Flags & TEXINFO_GOURAUD)
	//	FanFace_r(Node, Face);

	if ( Tex->Flags & TEXINFO_NO_LIGHTMAP)
		return;
//*/
	Tex = &GFXTexInfo[Face->TexInfo];
	if (!Tex) return;  // Safety check
	for (Axis = 0 ; Axis < 2 ; Axis++)
	{
		while (1)
		{
			float		TexSize, SplitDist;
			float		Mins16, Maxs16;

			Mins = 999999.0f;
			Maxs = -999999.0f;
			
			Temp = Tex->Vecs[Axis];

			for (i=0 ; i<Face->Poly->NumVerts ; i++)
			{
				v = Vector3DotProduct(Face->Poly->Verts[i], Temp);
				if (v < Mins)
					Mins = v;
				if (v > Maxs)
					Maxs = v;
			}
			
			Mins16 = (float)floor(Mins/16.0f)*16.0f;
			Maxs16 = (float)ceil(Maxs/16.0f)*16.0f;

			TexSize = Maxs - Mins;
			//TexSize = Maxs16 - Mins16;

			// Default to the Subdivide size
			SplitDist = SubdivideSize;

		#if 0
			if (TexSize <= SubdivideSize)
			{
				int32_t		Log, LogSize;
				float		Ratio;

				LogSize = SnapToPower2((int32_t)TexSize);
				Log = GetLog(LogSize);

				if (LogSize <= 16)
					break;						// Don't bother with small lmaps

				Ratio = TexSize / (float)LogSize;	// Get the percentage of the space being taken advantage of if it were to be snapped to power of 2 texture in drivers

				if (Ratio >= 0.75f)
					break;			// No need to split, if going to use at least 50% of the space

				// The lmap is not going to use a big enough percentage of the size it's currently going to be snapped to
				// So, cut it along the next size down...
				Log--;				// Split along next log down

				// Overide the subdivide size with the next size down
				SplitDist = (float)(1<<Log);
			}
		#else
			if (TexSize <= SubdivideSize)
				break;
		#endif
			
			// Make a copy
			CopyPoly(Face->Poly, &p);

			// Split it
			NumSubdivided++;
			
			v = Vector3NormalizeEx(&Temp);

			Dist = (Mins + SplitDist - 16)/v;
			//Dist = (Mins16 + SplitDist)/v;

			Plane.Normal = Temp;
			Plane.Dist = Dist;
			Plane.Type = PLANE_ANY;
			
			SplitPoly(p, &Plane, &Frontp, &Backp, false);

			if (!Frontp || !Backp)
			{
				printf("*WARNING* SubdivideFace: didn't split the polygon: %f, %f, %f\n", Mins, Maxs, SplitDist);
				break;
			}

			Face->Split[0] = NewFaceFromFace (Face);
			Face->Split[0]->Poly = Frontp;
			Face->Split[0]->Next = Node->Faces;
			Node->Faces = Face->Split[0];

			Face->Split[1] = NewFaceFromFace (Face);
			Face->Split[1]->Poly = Backp;
			Face->Split[1]->Next = Node->Faces;
			Node->Faces = Face->Split[1];

			SubdivideFace (Node, Face->Split[0]);
			SubdivideFace (Node, Face->Split[1]);
			return;
		}
	}
}

//====================================================================================
//	SubdivideNodeFaces
//====================================================================================
void SubdivideNodeFaces (GBSP_Node *Node)
{
	GBSP_Face	*f;

	for (f = Node->Faces ; f ; f=f->Next)
	{
		SubdivideFace (Node, f);
	}
}

//====================================================================================
//	CheckFace
//====================================================================================
bool CheckFace(GBSP_Face *Face, bool Verb)
{
	int32_t		i, j, NumVerts;
	Vector3		Vect1, *Verts, Normal, V1, V2, EdgeNormal;
	GBSP_Poly	*Poly;
	float		Dist, PDist, EdgeDist;
	
	Poly = Face->Poly;
	Verts = Poly->Verts;
	NumVerts = Poly->NumVerts;

	if (NumVerts < 3)
	{
		if (Verb)
			printf("CheckFace:  NumVerts < 3.\n");
		return false;
	}
	
	Normal = Planes[Face->PlaneNum].Normal;
	PDist = Planes[Face->PlaneNum].Dist;
	if (Face->PlaneSide)
	{
		Normal = Vector3Subtract(VecOrigin, Normal);
		PDist = -PDist;
	}

	//
	//	Check for degenerate edges, convexity, and make sure it's planar
	//
	for (i=0; i< NumVerts; i++)
	{
		V1 = Verts[i];
		V2 = Verts[(i+1)%NumVerts];

		//	Check for degenreate edge
		Vect1 = Vector3Subtract(V2, V1);
		Dist = Vector3Length(Vect1);
		if (fabs(Dist) < DEGENERATE_EPSILON)
		{
			if (Verb)
				printf("WARNING CheckFace:  Degenerate Edge.\n");
			return false;
		}

		// Check for planar
		Dist = Vector3DotProduct(V1, Normal) - PDist;
		if (Dist > ON_EPSILON || Dist <-ON_EPSILON)
		{
			if (Verb)
				printf("WARNING CheckFace:  Non planar: %f.\n", Dist);
			return false;
		}

		EdgeNormal = Vector3CrossProduct(Normal, Vect1);
		EdgeNormal = Vector3Normalize(EdgeNormal);
		EdgeDist = Vector3DotProduct(V1, EdgeNormal);
		
		// Check for convexity
		for (j=0; j< NumVerts; j++)
		{
			Dist = Vector3DotProduct(Verts[j], EdgeNormal) - EdgeDist;
			if (Dist > ON_EPSILON)
			{
				if (Verb)
					printf("CheckFace:  Face not convex.\n");
				return false;
			}
		}
	}

	return true;
}

//====================================================================================
//	GetFaceListBOX
//====================================================================================
void GetFaceListBOX(GBSP_Face *Faces, Vector3 *Mins, Vector3 *Maxs)
{
	GBSP_Face	*Face;
	GBSP_Poly	*Poly;
	Vector3		Vert;
	int32_t		i;

	ClearBounds(Mins, Maxs);

	// Go through each face in this brush...
	for (Face = Faces; Face; Face = Face->Next)
	{
		Poly = Face->Poly;
		for (i=0; i< Poly->NumVerts; i++)
		{
			Vert = Poly->Verts[i];

			// First get maxs
			if (Vert.x > Maxs->x)
				Maxs->x = Vert.x;
			if (Vert.y > Maxs->y)
				Maxs->y = Vert.y;
			if (Vert.z > Maxs->z)
				Maxs->z = Vert.z;

			// Then check mins
			if (Vert.x < Mins->x)
				Mins->x = Vert.x;
			if (Vert.y < Mins->y)
				Mins->y = Vert.y;
			if (Vert.z < Mins->z)
				Mins->z = Vert.z;
		}
	}
}

