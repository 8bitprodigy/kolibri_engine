/****************************************************************************************/
/*  mathlib.c - Math utility functions                                                  */
/*  Converted from Genesis3D MATHLIB.CPP to C99/raylib                                  */
/****************************************************************************************/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "bsp.h"
#include "mathlib.h"
#include <raymath.h>

/****************************************************************************************/
/*  MathLib.cpp                                                                         */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Various math functions not included in Vec3d.h, etc...                 */
/*                                                                                      */
/****************************************************************************************/


Vector3	VecOrigin = {0.0f, 0.0f, 0.0f};

//====================================================================================
// ClearBounds
//====================================================================================
void ClearBounds(Vector3 *Mins, Vector3 *Maxs)
{
	Mins->x =  MIN_MAX_BOUNDS;
	Mins->y =  MIN_MAX_BOUNDS;
	Mins->z =  MIN_MAX_BOUNDS;

	Maxs->x = -MIN_MAX_BOUNDS;
	Maxs->y = -MIN_MAX_BOUNDS;
	Maxs->z = -MIN_MAX_BOUNDS;
}

//=======================================================================================
//	AddPointToBounds
//=======================================================================================
void AddPointToBounds(Vector3 *v, Vector3 *Mins, Vector3 *Maxs)
{
	int32_t	i;
	float 	Val;

	for (i=0 ; i<3 ; i++)
	{
		Val = VectorToSUB(*v, i);

		if (Val < VectorToSUB(*Mins, i))
			VectorToSUB(*Mins, i) = Val;
		if (Val > VectorToSUB(*Maxs, i))
			VectorToSUB(*Maxs, i) = Val;
	}
}

//=======================================================================================
//	ColorNormalize
//=======================================================================================
float ColorNormalizeBSP(Vector3 *C1, Vector3 *C2)
{
	float	Max;

	Max = C1->x;
	if (C1->y > Max)
		Max = C1->y;
	if (C1->z > Max)
		Max = C1->z;

	if (Max == 0.0f)
		return 0.0f;
	
	*C2 = Vector3Scale(*C1, 1.0f / Max);

	return Max;
}

//=======================================================================================
//	ColorClamp
//=======================================================================================
float ColorClamp(Vector3 *C1, float Clamp, Vector3 *C2)
{
	int32_t	i;
	float	Max, Max2;
	Vector3	C3;

	Max = -1.0f;

	C3 = *C1;

	for (i=0; i<3; i++)
	{
		if (VectorToSUB(C3, i) < 1.0f)
			VectorToSUB(C3, i) = 1.0f;

		if (VectorToSUB(C3, i) > Max)
			Max = VectorToSUB(C3, i);
	}
	
	Max2 = Max;

	if (Max2 > Clamp)
		Max2 = Clamp;

	*C2 = Vector3Scale(*C1, Max2 / Max);

	return Max;
}

//=======================================================================================
//	PlaneType
//=======================================================================================
int PlaneType(Vector3 *V1)
{
	float	X, Y, Z;

	X = (float)fabs(V1->x);
	Y = (float)fabs(V1->y);
	Z = (float)fabs(V1->z);

	if (X == 1.0f)
		return PLANE_X;
	else if (Y == 1.0f)
		return PLANE_Y;
	else if (Z == 1.0f)
		return PLANE_Z;

	if (X >= Y && X >= Z)
		return PLANE_ANYX;
	else if (Y >= X && Y >= Z)
		return PLANE_ANYY;
	else
		return PLANE_ANYZ;
}

// Backward-compat alias used internally by bsp.c
int32_t geVec3d_PlaneType(Vector3 *V1) { return PlaneType(V1); }

//=======================================================================================
//	PlanePointDistance
//	Signed distance from a point to a plane (positive = front side).
//=======================================================================================
float PlanePointDistance(GBSP_Plane *Plane, Vector3 *Point)
{
	return Vector3DotProduct(Plane->Normal, *Point) - Plane->Dist;
}



//=======================================================================================
//	Vector3NormalizeEx
//	Normalizes vector in-place and returns original length (like geVec3d_Normalize)
//=======================================================================================
float Vector3NormalizeEx(Vector3 *v)
{
	float len = Vector3Length(*v);
	if (len > 0.0f) {
		float inv = 1.0f / len;
		v->x *= inv;
		v->y *= inv;
		v->z *= inv;
	}
	return len;
}
