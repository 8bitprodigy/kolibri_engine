/****************************************************************************************/
/*  bsp.h                                                                               */
/*                                                                                      */
/*  Core BSP data structures and types                                                  */
/*  Converted from Genesis3D to use raylib/C99                                          */
/*                                                                                      */
/****************************************************************************************/
#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>

#include "common.h"
#include "mathlib.h"


#define GBSP_VERSION                15

#define GBSP_CHUNK_HEADER           0

#define GBSP_CHUNK_MODELS           1
#define GBSP_CHUNK_NODES            2
#define GBSP_CHUNK_BNODES           3
#define GBSP_CHUNK_LEAFS            4
#define GBSP_CHUNK_CLUSTERS         5	
#define GBSP_CHUNK_AREAS            6	
#define GBSP_CHUNK_AREA_PORTALS     7	
#define GBSP_CHUNK_LEAF_SIDES       8
#define GBSP_CHUNK_PORTALS          9
#define GBSP_CHUNK_PLANES           10
#define GBSP_CHUNK_FACES            11
#define GBSP_CHUNK_LEAF_FACES       12
#define GBSP_CHUNK_VERT_INDEX       13
#define GBSP_CHUNK_VERTS            14
#define GBSP_CHUNK_RGB_VERTS        15
#define GBSP_CHUNK_ENTDATA          16

#define GBSP_CHUNK_TEXINFO          17
#define GBSP_CHUNK_TEXTURES         18 
#define GBSP_CHUNK_TEXDATA          19

#define GBSP_CHUNK_LIGHTDATA        20
#define GBSP_CHUNK_VISDATA          21

#define GBSP_CHUNK_SKYDATA          22

#define GBSP_CHUNK_PALETTES         23

#define GBSP_CHUNK_MOTIONS          24

#define GBSP_CHUNK_END              0xffff

#define MAX_GBSP_ENTDATA            200000


#define BSP_CONTENTS_SOLID2         (1<<0)		// Solid (Visible)
#define BSP_CONTENTS_WINDOW2        (1<<1)		// Window (Visible)
#define BSP_CONTENTS_EMPTY2         (1<<2)		// Empty but Visible (water, lava, etc...)

#define BSP_CONTENTS_TRANSLUCENT2   (1<<3)		// Vis will see through it
#define BSP_CONTENTS_WAVY2          (1<<4)		// Wavy (Visible)
#define BSP_CONTENTS_DETAIL2        (1<<5)		// Won't be included in vis oclusion

#define BSP_CONTENTS_CLIP2          (1<<6)		// Structural but not visible
#define BSP_CONTENTS_HINT2          (1<<7)		// Primary splitter (Non-Visible)
#define BSP_CONTENTS_AREA2          (1<<8)		// Area seperator leaf (Non-Visible)

#define BSP_CONTENTS_FLOCKING       (1<<9)		// flocking flag.  Not really a contents type
#define BSP_CONTENTS_SHEET          (1<<10)
#define RESERVED3                   (1<<11)
#define RESERVED4                   (1<<12)
#define RESERVED5                   (1<<13)
#define RESERVED6                   (1<<14)
#define RESERVED7                   (1<<15)

// 16-31 reserved for user contents
#define BSP_CONTENTS_USER1          (1<<16)
#define BSP_CONTENTS_USER2          (1<<17)
#define BSP_CONTENTS_USER3          (1<<18)
#define BSP_CONTENTS_USER4          (1<<19)
#define BSP_CONTENTS_USER5          (1<<20)
#define BSP_CONTENTS_USER6          (1<<21)
#define BSP_CONTENTS_USER7          (1<<22)
#define BSP_CONTENTS_USER8          (1<<23)
#define BSP_CONTENTS_USER9          (1<<24)
#define BSP_CONTENTS_USER10         (1<<25)
#define BSP_CONTENTS_USER11         (1<<26)
#define BSP_CONTENTS_USER12         (1<<27)
#define BSP_CONTENTS_USER13         (1<<28)
#define BSP_CONTENTS_USER14         (1<<29)
#define BSP_CONTENTS_USER15         (1<<30)
#define BSP_CONTENTS_USER16         (1<<31)
// 16-31 reserved for user contents


// These contents are all solid types
#define BSP_CONTENTS_SOLID_CLIP    (BSP_CONTENTS_SOLID2 | BSP_CONTENTS_WINDOW2 | BSP_CONTENTS_CLIP2)

// These contents are all visible types
#define BSP_VISIBLE_CONTENTS       (BSP_CONTENTS_SOLID2 | \
									BSP_CONTENTS_EMPTY2 | \
									BSP_CONTENTS_WINDOW2 | \
									BSP_CONTENTS_SHEET | \
									BSP_CONTENTS_WAVY2)

// These contents define where faces are NOT allowed to merge across
#define BSP_MERGE_SEP_CONTENTS     (BSP_CONTENTS_WAVY2 | \
									BSP_CONTENTS_HINT2 | \
									BSP_CONTENTS_AREA2)
//*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=
//*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=

typedef struct
{
	int32 
        Type,     // Type of chunk
        Size,     // Size of each element
        Elements; // Number of elements
} 
GBSP_Chunk;

typedef struct
{
	int32  
        Type,
        Size,
        Elements;
	void  *Data;
} 
GBSP_ChunkData;

typedef struct
{
	char   TAG[5];						// 'G','B','S','P','0'
	int32  Version;
	double BSPTime;					// Time at which the BSP data was created...
} 
GBSP_Header;

typedef struct
{
	Vector3 Axis;						// Axis of rotation
	float   Dpm;						// Degres per minute
	int32   Textures[6];				// Texture indexes for all six sides...
	float   DrawScale;
} 
GFX_SkyData;

typedef struct
{
	Vector3 Normal;
	float   Dist;
	int32   Type;						// Defined in MathLib.h (PLANE_X, PLANE_Y, etc...)
} 
GFX_Plane;

typedef struct
{
	int32   
        Children[2], // Children, indexed into GFXNodes, < 0 = Leaf
        NumFaces,    // Num faces
        FirstFace,   // First face
        PlaneNum;
	Vector3 
        Mins,        // For BBox frustum culling
        Maxs;
} 
GFX_Node;

typedef struct
{
	int32			
        Children[2], // Children, indexed into GFXBNodes, < 0 = Contents
        PlaneNum;
} 
GFX_BNode;

typedef struct
{
	int32			
        PlaneNum,
        PlaneSide;
} 
GFX_LeafSide;

typedef struct
{
	int32
        ModelNum,
        Area;
} 
GFX_AreaPortal;

typedef struct
{
	int32
        NumAreaPortals,
        FirstAreaPortal;
} 
GFX_Area;

typedef struct
{
	int32   Contents;					// Contents of leaf
	Vector3
        Mins,						// For BBox vis testing
        Maxs;
    int32
        FirstFace,					// First face in GFXLeafFaces 
        NumFaces,  
         
        FirstPortal,				// Number of portals
        NumPortals,					// First portal    
        Cluster,					// CHANGE: CLUSTER
        
        Area,    
        FirstSide,					// Planes (plus bevels) pointing out of leaf
        NumSides;					// (For axial bounding box collisions)
} 
GFX_Leaf;

typedef struct
{
	int32			VisOfs;
} GFX_Cluster;

typedef struct
{
    int32
        FirstVert,					// First vertex indexed in GFXVertices
        NumVerts,					// Number of vertices in face
        PlaneNum,					// PlaneNum 
        PlaneSide,					// 0 = Same direction of plane normal
        TexInfo,
        LightOfs,					// Offset info GFXLightData, -1 = No light
        LWidth,
        LHeight;
	uint8 LTypes[4];
} 
GFX_Face;

typedef struct
{
	int32			RootNode[2];				// Top level Node in GFXNodes/GFXBNodes
	Vector3			Mins;
	Vector3			Maxs;
	Vector3			Origin;						// Center of model
	int32			FirstFace;					// First face in GFXFaces
	int32			NumFaces;					// Number of faces
	int32			FirstLeaf;					// First leaf in GFXLeafs;
	int32			NumLeafs;					// Number of leafs (not including solid leaf)
	int32			FirstCluster;
	int32			NumClusters;
	int32			Areas[2];					// Area on each side of the model
//	geMotion *		Motion;
} 
GFX_Model;

typedef struct
{
	uint8			RGB[256*3];
} GFX_Palette;

typedef struct
{
	char			Name[32];
	uint32			Flags;
	int32			Width;
	int32			Height;
	int32			Offset;
	int32			PaletteIndex;
} GFX_Texture;

typedef struct
{
	Vector3			Vecs[2];
	float			Shift[2];
	float			DrawScale[2];
	int32			Flags;
	float			FaceLight;
	float			ReflectiveScale;
	float			Alpha;
	float			MipMapBias;
	int32			Texture;
} GFX_TexInfo;

typedef struct
{
	Vector3			Origin;						// Center of portal
	int32			LeafTo;						// Leaf looking into
} GFX_Portal;

//====================================================================================
// Global defines
//====================================================================================
#define MAX_BSP_MODELS          2048
#define MAX_BSP_PLANES          32000
#define MAX_WELDED_VERTS        64000

#define PSIDE_FRONT             1
#define PSIDE_BACK              2
#define PSIDE_BOTH              (PSIDE_FRONT|PSIDE_BACK)
#define PSIDE_FACING            4

#define PLANENUM_LEAF           -1

#define DIST_EPSILON            0.01f
#define ANGLE_EPSILON           0.00001f

//====================================================================================
// Forward declarations
//====================================================================================
typedef struct GBSP_Node GBSP_Node;
typedef struct GBSP_Portal GBSP_Portal;
typedef struct GBSP_Brush GBSP_Brush;
typedef struct GBSP_Side GBSP_Side;
typedef struct GBSP_Face GBSP_Face;
typedef struct GBSP_Poly GBSP_Poly;
typedef struct GBSP_Plane GBSP_Plane;
typedef struct GBSP_Model GBSP_Model;
typedef struct MAP_Brush MAP_Brush;

//====================================================================================
// Polygon
//====================================================================================
struct GBSP_Poly
{
    int32_t     NumVerts;
    Vector3     *Verts;
};

//====================================================================================
// Plane
//====================================================================================
struct GBSP_Plane
{
    Vector3     Normal;
    float       Dist;
    int32_t     Type;
};

//====================================================================================
// Side (face of a brush)
//====================================================================================
// Side flags
#define SIDE_HINT       (1<<0)      // Side is a hint side
#define SIDE_SHEET      (1<<1)      // Side is a sheet
#define SIDE_VISIBLE    (1<<2)      // Side is visible
#define SIDE_TESTED     (1<<3)      // Side has been tested
#define SIDE_NODE       (1<<4)      // Side was used as BSP split

struct GBSP_Side
{
    GBSP_Poly   *Poly;
    int32_t     PlaneNum;
    uint8_t     PlaneSide;
    int32_t     TexInfo;
    uint8_t     Flags;
};

typedef struct
{
	int32	PlaneNum;
	int32	PlaneSide;
} 
GBSP_LeafSide;

typedef struct 
GBSP_Node2
{
	struct GBSP_Node2		*Children[2];
	int32			PlaneNum;			// -1 == Leaf
	
	// For leafs
	int32			Contents;
} 
GBSP_Node2;


//====================================================================================
// Face (renderable polygon)
//====================================================================================
struct GBSP_Face
{
    GBSP_Face   *Next;
    GBSP_Face   *Original;
    GBSP_Poly   *Poly;
    int32_t     Contents[2];
    int32_t     TexInfo;
    int32_t     PlaneNum;
    int32_t     PlaneSide;
    int32_t     Entity;
    uint8_t     Visible;
    
    // For output
    int32_t     OutputNum;
    int32_t     *IndexVerts;
    int32_t     FirstIndexVert;
    int32_t     NumIndexVerts;
    
    GBSP_Portal *Portal;
    GBSP_Face   *Split[2];
    GBSP_Face   *Merged;
};

//====================================================================================
// Portal (connection between leaves)
//====================================================================================
struct GBSP_Portal
{
    GBSP_Poly   *Poly;
    GBSP_Node   *Nodes[2];          // Nodes on each side
    GBSP_Portal *Next[2];           // Next portal for each node
    int32_t     PlaneNum;
    
    GBSP_Node   *OnNode;            // Node that created this portal
    GBSP_Face   *Face[2];           // Faces created from this portal
    GBSP_Side   *Side;              // Side matched to this portal
    uint8_t     SideFound;          // Whether FindPortalSide ran
};

//====================================================================================
// Node (interior node or leaf)
//====================================================================================
struct GBSP_Node
{
    // Node/Leaf common
    int32_t     PlaneNum;           // -1 if leaf
    int32_t     PlaneSide;
    int32_t     Contents;
    GBSP_Face   *Faces;
    GBSP_Node   *Children[2];
    GBSP_Node   *Parent;
    Vector3     Mins;
    Vector3     Maxs;
    
    // Leaf specific
    GBSP_Portal *Portals;
    int32_t     NumLeafFaces;
    GBSP_Face   **LeafFaces;
    int32_t     CurrentFill;        // For flood fill
    int32_t     Entity;             // Entity touching leaf
    int32_t     Occupied;           // Distance from outside
    int32_t     PortalLeafNum;
    
    int         Detail;
    int32_t     Cluster;
    int32_t     Area;
    
    GBSP_Brush  *Volume;
    GBSP_Side   *Side;
    GBSP_Brush  *BrushList;
    
    // For output
    int32_t     ChildrenID[2];
    int32_t     FirstFace;
    int32_t     NumFaces;
    int32_t     FirstPortal;
    int32_t     NumPortals;
    int32_t     FirstSide;
    int32_t     NumSides;
};

//====================================================================================
// Model
//====================================================================================
struct GBSP_Model
{
    GBSP_Node   *RootNode[2];       // 0 = DrawHull, 1 = ClipHull
    Vector3     Origin;
    GBSP_Node   OutsideNode;
    Vector3     Mins;
    Vector3     Maxs;
    
    // For output
    int32_t     RootNodeID[2];
    int32_t     FirstFace;
    int32_t     NumFaces;
    int32_t     FirstLeaf;
    int32_t     NumLeafs;
    int32_t     FirstCluster;
    int32_t     NumClusters;
    int32_t     NumSolidLeafs;
    
    int         IsAreaPortal;
    int32_t     Areas[2];
};

//====================================================================================
// Globals
//====================================================================================
extern GBSP_Model   BSPModels[MAX_BSP_MODELS];
extern int32_t      NumBSPModels;

extern bool         Verbose;
extern bool         EntityVerbose;

// Planes
extern GBSP_Plane   Planes[MAX_BSP_PLANES];
extern int32_t      NumPlanes;

extern bool CancelRequest;

//====================================================================================
// Functions
//====================================================================================

// Node allocation
GBSP_Node *AllocNode(void);
void FreeNode(GBSP_Node *Node);

// Cleanup
bool FreeAllGBSPData(void);
void CleanupGBSP(void);

// Plane operations
void PlaneFromVerts(Vector3 *Verts, GBSP_Plane *Plane);
void SidePlane(GBSP_Plane *Plane, int32_t *Side);
bool PlaneEqual(GBSP_Plane *Plane1, GBSP_Plane *Plane2);
void SnapVector(Vector3 *Normal);
float RoundInt(float In);
void SnapPlane(Vector3 *Normal, float *Dist);
void PlaneInverse(GBSP_Plane *Plane);
int32_t FindPlane(GBSP_Plane *Plane, int32_t *Side);
float PlanePointDistance(GBSP_Plane *Plane, Vector3 *Point);


// Fast point-to-plane distance (inline version)
static inline float Plane_PointDistanceFast(GBSP_Plane *Plane, Vector3 *Point)
{
    return PlanePointDistance(Plane, Point);
}

//====================================================================================
// Entity System (from MAP.H)
//====================================================================================

typedef struct MAP_Epair
{
    struct MAP_Epair *Next;
    char *Key;
    char *Value;
} MAP_Epair;

#define ENTITY_HAS_ORIGIN (1<<0)

typedef struct MAP_Entity
{
    void        *Brushes2;      // MAP_Brush list
    void        *Motion;        // Motion data
    MAP_Epair   *Epairs;        // Key-value pairs
    int32_t     ModelNum;
    char        ClassName[64];
    Vector3     Origin;
    float       Angle;
    int32_t     Light;
    int32_t     LType;
    char        Target[32];
    char        TargetName[32];
    uint32_t    Flags;
} MAP_Entity;

// Entity globals
extern int32_t NumEntities;
extern MAP_Entity *Entities;

// Entity functions
extern char *ValueForKey(MAP_Entity *ent, const char *key);
extern void SetKeyValue(MAP_Entity *ent, const char *key, const char *value);
extern float FloatForKey(MAP_Entity *ent, const char *key);
extern bool GetVectorForKey2(MAP_Entity *ent, const char *key, Vector3 *vec);

#endif
