/****************************************************************************************/
/*  mathlib.h                                                                           */
/*                                                                                      */
/*  Math utility functions for BSP compilation                                          */
/*  Converted from Genesis3D to use raylib Vector3                                      */
/*                                                                                      */
/****************************************************************************************/
#ifndef MATHLIB_H
#define MATHLIB_H

#include <raylib.h>
#include <math.h>
#include <stdbool.h>

// Epsilon values for floating point comparisons
#define ON_EPSILON          0.1f
#define VCOMPARE_EPSILON    ON_EPSILON 

// Bounds for bbox initialization
#define MIN_MAX_BOUNDS      15192.0f
#define MIN_MAX_BOUNDS2     (MIN_MAX_BOUNDS*2)

// Plane type constants
#define PLANE_X             0
#define PLANE_Y             1
#define PLANE_Z             2
#define PLANE_ANYX          3
#define PLANE_ANYY          4
#define PLANE_ANYZ          5
#define PLANE_ANY           6

// Macro to access vector components by index (0=x, 1=y, 2=z)
#define VectorToSUB(a, b)   (((float*)&a)[b])

// Global origin vector
extern Vector3 VecOrigin;

// Bounds operations
void ClearBounds(Vector3 *Mins, Vector3 *Maxs);
void AddPointToBounds(Vector3 *v, Vector3 *Mins, Vector3 *Maxs);

// Color operations (using Vector3 as RGB)
float ColorNormalizeBSP(Vector3 *C1, Vector3 *C2);
float ColorClamp(Vector3 *C1, float Clamp, Vector3 *C2);

// Plane type determination
int PlaneType(Vector3 *Normal);

// Vector comparison with epsilon
static inline bool Vector3Compare(Vector3 a, Vector3 b, float epsilon)
{
    return (fabsf(a.x - b.x) < epsilon &&
            fabsf(a.y - b.y) < epsilon &&
            fabsf(a.z - b.z) < epsilon);
}

#endif
