/****************************************************************************************/
/*  poly.h                                                                              */
/*                                                                                      */
/*  Polygon and face operations - clipping, splitting, merging                          */
/*  Converted from Genesis3D POLY.H to C99/raylib                                       */
/*                                                                                      */
/****************************************************************************************/
#ifndef POLY_H
#define POLY_H

#include <stdint.h>
#include <stdbool.h>
#include "bsp.h"
#include "mathlib.h"

// Vertex counting globals
extern bool     gCountVerts;
extern int32_t  gTotalVerts;
extern int32_t  gPeekVerts;

//====================================================================================
// Polygon operations
//====================================================================================
GBSP_Poly   *AllocPoly(int32_t NumVerts);
void        FreePoly(GBSP_Poly *Poly);
GBSP_Poly   *CreatePolyFromPlane(GBSP_Plane *Plane);
bool        ClipPoly(GBSP_Poly *InPoly, GBSP_Plane *Plane, bool FlipTest, GBSP_Poly **OutPoly);
bool        ClipPolyEpsilon(GBSP_Poly *InPoly, float Epsilon, GBSP_Plane *Plane, bool FlipTest, GBSP_Poly **OutPoly);
bool        SplitPoly(GBSP_Poly *InPoly, GBSP_Plane *Plane, GBSP_Poly **Front, GBSP_Poly **Back, bool FlipTest);
bool        SplitPolyEpsilon(GBSP_Poly *InPoly, float Epsilon, GBSP_Plane *Plane, GBSP_Poly **Front, GBSP_Poly **Back, bool FlipTest);
float       PolyArea(GBSP_Poly *Poly);
bool        CopyPoly(GBSP_Poly *In, GBSP_Poly **Out);
GBSP_Poly   *CopyPoly2(GBSP_Poly *In);
bool        ReversePoly(GBSP_Poly *In, GBSP_Poly **Out);
void        RemoveDegenerateEdges(GBSP_Poly *Poly);
bool        RemoveDegenerateEdges2(GBSP_Poly *Poly);
void        PolyCenter(GBSP_Poly *Poly, Vector3 *Center);
bool        PolyIsTiny(GBSP_Poly *Poly);

//====================================================================================
// Face operations
//====================================================================================
GBSP_Face   *AllocFace(int32_t NumVerts);
void        FreeFace(GBSP_Face *Face);
bool        SplitFace(GBSP_Face *In, GBSP_Plane *Split, GBSP_Face **Front, GBSP_Face **Back, bool FlipTest);
void        FreeFaceList(GBSP_Face *List);
int32_t     MergeFaceList(GBSP_Face *In, GBSP_Face **Out, bool Mirror);
int32_t     EdgeExist(Vector3 *Edge1, GBSP_Poly *Poly, int32_t *EdgeIndexOut);
GBSP_Face   *MergeFace(GBSP_Face *Face1, GBSP_Face *Face2);
bool        CheckFace(GBSP_Face *Face, bool Verb);
void        GetFaceListBOX(GBSP_Face *Faces, Vector3 *Mins, Vector3 *Maxs);

// Subdivision parameters
extern int32_t  NumSubdivides;
extern float    SubdivideSize;

#endif
