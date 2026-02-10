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
    int 
        face_start,
        face_count;
}
CompiledBrush;


/*
    BSP TREE DATA STRUCTURES
    
    This implements proper Binary Space Partitioning with portal-based flood-fill:
    1. Build BSP tree from all faces, partitioning space into leaves
    2. Create portals at each partition plane during tree building
    3. Clip portals against brush geometry to determine if blocked
    4. Flood-fill through open portals from outside to mark exterior void
    5. Cull faces that don't border playable interior space
    
    This matches the algorithm used by Quake's QBSP compiler.
*/

#define BSP_EPSILON 0.01f

/* Classification of a point/polygon relative to a plane */
typedef enum {
    SIDE_FRONT = 0,
    SIDE_BACK  = 1,
    SIDE_ON    = 2,
    SIDE_SPLIT = 3
} 
PlaneSide;

/* Leaf content type */
typedef enum {
    CONTENTS_EMPTY = 0,  /* Air/playable space */
    CONTENTS_SOLID = 1   /* Inside brush or outside void */
} 
LeafContents;

/* Forward declarations */
typedef struct BSPNode BSPNode;
typedef struct BSPLeaf BSPLeaf;
typedef struct BSPFace BSPFace;
typedef struct BSPPortal BSPPortal;
typedef struct BSPTree BSPTree;

/* BSPFace - A polygon in the BSP tree */
typedef struct BSPFace {
    Vector3        *vertices;
    int 
                    vertex_count,
                    original_face_idx;
    Vector3         normal;
    float           plane_dist;
    struct BSPFace *next;
} 
BSPFace;

/* ========================================================================
   PHASE 1: PORTAL DATA STRUCTURES
   ======================================================================== */

/* Forward declare winding_t (defined in mapscene_bsp.c) */
typedef struct winding_s winding_t;

/* BSPPortal - A connection between two adjacent leaves
 * 
 * Portals are created at partition planes during tree building.
 * They represent potential pathways between neighboring leaves.
 * A portal is "blocked" if it's completely clipped away by brush geometry,
 * meaning the two leaves are separated by a solid wall.
 * 
 * Portals enable accurate flood-fill: we only traverse through open portals,
 * naturally respecting brush geometry as barriers.
 */
typedef struct BSPPortal {
    /* Connectivity - which leaves does this portal connect? */
    int leaf_front;              /* Leaf index on front side of portal plane */
    int leaf_back;               /* Leaf index on back side of portal plane */
    
    /* Portal plane (the partition plane where portal was created) */
    Vector3 plane_normal;        /* Portal plane normal */
    float   plane_dist;          /* Portal plane distance from origin */
    
    /* Portal geometry - the actual polygon on the partition plane
     * NULL if portal was completely clipped away by brushes (blocked) */
    winding_t *winding;          /* Portal polygon (owned by portal) */
    
    /* Portal state */
    bool blocked;                /* True if portal is completely blocked by brushes */
    
    /* Linked list in leaf's portal list */
    struct BSPPortal *next_in_leaf;
} 
BSPPortal;

/* BSPNode - Interior node with splitting plane */
struct BSPNode {
    Vector3 plane_normal;
    float   plane_dist;
    int
            front_child,
            back_child;
    bool
            front_is_leaf,
            back_is_leaf;
};

/* BSPLeaf - Leaf node with classified contents */
struct BSPLeaf {
    BSPFace     *faces;
    Vector3
                 bounds_min,
                 bounds_max;
    /* Brush tracking (Stage 1c) - which brush volumes contain this leaf */
    int          face_count,
                 inside_brushes_capacity, /* Allocated size */
    /* Flood-fill data (Stage 3) */
                 leaf_index,
                 flood_parent;       /* Parent leaf in flood-fill (for leak path tracing), -1 if none */
    bool
                 is_reachable,
                 is_outside,        /* Touches void/world bounds (marked in Stage 2) */
                 flood_filled;      /* Reached by flood-fill from outside (Stage 3) */
    /* Final classification (Stage 4) - result of flood-fill */
    LeafContents contents;  /* SOLID or EMPTY (determined AFTER flood-fill) */
    
    /* ====================================================================
       PHASE 3: PORTAL CONNECTIVITY
       ==================================================================== */
    
    /* Portals are stored centrally in tree->portals array.
     * To find portals for this leaf, use GetLeafPortals() which iterates
     * the portal array and checks if portal->leaf_front or portal->leaf_back
     * matches this leaf's index.
     * 
     * This array-based approach is simpler than maintaining linked lists
     * and avoids the complexity of each portal needing two next pointers. */
};

/* BSPTree - Complete tree structure */
struct BSPTree {
    BSPNode *nodes;
    BSPLeaf *leaves;
    Vector3
             leak_entity_pos,
             leak_path[50];
    int
             leak_path_length,
             node_count,
             node_capacity,
             leaf_count,
             leaf_capacity,
             total_faces,
             visible_faces,
             max_tree_depth;
    bool     
             has_leak,
             root_is_leaf;
    
    /* ====================================================================
       PHASE 1: PORTAL STORAGE IN TREE
       ==================================================================== */
    
    /* Portal array - centralized storage for all portals
     * Each portal connects exactly 2 leaves (front and back children of a partition) */
    BSPPortal   *portals;           /* Array of all portals (owned by tree) */
    int          portal_count;      /* Number of portals created */
    int          portal_capacity;   /* Allocated capacity */
};


#endif /* MAPSCENE_TYPES_H */
