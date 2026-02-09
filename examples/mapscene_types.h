#ifndef MAPSCENE_TYPES_H
#define MAPSCENE_TYPES_H

#include <raylib.h>
#include <stdbool.h>

/*
    MAPSCENE_TYPES.H
    
    Shared type definitions used by both mapscene.c and mapscene_bsp.c
    This avoids circular dependencies and keeps the BSP code modular.
*/

/* CompiledFace - A polygon face from brush compilation */
typedef struct CompiledFace {
    int     
            vertex_start, /* Index into vertex array */
            vertex_count, /* Number of vertices in this face */
            brush_idx;    /* Track brush index */
    Vector3 normal;       /* Face plane normal */
    float   plane_dist;   /* Face plane distance from origin */
    bool    is_visible;   /* Visibility flag (used before BSP) */
} CompiledFace;


typedef struct
CompiledBrush
{
    int face_start;
    int face_count;
}
CompiledBrush;


/*
    BSP TREE DATA STRUCTURES
    
    This implements proper Binary Space Partitioning with flood-fill:
    1. Build BSP tree from all faces, partitioning space into leaves
    2. Classify each leaf as SOLID (inside brush) or EMPTY (air)
    3. Flood-fill from info_player_start to mark reachable EMPTY leaves as "inside"
    4. Cull faces that don't border any "inside" leaf
    
    This matches the algorithm used by Quake's QBSP compiler.
*/

#define BSP_EPSILON 0.01f

/* Classification of a point/polygon relative to a plane */
typedef enum {
    SIDE_FRONT = 0,
    SIDE_BACK  = 1,
    SIDE_ON    = 2,
    SIDE_SPLIT = 3
} PlaneSide;

/* Leaf content type */
typedef enum {
    CONTENTS_EMPTY = 0,  /* Air/playable space */
    CONTENTS_SOLID = 1   /* Inside brush or outside void */
} LeafContents;

/* Forward declarations */
typedef struct BSPNode BSPNode;
typedef struct BSPLeaf BSPLeaf;
typedef struct BSPFace BSPFace;
typedef struct BSPTree BSPTree;

/* BSPFace - A polygon in the BSP tree */
typedef struct BSPFace {
    Vector3 *vertices;
    int 
             vertex_count,
             original_face_idx;
    Vector3  normal;
    float    plane_dist;
    struct BSPFace *next;
} BSPFace;

/* BSPNode - Interior node with splitting plane */
struct BSPNode {
    Vector3 plane_normal;
    float plane_dist;
    
    int front_child;
    int back_child;
    
    bool front_is_leaf;
    bool back_is_leaf;
};

/* BSPLeaf - Leaf node with classified contents */
struct BSPLeaf {
    LeafContents contents;
    BSPFace *faces;
    int face_count;
    
    Vector3 bounds_min;
    Vector3 bounds_max;
    
    int leaf_index;
    bool is_reachable;  /* Set by flood-fill from player start */
    int flood_parent;   /* Parent leaf in flood-fill (for leak path tracing), -1 if none */
};

/* BSPTree - Complete tree structure */
struct BSPTree {
    BSPNode *nodes;
    int node_count;
    int node_capacity;
    
    BSPLeaf *leaves;
    int leaf_count;
    int leaf_capacity;
    
    bool root_is_leaf;
    
    int total_faces;
    int visible_faces;
    int max_tree_depth;
};


#endif /* MAPSCENE_TYPES_H */
