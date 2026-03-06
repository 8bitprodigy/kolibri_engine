/****************************************************************************************/
/*  vis.c                                                                               */
/*                                                                                      */
/*  Ported from Genesis3D Vis.cpp to C99/raylib                                        */
/*  Results written directly into GBSP_Model fields.                                    */
/****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include <raylib.h>
#include <raymath.h>

#include "bsp.h"
#include "poly.h"
#include "portals.h"
#include "vis.h"


/* =========================================================================
   Globals
   ========================================================================= */

int32_t     NumVisPortals      = 0;
int32_t     NumVisPortalBytes  = 0;
int32_t     NumVisPortalLongs  = 0;
VIS_Portal *VisPortalArray     = NULL;
VIS_Portal **VisSortedPortals  = NULL;
uint8_t    *PortalSeen         = NULL;
static uint8_t *PortalBits     = NULL;

int32_t     NumVisLeafs        = 0;
int32_t     NumVisLeafBytes    = 0;
int32_t     NumVisLeafLongs    = 0;
VIS_Leaf   *VisLeafs           = NULL;

int32_t     TotalVisibleLeafs  = 0;

bool        VisVerbose         = false;
bool        NoSort             = false;
bool        FullVis            = false;

static int32_t LeafSee = 0;


/* =========================================================================
   Cleanup
   ========================================================================= */

static void
FreeWorkingData(void)
{
    int32_t i;

    if (VisPortalArray)
    {
        for (i = 0; i < NumVisPortals; i++)
        {
            if (VisPortalArray[i].Poly)         FreePoly(VisPortalArray[i].Poly);
            if (VisPortalArray[i].FinalVisBits)  free(VisPortalArray[i].FinalVisBits);
            if (VisPortalArray[i].VisBits)       free(VisPortalArray[i].VisBits);
        }
        free(VisPortalArray);  VisPortalArray  = NULL;
    }

    if (VisSortedPortals) { free(VisSortedPortals); VisSortedPortals = NULL; }
    if (PortalSeen)       { free(PortalSeen);        PortalSeen       = NULL; }
    if (PortalBits)       { free(PortalBits);        PortalBits       = NULL; }
    if (VisLeafs)         { free(VisLeafs);          VisLeafs         = NULL; }

    NumVisPortals     = 0;
    NumVisPortalBytes = 0;
    NumVisPortalLongs = 0;
    NumVisLeafs       = 0;
    NumVisLeafBytes   = 0;
    NumVisLeafLongs   = 0;
    TotalVisibleLeafs = 0;
}

void
FreeModelVisData(GBSP_Model *Model)
{
    if (Model->LeafVisBits)
    {
        free(Model->LeafVisBits);
        Model->LeafVisBits    = NULL;
        Model->NumVisLeafs    = 0;
        Model->NumVisLeafBytes = 0;
    }
}


/* =========================================================================
   Portal sorting
   ========================================================================= */

static int
PComp(const void *a, const void *b)
{
    const VIS_Portal *pa = *(const VIS_Portal **)a;
    const VIS_Portal *pb = *(const VIS_Portal **)b;
    if (pa->MightSee == pb->MightSee) return  0;
    if (pa->MightSee <  pb->MightSee) return -1;
    return 1;
}

static void
SortPortals(void)
{
    int32_t i;
    for (i = 0; i < NumVisPortals; i++)
        VisSortedPortals[i] = &VisPortalArray[i];
    if (!NoSort)
        qsort(VisSortedPortals, NumVisPortals, sizeof(VisSortedPortals[0]), PComp);
}


/* =========================================================================
   CalcPortalInfo
   ========================================================================= */

static bool
CalcPortalInfo(VIS_Portal *Portal)
{
    GBSP_Poly *Poly = Portal->Poly;
    float      BestDist = 0.0f;
    int32_t    i;

    PolyCenter(Poly, &Portal->Center);

    for (i = 0; i < Poly->NumVerts; i++)
    {
        Vector3 v    = Vector3Subtract(Poly->Verts[i], Portal->Center);
        float   dist = Vector3Length(v);
        if (dist > BestDist) BestDist = dist;
    }

    Portal->Radius = BestDist;
    return true;
}


/* =========================================================================
   CollectLeafVisBits
   ========================================================================= */

static bool
CollectLeafVisBits(int32_t LeafNum, uint8_t *LeafVisBits)
{
    VIS_Portal *Portal, *SPortal;
    VIS_Leaf   *Leaf;
    uint8_t    *LeafBits, *VisBits;
    int32_t     k, Bit, SLeaf;

    Leaf     = &VisLeafs[LeafNum];
    LeafBits = &LeafVisBits[LeafNum * NumVisLeafBytes];

    memset(PortalBits, 0, NumVisPortalBytes);

    int32_t portal_count = 0;
    for (Portal = Leaf->Portals; Portal; Portal = Portal->Next)
    {
        if (++portal_count > NumVisPortals)
        {
            fprintf(stderr, "ERROR: CollectLeafVisBits: leaf %d portal list is corrupt (loop)\n", LeafNum);
            return false;
        }
        if      (Portal->FinalVisBits) VisBits = Portal->FinalVisBits;
        else if (Portal->VisBits)      VisBits = Portal->VisBits;
        else
        {
            fprintf(stderr, "ERROR: CollectLeafVisBits: no VisInfo for portal.\n");
            return false;
        }

        for (k = 0; k < NumVisPortalBytes; k++)
            PortalBits[k] |= VisBits[k];

        if (Portal->VisBits)      { free(Portal->VisBits);      Portal->VisBits      = NULL; }
        if (Portal->FinalVisBits) { free(Portal->FinalVisBits); Portal->FinalVisBits = NULL; }
    }

    /* Convert portal vis bits → leaf vis bits */
    for (k = 0; k < NumVisPortals; k++)
    {
        if (PortalBits[k >> 3] & (1 << (k & 7)))
        {
            SPortal = &VisPortalArray[k];
            SLeaf   = SPortal->Leaf;
            LeafBits[SLeaf >> 3] |= 1 << (SLeaf & 7);
        }
    }

    Bit = 1 << (LeafNum & 7);
    if (LeafBits[LeafNum >> 3] & Bit)
        printf("*WARNING* CollectLeafVisBits: Leaf %i can see himself!\n", LeafNum);
    LeafBits[LeafNum >> 3] |= Bit;

    LeafSee = 0;
    for (k = 0; k < NumVisLeafs; k++)
        if (LeafBits[k >> 3] & (1 << (k & 7)))
            LeafSee++;

    if (LeafSee == 0)
    {
        fprintf(stderr, "ERROR: CollectLeafVisBits: Leaf %d can't see anything.\n", LeafNum);
        return false;
    }

    return true;
}


/* =========================================================================
   VisAllLeafs
   ========================================================================= */

static bool
VisAllLeafs(uint8_t *LeafVisBits)
{
    int32_t i;

    PortalSeen = (uint8_t *)calloc(NumVisPortals, sizeof(uint8_t));
    if (!PortalSeen)
    {
        fprintf(stderr, "ERROR: VisAllLeafs: out of memory for PortalSeen.\n");
        return false;
    }

    for (i = 0; i < NumVisLeafs; i++)
    {
        if (i % 50 == 0)
            printf("  FastVis leaf %d / %d...\n", i, NumVisLeafs);
        if (i >= 350)
            printf("  FastVis leaf %d...\n", i);
        fflush(stdout);
        FloodLeafPortalsFast(i);
    }

    printf("  FastVis complete.\n"); fflush(stdout);

    SortPortals();

    printf("  SortPortals complete.\n"); fflush(stdout);

    if (FullVis)
    {
        if (!FloodPortalsSlow())
            return false;
        printf("  SlowVis complete.\n"); fflush(stdout);
    }

    printf("  Freeing PortalSeen...\n"); fflush(stdout);
    free(PortalSeen);
    PortalSeen = NULL;
    printf("  Allocating PortalBits (%d bytes)...\n", NumVisPortalBytes); fflush(stdout);

    PortalBits = (uint8_t *)calloc(NumVisPortalBytes, sizeof(uint8_t));
    if (!PortalBits)
    {
        fprintf(stderr, "ERROR: VisAllLeafs: out of memory for PortalBits.\n");
        return false;
    }

    printf("  CollectLeafVisBits starting...\n"); fflush(stdout);

    TotalVisibleLeafs = 0;

    for (i = 0; i < NumVisLeafs; i++)
    {
        if (i % 50 == 0) { printf("  Collect leaf %d / %d\n", i, NumVisLeafs); fflush(stdout); }
        LeafSee = 0;
        if (!CollectLeafVisBits(i, LeafVisBits))
            return false;
        TotalVisibleLeafs += LeafSee;
    }

    free(PortalBits);
    PortalBits = NULL;

    printf("Total visible areas            : %5i\n", TotalVisibleLeafs);
    printf("Average visible from each area : %5i\n",
           NumVisLeafs > 0 ? TotalVisibleLeafs / NumVisLeafs : 0);

    return true;
}


/* =========================================================================
   BuildVisPortals
       Walks the leaf nodes of the live BSP portal tree (must be called
       before FreePortals) to populate VisPortalArray and VisLeafs.
       Also stamps PortalLeafNum onto each non-solid leaf node so the
       renderer can look up vis rows at runtime.
   ========================================================================= */

/* Pass 1: number non-solid leaves, stamp PortalLeafNum */
static void
NumberLeafs_r(GBSP_Node *Node, int32_t *count)
{
    if (Node->PlaneNum == PLANENUM_LEAF)
    {
        Node->PortalLeafNum = (Node->Contents & BSP_CONTENTS_SOLID2) ? -1 : (*count)++;
        return;
    }
    NumberLeafs_r(Node->Children[0], count);
    NumberLeafs_r(Node->Children[1], count);
}

/* Pass 2: count directed portals */
static void
CountPortals_r(GBSP_Node *Node, int32_t *count)
{
    if (Node->PlaneNum == PLANENUM_LEAF)
    {
        if (Node->Contents & BSP_CONTENTS_SOLID2) return;
        if (Node->PortalLeafNum < 0) return;

        GBSP_Portal *p;
        int32_t      side;
        for (p = Node->Portals; p; p = p->Next[side])
        {
            side = (p->Nodes[1] == Node);
            if (side != 0) continue;

            GBSP_Node *n0 = p->Nodes[0];
            GBSP_Node *n1 = p->Nodes[1];
            if (n0->PlaneNum != PLANENUM_LEAF || n1->PlaneNum != PLANENUM_LEAF) continue;
            if (n0->PortalLeafNum < 0 || n1->PortalLeafNum < 0) continue;

            *count += 2;
        }
        return;
    }
    CountPortals_r(Node->Children[0], count);
    CountPortals_r(Node->Children[1], count);
}

/* Pass 3: fill arrays */
static void
FillPortals_r(GBSP_Node *Node, int32_t *idx)
{
    if (Node->PlaneNum == PLANENUM_LEAF)
    {
        if (Node->Contents & BSP_CONTENTS_SOLID2) return;
        if (Node->PortalLeafNum < 0) return;

        GBSP_Portal *p;
        int32_t      side;
        for (p = Node->Portals; p; p = p->Next[side])
        {
            side = (p->Nodes[1] == Node);
            if (side != 0) continue;

            GBSP_Node *n0 = p->Nodes[0];
            GBSP_Node *n1 = p->Nodes[1];
            if (n0->PlaneNum != PLANENUM_LEAF || n1->PlaneNum != PLANENUM_LEAF) continue;

            int32_t leaf0 = n0->PortalLeafNum;
            int32_t leaf1 = n1->PortalLeafNum;
            if (leaf0 < 0 || leaf1 < 0) continue;

            if (*idx + 1 >= NumVisPortals)
            {
                fprintf(stderr, "ERROR: FillPortals_r: portal index overflow\n");
                return;
            }

            /* Portal A: leaf0 -> leaf1
               GBSP poly normal faces Nodes[0]=leaf0. We need normal facing
               INTO leaf1 (away from leaf0), so reverse the winding. */
            {
                VIS_Portal *vp  = &VisPortalArray[*idx];
                GBSP_Poly  *cp  = NULL;
                GBSP_Poly  *rev = NULL;
                if (!CopyPoly(p->Poly, &cp) || !cp)
                {
                    fprintf(stderr, "ERROR: FillPortals_r: CopyPoly failed (A)\n");
                    return;
                }
                if (!ReversePoly(cp, &rev) || !rev)
                {
                    fprintf(stderr, "ERROR: FillPortals_r: ReversePoly failed (A)\n");
                    FreePoly(cp);
                    return;
                }
                FreePoly(cp);
                vp->Poly    = rev;
                vp->Leaf    = leaf1;
                vp->SrcLeaf = leaf0;
                PlaneFromVerts(rev->Verts, &vp->Plane);
                vp->Next  = VisLeafs[leaf0].Portals;
                VisLeafs[leaf0].Portals = vp;
                CalcPortalInfo(vp);
                (*idx)++;
            }

            /* Portal B: leaf1 -> leaf0
               Original poly normal faces leaf0, which is INTO leaf1's
               portal direction — use as-is. */
            {
                VIS_Portal *vp = &VisPortalArray[*idx];
                GBSP_Poly  *cp = NULL;
                if (!CopyPoly(p->Poly, &cp) || !cp)
                {
                    fprintf(stderr, "ERROR: FillPortals_r: CopyPoly failed (B)\n");
                    return;
                }
                vp->Poly    = cp;
                vp->Leaf    = leaf0;
                vp->SrcLeaf = leaf1;
                PlaneFromVerts(cp->Verts, &vp->Plane);
                vp->Next  = VisLeafs[leaf1].Portals;
                VisLeafs[leaf1].Portals = vp;
                CalcPortalInfo(vp);
                (*idx)++;
            }
        }
        return;
    }
    FillPortals_r(Node->Children[0], idx);
    FillPortals_r(Node->Children[1], idx);
}

static bool
BuildVisPortals(GBSP_Node *RootNode)
{
    int32_t leaf_count   = 0;
    int32_t portal_count = 0;
    int32_t idx          = 0;

    NumberLeafs_r(RootNode, &leaf_count);
    NumVisLeafs = leaf_count;
    if (NumVisLeafs == 0)
    {
        fprintf(stderr, "ERROR: BuildVisPortals: no non-solid leaves found.\n");
        return false;
    }

    CountPortals_r(RootNode, &portal_count);
    NumVisPortals = portal_count;
    if (NumVisPortals == 0)
    {
        fprintf(stderr, "ERROR: BuildVisPortals: no vis portals found.\n");
        return false;
    }
    if (NumVisPortals >= MAX_TEMP_PORTALS)
    {
        fprintf(stderr, "ERROR: BuildVisPortals: too many portals (%d).\n", NumVisPortals);
        return false;
    }

    printf("BuildVisPortals: %d leaves, %d directed portals\n",
           NumVisLeafs, NumVisPortals);

    VisPortalArray = (VIS_Portal *)calloc(NumVisPortals, sizeof(VIS_Portal));
    if (!VisPortalArray) { fprintf(stderr, "ERROR: BuildVisPortals: OOM VisPortalArray\n"); return false; }

    VisSortedPortals = (VIS_Portal **)calloc(NumVisPortals, sizeof(VIS_Portal *));
    if (!VisSortedPortals) { fprintf(stderr, "ERROR: BuildVisPortals: OOM VisSortedPortals\n"); return false; }

    VisLeafs = (VIS_Leaf *)calloc(NumVisLeafs, sizeof(VIS_Leaf));
    if (!VisLeafs) { fprintf(stderr, "ERROR: BuildVisPortals: OOM VisLeafs\n"); return false; }

    FillPortals_r(RootNode, &idx);

    if (idx != NumVisPortals)
    {
        fprintf(stderr, "WARNING: BuildVisPortals: expected %d portals, filled %d.\n",
                NumVisPortals, idx);
        NumVisPortals = idx;
    }

    NumVisLeafBytes   = ((NumVisLeafs   + 63) & ~63) >> 3;
    NumVisPortalBytes = ((NumVisPortals + 63) & ~63) >> 3;
    NumVisPortalLongs = NumVisPortalBytes / (int32_t)sizeof(uint32_t);
    NumVisLeafLongs   = NumVisLeafBytes   / (int32_t)sizeof(uint32_t);

    return true;
}


/* =========================================================================
   RunVis  — public entry point
   ========================================================================= */

bool
RunVis(GBSP_Node *RootNode, GBSP_Model *Model, bool full_vis, bool verbose)
{
    FullVis    = full_vis;
    VisVerbose = verbose;

    FreeWorkingData();
    FreeModelVisData(Model);

    printf(" --- RunVis --- \n");

    if (!BuildVisPortals(RootNode))
        goto fail;

    printf("NumPortals : %5i\n", NumVisPortals);
    printf("NumLeafs   : %5i\n", NumVisLeafs);

    /* Allocate the output table on the model */
    Model->LeafVisBits = (uint8_t *)calloc(NumVisLeafs * NumVisLeafBytes, sizeof(uint8_t));
    if (!Model->LeafVisBits)
    {
        fprintf(stderr, "ERROR: RunVis: OOM for LeafVisBits\n");
        goto fail;
    }
    Model->NumVisLeafs    = NumVisLeafs;
    Model->NumVisLeafBytes = NumVisLeafBytes;

    if (!VisAllLeafs(Model->LeafVisBits))
        goto fail;

    FreeWorkingData();
    return true;

fail:
    FreeWorkingData();
    FreeModelVisData(Model);
    return false;
}
