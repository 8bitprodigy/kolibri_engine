/****************************************************************************************/
/*  visflood.c                                                                          */
/*                                                                                      */
/*  Ported from Genesis3D VisFlood.cpp to C99/raylib                                   */
/*  Genesis3D infrastructure (GHook, geRam, geBoolean) replaced with standard C.       */
/****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include <raylib.h>
#include <raymath.h>

#include "bsp.h"
#include "poly.h"
#include "vis.h"


/* =========================================================================
   Module-local globals (mirror the originals in VisFlood.cpp)
   ========================================================================= */

static int32_t CanSee;
static int32_t SrcLeaf;
static int32_t MightSee;

#define ON_EPSILON 0.01f


/* =========================================================================
   PlaneDistanceFastP
       Signed distance from Point to Plane.  The original had per-axis fast
       paths for PLANE_X/Y/Z but fell through to the dot product anyway, so
       we just use the dot product directly.
   ========================================================================= */

static inline float
PlaneDistanceFastP(const Vector3 *Point, const GBSP_Plane *Plane)
{
    return Vector3DotProduct(Plane->Normal, *Point) - Plane->Dist;
}


/* =========================================================================
   PortalCanSeePortal
       Quick check: can Portal1 possibly see Portal2?
       Does NOT account for intervening geometry — that is the slow pass's job.
   ========================================================================= */

bool
PortalCanSeePortal(VIS_Portal *Portal1, VIS_Portal *Portal2)
{
    GBSP_Poly  *Poly;
    int32_t     i;
    float       Dist;

    /* All of Portal1's verts must have at least one behind Portal2's plane */
    Poly = Portal1->Poly;
    for (i = 0; i < Poly->NumVerts; i++)
    {
        Dist = PlaneDistanceFastP(&Poly->Verts[i], &Portal2->Plane);
        if (Dist < -ON_EPSILON)
            break;
    }
    if (i == Poly->NumVerts)
        return false;   /* Portal1 is entirely in front of Portal2 */

    /* All of Portal2's verts must have at least one in front of Portal1's plane */
    Poly = Portal2->Poly;
    for (i = 0; i < Poly->NumVerts; i++)
    {
        Dist = PlaneDistanceFastP(&Poly->Verts[i], &Portal1->Plane);
        if (Dist > ON_EPSILON)
            break;
    }
    if (i == Poly->NumVerts)
        return false;   /* Portal2 is entirely behind Portal1 */

    return true;
}


/* =========================================================================
   FloodPortalsFast_r  (iterative — avoids stack overflow on deep maps)
   ========================================================================= */

void
FloodPortalsFast_r(VIS_Portal *SrcPortal, VIS_Portal *DestPortal)
{
    /* Explicit stack: at most NumVisPortals entries */
    static VIS_Portal **Stack = NULL;
    static int32_t      StackCapacity = 0;

    if (!Stack || StackCapacity < NumVisPortals)
    {
        free(Stack);
        Stack = (VIS_Portal **)malloc(NumVisPortals * sizeof(VIS_Portal *));
        StackCapacity = NumVisPortals;
        if (!Stack) { fprintf(stderr, "ERROR: FloodPortalsFast_r: out of memory\n"); return; }
    }

    int32_t SeedPNum = (int32_t)(DestPortal - VisPortalArray);
    PortalSeen[SeedPNum] = 1;

    int32_t top = 0;
    Stack[top++] = DestPortal;

    while (top > 0)
    {
        VIS_Portal *Cur  = Stack[--top];
        int32_t     PNum = (int32_t)(Cur - VisPortalArray);

        /* Mark as visible from SrcPortal */
        int32_t Bit = 1 << (PNum & 7);
        if (!(SrcPortal->VisBits[PNum >> 3] & Bit))
        {
            SrcPortal->VisBits[PNum >> 3] |= Bit;
            SrcPortal->MightSee++;
            VisLeafs[SrcLeaf].MightSee++;
            MightSee++;
        }

        /* Push neighbors — mark seen at push time so they're never pushed twice */
        int32_t     LeafNum = Cur->Leaf;

        if (LeafNum < 0 || LeafNum >= NumVisLeafs)
        {
            fprintf(stderr, "ERROR: FloodPortalsFast_r: portal %d has bad Leaf=%d\n",
                    PNum, LeafNum);
            continue;
        }

        VIS_Leaf   *Leaf    = &VisLeafs[LeafNum];
        VIS_Portal *Portal;

        for (Portal = Leaf->Portals; Portal; Portal = Portal->Next)
        {
            int32_t NPNum = (int32_t)(Portal - VisPortalArray);
            if (NPNum < 0 || NPNum >= NumVisPortals)
            {
                fprintf(stderr, "ERROR: FloodPortalsFast_r: neighbor portal ptr out of range\n");
                break;
            }
            if (!PortalSeen[NPNum] && PortalCanSeePortal(SrcPortal, Portal))
            {
                PortalSeen[NPNum] = 1;
                if (top >= StackCapacity)
                {
                    fprintf(stderr, "ERROR: FloodPortalsFast_r: stack overflow! top=%d cap=%d\n", top, StackCapacity);
                    return;
                }
                Stack[top++] = Portal;
            }
        }
    }
}


/* =========================================================================
   FloodLeafPortalsFast
   ========================================================================= */

void
FloodLeafPortalsFast(int32_t LeafNum)
{
    VIS_Leaf   *Leaf;
    VIS_Portal *Portal;

    Leaf = &VisLeafs[LeafNum];

    if (!Leaf->Portals)
        return;

    SrcLeaf = LeafNum;

    for (Portal = Leaf->Portals; Portal; Portal = Portal->Next)
    {
        if (LeafNum >= 350)
        {
            int32_t pnum = (int32_t)(Portal - VisPortalArray);
            printf("    leaf %d portal %d -> leaf %d (NumVerts=%d)\n",
                   LeafNum, pnum, Portal->Leaf,
                   Portal->Poly ? Portal->Poly->NumVerts : -1);
            fflush(stdout);
        }

        Portal->VisBits = (uint8_t *)calloc(NumVisPortalBytes, sizeof(uint8_t));
        if (!Portal->VisBits)
        {
            fprintf(stderr, "ERROR: FloodLeafPortalsFast: out of memory.\n");
            return;
        }

        memset(PortalSeen, 0, NumVisPortals);
        MightSee = 0;

        FloodPortalsFast_r(Portal, Portal);
    }
}


/* =========================================================================
   ClipToSeperators
       Clips Target against the separating planes formed by edges of Source
       and vertices of Pass.  Used by the slow (full) vis pass to tighten
       visibility through a chain of portals.
   ========================================================================= */

static bool
ClipToSeperators(GBSP_Poly *Source, GBSP_Poly *Pass, GBSP_Poly *Target,
                 bool FlipClip, GBSP_Poly **Dest)
{
    int32_t    i, j, k, l;
    GBSP_Plane Plane;
    Vector3    v1, v2;
    float      d, Length;
    int32_t    Counts[3];
    bool       FlipTest;

    for (i = 0; i < Source->NumVerts; i++)
    {
        l  = (i + 1) % Source->NumVerts;
        v1 = Vector3Subtract(Source->Verts[l], Source->Verts[i]);

        for (j = 0; j < Pass->NumVerts; j++)
        {
            v2 = Vector3Subtract(Pass->Verts[j], Source->Verts[i]);

            Plane.Normal = Vector3CrossProduct(v1, v2);
            Length       = Vector3LengthSqr(Plane.Normal);

            if (Length < ON_EPSILON * ON_EPSILON)
                continue;

            Plane.Normal = Vector3Normalize(Plane.Normal);
            Plane.Dist   = Vector3DotProduct(Pass->Verts[j], Plane.Normal);

            /* Determine which side of the plane the rest of Source is on */
            FlipTest = false;
            for (k = 0; k < Source->NumVerts; k++)
            {
                if (k == i || k == l)
                    continue;

                d = Vector3DotProduct(Source->Verts[k], Plane.Normal) - Plane.Dist;
                if (d < -ON_EPSILON)
                {
                    FlipTest = false;
                    break;
                }
                else if (d > ON_EPSILON)
                {
                    FlipTest = true;
                    break;
                }
            }
            if (k == Source->NumVerts)
                continue;   /* all source verts on plane — degenerate, skip */

            if (FlipTest)
            {
                Plane.Normal = Vector3Negate(Plane.Normal);
                Plane.Dist   = -Plane.Dist;
            }

            /* Verify that all of Pass (except vertex j) is on the positive side */
            Counts[0] = Counts[1] = Counts[2] = 0;
            for (k = 0; k < Pass->NumVerts; k++)
            {
                if (k == j) continue;
                d = Vector3DotProduct(Pass->Verts[k], Plane.Normal) - Plane.Dist;
                if      (d < -ON_EPSILON) break;
                else if (d >  ON_EPSILON) Counts[0]++;
                else                      Counts[2]++;
            }
            if (k != Pass->NumVerts) continue;
            if (!Counts[0])          continue;

            /* Clip Target against this separating plane */
            if (!ClipPoly(Target, &Plane, FlipClip, &Target))
            {
                fprintf(stderr, "ERROR: ClipToSeperators: ClipPoly failed.\n");
                return false;
            }

            if (!Target)
            {
                *Dest = NULL;
                return true;
            }
        }
    }

    *Dest = Target;
    return true;
}


/* =========================================================================
   FloodPortalsSlow_r
   ========================================================================= */

static bool
FloodPortalsSlow_r(VIS_Portal *SrcPortal, VIS_Portal *DestPortal,
                   VIS_PStack *PrevStack)
{
    VIS_Leaf   *Leaf;
    VIS_Portal *Portal;
    int32_t     LeafNum, j, PNum;
    uint32_t   *Test, *Might, *Vis, More;
    VIS_PStack  Stack;

    PNum = (int32_t)(DestPortal - VisPortalArray);

    /* Mark DestPortal as definitely visible from SrcPortal */
    int32_t Bit = 1 << (PNum & 7);
    if (!(SrcPortal->FinalVisBits[PNum >> 3] & Bit))
    {
        SrcPortal->FinalVisBits[PNum >> 3] |= Bit;
        SrcPortal->CanSee++;
        VisLeafs[SrcLeaf].CanSee++;
        CanSee++;
    }

    LeafNum = DestPortal->Leaf;
    Leaf    = &VisLeafs[LeafNum];

    Might = (uint32_t *)Stack.VisBits;
    Vis   = (uint32_t *)SrcPortal->FinalVisBits;

    for (Portal = Leaf->Portals; Portal; Portal = Portal->Next)
    {
        PNum = (int32_t)(Portal - VisPortalArray);
        Bit  = 1 << (PNum & 7);

        /* Fast-vis says we can't reach this portal */
        if (!(SrcPortal->VisBits[PNum >> 3] & Bit))
            continue;

        /* Previous stack frame can't reach this portal */
        if (!(PrevStack->VisBits[PNum >> 3] & Bit))
            continue;

        /* Check if this portal can contribute any new visibility */
        Test = Portal->Done
             ? (uint32_t *)Portal->FinalVisBits
             : (uint32_t *)Portal->VisBits;

        More = 0;
        for (j = 0; j < NumVisPortalLongs; j++)
        {
            Might[j] = ((uint32_t *)PrevStack->VisBits)[j] & Test[j];
            More    |= (Might[j] & ~Vis[j]);
        }

        if (!More && (SrcPortal->FinalVisBits[PNum >> 3] & Bit))
            continue;   /* nothing new */

        /* Copy and clip the pass polygon against SrcPortal's plane */
        if (!CopyPoly(Portal->Poly, &Stack.Pass))
            return false;

        if (!ClipPoly(Stack.Pass, &SrcPortal->Plane, false, &Stack.Pass))
            return false;
        if (!Stack.Pass)
            continue;

        /* Copy and clip the source polygon against this portal's plane */
        if (!CopyPoly(PrevStack->Source, &Stack.Source))
            return false;

        if (!ClipPoly(Stack.Source, &Portal->Plane, true, &Stack.Source))
            return false;
        if (!Stack.Source)
        {
            FreePoly(Stack.Pass);
            continue;
        }

        /* If no previous pass polygon, recurse immediately */
        if (!PrevStack->Pass)
        {
            if (!FloodPortalsSlow_r(SrcPortal, Portal, &Stack))
                return false;
            FreePoly(Stack.Source);
            FreePoly(Stack.Pass);
            continue;
        }

        /* Clip pass through separators */
        if (!ClipToSeperators(Stack.Source, PrevStack->Pass, Stack.Pass,
                               false, &Stack.Pass))
            return false;
        if (!Stack.Pass)
        {
            FreePoly(Stack.Source);
            continue;
        }

        if (!ClipToSeperators(PrevStack->Pass, Stack.Source, Stack.Pass,
                               true, &Stack.Pass))
            return false;
        if (!Stack.Pass)
        {
            FreePoly(Stack.Source);
            continue;
        }

        if (!FloodPortalsSlow_r(SrcPortal, Portal, &Stack))
            return false;

        FreePoly(Stack.Source);
        FreePoly(Stack.Pass);
    }

    return true;
}


/* =========================================================================
   FloodPortalsSlow
   ========================================================================= */

bool
FloodPortalsSlow(void)
{
    VIS_Portal *Portal;
    VIS_PStack  PStack;
    int32_t     k, i, PNum;

    for (k = 0; k < NumVisPortals; k++)
        VisPortalArray[k].Done = false;

    for (k = 0; k < NumVisPortals; k++)
    {
        Portal = VisSortedPortals[k];

        Portal->FinalVisBits = (uint8_t *)calloc(NumVisPortalBytes, sizeof(uint8_t));
        if (!Portal->FinalVisBits)
        {
            fprintf(stderr, "ERROR: FloodPortalsSlow: out of memory.\n");
            return false;
        }

        memset(PortalSeen, 0, NumVisPortals);
        CanSee = 0;

        for (i = 0; i < NumVisPortalBytes; i++)
            PStack.VisBits[i] = Portal->VisBits[i];

        if (!CopyPoly(Portal->Poly, &PStack.Source))
            return false;
        PStack.Pass = NULL;

        /* SrcLeaf is the leaf this portal belongs to (not the leaf it looks into) */
        SrcLeaf = Portal->SrcLeaf;

        if (!FloodPortalsSlow_r(Portal, Portal, &PStack))
            return false;

        FreePoly(PStack.Source);

        Portal->Done = true;

        PNum = (int32_t)(Portal - VisPortalArray);
        if (VisVerbose)
            printf("Portal: %4i - Fast Vis: %4i, Full Vis: %4i\n",
                   k + 1, Portal->MightSee, Portal->CanSee);
    }

    return true;
}
