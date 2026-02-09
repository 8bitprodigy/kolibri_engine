/*
 * mapscene_bsp.c - BSP compiler following QBSP algorithm
 * 
 * Based on Quake QBSP source code:
 * - Brushes are converted to "sides" (one per face)
 * - Sides are represented as "windings" (ordered vertex lists)
 * - Tree partitions SIDES, not individual vertices
 * - Face splitting only happens during partitioning
 * - Much simpler than trying to work with raw geometry
 */

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>

#include "mapscene_bsp.h"
#include "mapscene_types.h"
#include "v220_map_parser.h"

#define EPSILON 0.01f
#define MAX_POINTS_ON_WINDING 64

/* ========================================================================
   WINDING - Ordered list of vertices representing a polygon
   ======================================================================== */

typedef struct {
    int numpoints;
    Vector3 points[MAX_POINTS_ON_WINDING];
} winding_t;

static winding_t*
AllocWinding(int points)
{
    if (points > MAX_POINTS_ON_WINDING) {
        DBG_OUT("[BSP] ERROR: Winding with %d points (max %d)", points, MAX_POINTS_ON_WINDING);
        points = MAX_POINTS_ON_WINDING;
    }
    
    winding_t *w = malloc(sizeof(winding_t));
    w->numpoints = points;
    return w;
}

static void
FreeWinding(winding_t *w)
{
    free(w);
}

/* Create a huge winding on a plane */
static winding_t*
BaseWindingForPlane(Vector3 normal, float dist)
{
    /* Find major axis */
    float max = -1;
    int x = -1;
    
    for (int i = 0; i < 3; i++) {
        float v = fabsf(((float*)&normal)[i]);
        if (v > max) {
            max = v;
            x = i;
        }
    }
    
    Vector3 vup = {0, 0, 0};
    if (x == 0 || x == 1) {
        ((float*)&vup)[2] = 1;
    } else {
        ((float*)&vup)[0] = 1;
    }
    
    /* Project vup onto plane */
    float d = Vector3DotProduct(vup, normal);
    Vector3 vup_proj = {
        vup.x - normal.x * d,
        vup.y - normal.y * d,
        vup.z - normal.z * d
    };
    vup = Vector3Normalize(vup_proj);
    
    Vector3 org = Vector3Scale(normal, dist);
    Vector3 vright = Vector3CrossProduct(vup, normal);
    vup = Vector3Scale(vup, 32768);
    vright = Vector3Scale(vright, 32768);
    
    /* Make a really big quad */
    winding_t *w = AllocWinding(4);
    w->points[0] = Vector3Subtract(Vector3Subtract(org, vright), vup);
    w->points[1] = Vector3Add(Vector3Subtract(org, vright), vup);
    w->points[2] = Vector3Add(Vector3Add(org, vright), vup);
    w->points[3] = Vector3Subtract(Vector3Add(org, vright), vup);
    
    return w;
}

/* Clip winding to plane, keeping the part in front */
static winding_t*
ClipWindingToPlane(winding_t *in, Vector3 normal, float dist)
{
    float dists[MAX_POINTS_ON_WINDING + 1];
    int sides[MAX_POINTS_ON_WINDING + 1];
    int counts[3] = {0, 0, 0};
    
    /* Determine sides for each point */
    for (int i = 0; i < in->numpoints; i++) {
        float dot = Vector3DotProduct(in->points[i], normal) - dist;
        dists[i] = dot;
        
        if (dot > EPSILON) {
            sides[i] = SIDE_FRONT;
            counts[SIDE_FRONT]++;
        } else if (dot < -EPSILON) {
            sides[i] = SIDE_BACK;
            counts[SIDE_BACK]++;
        } else {
            sides[i] = SIDE_ON;
            counts[SIDE_ON]++;
        }
    }
    
    sides[in->numpoints] = sides[0];
    dists[in->numpoints] = dists[0];
    
    /* All points on back side */
    if (!counts[SIDE_FRONT]) {
        FreeWinding(in);
        return NULL;
    }
    
    /* All points on front side */
    if (!counts[SIDE_BACK]) {
        return in;
    }
    
    /* Winding is split */
    winding_t *neww = AllocWinding(in->numpoints + 4);
    neww->numpoints = 0;
    
    for (int i = 0; i < in->numpoints; i++) {
        Vector3 p1 = in->points[i];
        
        if (sides[i] == SIDE_ON) {
            neww->points[neww->numpoints++] = p1;
            continue;
        }
        
        if (sides[i] == SIDE_FRONT) {
            neww->points[neww->numpoints++] = p1;
        }
        
        if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i]) {
            continue;
        }
        
        /* Generate split point */
        Vector3 p2 = in->points[(i+1) % in->numpoints];
        float dot = dists[i] / (dists[i] - dists[i+1]);
        
        Vector3 mid = {
            p1.x + dot * (p2.x - p1.x),
            p1.y + dot * (p2.y - p1.y),
            p1.z + dot * (p2.z - p1.z)
        };
        
        neww->points[neww->numpoints++] = mid;
    }
    
    FreeWinding(in);
    return neww;
}

/* ========================================================================
   BRUSH SIDE - One face of a brush
   ======================================================================== */

typedef struct side_s {
    int planenum;          // Index into planes array
    int brush_idx;         // Which brush this side came from
    winding_t *winding;    // NULL until we build the actual geometry
    struct side_s *next;
} side_t;

/* ========================================================================
   SURFACES - List of sides at a node/leaf
   ======================================================================== */

typedef struct surface_s {
    side_t *onnode;        // Sides that are on the partition plane
    struct surface_s *next;
} surface_t;

/* ========================================================================
   NODE/LEAF STRUCTURES
   ======================================================================== */

typedef struct node_s {
    int planenum;          // -1 for leaf
    
    struct node_s *children[2];
    
    side_t *sides;         // For leaves: sides in this leaf
    
    Vector3 mins, maxs;
} node_t;

/* ========================================================================
   GLOBAL DATA
   ======================================================================== */

static Vector3 *g_plane_normals = NULL;
static float *g_plane_dists = NULL;
static int g_num_planes = 0;
static int g_plane_capacity = 0;

static const MapData *g_map_data = NULL;

/* ========================================================================
   PLANE MANAGEMENT
   ======================================================================== */

static int
FindOrAddPlane(Vector3 normal, float dist)
{
    /* Check if plane already exists */
    for (int i = 0; i < g_num_planes; i++) {
        if (fabsf(g_plane_normals[i].x - normal.x) < 0.001f &&
            fabsf(g_plane_normals[i].y - normal.y) < 0.001f &&
            fabsf(g_plane_normals[i].z - normal.z) < 0.001f &&
            fabsf(g_plane_dists[i] - dist) < 0.01f) {
            return i;
        }
    }
    
    /* Add new plane */
    if (g_num_planes >= g_plane_capacity) {
        g_plane_capacity = g_plane_capacity ? g_plane_capacity * 2 : 512;
        g_plane_normals = realloc(g_plane_normals, g_plane_capacity * sizeof(Vector3));
        g_plane_dists = realloc(g_plane_dists, g_plane_capacity * sizeof(float));
    }
    
    g_plane_normals[g_num_planes] = normal;
    g_plane_dists[g_num_planes] = dist;
    
    return g_num_planes++;
}

/* ========================================================================
   SIDE BUILDING
   ======================================================================== */

static side_t*
MakeSidesFromBrush(const MapBrush *brush, int brush_idx)
{
    side_t *side_list = NULL;
    
    for (int i = 0; i < brush->plane_count; i++) {
        const MapPlane *mp = &brush->planes[i];
        
        /* Valve 220 planes point OUTWARD from solid.
         * Keep them that way - we'll negate during clipping instead. */
        Vector3 normal = mp->normal;
        float dist = mp->distance;
        
        /* Add plane to global list */
        int planenum = FindOrAddPlane(normal, dist);
        
        /* Create side */
        side_t *side = calloc(1, sizeof(side_t));
        side->planenum = planenum;
        side->brush_idx = brush_idx;  // Track which brush this came from
        side->next = side_list;
        side_list = side;
    }
    
    return side_list;
}

/* Build winding for a single brush's sides */
static void
MakeWindingsForBrush(side_t *brush_sides)
{
    /* Count sides */
    int side_count = 0;
    for (side_t *s = brush_sides; s; s = s->next) side_count++;
    
    static int brush_debug_count = 0;
    bool debug = (brush_debug_count < 3);
    
    if (debug) {
        DBG_OUT("[Stage 1a DEBUG] Building windings for brush with %d sides", side_count);
    }
    
    for (side_t *s = brush_sides; s; s = s->next) {
        /* Start with base winding */
        s->winding = BaseWindingForPlane(
            g_plane_normals[s->planenum],
            g_plane_dists[s->planenum]
        );
        
        if (debug) {
            DBG_OUT("[Stage 1a DEBUG]   Side plane: normal=(%.2f,%.2f,%.2f) dist=%.2f, base winding has %d points",
                    g_plane_normals[s->planenum].x,
                    g_plane_normals[s->planenum].y,
                    g_plane_normals[s->planenum].z,
                    g_plane_dists[s->planenum],
                    s->winding ? s->winding->numpoints : 0);
        }
        
        /* Clip to all OTHER planes of THIS BRUSH 
         * Planes now point INWARD (toward solid)
         * We want to keep the solid part, so clip to BACK side */
        int clip_count = 0;
        for (side_t *clip = brush_sides; clip && s->winding; clip = clip->next) {
            if (clip == s) continue;
            
            /* DEBUG: Check classification before clipping */
            if (debug) {
                int front = 0, back = 0, on = 0;
                for (int i = 0; i < s->winding->numpoints; i++) {
                    float d = Vector3DotProduct(s->winding->points[i], g_plane_normals[clip->planenum]) 
                            - g_plane_dists[clip->planenum];
                    if (d > EPSILON) front++;
                    else if (d < -EPSILON) back++;
                    else on++;
                }
                DBG_OUT("[Stage 1a DEBUG]     Clipping vs plane: normal=(%.2f,%.2f,%.2f) dist=%.2f -> %d front, %d back, %d on",
                        g_plane_normals[clip->planenum].x,
                        g_plane_normals[clip->planenum].y,
                        g_plane_normals[clip->planenum].z,
                        g_plane_dists[clip->planenum],
                        front, back, on);
            }
            
            /* Negate plane to clip to inside of brush */
            Vector3 neg_normal = {
                -g_plane_normals[clip->planenum].x,
                -g_plane_normals[clip->planenum].y,
                -g_plane_normals[clip->planenum].z
            };
            float neg_dist = -g_plane_dists[clip->planenum];
            
            s->winding = ClipWindingToPlane(
                s->winding,
                neg_normal,
                neg_dist
            );
            
            clip_count++;
            
            if (!s->winding && debug) {
                DBG_OUT("[Stage 1a DEBUG]       -> CLIPPED AWAY (all points in front = outside)");
                break;
            } else if (debug && s->winding) {
                DBG_OUT("[Stage 1a DEBUG]       -> Still have %d points", s->winding->numpoints);
            }
        }
        
        if (debug && s->winding) {
            DBG_OUT("[Stage 1a DEBUG]   ✓ Final winding has %d points", s->winding->numpoints);
        } else if (debug) {
            DBG_OUT("[Stage 1a DEBUG]   ✗ Winding was clipped away");
        }
    }
    
    brush_debug_count++;
}

/* ========================================================================
   TREE BUILDING
   ======================================================================== */

/* Choose best partition plane from sides */
static int
SelectPartition(side_t *sides)
{
    if (!sides) return -1;
    
    int best_plane = -1;
    int best_score = INT_MAX;
    
    /* Try planes from first few sides */
    int tested = 0;
    for (side_t *s = sides; s && tested < 8; s = s->next, tested++) {
        int planenum = s->planenum;
        
        /* Score this plane */
        int front = 0, back = 0, splits = 0, on = 0;
        
        for (side_t *test = sides; test; test = test->next) {
            if (!test->winding) continue;
            
            /* Classify winding to plane */
            PlaneSide side = SIDE_ON;
            int front_count = 0, back_count = 0;
            
            for (int i = 0; i < test->winding->numpoints; i++) {
                float d = Vector3DotProduct(test->winding->points[i], 
                                           g_plane_normals[planenum]) 
                        - g_plane_dists[planenum];
                
                if (d > EPSILON) front_count++;
                else if (d < -EPSILON) back_count++;
            }
            
            if (front_count && back_count) side = SIDE_SPLIT;
            else if (front_count) side = SIDE_FRONT;
            else if (back_count) side = SIDE_BACK;
            else side = SIDE_ON;
            
            if (side == SIDE_FRONT) front++;
            else if (side == SIDE_BACK) back++;
            else if (side == SIDE_ON) on++;
            else if (side == SIDE_SPLIT) splits++;
        }
        
        /* Must actually partition */
        if (front == 0 || back == 0) continue;
        
        /* Score: minimize splits and imbalance */
        int score = splits * 8 + abs(front - back);
        
        if (score < best_score) {
            best_score = score;
            best_plane = planenum;
        }
    }
    
    return best_plane;
}

/* Split a winding by plane, producing front and back pieces */
static void
SplitWinding(winding_t *in, Vector3 normal, float dist,
             winding_t **front, winding_t **back)
{
    *front = *back = NULL;
    
    float dists[MAX_POINTS_ON_WINDING + 1];
    int sides[MAX_POINTS_ON_WINDING + 1];
    int counts[3] = {0, 0, 0};
    
    /* Classify points */
    for (int i = 0; i < in->numpoints; i++) {
        float d = Vector3DotProduct(in->points[i], normal) - dist;
        dists[i] = d;
        
        if (d > EPSILON) sides[i] = SIDE_FRONT, counts[0]++;
        else if (d < -EPSILON) sides[i] = SIDE_BACK, counts[1]++;
        else sides[i] = SIDE_ON, counts[2]++;
    }
    sides[in->numpoints] = sides[0];
    dists[in->numpoints] = dists[0];
    
    if (!counts[0]) { *back = in; return; }
    if (!counts[1]) { *front = in; return; }
    
    /* Split */
    winding_t *f = AllocWinding(in->numpoints + 4);
    winding_t *b = AllocWinding(in->numpoints + 4);
    f->numpoints = b->numpoints = 0;
    
    for (int i = 0; i < in->numpoints; i++) {
        Vector3 p1 = in->points[i];
        
        if (sides[i] == SIDE_ON) {
            f->points[f->numpoints++] = p1;
            b->points[b->numpoints++] = p1;
            continue;
        }
        
        if (sides[i] == SIDE_FRONT)
            f->points[f->numpoints++] = p1;
        if (sides[i] == SIDE_BACK)
            b->points[b->numpoints++] = p1;
        
        if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
            continue;
        
        /* Split point */
        Vector3 p2 = in->points[(i+1) % in->numpoints];
        float dot = dists[i] / (dists[i] - dists[i+1]);
        Vector3 mid = {
            p1.x + dot * (p2.x - p1.x),
            p1.y + dot * (p2.y - p1.y),
            p1.z + dot * (p2.z - p1.z)
        };
        
        f->points[f->numpoints++] = mid;
        b->points[b->numpoints++] = mid;
    }
    
    FreeWinding(in);
    *front = f;
    *back = b;
}

/* Partition sides by plane */
static void
PartitionSides(side_t *sides, int planenum, side_t **front, side_t **back)
{
    *front = *back = NULL;
    
    Vector3 normal = g_plane_normals[planenum];
    float dist = g_plane_dists[planenum];
    
    while (sides) {
        side_t *next = sides->next;
        sides->next = NULL;
        
        if (!sides->winding) {
            sides->next = *front;
            *front = sides;
            sides = next;
            continue;
        }
        
        /* Classify */
        int front_count = 0, back_count = 0;
        for (int i = 0; i < sides->winding->numpoints; i++) {
            float d = Vector3DotProduct(sides->winding->points[i], normal) - dist;
            if (d > EPSILON) front_count++;
            else if (d < -EPSILON) back_count++;
        }
        
        if (front_count && back_count) {
            /* Split */
            winding_t *fw, *bw;
            SplitWinding(sides->winding, normal, dist, &fw, &bw);
            
            if (fw) {
                side_t *fs = malloc(sizeof(side_t));
                *fs = *sides;
                fs->winding = fw;
                fs->next = *front;
                *front = fs;
            }
            
            if (bw) {
                sides->winding = bw;
                sides->next = *back;
                *back = sides;
            } else {
                free(sides);
            }
        } else if (front_count) {
            sides->next = *front;
            *front = sides;
        } else {
            sides->next = *back;
            *back = sides;
        }
        
        sides = next;
    }
}

/* Build tree recursively */
static node_t*
BuildTree_r(side_t *sides, int depth)
{
    if (!sides || depth > 100) {
        node_t *leaf = calloc(1, sizeof(node_t));
        leaf->planenum = -1;
        leaf->sides = sides;
        return leaf;
    }
    
    int planenum = SelectPartition(sides);
    if (planenum == -1) {
        node_t *leaf = calloc(1, sizeof(node_t));
        leaf->planenum = -1;
        leaf->sides = sides;
        return leaf;
    }
    
    side_t *fronts, *backs;
    PartitionSides(sides, planenum, &fronts, &backs);
    
    /* Create the partition plane as a face and add to both sides
     * This ensures walls show up as orange on SOLID side and green on EMPTY side */
    
    // Create winding for partition plane
    winding_t *partition_winding = BaseWindingForPlane(
        g_plane_normals[planenum],
        g_plane_dists[planenum]
    );
    
    // Clip it to all sides to get the actual boundary
    for (side_t *s = sides; s && partition_winding; s = s->next) {
        if (!s->winding || s->planenum == planenum) continue;
        
        partition_winding = ClipWindingToPlane(
            partition_winding,
            g_plane_normals[s->planenum],
            g_plane_dists[s->planenum]
        );
    }
    
    if (partition_winding && partition_winding->numpoints >= 3) {
        // Add to front side
        side_t *front_partition = malloc(sizeof(side_t));
        front_partition->planenum = planenum;
        front_partition->brush_idx = -1;  // Partition plane, not from original brush
        front_partition->winding = partition_winding;
        front_partition->next = fronts;
        fronts = front_partition;
        
        // Add inverted copy to back side (so it faces the other way)
        winding_t *back_winding = AllocWinding(partition_winding->numpoints);
        back_winding->numpoints = partition_winding->numpoints;
        // Reverse winding order
        for (int i = 0; i < partition_winding->numpoints; i++) {
            back_winding->points[i] = partition_winding->points[partition_winding->numpoints - 1 - i];
        }
        
        side_t *back_partition = malloc(sizeof(side_t));
        back_partition->planenum = planenum;
        back_partition->brush_idx = -1;
        back_partition->winding = back_winding;
        back_partition->next = backs;
        backs = back_partition;
    }
    
    node_t *node = calloc(1, sizeof(node_t));
    node->planenum = planenum;
    node->children[0] = BuildTree_r(fronts, depth + 1);
    node->children[1] = BuildTree_r(backs, depth + 1);
    
    return node;
}

/* Count nodes and leaves */
static void
CountNodes(node_t *node, int *nodes, int *leaves)
{
    if (!node) return;
    
    if (node->planenum == -1) {
        (*leaves)++;
    } else {
        (*nodes)++;
        CountNodes(node->children[0], nodes, leaves);
        CountNodes(node->children[1], nodes, leaves);
    }
}

/* Convert winding to BSPFace */
static BSPFace*
WindingToFace(winding_t *w, int planenum, int brush_idx)
{
    if (!w || w->numpoints < 3) return NULL;
    
    BSPFace *face = malloc(sizeof(BSPFace));
    face->vertices = malloc(w->numpoints * sizeof(Vector3));
    face->vertex_count = w->numpoints;
    
    for (int i = 0; i < w->numpoints; i++) {
        face->vertices[i] = w->points[i];
    }
    
    face->normal = g_plane_normals[planenum];
    face->plane_dist = g_plane_dists[planenum];
    face->original_face_idx = brush_idx;
    face->next = NULL;
    
    return face;
}

/* Flatten tree to arrays */
static int
FlattenNode(node_t *node, BSPTree *tree, int *node_idx, int *leaf_idx)
{
    if (!node) return -1;
    
    if (node->planenum == -1) {
        /* Leaf */
        if (*leaf_idx >= tree->leaf_capacity) {
            DBG_OUT("[BSP] ERROR: Too many leaves!");
            return -1;
        }
        
        int idx = (*leaf_idx)++;
        BSPLeaf *leaf = &tree->leaves[idx];
        
        leaf->leaf_index = idx;
        leaf->contents = CONTENTS_EMPTY;  // Will classify later
        leaf->is_reachable = false;
        leaf->faces = NULL;
        leaf->face_count = 0;
        
        /* Convert sides to faces */
        for (side_t *s = node->sides; s; s = s->next) {
            if (!s->winding) continue;
            
            BSPFace *face = WindingToFace(s->winding, s->planenum, s->brush_idx);
            if (face) {
                face->next = leaf->faces;
                leaf->faces = face;
                leaf->face_count++;
                tree->total_faces++;
            }
        }
        
        /* Calculate bounds */
        leaf->bounds_min = (Vector3){FLT_MAX, FLT_MAX, FLT_MAX};
        leaf->bounds_max = (Vector3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
        
        for (BSPFace *f = leaf->faces; f; f = f->next) {
            for (int i = 0; i < f->vertex_count; i++) {
                Vector3 v = f->vertices[i];
                leaf->bounds_min.x = fminf(leaf->bounds_min.x, v.x);
                leaf->bounds_min.y = fminf(leaf->bounds_min.y, v.y);
                leaf->bounds_min.z = fminf(leaf->bounds_min.z, v.z);
                leaf->bounds_max.x = fmaxf(leaf->bounds_max.x, v.x);
                leaf->bounds_max.y = fmaxf(leaf->bounds_max.y, v.y);
                leaf->bounds_max.z = fmaxf(leaf->bounds_max.z, v.z);
            }
        }
        
        return ~idx;  // Negative index for leaf
    } else {
        /* Node */
        if (*node_idx >= tree->node_capacity) {
            DBG_OUT("[BSP] ERROR: Too many nodes!");
            return -1;
        }
        
        int idx = (*node_idx)++;
        BSPNode *bsp_node = &tree->nodes[idx];
        
        bsp_node->plane_normal = g_plane_normals[node->planenum];
        bsp_node->plane_dist = g_plane_dists[node->planenum];
        
        bsp_node->front_child = FlattenNode(node->children[0], tree, node_idx, leaf_idx);
        bsp_node->back_child = FlattenNode(node->children[1], tree, node_idx, leaf_idx);
        
        bsp_node->front_is_leaf = (bsp_node->front_child < 0);
        bsp_node->back_is_leaf = (bsp_node->back_child < 0);
        
        if (bsp_node->front_is_leaf) bsp_node->front_child = ~bsp_node->front_child;
        if (bsp_node->back_is_leaf) bsp_node->back_child = ~bsp_node->back_child;
        
        return idx;
    }
}

/* Classify leaves as SOLID or EMPTY */
static void
ClassifyLeaves(BSPTree *tree, const MapData *map_data)
{
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->face_count == 0) {
            leaf->contents = CONTENTS_EMPTY;
            continue;
        }
        
        /* Test point = average of all face vertices */
        Vector3 test_point = {0, 0, 0};
        int vert_count = 0;
        
        for (BSPFace *f = leaf->faces; f; f = f->next) {
            for (int v = 0; v < f->vertex_count; v++) {
                test_point.x += f->vertices[v].x;
                test_point.y += f->vertices[v].y;
                test_point.z += f->vertices[v].z;
                vert_count++;
            }
        }
        
        if (vert_count > 0) {
            test_point.x /= vert_count;
            test_point.y /= vert_count;
            test_point.z /= vert_count;
        }
        
        /* Test against all brushes */
        bool is_solid = false;
        
        for (int b = 0; b < map_data->world_brush_count && !is_solid; b++) {
            const MapBrush *brush = &map_data->world_brushes[b];
            
            /* Point-in-brush test */
            bool inside = true;
            for (int p = 0; p < brush->plane_count; p++) {
                const MapPlane *plane = &brush->planes[p];
                
                /* Note: We need to use the ORIGINAL planes, not inverted */
                float dist = Vector3DotProduct(test_point, plane->normal) - plane->distance;
                
                if (dist > EPSILON) {
                    inside = false;
                    break;
                }
            }
            
            if (inside) {
                is_solid = true;
            }
        }
        
        leaf->contents = is_solid ? CONTENTS_SOLID : CONTENTS_EMPTY;
    }
}

/* ========================================================================
   PUBLIC API (STUBS FOR NOW)
   ======================================================================== */

BSPTree*
BSP_Build(const MapData *map_data)
{
    if (!map_data) return NULL;
    
    g_map_data = map_data;
    g_num_planes = 0;
    
    DBG_OUT("[Stage 1a] Building windings for %d brushes...", 
            map_data->world_brush_count);
    
    /* STAGE 1a: Convert all brushes to sides and build windings per brush */
    side_t *all_sides = NULL;
    
    for (int i = 0; i < map_data->world_brush_count; i++) {
        side_t *brush_sides = MakeSidesFromBrush(&map_data->world_brushes[i], i);
        
        /* Build windings for this brush */
        MakeWindingsForBrush(brush_sides);
        
        /* Link to master list */
        side_t *tail = brush_sides;
        while (tail && tail->next) tail = tail->next;
        if (tail) {
            tail->next = all_sides;
            all_sides = brush_sides;
        }
    }
    
    /* Count valid windings */
    int valid_count = 0;
    for (side_t *s = all_sides; s; s = s->next) {
        if (s->winding) valid_count++;
    }
    
    DBG_OUT("[Stage 1a] Generated %d sides, %d planes, %d valid windings",
            map_data->world_brush_count * 6, g_num_planes, valid_count);
    
    if (valid_count == 0) {
        DBG_OUT("[Stage 1a] ERROR: No valid windings! Cannot build tree.");
        return NULL;
    }
    
    /* STAGE 1b: Build BSP tree */
    DBG_OUT("[Stage 1b] Building BSP tree...");
    node_t *root = BuildTree_r(all_sides, 0);
    
    /* Count nodes and leaves */
    int node_count = 0, leaf_count = 0;
    CountNodes(root, &node_count, &leaf_count);
    
    DBG_OUT("[Stage 1b] Tree built: %d nodes, %d leaves", node_count, leaf_count);
    
    /* Allocate BSPTree */
    BSPTree *tree = calloc(1, sizeof(BSPTree));
    tree->node_capacity = node_count + 16;
    tree->leaf_capacity = leaf_count + 16;
    tree->nodes = calloc(tree->node_capacity, sizeof(BSPNode));
    tree->leaves = calloc(tree->leaf_capacity, sizeof(BSPLeaf));
    tree->root_is_leaf = (node_count == 0);
    
    /* Flatten tree to arrays */
    DBG_OUT("[Stage 1b] Flattening tree...");
    int node_idx = 0, leaf_idx = 0;
    FlattenNode(root, tree, &node_idx, &leaf_idx);
    
    tree->node_count = node_idx;
    tree->leaf_count = leaf_idx;
    
    DBG_OUT("[Stage 1b] Flattened: %d nodes, %d leaves, %d faces",
            tree->node_count, tree->leaf_count, tree->total_faces);
    
    /* Compute tree depth */
    int max_depth = 0;
    for (int i = 0; i < tree->leaf_count; i++) {
        // Approximate depth by tracing back
        // (We don't track this during building, but could add it)
    }
    
    /* Print statistics */
    DBG_OUT("[Stage 1b] Tree statistics:");
    DBG_OUT("[Stage 1b]   Avg faces/leaf: %.1f", 
            tree->total_faces / (float)tree->leaf_count);
    DBG_OUT("[Stage 1b]   Empty leaves: %d", 
            tree->leaf_count - tree->total_faces); // Approximate
    
    return tree;
    
    /* TODO: Stage 1c - Classify leaves as SOLID/EMPTY
     * TODO: Stage 1d - Validation & stats
     */
}

void BSP_Free(BSPTree *tree) { }
void BSP_PrintStats(const BSPTree *tree) { }
bool BSP_Validate(const BSPTree *tree) { return false; }
const BSPLeaf* BSP_FindLeaf(const BSPTree *tree, Vector3 point) { return NULL; }
LeafContents BSP_GetPointContents(const BSPTree *tree, Vector3 point) { return CONTENTS_EMPTY; }
void 
BSP_DebugDrawLeafBounds(const BSPTree *tree)
{
    if (!tree) return;
    
    /* Stage 1b: Draw leaf bounding boxes and faces within them */
    DrawText(TextFormat("Stage 1b: %d nodes, %d leaves, %d faces", 
             tree->node_count, tree->leaf_count, tree->total_faces), 
             10, 10, 20, WHITE);
    
    for (int i = 0; i < tree->leaf_count; i++) {
        const BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->face_count == 0) {
            /* Draw empty leaf bounds in gray wireframe */
            DrawBoundingBox((BoundingBox){leaf->bounds_min, leaf->bounds_max}, 
                           (Color){100, 100, 100, 255});
            continue;
        }
        
        /* Draw leaf bounding box in cyan wireframe */
        DrawBoundingBox((BoundingBox){leaf->bounds_min, leaf->bounds_max}, 
                       (Color){0, 255, 255, 128});
        
        /* Draw faces within this leaf */
        for (BSPFace *face = leaf->faces; face; face = face->next) {
            if (face->vertex_count < 3) continue;
            
            /* Color by brush index */
            Color brush_colors[] = {
                {255, 0, 0, 255},     // Red
                {0, 255, 0, 255},     // Green
                {0, 0, 255, 255},     // Blue
                {255, 255, 0, 255},   // Yellow
                {255, 0, 255, 255},   // Magenta
                {0, 255, 255, 255},   // Cyan
                {255, 128, 0, 255},   // Orange
                {128, 0, 255, 255},   // Purple
            };
            int brush_idx = face->original_face_idx;
            Color color = brush_colors[brush_idx % 8];
            
            /* Draw wireframe */
            for (int v = 0; v < face->vertex_count; v++) {
                int next = (v + 1) % face->vertex_count;
                DrawLine3D(face->vertices[v], face->vertices[next], color);
            }
        }
    }
}
