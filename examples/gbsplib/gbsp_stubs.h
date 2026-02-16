/****************************************************************************************/
/*  gbsp_stubs.h - Minimal stubs for engine-agnostic BSP compilation                    */
/****************************************************************************************/
#ifndef GBSP_STUBS_H
#define GBSP_STUBS_H

#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>
#include "bsp.h"  // Gets MAP_Entity and other core types

//====================================================================================
// Content Flags
//====================================================================================
#define BSP_CONTENTS_SOLID              0x00000001
#define BSP_CONTENTS_WINDOW             0x00000002
#define BSP_CONTENTS_EMPTY              0x00000010
#define BSP_CONTENTS_TRANSLUCENT        0x00000020
#define BSP_CONTENTS_WAVY               0x00000040
#define BSP_CONTENTS_DETAIL             0x00000100
#define BSP_CONTENTS_CLIP               0x00010000
#define BSP_CONTENTS_HINT               0x00020000
#define BSP_CONTENTS_AREA               0x00040000
#define BSP_CONTENTS_AREAPORTAL         0x00080000
#define BSP_CONTENTS_SHEET              0x00100000

// Version 2 flags (with 2 suffix)
#define BSP_CONTENTS_SOLID2             BSP_CONTENTS_SOLID
#define BSP_CONTENTS_WINDOW2            BSP_CONTENTS_WINDOW
#define BSP_CONTENTS_EMPTY2             BSP_CONTENTS_EMPTY
#define BSP_CONTENTS_TRANSLUCENT2       BSP_CONTENTS_TRANSLUCENT
#define BSP_CONTENTS_WAVY2              BSP_CONTENTS_WAVY
#define BSP_CONTENTS_DETAIL2            BSP_CONTENTS_DETAIL
#define BSP_CONTENTS_CLIP2              BSP_CONTENTS_CLIP
#define BSP_CONTENTS_HINT2              BSP_CONTENTS_HINT
#define BSP_CONTENTS_AREA2              BSP_CONTENTS_AREA
#define BSP_CONTENTS_SOLID_CLIP         (BSP_CONTENTS_SOLID | BSP_CONTENTS_CLIP)

// Visible contents (what shows up in rendering)
#define BSP_VISIBLE_CONTENTS            (BSP_CONTENTS_SOLID | BSP_CONTENTS_WINDOW | \
                                         BSP_CONTENTS_EMPTY | BSP_CONTENTS_TRANSLUCENT | \
                                         BSP_CONTENTS_WAVY)

//====================================================================================
// Texture Info Flags
//====================================================================================
#define TEXINFO_MIRROR                  0x0001
#define TEXINFO_SKY                     0x0002
#define TEXINFO_TRANS                   0x0004
#define TEXINFO_GOURAUD                 0x0008
#define TEXINFO_FLAT                    0x0010
#define TEXINFO_FULLBRIGHT              0x0020

//====================================================================================
// Side Flags
//====================================================================================
#define SIDE_HINT                       0x01
#define SIDE_SHEET                      0x02
#define SIDE_VISIBLE                    0x04
#define SIDE_TESTED                     0x08
#define SIDE_NODE                       0x10

//====================================================================================
// Entity Flags
//====================================================================================
#define ENTITY_HAS_ORIGIN               0x0001

//====================================================================================
// Texture Info Structure (Minimal)
//====================================================================================
typedef struct GFX_TexInfo
{
    int32_t     Texture;            // Texture index
    Vector3     Vecs[2];            // UV vectors
    float       Shift[2];           // UV shift
    float       DrawScale[2];       // UV scale
    float       FaceLight;          // Light emission
    uint32_t    Flags;              // TEXINFO_* flags
    float       Alpha;              // Transparency
    float       MipMapBias;         // Mipmap bias
    float       ReflectiveScale;    // Reflection scale
} GFX_TexInfo;

//====================================================================================
// Map File Structures (Minimal for loading)
//====================================================================================
typedef struct MAP_BrushHeader
{
    int32_t     NumFaces;
    uint32_t    Flags;              // Content flags
} MAP_BrushHeader;

typedef struct MAP_FaceHeader
{
    char        TexName[64];        // Texture name
    float       uVecX, uVecY, uVecZ;// U texture vector
    float       vVecX, vVecY, vVecZ;// V texture vector
    float       OffsetX, OffsetY;   // UV offset
    float       DrawScaleX, DrawScaleY; // UV scale
    float       FaceLight;          // Light emission
    uint32_t    Flags;              // Face flags
    float       Alpha;              // Transparency
    float       MipMapBias;         // Mipmap bias
    float       ReflectiveScale;    // Reflection scale
} MAP_FaceHeader;

//====================================================================================
// Sky Data
//====================================================================================
typedef struct {
    int32_t Textures[6];
    Vector3 Axis;
    float Dpm;
    float DrawScale;
} GFX_SkyData_t;

extern GFX_SkyData_t GFXSkyData;

#define TEXTURE_SKYBOX 0x0001

//====================================================================================
// Texture System Stubs
//====================================================================================

// Global texture info array
extern GFX_TexInfo *TexInfo;
extern int32_t NumTexInfo;

// Find or create texture by name (returns index)
static inline int32_t FindTextureIndex(const char *name, int32_t flags)
{
    // Stub: Just return 0 (default texture)
    // TODO: Implement proper texture lookup
    return 0;
}

// Find or create TexInfo (returns index)
static inline int32_t FindTexInfo(GFX_TexInfo *ti)
{
    // Stub: Just return 0
    // TODO: Implement proper texinfo lookup/creation
    if (!TexInfo)
    {
        // Allocate initial array
        NumTexInfo = 1;
        TexInfo = malloc(sizeof(GFX_TexInfo) * 256);
        memset(TexInfo, 0, sizeof(GFX_TexInfo) * 256);
    }
    
    // For now, just add it
    if (NumTexInfo < 256)
    {
        TexInfo[NumTexInfo] = *ti;
        return NumTexInfo++;
    }
    
    return 0;
}

//====================================================================================
// Brush Counting Globals
//====================================================================================
extern int32_t NumSolidBrushes;
extern int32_t NumCutBrushes;
extern int32_t NumHollowCutBrushes;
extern int32_t NumDetailBrushes;
extern int32_t NumTotalBrushes;

//====================================================================================
// Cancel Request (for interrupting compilation)
//====================================================================================
extern bool CancelRequest;

//====================================================================================
// GFX File Structures (For output - can be stubs for now)
//====================================================================================

typedef struct GFX_Area
{
    int32_t FirstAreaPortal;
    int32_t NumAreaPortals;
} GFX_Area;

typedef struct GFX_AreaPortal
{
    int32_t Area;
    int32_t ModelNum;
} GFX_AreaPortal;

extern GFX_Area *GFXAreas;
extern GFX_AreaPortal *GFXAreaPortals;
extern int32_t NumGFXAreas;
extern int32_t NumGFXAreaPortals;

// Entity data (for output)
extern void *GFXEntData;
extern int32_t NumGFXEntData;

//====================================================================================
// Model Functions
//====================================================================================
static inline GBSP_Model *ModelForLeafNode(GBSP_Node *Node)
{
    // Stub: return first model
    // TODO: Implement proper model lookup based on entity
    extern GBSP_Model BSPModels[];
    return &BSPModels[0];
}

//====================================================================================
// Initialization
//====================================================================================
static inline void InitializeStubs(void)
{
    // Initialize globals
    NumEntities = 1; // World entity
    Entities = malloc(sizeof(MAP_Entity) * 256);
    memset(Entities, 0, sizeof(MAP_Entity) * 256);
    
    NumSolidBrushes = 0;
    NumCutBrushes = 0;
    NumHollowCutBrushes = 0;
    NumDetailBrushes = 0;
    NumTotalBrushes = 0;
    
    CancelRequest = false;
    
    TexInfo = NULL;
    NumTexInfo = 0;
    
    GFXAreas = NULL;
    GFXAreaPortals = NULL;
    NumGFXAreas = 0;
    NumGFXAreaPortals = 0;
    
    GFXEntData = NULL;
    NumGFXEntData = 0;
    
    // Initialize sky data
    memset(&GFXSkyData, 0, sizeof(GFXSkyData));
    for (int i = 0; i < 6; i++)
        GFXSkyData.Textures[i] = -1;
}

static inline void CleanupStubs(void)
{
    if (Entities) free(Entities);
    if (TexInfo) free(TexInfo);
    if (GFXAreas) free(GFXAreas);
    if (GFXAreaPortals) free(GFXAreaPortals);
    if (GFXEntData) free(GFXEntData);
}

#endif // GBSP_STUBS_H
