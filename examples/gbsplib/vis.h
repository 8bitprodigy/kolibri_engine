/****************************************************************************************/
/*  vis.h                                                                               */
/*                                                                                      */
/*  Ported from Genesis3D Vis.h / VisFlood.cpp to C99/raylib                           */
/*  File I/O and Genesis3D infrastructure removed; operates on the live portal tree     */
/*  inside GBSP_Model while portals are still intact (before FreePortals is called).   */
/****************************************************************************************/
#ifndef VIS_H
#define VIS_H

#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>

#include "bsp.h"
#include "poly.h"


typedef struct VIS_Portal
{
    struct VIS_Portal *Next;
    GBSP_Poly  *Poly;
    GBSP_Plane  Plane;
    Vector3     Center;
    float       Radius;
    uint8_t    *VisBits;
    uint8_t    *FinalVisBits;
    int32_t     Leaf;       /* leaf this portal looks INTO  */
    int32_t     SrcLeaf;    /* leaf this portal belongs TO  */
    int32_t     MightSee;
    int32_t     CanSee;
    bool        Done;
}
VIS_Portal;

typedef struct
{
    VIS_Portal *Portals;
    int32_t     MightSee;
    int32_t     CanSee;
}
VIS_Leaf;

#define MAX_TEMP_PORTALS 25000

typedef struct
{
    uint8_t    VisBits[MAX_TEMP_PORTALS / 8];
    GBSP_Poly *Source;
    GBSP_Poly *Pass;
}
VIS_PStack;


/* Globals shared between vis.c and visflood.c */
extern int32_t     NumVisPortals;
extern int32_t     NumVisPortalBytes;
extern int32_t     NumVisPortalLongs;
extern VIS_Portal *VisPortalArray;
extern VIS_Portal **VisSortedPortals;
extern uint8_t    *PortalSeen;

extern int32_t     NumVisLeafs;
extern int32_t     NumVisLeafBytes;
extern int32_t     NumVisLeafLongs;
extern VIS_Leaf   *VisLeafs;

extern int32_t     TotalVisibleLeafs;
extern bool        VisVerbose;
extern bool        NoSort;
extern bool        FullVis;


/*
    RunVis
        Call BEFORE FreePortals while GBSP_Portal lists on leaf nodes are live.
        Writes results into Model->LeafVisBits, Model->NumVisLeafs,
        Model->NumVisLeafBytes.
*/
bool RunVis(GBSP_Node *RootNode, GBSP_Model *Model, bool full_vis, bool verbose);

/* Release Model->LeafVisBits and zero the vis counts. */
void FreeModelVisData(GBSP_Model *Model);

/* visflood.c entry points */
void FloodLeafPortalsFast(int32_t LeafNum);
bool FloodPortalsSlow    (void);
bool PortalCanSeePortal  (VIS_Portal *Portal1, VIS_Portal *Portal2);


#endif /* VIS_H */
