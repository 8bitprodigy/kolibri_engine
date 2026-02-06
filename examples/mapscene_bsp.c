/*
    STAGE 1 - BSP TREE BUILDING ONLY
    
    This is a minimal, clean implementation that ONLY:
    1. Builds the BSP tree structure
    2. Distributes faces to leaves during splitting
    3. Computes leaf bounds
    
    NO flood-fill, NO culling, NO portal generation yet.
    This lets us verify the tree is built correctly before adding complexity.
    
    FIXES:
    - Removed MAX_TREE_DEPTH (QBSP doesn't have one)
    - Better split plane selection heuristic
    - Proper termination conditions
    - Coplanar face handling improvements
*/

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mapscene_bsp.h"
#include "mapscene_types.h"
#include "v220_map_parser.h"
#include "kolibri.h"


#define INITIAL_NODE_CAPACITY 512
#define INITIAL_LEAF_CAPACITY 256
#define MIN_FACES_FOR_SPLIT 1      /* Split if we have at least 1 face */
#define MAX_SPLIT_VERTS 128
#define SPLIT_SAMPLE_SIZE 16       /* Sample more faces for better splits */

/* ========================================================================
   SIMPLE FACE LIST - For managing faces during tree building
   ======================================================================== */

typedef struct {
    BSPFace **faces;
    int face_count;
    int face_capacity;
} FaceList;

static FaceList *
FaceList_Create(void)
{
    FaceList *list = malloc(sizeof(FaceList));
    if (!list)
        return NULL;
    
    memset(list, 0, sizeof(FaceList));
    list->face_capacity = 64;
    list->faces = calloc(list->face_capacity, sizeof(BSPFace *));
    if (!list->faces) {
        free(list);
        return NULL;
    }
    return list;
}

static void
FaceList_Add(FaceList *list, BSPFace *face)
{
    if (!list || !face)
        return;
    
    if (list->face_count >= list->face_capacity) {
        int new_capacity = list->face_capacity * 2;
        BSPFace **new_faces = realloc(list->faces, new_capacity * sizeof(BSPFace *));
        if (!new_faces)
            return;
        list->faces = new_faces;
        list->face_capacity = new_capacity;
    }
    
    list->faces[list->face_count++] = face;
}

static void
FaceList_Free(FaceList *list)
{
    if (!list)
        return;
    free(list->faces);
    free(list);
}

/* ========================================================================
   BSP FACE HELPERS
   ======================================================================== */

static BSPFace *
BSPFace_Create(int vertex_count)
{
    if (vertex_count <= 0 || vertex_count > MAX_SPLIT_VERTS)
        return NULL;
    
    BSPFace *face = malloc(sizeof(BSPFace));
    if (!face)
        return NULL;
    
    face->vertices = malloc(vertex_count * sizeof(Vector3));
    if (!face->vertices) {
        free(face);
        return NULL;
    }
    
    face->vertex_count = vertex_count;
    face->next = NULL;
    face->normal = (Vector3){0, 0, 0};
    face->plane_dist = 0.0f;
    face->original_face_idx = -1;
    
    return face;
}

static void
BSPFace_Free(BSPFace *face)
{
    if (face) {
        free(face->vertices);
        free(face);
    }
}

/* ========================================================================
   PLANE CLASSIFICATION
   ======================================================================== */

PlaneSide
BSP_ClassifyPoint(Vector3 point, Vector3 plane_normal, float plane_dist)
{
    float d = Vector3DotProduct(point, plane_normal) - plane_dist;
    
    if (d > BSP_EPSILON)
        return SIDE_FRONT;
    else if (d < -BSP_EPSILON)
        return SIDE_BACK;
    else
        return SIDE_ON;
}

PlaneSide
BSP_ClassifyPolygon(
    const Vector3 *verts,
    int vert_count,
    Vector3 plane_normal,
    float plane_dist)
{
    int front_count = 0;
    int back_count = 0;
    
    for (int i = 0; i < vert_count; i++) {
        PlaneSide side = BSP_ClassifyPoint(verts[i], plane_normal, plane_dist);
        if (side == SIDE_FRONT)
            front_count++;
        else if (side == SIDE_BACK)
            back_count++;
    }
    
    if (front_count > 0 && back_count > 0)
        return SIDE_SPLIT;
    else if (front_count > 0)
        return SIDE_FRONT;
    else if (back_count > 0)
        return SIDE_BACK;
    else
        return SIDE_ON;
}

/* ========================================================================
   POLYGON SPLITTING
   ======================================================================== */

static void
SplitFace(
    const BSPFace *face,
    Vector3 split_normal,
    float split_dist,
    BSPFace **out_front,
    BSPFace **out_back)
{
    *out_front = NULL;
    *out_back = NULL;
    
    Vector3 front_verts[MAX_SPLIT_VERTS];
    Vector3 back_verts[MAX_SPLIT_VERTS];
    int front_count = 0;
    int back_count = 0;
    
    for (int i = 0; i < face->vertex_count; i++) {
        Vector3 cur = face->vertices[i];
        Vector3 next = face->vertices[(i + 1) % face->vertex_count];
        
        float d_cur = Vector3DotProduct(cur, split_normal) - split_dist;
        float d_next = Vector3DotProduct(next, split_normal) - split_dist;
        
        /* Add current vertex to appropriate side(s) */
        if (d_cur >= -BSP_EPSILON && front_count < MAX_SPLIT_VERTS)
            front_verts[front_count++] = cur;
        if (d_cur <= BSP_EPSILON && back_count < MAX_SPLIT_VERTS)
            back_verts[back_count++] = cur;
        
        /* Check if edge crosses the plane */
        if ((d_cur > BSP_EPSILON && d_next < -BSP_EPSILON) ||
            (d_cur < -BSP_EPSILON && d_next > BSP_EPSILON))
        {
            float t = d_cur / (d_cur - d_next);
            Vector3 intersection = Vector3Add(cur, 
                Vector3Scale(Vector3Subtract(next, cur), t));
            
            if (front_count < MAX_SPLIT_VERTS)
                front_verts[front_count++] = intersection;
            if (back_count < MAX_SPLIT_VERTS)
                back_verts[back_count++] = intersection;
        }
    }
    
    /* Create front piece if valid */
    if (front_count >= 3) {
        *out_front = BSPFace_Create(front_count);
        if (*out_front) {
            memcpy((*out_front)->vertices, front_verts, 
                   front_count * sizeof(Vector3));
            (*out_front)->normal = face->normal;
            (*out_front)->plane_dist = face->plane_dist;
            (*out_front)->original_face_idx = face->original_face_idx;
        }
    }
    
    /* Create back piece if valid */
    if (back_count >= 3) {
        *out_back = BSPFace_Create(back_count);
        if (*out_back) {
            memcpy((*out_back)->vertices, back_verts, 
                   back_count * sizeof(Vector3));
            (*out_back)->normal = face->normal;
            (*out_back)->plane_dist = face->plane_dist;
            (*out_back)->original_face_idx = face->original_face_idx;
        }
    }
}

/* ========================================================================
   SPLIT PLANE SELECTION - Improved heuristic
   ======================================================================== */

static int
SelectSplitPlane(const FaceList *face_list, Vector3 *out_normal, float *out_dist)
{
    if (!face_list || face_list->face_count == 0)
        return -1;
    
    /* QBSP-style heuristic:
     * 1. Minimize splits (faces cut by the plane)
     * 2. Balance front/back distribution
     * 3. Favor axis-aligned planes slightly
     */
    
    int best_idx = -1;
    int best_score = INT_MAX;
    int best_front = 0;
    int best_back = 0;
    
    /* Sample more faces for better quality splits */
    int sample_count = face_list->face_count;
    if (sample_count > SPLIT_SAMPLE_SIZE)
        sample_count = SPLIT_SAMPLE_SIZE;
    
    for (int i = 0; i < sample_count; i++) {
        const BSPFace *candidate = face_list->faces[i];
        Vector3 normal = candidate->normal;
        float dist = candidate->plane_dist;
        
        int front_count = 0;
        int back_count = 0;
        int split_count = 0;
        int coplanar_count = 0;
        
        /* Test ALL faces (including this one) against this plane.
         * The candidate itself will be classified as coplanar. */
        for (int j = 0; j < face_list->face_count; j++) {
            const BSPFace *test = face_list->faces[j];
            PlaneSide side = BSP_ClassifyPolygon(test->vertices, 
                                                 test->vertex_count,
                                                 normal, dist);
            
            switch (side) {
                case SIDE_FRONT: front_count++; break;
                case SIDE_BACK:  back_count++;  break;
                case SIDE_SPLIT: split_count++; break;
                case SIDE_ON:    
                    /* Coplanar faces will be distributed by normal direction.
                     * Count them towards the side they'll actually go to. */
                    if (Vector3DotProduct(normal, test->normal) > 0)
                        front_count++;
                    else
                        back_count++;
                    coplanar_count++;
                    break;
            }
        }
        
        /* CRITICAL: Skip planes that don't actually partition the set.
         * After distributing coplanar faces, we need both sides non-empty. */
        if (front_count == 0 || back_count == 0) {
            continue;
        }
        
        /* Score the split:
         * - Heavily penalize splits (each split creates 2 faces)
         * - Moderately penalize imbalance
         * - Slightly favor axis-aligned planes
         */
        int balance = abs(front_count - back_count);
        int score = split_count * 8 + balance;
        
        /* Bonus for axis-aligned planes (helps with boxy architecture) */
        if (fabsf(normal.x) > 0.99f || fabsf(normal.y) > 0.99f || fabsf(normal.z) > 0.99f)
            score -= 1;
        
        if (score < best_score) {
            best_score = score;
            best_idx = i;
            best_front = front_count;
            best_back = back_count;
        }
    }
    
    /* If we didn't find ANY plane that partitions the set, return -1 */
    if (best_idx < 0) {
        return -1;
    }
    
    /* Double-check that our best plane actually partitions */
    if (best_front == 0 || best_back == 0) {
        return -1;
    }
    
    *out_normal = face_list->faces[best_idx]->normal;
    *out_dist = face_list->faces[best_idx]->plane_dist;
    return best_idx;
}

/* ========================================================================
   TREE BUILDING - Recursive
   ======================================================================== */

static int
AllocNode(BSPTree *tree)
{
    if (tree->node_count >= tree->node_capacity) {
        int new_capacity = tree->node_capacity * 2;
        BSPNode *new_nodes = realloc(tree->nodes, 
                                     new_capacity * sizeof(BSPNode));
        if (!new_nodes)
            return -1;
        tree->nodes = new_nodes;
        tree->node_capacity = new_capacity;
    }
    
    int idx = tree->node_count++;
    memset(&tree->nodes[idx], 0, sizeof(BSPNode));
    return idx;
}

static int
AllocLeaf(BSPTree *tree)
{
    if (tree->leaf_count >= tree->leaf_capacity) {
        int new_capacity = tree->leaf_capacity * 2;
        BSPLeaf *new_leaves = realloc(tree->leaves, 
                                      new_capacity * sizeof(BSPLeaf));
        if (!new_leaves)
            return -1;
        tree->leaves = new_leaves;
        tree->leaf_capacity = new_capacity;
    }
    
    int idx = tree->leaf_count++;
    memset(&tree->leaves[idx], 0, sizeof(BSPLeaf));
    tree->leaves[idx].leaf_index = idx;
    tree->leaves[idx].bounds_min = (Vector3){FLT_MAX, FLT_MAX, FLT_MAX};
    tree->leaves[idx].bounds_max = (Vector3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    return idx;
}

static void
AddFaceToLeaf(BSPTree *tree, int leaf_idx, BSPFace *face)
{
    BSPLeaf *leaf = &tree->leaves[leaf_idx];
    
    /* Link face into leaf's list */
    face->next = leaf->faces;
    leaf->faces = face;
    leaf->face_count++;
    
    /* Update leaf bounds */
    for (int i = 0; i < face->vertex_count; i++) {
        Vector3 v = face->vertices[i];
        if (v.x < leaf->bounds_min.x) leaf->bounds_min.x = v.x;
        if (v.y < leaf->bounds_min.y) leaf->bounds_min.y = v.y;
        if (v.z < leaf->bounds_min.z) leaf->bounds_min.z = v.z;
        if (v.x > leaf->bounds_max.x) leaf->bounds_max.x = v.x;
        if (v.y > leaf->bounds_max.y) leaf->bounds_max.y = v.y;
        if (v.z > leaf->bounds_max.z) leaf->bounds_max.z = v.z;
    }
}

static int
BuildTree_Recursive(BSPTree *tree, FaceList *face_list, int depth, bool *is_leaf)
{
    /* Track max depth */
    if (depth > tree->max_tree_depth)
        tree->max_tree_depth = depth;
    
    /* EMERGENCY SAFETY VALVE: If we somehow hit insane depth, bail out.
     * This prevents stack overflow while we debug. A properly working BSP
     * should NEVER hit this - if you see this message, there's a bug. */
    if (depth > 1000) {
        DBG_OUT("[BSP EMERGENCY] Depth %d: %d faces, creating emergency leaf", 
                depth, face_list->face_count);
        
        /* Diagnose why we got here */
        if (face_list->face_count > 0) {
            DBG_OUT("  First face: normal=(%.3f,%.3f,%.3f) dist=%.3f verts=%d",
                    face_list->faces[0]->normal.x,
                    face_list->faces[0]->normal.y,
                    face_list->faces[0]->normal.z,
                    face_list->faces[0]->plane_dist,
                    face_list->faces[0]->vertex_count);
            
            if (face_list->face_count > 1) {
                DBG_OUT("  Second face: normal=(%.3f,%.3f,%.3f) dist=%.3f verts=%d",
                        face_list->faces[1]->normal.x,
                        face_list->faces[1]->normal.y,
                        face_list->faces[1]->normal.z,
                        face_list->faces[1]->plane_dist,
                        face_list->faces[1]->vertex_count);
            }
        }
        
        int leaf_idx = AllocLeaf(tree);
        if (leaf_idx >= 0) {
            for (int i = 0; i < face_list->face_count; i++) {
                AddFaceToLeaf(tree, leaf_idx, face_list->faces[i]);
            }
        }
        *is_leaf = true;
        return leaf_idx;
    }
    
    /* TERMINATION CONDITIONS:
     * 1. No faces left - create empty leaf
     * 2. Only 1 face - create leaf with that face
     */
    
    if (face_list->face_count == 0) {
        /* Empty leaf */
        if (depth < 5) {
            DBG_OUT("[BSP]   Depth %d: Empty leaf", depth);
        }
        int leaf_idx = AllocLeaf(tree);
        *is_leaf = true;
        return leaf_idx;
    }
    
    if (face_list->face_count == 1) {
        /* Single face - create leaf */
        if (depth < 5) {
            DBG_OUT("[BSP]   Depth %d: Single face leaf", depth);
        }
        int leaf_idx = AllocLeaf(tree);
        if (leaf_idx >= 0) {
            AddFaceToLeaf(tree, leaf_idx, face_list->faces[0]);
        }
        *is_leaf = true;
        return leaf_idx;
    }
    
    /* SELECT SPLIT PLANE
     * If SelectSplitPlane returns -1, it means no plane can partition the set.
     * This happens when all faces are coplanar or otherwise inseparable.
     */
    Vector3 split_normal;
    float split_dist;
    int split_idx = SelectSplitPlane(face_list, &split_normal, &split_dist);
    
    if (split_idx < 0) {
        /* No valid split plane found - create leaf with all faces */
        if (depth < 5) {
            DBG_OUT("[BSP]   Depth %d: No valid split (%d faces)", depth, face_list->face_count);
        }
        int leaf_idx = AllocLeaf(tree);
        if (leaf_idx >= 0) {
            for (int i = 0; i < face_list->face_count; i++) {
                AddFaceToLeaf(tree, leaf_idx, face_list->faces[i]);
            }
        }
        *is_leaf = true;
        return leaf_idx;
    }
    
    /* CREATE NODE */
    int node_idx = AllocNode(tree);
    if (node_idx < 0) {
        *is_leaf = false;
        return -1;
    }
    
    tree->nodes[node_idx].plane_normal = split_normal;
    tree->nodes[node_idx].plane_dist = split_dist;
    
    /* PARTITION FACES */
    FaceList *front_list = FaceList_Create();
    FaceList *back_list = FaceList_Create();
    
    if (!front_list || !back_list) {
        FaceList_Free(front_list);
        FaceList_Free(back_list);
        *is_leaf = false;
        return node_idx;
    }
    
    int splits_performed = 0;
    int faces_classified = 0;
    
    for (int i = 0; i < face_list->face_count; i++) {
        BSPFace *face = face_list->faces[i];
        
        PlaneSide side = BSP_ClassifyPolygon(face->vertices, 
                                             face->vertex_count,
                                             split_normal, split_dist);
        
        switch (side) {
        case SIDE_FRONT:
            FaceList_Add(front_list, face);
            faces_classified++;
            break;
        case SIDE_BACK:
            FaceList_Add(back_list, face);
            faces_classified++;
            break;
        case SIDE_ON:
            /* Coplanar face - put on the side it faces */
            if (Vector3DotProduct(split_normal, face->normal) > 0)
                FaceList_Add(front_list, face);
            else
                FaceList_Add(back_list, face);
            faces_classified++;
            break;
        case SIDE_SPLIT: {
            /* Split the face */
            BSPFace *front_piece, *back_piece;
            SplitFace(face, split_normal, split_dist, 
                     &front_piece, &back_piece);
            
            if (front_piece)
                FaceList_Add(front_list, front_piece);
            if (back_piece)
                FaceList_Add(back_list, back_piece);
            
            splits_performed++;
            faces_classified++;
            
            /* Free original face since we split it */
            BSPFace_Free(face);
            break;
        }
        }
    }
    
    /* SAFETY CHECK: If we didn't classify any faces, we have a problem.
     * This shouldn't happen but prevents infinite recursion. */
    if (faces_classified == 0) {
        DBG_OUT("[BSP ERROR] Failed to classify any faces at depth %d", depth);
        int leaf_idx = AllocLeaf(tree);
        if (leaf_idx >= 0) {
            for (int i = 0; i < face_list->face_count; i++) {
                AddFaceToLeaf(tree, leaf_idx, face_list->faces[i]);
            }
        }
        FaceList_Free(front_list);
        FaceList_Free(back_list);
        *is_leaf = true;
        return leaf_idx;
    }
    
    /* SAFETY CHECK: If both sides are empty somehow, create empty leaf */
    if (front_list->face_count == 0 && back_list->face_count == 0) {
        DBG_OUT("[BSP WARNING] Both sides empty at depth %d", depth);
        FaceList_Free(front_list);
        FaceList_Free(back_list);
        int leaf_idx = AllocLeaf(tree);
        *is_leaf = true;
        return leaf_idx;
    }
    
    /* Log progress at early depths or when splits happen */
    if (splits_performed > 0 || depth < 3) {
        DBG_OUT("[BSP]   Depth %d: %d faces -> %d front, %d back (%d splits)",
                depth, face_list->face_count, 
                front_list->face_count, back_list->face_count,
                splits_performed);
    }
    
    /* RECURSE TO BUILD CHILDREN */
    bool front_is_leaf = false;
    bool back_is_leaf = false;
    
    int front_child = BuildTree_Recursive(tree, front_list, depth + 1, 
                                          &front_is_leaf);
    int back_child = BuildTree_Recursive(tree, back_list, depth + 1, 
                                         &back_is_leaf);
    
    tree->nodes[node_idx].front_child = front_child;
    tree->nodes[node_idx].back_child = back_child;
    tree->nodes[node_idx].front_is_leaf = front_is_leaf;
    tree->nodes[node_idx].back_is_leaf = back_is_leaf;
    
    FaceList_Free(front_list);
    FaceList_Free(back_list);
    
    *is_leaf = false;
    return node_idx;
}

/* ========================================================================
   MAIN BUILD FUNCTION - STAGE 1 ONLY
   ======================================================================== */

BSPTree *
BSP_Build(
    const CompiledFace  *compiled_faces,
    int                  face_count,
    const Vector3       *vertices,
    int                  vertex_count,
    const CompiledBrush *brushes,
    int                  brush_count,
    const MapData       *map_data)
{
    DBG_OUT("[BSP] Stage 1: Building tree from %d faces...", face_count);
    
    /* Allocate tree */
    BSPTree *tree = malloc(sizeof(BSPTree));
    if (!tree)
        return NULL;
    
    memset(tree, 0, sizeof(BSPTree));
    tree->node_capacity = INITIAL_NODE_CAPACITY;
    tree->leaf_capacity = INITIAL_LEAF_CAPACITY;
    tree->nodes = malloc(tree->node_capacity * sizeof(BSPNode));
    tree->leaves = malloc(tree->leaf_capacity * sizeof(BSPLeaf));
    
    if (!tree->nodes || !tree->leaves) {
        free(tree->nodes);
        free(tree->leaves);
        free(tree);
        return NULL;
    }
    
    tree->total_faces = face_count;
    
    /* Convert CompiledFaces to BSPFaces */
    FaceList *initial_faces = FaceList_Create();
    if (!initial_faces) {
        free(tree->nodes);
        free(tree->leaves);
        free(tree);
        return NULL;
    }
    
    for (int i = 0; i < face_count; i++) {
        const CompiledFace *cf = &compiled_faces[i];
        
        /* Skip invisible or degenerate faces */
        if (!cf->is_visible || cf->vertex_count < 3)
            continue;
        
        BSPFace *bsp_face = BSPFace_Create(cf->vertex_count);
        if (!bsp_face)
            continue;
        
        /* Copy vertices */
        for (int v = 0; v < cf->vertex_count; v++)
            bsp_face->vertices[v] = vertices[cf->vertex_start + v];
        
        bsp_face->normal = cf->normal;
        bsp_face->plane_dist = cf->plane_dist;
        bsp_face->original_face_idx = i;
        
        FaceList_Add(initial_faces, bsp_face);
    }
    
    DBG_OUT("[BSP] Building tree with %d valid faces...", 
            initial_faces->face_count);
    
    /* Build the tree recursively */
    if (initial_faces->face_count > 0) {
        BuildTree_Recursive(tree, initial_faces, 0, &tree->root_is_leaf);
    } else {
        /* No faces - create single empty leaf */
        tree->root_is_leaf = true;
        AllocLeaf(tree);
    }
    
    FaceList_Free(initial_faces);
    
    /* For Stage 1, mark all faces as visible */
    tree->visible_faces = tree->total_faces;
    
    DBG_OUT("[BSP] Stage 1 complete:");
    DBG_OUT("  Nodes: %d", tree->node_count);
    DBG_OUT("  Leaves: %d", tree->leaf_count);
    DBG_OUT("  Max depth: %d", tree->max_tree_depth);
    DBG_OUT("  Total faces: %d", tree->total_faces);
    
    return tree;
}

/* ========================================================================
   UTILITY FUNCTIONS
   ======================================================================== */

void
BSP_Free(BSPTree *tree)
{
    if (!tree)
        return;
    
    /* Free all faces in all leaves */
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPFace *face = tree->leaves[i].faces;
        while (face) {
            BSPFace *next = face->next;
            BSPFace_Free(face);
            face = next;
        }
    }
    
    free(tree->nodes);
    free(tree->leaves);
    free(tree);
}

const BSPLeaf *
BSP_FindLeaf(const BSPTree *tree, Vector3 point)
{
    if (!tree)
        return NULL;
    
    if (tree->root_is_leaf)
        return &tree->leaves[0];
    
    int node_idx = 0;
    bool is_leaf = false;
    
    while (!is_leaf) {
        const BSPNode *node = &tree->nodes[node_idx];
        PlaneSide side = BSP_ClassifyPoint(point, node->plane_normal, 
                                           node->plane_dist);
        
        if (side == SIDE_FRONT || side == SIDE_ON) {
            node_idx = node->front_child;
            is_leaf = node->front_is_leaf;
        } else {
            node_idx = node->back_child;
            is_leaf = node->back_is_leaf;
        }
    }
    
    return &tree->leaves[node_idx];
}

void
BSP_PrintStats(const BSPTree *tree)
{
    if (!tree)
        return;
    
    DBG_OUT("=== BSP Tree Statistics (Stage 1) ===");
    DBG_OUT("  Nodes: %d", tree->node_count);
    DBG_OUT("  Leaves: %d", tree->leaf_count);
    DBG_OUT("  Total faces: %d", tree->total_faces);
    DBG_OUT("  Visible faces: %d", tree->visible_faces);
    DBG_OUT("  Max depth: %d", tree->max_tree_depth);
    
    /* Count faces per leaf */
    int total_leaf_faces = 0;
    int min_faces = INT_MAX;
    int max_faces = 0;
    
    for (int i = 0; i < tree->leaf_count; i++) {
        int count = tree->leaves[i].face_count;
        total_leaf_faces += count;
        if (count < min_faces) min_faces = count;
        if (count > max_faces) max_faces = count;
    }
    
    DBG_OUT("  Faces in leaves: %d total (%d min, %d max, %.1f avg)",
            total_leaf_faces, min_faces, max_faces,
            (float)total_leaf_faces / (tree->leaf_count ? tree->leaf_count : 1));
}

bool
BSP_Validate(const BSPTree *tree)
{
    if (!tree)
        return false;
    
    /* Validate node references */
    for (int i = 0; i < tree->node_count; i++) {
        const BSPNode *node = &tree->nodes[i];
        
        if (node->front_is_leaf) {
            if (node->front_child < 0 || node->front_child >= tree->leaf_count) {
                DBG_OUT("[BSP ERROR] Node %d: invalid front leaf %d", 
                        i, node->front_child);
                return false;
            }
        } else {
            if (node->front_child < 0 || node->front_child >= tree->node_count) {
                DBG_OUT("[BSP ERROR] Node %d: invalid front node %d", 
                        i, node->front_child);
                return false;
            }
        }
        
        if (node->back_is_leaf) {
            if (node->back_child < 0 || node->back_child >= tree->leaf_count) {
                DBG_OUT("[BSP ERROR] Node %d: invalid back leaf %d", 
                        i, node->back_child);
                return false;
            }
        } else {
            if (node->back_child < 0 || node->back_child >= tree->node_count) {
                DBG_OUT("[BSP ERROR] Node %d: invalid back node %d", 
                        i, node->back_child);
                return false;
            }
        }
    }
    
    DBG_OUT("[BSP] Tree validation passed");
    return true;
}
