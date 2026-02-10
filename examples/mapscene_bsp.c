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
} Winding;

static Winding*
AllocWinding(int points)
{
    if (points > MAX_POINTS_ON_WINDING) {
        DBG_OUT("[BSP] ERROR: Winding with %d points (max %d)", points, MAX_POINTS_ON_WINDING);
        points = MAX_POINTS_ON_WINDING;
    }
    
    Winding *w = malloc(sizeof(Winding));
    w->numpoints = points;
    return w;
}

static void
FreeWinding(Winding *w)
{
    free(w);
}

static Winding*
CopyWinding(Winding *w)
{
    if (!w) return NULL;
    
    Winding *copy = AllocWinding(w->numpoints);
    copy->numpoints = w->numpoints;
    for (int i = 0; i < w->numpoints; i++) {
        copy->points[i] = w->points[i];
    }
    return copy;
}

/* Create a huge winding on a plane */
static Winding*
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
    Winding *w = AllocWinding(4);
    w->points[0] = Vector3Subtract(Vector3Subtract(org, vright), vup);
    w->points[1] = Vector3Add(Vector3Subtract(org, vright), vup);
    w->points[2] = Vector3Add(Vector3Add(org, vright), vup);
    w->points[3] = Vector3Subtract(Vector3Add(org, vright), vup);
    
    return w;
}

/* Clip winding to plane, keeping the part in front */
static Winding*
ClipWindingToPlane(Winding *in, Vector3 normal, float dist)
{
    /* Safety check */
    if (!in || in->numpoints < 3 || in->numpoints > MAX_POINTS_ON_WINDING) {
        DBG_OUT("[ERROR] ClipWindingToPlane: Invalid winding (numpoints=%d, max=%d)", 
                in ? in->numpoints : -1, MAX_POINTS_ON_WINDING);
        if (in) FreeWinding(in);
        return NULL;
    }
    
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
    Winding *neww = AllocWinding(in->numpoints + 4);
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
    Winding *winding;    // NULL until we build the actual geometry
    struct side_s *next;
} Side;

/* ========================================================================
   SURFACES - List of sides at a node/leaf
   ======================================================================== */

typedef struct surface_s {
    Side *onnode;        // Sides that are on the partition plane
    struct surface_s *next;
} Surface;

/* ========================================================================
   NODE/LEAF STRUCTURES
   ======================================================================== */

typedef struct node_s {
    int planenum;          // -1 for leaf
    
    struct node_s *children[2];
    
    Side *sides;         // For leaves: sides in this leaf
    
    Vector3 mins, maxs;
} Node;

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

static Side*
MakeSidesFromBrush(const MapBrush *brush, int brush_idx)
{
    Side *side_list = NULL;
    
    for (int i = 0; i < brush->plane_count; i++) {
        const MapPlane *mp = &brush->planes[i];
        
        /* Valve 220 planes point OUTWARD from solid.
         * Keep them that way - we'll negate during clipping instead. */
        Vector3 normal = mp->normal;
        float dist = mp->distance;
        
        /* Add plane to global list */
        int planenum = FindOrAddPlane(normal, dist);
        
        /* Create side */
        Side *side = calloc(1, sizeof(Side));
        side->planenum = planenum;
        side->brush_idx = brush_idx;  // Track which brush this came from
        side->next = side_list;
        side_list = side;
    }
    
    return side_list;
}

/* Build winding for a single brush's sides */
static void
MakeWindingsForBrush(Side *brush_sides)
{
    /* Count sides */
    int side_count = 0;
    for (Side *s = brush_sides; s; s = s->next) side_count++;
    
    static int brush_debug_count = 0;
    bool debug = (brush_debug_count < 3);
    
    if (debug) {
        DBG_OUT("[Stage 1a DEBUG] Building windings for brush with %d sides", side_count);
    }
    
    for (Side *s = brush_sides; s; s = s->next) {
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
        for (Side *clip = brush_sides; clip && s->winding; clip = clip->next) {
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
SelectPartition(Side *sides)
{
    if (!sides) return -1;
    
    int best_plane = -1;
    int best_score = INT_MAX;
    
    /* Try planes from first few sides */
    int tested = 0;
    for (Side *s = sides; s && tested < 8; s = s->next, tested++) {
        int planenum = s->planenum;
        
        /* Score this plane */
        int front = 0, back = 0, splits = 0, on = 0;
        
        for (Side *test = sides; test; test = test->next) {
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
SplitWinding(Winding *in, Vector3 normal, float dist,
             Winding **front, Winding **back)
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
    Winding *f = AllocWinding(in->numpoints + 4);
    Winding *b = AllocWinding(in->numpoints + 4);
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
PartitionSides(Side *sides, int planenum, Side **front, Side **back)
{
    *front = *back = NULL;
    
    Vector3 normal = g_plane_normals[planenum];
    float dist = g_plane_dists[planenum];
    
    while (sides) {
        Side *next = sides->next;
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
            Winding *fw, *bw;
            SplitWinding(sides->winding, normal, dist, &fw, &bw);
            
            if (fw) {
                Side *fs = malloc(sizeof(Side));
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
static Node*
BuildTree_r(Side *sides, int depth)
{
    if (!sides || depth > 100) {
        Node *leaf = calloc(1, sizeof(Node));
        leaf->planenum = -1;
        leaf->sides = sides;
        return leaf;
    }
    
    int planenum = SelectPartition(sides);
    if (planenum == -1) {
        Node *leaf = calloc(1, sizeof(Node));
        leaf->planenum = -1;
        leaf->sides = sides;
        return leaf;
    }
    
    Side *fronts, *backs;
    PartitionSides(sides, planenum, &fronts, &backs);
    
    /* Create the partition plane as a face and add to both sides
     * This ensures walls show up as orange on SOLID side and green on EMPTY side */
    
    // Create winding for partition plane
    Winding *partition_winding = BaseWindingForPlane(
        g_plane_normals[planenum],
        g_plane_dists[planenum]
    );
    
    // Clip it to all sides to get the actual boundary
    for (Side *s = sides; s && partition_winding; s = s->next) {
        if (!s->winding || s->planenum == planenum) continue;
        
        partition_winding = ClipWindingToPlane(
            partition_winding,
            g_plane_normals[s->planenum],
            g_plane_dists[s->planenum]
        );
    }
    
    if (partition_winding && partition_winding->numpoints >= 3) {
        // Add to front side
        Side *front_partition = malloc(sizeof(Side));
        front_partition->planenum = planenum;
        front_partition->brush_idx = -1;  // Partition plane, not from original brush
        front_partition->winding = partition_winding;
        front_partition->next = fronts;
        fronts = front_partition;
        
        // Add inverted copy to back side (so it faces the other way)
        Winding *back_winding = AllocWinding(partition_winding->numpoints);
        back_winding->numpoints = partition_winding->numpoints;
        // Reverse winding order
        for (int i = 0; i < partition_winding->numpoints; i++) {
            back_winding->points[i] = partition_winding->points[partition_winding->numpoints - 1 - i];
        }
        
        Side *back_partition = malloc(sizeof(Side));
        back_partition->planenum = planenum;
        back_partition->brush_idx = -1;
        back_partition->winding = back_winding;
        back_partition->next = backs;
        backs = back_partition;
    }
    
    Node *node = calloc(1, sizeof(Node));
    node->planenum = planenum;
    node->children[0] = BuildTree_r(fronts, depth + 1);
    node->children[1] = BuildTree_r(backs, depth + 1);
    
    return node;
}

/* Count nodes and leaves */
static void
CountNodes(Node *node, int *nodes, int *leaves)
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
WindingToFace(Winding *w, int planenum, int brush_idx)
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
FlattenNode(Node *node, BSPTree *tree, int *node_idx, int *leaf_idx)
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
        leaf->contents = CONTENTS_EMPTY;  // Will classify in Stage 4
        leaf->is_outside = false;         // Will mark in Stage 2
        leaf->flood_filled = false;       // Will mark in Stage 3
        leaf->flood_parent = -1;          // Will set in Stage 3
        leaf->faces = NULL;
        leaf->face_count = 0;
        
        /* Convert sides to faces */
        for (Side *s = node->sides; s; s = s->next) {
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

/* ========================================================================
   SIDE SPLITTING
   ======================================================================== */

/* Split sides into front and back lists based on a partition plane
 * This is the QBSP way: windings that cross the plane are CLIPPED into two pieces */
static void
SplitSides(Side *sides, int planenum, Side **front, Side **back)
{
    *front = NULL;
    *back = NULL;
    
    while (sides) {
        Side *next = sides->next;
        sides->next = NULL;
        
        if (!sides->winding) {
            /* No winding - put in back */
            sides->next = *back;
            *back = sides;
            sides = next;
            continue;
        }
        
        /* Classify winding against partition plane */
        int front_count = 0, back_count = 0;
        
        for (int i = 0; i < sides->winding->numpoints; i++) {
            float d = Vector3DotProduct(sides->winding->points[i], g_plane_normals[planenum])
                    - g_plane_dists[planenum];
            
            if (d > EPSILON) front_count++;
            else if (d < -EPSILON) back_count++;
        }
        
        /* Decide what to do with this side */
        if (front_count == 0 && back_count == 0) {
            /* On plane - put in back */
            sides->next = *back;
            *back = sides;
        }
        else if (back_count == 0) {
            /* Entirely in front */
            sides->next = *front;
            *front = sides;
        }
        else if (front_count == 0) {
            /* Entirely in back */
            sides->next = *back;
            *back = sides;
        }
        else {
            /* Crosses plane - SPLIT IT (the QBSP way) */
            
            /* Create front side */
            Side *front_side = calloc(1, sizeof(Side));
            front_side->planenum = sides->planenum;
            front_side->brush_idx = sides->brush_idx;
            
            /* Clip a COPY of the winding to front side of partition plane */
            Winding *front_winding = CopyWinding(sides->winding);
            front_side->winding = ClipWindingToPlane(
                front_winding,
                g_plane_normals[planenum],
                g_plane_dists[planenum]
            );
            
            /* Clip original winding to back side of partition plane
             * Negate plane to keep back side */
            Vector3 neg_normal = {
                -g_plane_normals[planenum].x,
                -g_plane_normals[planenum].y,
                -g_plane_normals[planenum].z
            };
            float neg_dist = -g_plane_dists[planenum];
            
            Winding *back_winding = ClipWindingToPlane(
                sides->winding,
                neg_normal,
                neg_dist
            );
            
            /* Original winding is now consumed/freed */
            sides->winding = back_winding;
            
            /* Add both to their respective lists */
            if (front_side->winding) {
                front_side->next = *front;
                *front = front_side;
            } else {
                free(front_side);
            }
            
            if (sides->winding) {
                sides->next = *back;
                *back = sides;
            } else {
                free(sides);
            }
        }
        
        sides = next;
    }
}

/* ========================================================================
   RECURSIVE TREE BUILDING (with depth limit)
   ======================================================================== */

/* Recursive tree node structure (temporary, gets flattened later) */
typedef struct BspNode {
    int planenum;                   // Partition plane
    Side *sides;                  // Faces in this node (for leaf)
    struct BspNode *children[2]; // [0]=front, [1]=back
    bool is_leaf;
    
    /* PHASE 2: Portal on this partition plane (temporary storage) */
    Winding *portal_winding;        // Portal created at this node
    
    /* PHASE 3: Leaf index (for leaves only, -1 for nodes) */
    int leaf_index;                 // Set during flattening
} BspNode;

static BspNode*
BuildTreeRecursive(Side *sides, int depth)
{
    if (!sides) {
        /* Empty - create empty leaf */
        BspNode *leaf = calloc(1, sizeof(BspNode));
        leaf->is_leaf = true;
        leaf->sides = NULL;
        leaf->portal_winding = NULL;  /* PHASE 2: No portal at leaf */
        leaf->leaf_index = -1;         /* PHASE 3: Will be set during flattening */
        return leaf;
    }
    
    /* Count sides */
    int count = 0;
    for (Side *s = sides; s; s = s->next) count++;
    
    /* Stop recursion when very few sides left (≤ 4)
     * No max depth limit - let the tree grow as needed */
    if (count <= 4) {
        BspNode *leaf = calloc(1, sizeof(BspNode));
        leaf->is_leaf = true;
        leaf->sides = sides;
        leaf->portal_winding = NULL;  /* PHASE 2: No portal at leaf */
        leaf->leaf_index = -1;         /* PHASE 3: Will be set during flattening */
        return leaf;
    }
    
    /* Pick partition plane */
    int partition_plane = SelectPartition(sides);
    if (partition_plane == -1) {
        /* No good plane - make leaf */
        BspNode *leaf = calloc(1, sizeof(BspNode));
        leaf->is_leaf = true;
        leaf->sides = sides;
        leaf->portal_winding = NULL;  /* PHASE 2: No portal at leaf */
        leaf->leaf_index = -1;         /* PHASE 3: Will be set during flattening */
        return leaf;
    }
    
    /* Split sides */
    Side *front_sides = NULL, *back_sides = NULL;
    SplitSides(sides, partition_plane, &front_sides, &back_sides);
    
    /* Create node */
    BspNode *node = calloc(1, sizeof(BspNode));
    node->is_leaf = false;
    node->planenum = partition_plane;
    
    /* PHASE 2: Create portal on the partition plane */
    node->portal_winding = BaseWindingForPlane(
        g_plane_normals[partition_plane],
        g_plane_dists[partition_plane]
    );
    
    /* PHASE 2: Clip portal against all brush faces in this node
     * Any part of the portal that's inside a brush gets clipped away */
    for (Side *side = sides; side; side = side->next) {
        if (!side->winding) continue;
        
        /* Get the plane of this brush face */
        Vector3 normal = g_plane_normals[side->planenum];
        float dist = g_plane_dists[side->planenum];
        
        /* Clip portal to the FRONT (outside) of this brush face
         * This removes the part of the portal that's inside the brush */
        node->portal_winding = ClipWindingToPlane(
            node->portal_winding,
            normal,
            dist
        );
        
        /* If portal was completely clipped away, it's fully blocked */
        if (!node->portal_winding) {
            break;  /* No portal here - completely inside brushes */
        }
    }
    
    /* Recurse */
    node->children[0] = BuildTreeRecursive(front_sides, depth + 1);
    node->children[1] = BuildTreeRecursive(back_sides, depth + 1);
    
    return node;
}

/* Count nodes and leaves in tree */
static void
CountTreeNodes(BspNode *node, int *node_count, int *leaf_count)
{
    if (!node) return;
    
    if (node->is_leaf) {
        (*leaf_count)++;
    } else {
        (*node_count)++;
        CountTreeNodes(node->children[0], node_count, leaf_count);
        CountTreeNodes(node->children[1], node_count, leaf_count);
    }
}

/* ========================================================================
   STAGE 1C: LEAF CLASSIFICATION
   ======================================================================== */

/*
 * ClassifyLeaves - Mark each leaf as SOLID or EMPTY
 * 
 * A leaf is SOLID if it contains any face that came from a brush
 * (i.e., face->original_face_idx >= 0)
 * 
 * A leaf is EMPTY if it contains no brush faces (open space/air)
 */
static void
ClassifyLeaves(BSPTree *tree)
{
    int solid_count = 0;
    int empty_count = 0;
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        // Compute leaf center
        Vector3 center = {
            (leaf->bounds_min.x + leaf->bounds_max.x) * 0.5f,
            (leaf->bounds_min.y + leaf->bounds_max.y) * 0.5f,
            (leaf->bounds_min.z + leaf->bounds_max.z) * 0.5f
        };
        
        bool inside_brush = false;
        
        for (BSPFace *face = leaf->faces; face; face = face->next) {
            if (face->original_face_idx < 0) continue;
            
            // Check if center is on negative (inside) side of face
            float dist = Vector3DotProduct(
                    center, 
                    face->normal
                ) - face->plane_dist;

            DBG_OUT("[ClassifyLeaves] Distance = %.4f", dist);
            
            if (dist < -EPSILON) {
                inside_brush = true;
                break;
            }
        }

        DBG_OUT(
                "[ClassifyLeaves] Inside brush: %s", 
                inside_brush?"true":"false"
            );
        
        leaf->contents = inside_brush ? CONTENTS_SOLID : CONTENTS_EMPTY;
        if (inside_brush) solid_count++;
        else empty_count++;
    }
    
    DBG_OUT("[Stage 1c] Classified %d leaves: %d SOLID, %d EMPTY",
            tree->leaf_count, solid_count, empty_count);
}

/* Flatten recursive tree to arrays */
static int
FlattenTreeNode(BspNode *node, BSPTree *tree, int *node_idx, int *leaf_idx)
{
    if (!node) return -1;
    
    if (node->is_leaf) {
        /* Create leaf */
        int idx = (*leaf_idx)++;
        if (idx >= tree->leaf_capacity) {
            DBG_OUT("[ERROR] Leaf capacity exceeded!");
            return ~0;
        }
        
        BSPLeaf *leaf = &tree->leaves[idx];
        leaf->leaf_index = idx;
        leaf->contents = CONTENTS_EMPTY;
        leaf->face_count = 0;
        leaf->faces = NULL;
        leaf->bounds_min = (Vector3){FLT_MAX, FLT_MAX, FLT_MAX};
        leaf->bounds_max = (Vector3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
        
        /* Convert sides to faces */
        for (Side *s = node->sides; s; s = s->next) {
            if (!s->winding) continue;
            
            BSPFace *face = malloc(sizeof(BSPFace));
            face->vertices = malloc(s->winding->numpoints * sizeof(Vector3));
            face->vertex_count = s->winding->numpoints;
            
            for (int i = 0; i < s->winding->numpoints; i++) {
                face->vertices[i] = s->winding->points[i];
                
                /* Update bounds */
                Vector3 v = s->winding->points[i];
                leaf->bounds_min.x = fminf(leaf->bounds_min.x, v.x);
                leaf->bounds_min.y = fminf(leaf->bounds_min.y, v.y);
                leaf->bounds_min.z = fminf(leaf->bounds_min.z, v.z);
                leaf->bounds_max.x = fmaxf(leaf->bounds_max.x, v.x);
                leaf->bounds_max.y = fmaxf(leaf->bounds_max.y, v.y);
                leaf->bounds_max.z = fmaxf(leaf->bounds_max.z, v.z);
            }
            
            face->normal = g_plane_normals[s->planenum];
            face->plane_dist = g_plane_dists[s->planenum];
            face->original_face_idx = s->brush_idx;
            
            face->next = leaf->faces;
            leaf->faces = face;
            leaf->face_count++;
            tree->total_faces++;
        }
        
        /* PHASE 3: Store leaf index back in BspNode for portal linking */
        node->leaf_index = idx;
        
        return ~idx; // Leaf index (negated)
    } else {
        /* Create node */
        int idx = (*node_idx)++;
        if (idx >= tree->node_capacity) {
            DBG_OUT("[ERROR] Node capacity exceeded!");
            return 0;
        }
        
        BSPNode *bsp_node = &tree->nodes[idx];
        bsp_node->plane_normal = g_plane_normals[node->planenum];
        bsp_node->plane_dist = g_plane_dists[node->planenum];
        
        /* Recurse */
        bsp_node->front_child = FlattenTreeNode(node->children[0], tree, node_idx, leaf_idx);
        bsp_node->back_child = FlattenTreeNode(node->children[1], tree, node_idx, leaf_idx);
        
        bsp_node->front_is_leaf = (bsp_node->front_child < 0);
        bsp_node->back_is_leaf = (bsp_node->back_child < 0);
        
        if (bsp_node->front_is_leaf) bsp_node->front_child = ~bsp_node->front_child;
        if (bsp_node->back_is_leaf) bsp_node->back_child = ~bsp_node->back_child;
        
        return idx;
    }
}

/* ========================================================================
   PHASE 2: PORTAL FINALIZATION
   
   After tree building, traverse the node tree to finalize portals.
   This converts temporary portal windings into BSPPortal structures.
   Leaf connectivity will be set up in Phase 3.
   ======================================================================== */

/* PHASE 3: Get the leaf index that a node subtree leads to
 * For leaves: return their stored leaf_index
 * For nodes: traverse down to any leaf in the subtree */
static int
GetLeafIndex(BspNode *node)
{
    if (!node) return -1;
    
    /* If this is a leaf, return its index */
    if (node->is_leaf) {
        return node->leaf_index;
    }
    
    /* For nodes, traverse down to front child
     * (We just need ANY leaf from this subtree for portal connectivity) */
    return GetLeafIndex(node->children[0]);
}

/* Finalize portals: Convert node portal windings to BSPPortal structures */
static void
FinalizePortals_Recursive(BspNode *node, BSPTree *tree, int *portal_idx)
{
    if (!node || node->is_leaf) {
        return;  /* Leaves don't have portals */
    }
    
    /* If this node has a portal winding, finalize it */
    if (node->portal_winding && node->portal_winding->numpoints >= 3) {
        /* Create BSPPortal entry */
        if (*portal_idx < tree->portal_capacity) {
            BSPPortal *portal = &tree->portals[*portal_idx];
            (*portal_idx)++;
            
            /* Set portal properties */
            portal->plane_normal = g_plane_normals[node->planenum];
            portal->plane_dist = g_plane_dists[node->planenum];
            portal->winding = node->portal_winding;
            portal->blocked = false;  /* Has winding = not blocked */
            
            /* PHASE 3: Set leaf indices from front/back children */
            portal->leaf_front = GetLeafIndex(node->children[0]);
            portal->leaf_back = GetLeafIndex(node->children[1]);
            
            if (portal->leaf_front == -1 || portal->leaf_back == -1) {
                DBG_OUT("[Phase 3] WARNING: Portal %d has invalid leaf indices (%d, %d)",
                        *portal_idx - 1, portal->leaf_front, portal->leaf_back);
            }
            
            portal->next_in_leaf = NULL;
            
            /* Transfer ownership to portal - don't free */
            node->portal_winding = NULL;
        }
    } else if (!node->portal_winding) {
        /* Portal was completely clipped - it's blocked */
        if (*portal_idx < tree->portal_capacity) {
            BSPPortal *portal = &tree->portals[*portal_idx];
            (*portal_idx)++;
            
            portal->plane_normal = g_plane_normals[node->planenum];
            portal->plane_dist = g_plane_dists[node->planenum];
            portal->winding = NULL;
            portal->blocked = true;
            
            /* PHASE 3: Set leaf indices even for blocked portals */
            portal->leaf_front = GetLeafIndex(node->children[0]);
            portal->leaf_back = GetLeafIndex(node->children[1]);
            
            portal->next_in_leaf = NULL;
        }
    }
    
    /* Recurse to children */
    FinalizePortals_Recursive(node->children[0], tree, portal_idx);
    FinalizePortals_Recursive(node->children[1], tree, portal_idx);
}

/* Free the temporary node tree after portals are finalized */
static void
FreeNodeTree(BspNode *node)
{
    if (!node) return;
    
    /* Free children first */
    if (!node->is_leaf) {
        FreeNodeTree(node->children[0]);
        FreeNodeTree(node->children[1]);
    }
    
    /* Free sides in leaf (these were already converted to BSPFaces) */
    if (node->is_leaf) {
        Side *side = node->sides;
        while (side) {
            Side *next = side->next;
            if (side->winding) {
                FreeWinding(side->winding);
            }
            free(side);
            side = next;
        }
    }
    
    /* Free portal winding if not transferred to portal */
    if (node->portal_winding) {
        FreeWinding(node->portal_winding);
    }
    
    /* Free the node itself */
    free(node);
}

/* PHASE 3: Get all portals connected to a leaf
 * 
 * Instead of storing portals in leaf lists, we iterate the tree's
 * portal array and check which ones connect to this leaf.
 * 
 * Returns: Number of portals found
 * Fills portal_buffer with pointers to portals (up to max_portals)
 */
static int
GetLeafPortals(const BSPTree *tree, int leaf_index, 
               BSPPortal **portal_buffer, int max_portals)
{
    int count = 0;
    
    for (int i = 0; i < tree->portal_count && count < max_portals; i++) {
        BSPPortal *portal = &tree->portals[i];
        
        /* Check if this portal connects to the target leaf */
        if (portal->leaf_front == leaf_index || portal->leaf_back == leaf_index) {
            portal_buffer[count++] = portal;
        }
    }
    
    return count;
}

/* ========================================================================
   STAGE 1.5: CLASSIFY LEAF CONTENTS (SOLID vs EMPTY)
   
   Determine which leaves are inside solid brushes vs in open space.
   A leaf is SOLID if its center point is inside any brush volume.
   ======================================================================== */

/* Check if a point is inside a brush (all brush planes face outward from brush volume) */
static bool
PointInsideBrush(Vector3 point, const MapBrush *brush)
{
    /* For each plane of the brush, check which side the point is on.
     * Brush normals point OUTWARD, so if point is on the BACK (negative) side
     * of ALL planes, it's inside the brush. */
    
    for (int i = 0; i < brush->plane_count; i++) {
        const MapPlane *plane = &brush->planes[i];
        
        /* Check which side of plane the point is on */
        float dist = Vector3DotProduct(point, plane->normal) - plane->distance;
        
        /* If point is on the FRONT (positive) side of ANY plane, it's outside */
        if (dist > BSP_EPSILON) {
            return false;  /* Outside this brush */
        }
    }
    
    /* Point is on back/inside of ALL planes → inside brush */
    return true;
}

/* Classify all leaves as SOLID or EMPTY */
static void
ClassifyLeafContents(BSPTree *tree, const MapData *map_data)
{
    int solid_count = 0;
    int empty_count = 0;
    
    /* Check each leaf */
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        /* Skip leaves with invalid bounds */
        if (leaf->bounds_min.x >= leaf->bounds_max.x) {
            leaf->contents = CONTENTS_EMPTY;
            empty_count++;
            continue;
        }
        
        /* METHOD 1: Check if any faces in this leaf belong to a real brush
         * (not world boundary faces which have brush_idx = -1)
         * If the leaf contains brush geometry faces, it's likely inside/adjacent to solid */
        bool has_brush_faces = false;
        for (BSPFace *face = leaf->faces; face; face = face->next) {
            /* Check if this face belongs to a real brush (not world boundary) */
            if (face->original_face_idx >= 0 && face->original_face_idx < map_data->world_brush_count * 6) {
                has_brush_faces = true;
                break;
            }
        }
        
        /* METHOD 2: Check if leaf center is inside any brush */
        Vector3 center = {
            (leaf->bounds_min.x + leaf->bounds_max.x) * 0.5f,
            (leaf->bounds_min.y + leaf->bounds_max.y) * 0.5f,
            (leaf->bounds_min.z + leaf->bounds_max.z) * 0.5f
        };
        
        bool center_inside_brush = false;
        for (int b = 0; b < map_data->world_brush_count; b++) {
            if (PointInsideBrush(center, &map_data->world_brushes[b])) {
                center_inside_brush = true;
                break;
            }
        }
        
        /* Classify: SOLID if center is inside a brush
         * Leaves with brush faces but center outside are still EMPTY (they're adjacent to walls) */
        if (center_inside_brush) {
            leaf->contents = CONTENTS_SOLID;
            solid_count++;
        } else {
            leaf->contents = CONTENTS_EMPTY;
            empty_count++;
        }
    }
    
    DBG_OUT("[Stage 1.5] Classified %d leaves: %d SOLID, %d EMPTY",
            tree->leaf_count, solid_count, empty_count);
}

/* ========================================================================
   STAGE 2: MARK OUTSIDE LEAVES
   ======================================================================== */

/* Calculate world bounds from all brush geometry */
/* Mark leaves that have unbounded edges (reach to infinity) as "outside"
 * 
 * In BSP compilation, leaves that extend beyond all brush geometry are
 * considered "outside" or "void". We detect these by looking for leaves
 * whose bounds extend very far - indicating they weren't fully clipped
 * by any brush faces.
 * 
 * The threshold (16384 units) is typical for Quake-style engines where
 * BaseWindingForPlane creates huge initial windings (±32768).
 */
/* Mark leaves that touch the world boundary as "outside"
 * 
 * After adding the world boundary box in Stage 1a.5, leaves that touch
 * this boundary (at ±65536 units) are considered "outside" void.
 * These are the starting points for flood-fill in Stage 3.
 */
static void
MarkOutsideLeaves(BSPTree *tree, const MapData *map_data)
{
    (void)map_data;
    
    const float WORLD_SIZE = 65536.0f;
    const float BOUNDARY_EPSILON = 100.0f;  /* Tolerance for "touching" boundary */
    
    int outside_count = 0;
    
    DBG_OUT("[Stage 2] Detecting leaves touching world boundary (±%.0f)...", WORLD_SIZE);
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        /* Skip leaves with invalid bounds */
        if (leaf->bounds_min.x > leaf->bounds_max.x) {
            continue;
        }
        
        /* Check if leaf bounds touch or extend beyond world boundary */
        bool touches_boundary = (
            leaf->bounds_min.x <= -WORLD_SIZE + BOUNDARY_EPSILON ||
            leaf->bounds_max.x >=  WORLD_SIZE - BOUNDARY_EPSILON ||
            leaf->bounds_min.y <= -WORLD_SIZE + BOUNDARY_EPSILON ||
            leaf->bounds_max.y >=  WORLD_SIZE - BOUNDARY_EPSILON ||
            leaf->bounds_min.z <= -WORLD_SIZE + BOUNDARY_EPSILON ||
            leaf->bounds_max.z >=  WORLD_SIZE - BOUNDARY_EPSILON
        );
        
        if (touches_boundary) {
            leaf->is_outside = true;
            outside_count++;
            
            DBG_OUT("[Stage 2]   Leaf %d touches boundary: bounds=(%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)",
                    i,
                    leaf->bounds_min.x, leaf->bounds_min.y, leaf->bounds_min.z,
                    leaf->bounds_max.x, leaf->bounds_max.y, leaf->bounds_max.z);
        }
    }
    
    DBG_OUT("[Stage 2] Marked %d/%d leaves as outside (%.1f%%)",
            outside_count, tree->leaf_count,
            100.0f * outside_count / tree->leaf_count);
    
    if (outside_count == 0) {
        DBG_OUT("[Stage 2] WARNING: No outside leaves detected!");
        DBG_OUT("[Stage 2] This means the world boundary box didn't work properly.");
    }
}

/* ========================================================================
   STAGE 3: FLOOD-FILL FROM OUTSIDE
   
   Propagate from "outside" leaves through neighboring leaves to mark
   everything that's part of the exterior void. Uses breadth-first search.
   ======================================================================== */

/* Check if two leaves are separated by a solid face (brush geometry)
 * Returns true if there's a face from one leaf that lies between the two leaves
 * and blocks movement between them. */
static bool
LeavesSeparatedByFace(const BSPTree *tree, int leaf_a_idx, int leaf_b_idx)
{
    const BSPLeaf *leaf_a = &tree->leaves[leaf_a_idx];
    const BSPLeaf *leaf_b = &tree->leaves[leaf_b_idx];
    
    /* Get center points of both leaves */
    Vector3 center_a = {
        (leaf_a->bounds_min.x + leaf_a->bounds_max.x) * 0.5f,
        (leaf_a->bounds_min.y + leaf_a->bounds_max.y) * 0.5f,
        (leaf_a->bounds_min.z + leaf_a->bounds_max.z) * 0.5f
    };
    Vector3 center_b = {
        (leaf_b->bounds_min.x + leaf_b->bounds_max.x) * 0.5f,
        (leaf_b->bounds_min.y + leaf_b->bounds_max.y) * 0.5f,
        (leaf_b->bounds_min.z + leaf_b->bounds_max.z) * 0.5f
    };
    
    /* Check all faces in leaf_a */
    for (const BSPFace *face = leaf_a->faces; face; face = face->next) {
        if (face->vertex_count < 3) continue;
        
        /* Calculate which side of this face each center is on */
        float dist_a = Vector3DotProduct(center_a, face->normal) - face->plane_dist;
        float dist_b = Vector3DotProduct(center_b, face->normal) - face->plane_dist;
        
        /* If centers are on opposite sides of this face, it separates them */
        if ((dist_a > BSP_EPSILON && dist_b < -BSP_EPSILON) ||
            (dist_a < -BSP_EPSILON && dist_b > BSP_EPSILON)) {
            return true;  /* Face separates the leaves */
        }
    }
    
    /* Also check faces in leaf_b */
    for (const BSPFace *face = leaf_b->faces; face; face = face->next) {
        if (face->vertex_count < 3) continue;
        
        float dist_a = Vector3DotProduct(center_a, face->normal) - face->plane_dist;
        float dist_b = Vector3DotProduct(center_b, face->normal) - face->plane_dist;
        
        if ((dist_a > BSP_EPSILON && dist_b < -BSP_EPSILON) ||
            (dist_a < -BSP_EPSILON && dist_b > BSP_EPSILON)) {
            return true;  /* Face separates the leaves */
        }
    }
    
    return false;  /* No separating face found */
}

/* Check if two leaves are neighbors (share a partition plane) AND not separated by brush faces
 * This is the proper QBSP approach: check both spatial adjacency and brush separation */
static bool
LeavesAreNeighbors(const BSPTree *tree, int leaf_a_idx, int leaf_b_idx)
{
    const BSPLeaf *leaf_a = &tree->leaves[leaf_a_idx];
    const BSPLeaf *leaf_b = &tree->leaves[leaf_b_idx];
    
    /* First: Simple bounds-based check - do the leaves touch? */
    const float TOUCH_EPSILON = 0.1f;
    
    /* Check if bounds overlap or touch in at least one dimension */
    bool x_adjacent = (fabsf(leaf_a->bounds_max.x - leaf_b->bounds_min.x) < TOUCH_EPSILON ||
                       fabsf(leaf_b->bounds_max.x - leaf_a->bounds_min.x) < TOUCH_EPSILON);
    bool y_adjacent = (fabsf(leaf_a->bounds_max.y - leaf_b->bounds_min.y) < TOUCH_EPSILON ||
                       fabsf(leaf_b->bounds_max.y - leaf_a->bounds_min.y) < TOUCH_EPSILON);
    bool z_adjacent = (fabsf(leaf_a->bounds_max.z - leaf_b->bounds_min.z) < TOUCH_EPSILON ||
                       fabsf(leaf_b->bounds_max.z - leaf_a->bounds_min.z) < TOUCH_EPSILON);
    
    /* Leaves must be adjacent in exactly one axis and overlap in the other two */
    bool x_overlap = (leaf_a->bounds_min.x < leaf_b->bounds_max.x + TOUCH_EPSILON &&
                      leaf_a->bounds_max.x > leaf_b->bounds_min.x - TOUCH_EPSILON);
    bool y_overlap = (leaf_a->bounds_min.y < leaf_b->bounds_max.y + TOUCH_EPSILON &&
                      leaf_a->bounds_max.y > leaf_b->bounds_min.y - TOUCH_EPSILON);
    bool z_overlap = (leaf_a->bounds_min.z < leaf_b->bounds_max.z + TOUCH_EPSILON &&
                      leaf_a->bounds_max.z > leaf_b->bounds_min.z - TOUCH_EPSILON);
    
    bool spatially_adjacent = (x_adjacent && y_overlap && z_overlap) ||
                              (y_adjacent && x_overlap && z_overlap) ||
                              (z_adjacent && x_overlap && y_overlap);
    
    if (!spatially_adjacent) {
        return false;  /* Not even touching - definitely not neighbors */
    }
    
    /* Second: Check if there's a solid brush face between them
     * If leaves are spatially adjacent BUT separated by a face (like a ceiling),
     * they should NOT flood into each other */
    if (LeavesSeparatedByFace(tree, leaf_a_idx, leaf_b_idx)) {
        return false;  /* Separated by brush geometry - not traversable neighbors */
    }
    
    return true;  /* Spatially adjacent AND no separating face - can flood */
}

/* Flood-fill from outside leaves to mark the exterior void
 * 
 * Algorithm:
 * 1. Start from all leaves marked as "outside" (from Stage 2)
 * 2. Use breadth-first search to propagate to neighboring leaves
 * 3. Only flood through "empty" space (leaves with few faces)
 * 4. Stop at leaves with many faces (dense geometry = walls)
 * 
 * After this, flood_filled leaves = exterior void (unreachable)
 *              non-flooded leaves = sealed interior (playable space)
 */
static void
FloodFillFromOutside(BSPTree *tree)
{
    DBG_OUT("[Stage 3] Starting flood-fill from %d outside leaves...", 
            tree->leaf_count);
    
    /* Simple queue for BFS (fixed size, should be enough for most maps) */
    int *queue = malloc(tree->leaf_count * sizeof(int));
    int queue_start = 0;
    int queue_end = 0;
    
    /* Initialize: add all outside leaves to queue */
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->is_outside) {
            leaf->flood_filled = true;
            leaf->flood_parent = -1;  /* No parent for seed leaves */
            queue[queue_end++] = i;
            
            DBG_OUT("[Stage 3]   Seed leaf %d (faces=%d)", i, leaf->face_count);
        }
    }
    
    int initial_seeds = queue_end;
    int flooded_count = initial_seeds;
    
    /* Configurable flood threshold
     * Lower = more aggressive flooding (marks more as void)
     * Higher = more conservative (keeps more as interior)
     * 
     * For small test maps: try 5-10
     * For large maps with lots of detail: try 15-20 */
    const int FLOOD_FACE_THRESHOLD = 15;  /* Increased from 10 */
    
    DBG_OUT("[Stage 3]   Flood threshold: %d faces (leaves with >= %d faces won't flood through)",
            FLOOD_FACE_THRESHOLD, FLOOD_FACE_THRESHOLD);
    
    /* BFS flood-fill */
    while (queue_start < queue_end) {
        int current_idx = queue[queue_start++];
        BSPLeaf *current = &tree->leaves[current_idx];
        
        /* Try to flood to all neighboring leaves */
        for (int neighbor_idx = 0; neighbor_idx < tree->leaf_count; neighbor_idx++) {
            if (neighbor_idx == current_idx) continue;
            
            BSPLeaf *neighbor = &tree->leaves[neighbor_idx];
            
            /* Skip if already flooded */
            if (neighbor->flood_filled) continue;
            
            /* CRITICAL: Can't flood through SOLID leaves (inside walls/brushes)!
             * Only EMPTY leaves can be flooded */
            if (neighbor->contents == CONTENTS_SOLID) {
                continue;  /* This leaf is inside a brush - can't flood through it */
            }
            
            /* Check if leaves are neighbors */
            if (!LeavesAreNeighbors(tree, current_idx, neighbor_idx)) {
                continue;
            }
            
            /* Flood condition: only flood through "empty" space
             * Dense geometry (many faces) = walls, stop flooding
             * Sparse geometry (few faces) = open space, continue flooding */
            
            if (neighbor->face_count >= FLOOD_FACE_THRESHOLD) {
                /* Too many faces - this is a wall, don't flood */
                continue;
            }
            
            /* Flood to this neighbor */
            neighbor->flood_filled = true;
            neighbor->flood_parent = current_idx;
            queue[queue_end++] = neighbor_idx;
            flooded_count++;
            
            DBG_OUT("[Stage 3]   Flooded leaf %d (faces=%d) from leaf %d",
                    neighbor_idx, neighbor->face_count, current_idx);
        }
    }
    
    free(queue);
    
    DBG_OUT("[Stage 3] Flood-fill complete:");
    DBG_OUT("[Stage 3]   Seeds: %d leaves", initial_seeds);
    DBG_OUT("[Stage 3]   Flooded: %d/%d leaves (%.1f%%)",
            flooded_count, tree->leaf_count,
            100.0f * flooded_count / tree->leaf_count);
    DBG_OUT("[Stage 3]   Interior (non-flooded): %d leaves (%.1f%%)",
            tree->leaf_count - flooded_count,
            100.0f * (tree->leaf_count - flooded_count) / tree->leaf_count);
}

/* ========================================================================
   STAGE 4: LEAK DETECTION
   
   Check if any point entities (like info_player_start) ended up in
   flooded leaves. If so, the map has a LEAK - the void can reach the
   playable area.
   ======================================================================== */

/* Helper: Parse Vector3 from "x y z" string format */
static Vector3
ParseVector3FromString(const char *str)
{
    Vector3 v = {0, 0, 0};
    if (str) {
        sscanf(str, "%f %f %f", &v.x, &v.y, &v.z);
    }
    return v;
}

/* Check for leaks - point entities in flooded (void) leaves */
static void
CheckForLeaks(BSPTree *tree, const MapData *map_data)
{
    int leak_count = 0;
    
    /* Initialize leak data */
    tree->has_leak = false;
    tree->leak_path_length = 0;
    
    DBG_OUT("[Stage 4] Checking %d entities for leaks...", 
            map_data->entity_count);
    
    /* Check all entities */
    for (int i = 0; i < map_data->entity_count; i++) {
        const MapEntity *entity = &map_data->entities[i];
        
        /* Get classname */
        const char *classname = GetEntityProperty((MapEntity*)entity, "classname");
        if (!classname) continue;
        
        /* Skip worldspawn (entity 0) and brush entities */
        if (strcmp(classname, "worldspawn") == 0) continue;
        if (entity->brush_count > 0) continue;  /* Brush entity, not a point */
        
        /* Get origin */
        const char *origin_str = GetEntityProperty((MapEntity*)entity, "origin");
        if (!origin_str) {
            DBG_OUT("[Stage 4]   Entity %d (%s): No origin, skipping", 
                    i, classname);
            continue;
        }
        
        Vector3 origin = ParseVector3FromString(origin_str);
        
        /* Find which leaf contains this entity */
        const BSPLeaf *leaf = BSP_FindLeaf(tree, origin);
        
        if (!leaf) {
            DBG_OUT("[Stage 4]   Entity %d (%s) at (%.1f,%.1f,%.1f): Could not find leaf!",
                    i, classname, origin.x, origin.y, origin.z);
            continue;
        }
        
        int leaf_idx = leaf - tree->leaves;  /* Calculate leaf index */
        
        /* Check if this leaf was flooded (= in the void) */
        if (leaf->flood_filled) {
            DBG_OUT("[Stage 4]   *** LEAK DETECTED ***");
            DBG_OUT("[Stage 4]   Entity %d (%s) at (%.1f,%.1f,%.1f) is in VOID (leaf %d)",
                    i, classname, origin.x, origin.y, origin.z, leaf_idx);
            DBG_OUT("[Stage 4]   The void can reach this entity - map is not sealed!");
            
            /* Store leak path for visualization (only first leak) */
            if (!tree->has_leak) {
                tree->has_leak = true;
                tree->leak_entity_pos = origin;
                
                /* Trace back through flood_parent chain to build path */
                DBG_OUT("[Stage 4]   Leak path (entity -> void):");
                int current_idx = leaf_idx;
                int depth = 0;
                
                while (current_idx != -1 && depth < 50) {
                    const BSPLeaf *step = &tree->leaves[current_idx];
                    Vector3 center = {
                        (step->bounds_min.x + step->bounds_max.x) * 0.5f,
                        (step->bounds_min.y + step->bounds_max.y) * 0.5f,
                        (step->bounds_min.z + step->bounds_max.z) * 0.5f
                    };
                    
                    /* Store in path array */
                    tree->leak_path[depth] = center;
                    
                    const char *type = step->is_outside ? "OUTSIDE" : "interior";
                    const char *contents = step->contents == CONTENTS_SOLID ? "SOLID" : "EMPTY";
                    DBG_OUT("[Stage 4]     Step %d: Leaf %d (%s, %s, %d faces) at (%.1f,%.1f,%.1f)",
                            depth, current_idx, type, contents, step->face_count,
                            center.x, center.y, center.z);
                    
                    current_idx = step->flood_parent;
                    depth++;
                }
                
                tree->leak_path_length = depth;
            }
            
            leak_count++;
        } else {
            DBG_OUT("[Stage 4]   Entity %d (%s) at (%.1f,%.1f,%.1f): OK (leaf %d, interior)",
                    i, classname, origin.x, origin.y, origin.z, leaf_idx);
        }
    }
    
    if (leak_count > 0) {
        DBG_OUT("[Stage 4] ========================================");
        DBG_OUT("[Stage 4] COMPILE ERROR: %d LEAK(S) DETECTED!", leak_count);
        DBG_OUT("[Stage 4] Fix these leaks before the map can be played.");
        DBG_OUT("[Stage 4] ========================================");
    } else {
        DBG_OUT("[Stage 4] No leaks detected - map is properly sealed!");
    }
}

/* ========================================================================
   STAGE 3: FLOOD-FILL FROM OUTSIDE
   
   Propagate from "outside" leaves through empty space to mark all void.
   The algorithm:
   1. Start from all leaves marked as "outside" (Stage 2)
   2. Recursively flood to neighboring leaves
   3. Only flood through leaves with FEW faces (empty space)
   4. Stop at leaves with MANY faces (solid walls)
   
/* ========================================================================
   PUBLIC API
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
    Side *all_sides = NULL;
    
    for (int i = 0; i < map_data->world_brush_count; i++) {
        Side *brush_sides = MakeSidesFromBrush(&map_data->world_brushes[i], i);
        
        /* Build windings for this brush */
        MakeWindingsForBrush(brush_sides);
        
        /* Link to master list */
        Side *tail = brush_sides;
        while (tail && tail->next) tail = tail->next;
        if (tail) {
            tail->next = all_sides;
            all_sides = brush_sides;
        }
    }
    
    /* Count valid windings */
    int valid_count = 0;
    for (Side *s = all_sides; s; s = s->next) {
        if (s->winding) valid_count++;
    }
    
    DBG_OUT("[Stage 1a] Generated %d sides, %d planes, %d valid windings",
            map_data->world_brush_count * 6, g_num_planes, valid_count);
    
    if (valid_count == 0) {
        DBG_OUT("[Stage 1a] ERROR: No valid windings! Cannot build tree.");
        return NULL;
    }
    
    /* STAGE 1a.5: Add world boundary box
     * This creates a HUGE box around the entire map to ensure the BSP tree
     * partitions the exterior void. Leaves that touch this boundary will be
     * marked as "outside" for leak detection.
     * 
     * The box is intentionally MUCH larger than any map geometry so that:
     * 1. All map brushes are fully inside it
     * 2. Leaves at the boundary extend to "infinity" (unbounded)
     * 3. We can detect when interior space connects to this void (= leak)
     */
    const float WORLD_SIZE = 65536.0f;  /* ±65536 = 131072 unit cube (Quake standard) */
    
    DBG_OUT("[Stage 1a.5] Adding world boundary box (±%.0f units)...", WORLD_SIZE);
    
    /* Create 6 sides for the world box (inverted normals - we want the OUTSIDE) */
    Vector3 box_normals[6] = {
        { 1, 0, 0},  /* +X wall (normal points inward) */
        {-1, 0, 0},  /* -X wall */
        { 0, 1, 0},  /* +Y wall */
        { 0,-1, 0},  /* -Y wall */
        { 0, 0, 1},  /* +Z wall */
        { 0, 0,-1}   /* -Z wall */
    };
    
    for (int i = 0; i < 6; i++) {
        int planenum = FindOrAddPlane(box_normals[i], WORLD_SIZE);
        
        Side *side = calloc(1, sizeof(Side));
        side->planenum = planenum;
        side->brush_idx = -1;  /* Special marker: world boundary, not a real brush */
        
        /* Create huge winding for this plane */
        side->winding = BaseWindingForPlane(box_normals[i], WORLD_SIZE);
        
        /* Add to list */
        side->next = all_sides;
        all_sides = side;
    }
    
    DBG_OUT("[Stage 1a.5] Added 6 world boundary sides");
    
    /* STAGE 1b: Build BSP tree with limited recursion */
    DBG_OUT("[Stage 1b] Building tree (no depth limit)...");
    
    BspNode *root = BuildTreeRecursive(all_sides, 0);
    
    /* Count nodes and leaves */
    int node_count = 0, leaf_count = 0;
    CountTreeNodes(root, &node_count, &leaf_count);
    
    DBG_OUT("[Stage 1b] Tree built: %d nodes, %d leaves", node_count, leaf_count);
    
    /* Allocate BSPTree */
    BSPTree *tree = calloc(1, sizeof(BSPTree));
    tree->node_capacity = node_count + 16;
    tree->leaf_capacity = leaf_count + 16;
    tree->nodes = calloc(tree->node_capacity, sizeof(BSPNode));
    tree->leaves = calloc(tree->leaf_capacity, sizeof(BSPLeaf));
    tree->root_is_leaf = (node_count == 0);
    
    /* PHASE 1: Allocate portal storage
     * Initial capacity: estimate 2 portals per leaf (typical BSP tree)
     * Actual count will be determined during tree building in Phase 2 */
    tree->portal_capacity = leaf_count * 2 + 16;
    tree->portal_count = 0;
    tree->portals = calloc(tree->portal_capacity, sizeof(BSPPortal));
    
    /* Flatten tree */
    DBG_OUT("[Stage 1b] Flattening tree...");
    int node_idx = 0, leaf_idx = 0;
    FlattenTreeNode(root, tree, &node_idx, &leaf_idx);
    
    tree->node_count = node_idx;
    tree->leaf_count = leaf_idx;
    
    DBG_OUT("[Stage 1b] Result: %d nodes, %d leaves, %d faces",
            tree->node_count, tree->leaf_count, tree->total_faces);
    
    /* PHASE 2: Finalize portals */
    DBG_OUT("[Phase 2] Finalizing portals...");
    int portal_idx = 0;
    FinalizePortals_Recursive(root, tree, &portal_idx);
    tree->portal_count = portal_idx;
    
    /* Count blocked vs open portals */
    int blocked_count = 0;
    int open_count = 0;
    for (int i = 0; i < tree->portal_count; i++) {
        if (tree->portals[i].blocked) {
            blocked_count++;
        } else {
            open_count++;
        }
    }
    
    DBG_OUT("[Phase 2] Created %d portals (%d blocked, %d open)",
            tree->portal_count, blocked_count, open_count);
    
    /* PHASE 2: Free the temporary node tree (portals are now in tree->portals) */
    FreeNodeTree(root);
    root = NULL;
    
    /* STAGE 1c: Validation & stats */
    BSP_Validate(tree);
    BSP_PrintStats(tree);
    
    /* STAGE 1.5: Classify leaf contents (SOLID vs EMPTY)
     * Determine which leaves are inside solid brushes (walls) vs open space.
     * This is critical for flood-fill - we can't flood through SOLID leaves! */
    DBG_OUT("[Stage 1.5] Classifying leaf contents...");
    ClassifyLeafContents(tree, map_data);
    
    /* STAGE 2: Mark outside leaves */
    DBG_OUT("[Stage 2] Marking outside leaves...");
    MarkOutsideLeaves(tree, map_data);
    
    /* STAGE 3: Flood-fill from outside */
    DBG_OUT("[Stage 3] Flood-filling from outside...");
    FloodFillFromOutside(tree);
    
    /* STAGE 4: Leak detection - check if any point entities are in flooded leaves */
    DBG_OUT("[Stage 4] Checking for leaks...");
    CheckForLeaks(tree, map_data);
    
    /* STAGE 5: Final classification and face culling */
    /* TODO: Implement ClassifyAndCullFaces
    DBG_OUT("[Stage 5] Classifying leaves and culling hidden faces...");
    ClassifyAndCullFaces(tree);
    */
    
    /* Print final stats */
    BSP_PrintStats(tree);
    
    return tree;
}

void 
BSP_Free(BSPTree *tree) 
{
    if (!tree) return;
    
    /* PHASE 1: Free portal windings */
    for (int i = 0; i < tree->portal_count; i++) {
        if (tree->portals[i].winding) {
            FreeWinding(tree->portals[i].winding);
        }
    }
    
    /* PHASE 1: Free portal array */
    free(tree->portals);
    
    /* Free leaf faces */
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPFace *face = tree->leaves[i].faces;
        while (face) {
            BSPFace *next = face->next;
            free(face->vertices);
            free(face);
            face = next;
        }
    }
    
    /* Free main arrays */
    free(tree->nodes);
    free(tree->leaves);
    free(tree);
}
/* Print detailed BSP tree statistics */
void
BSP_PrintStats(const BSPTree *tree)
{
    if (!tree) {
        DBG_OUT("[Stats] No tree to analyze");
        return;
    }
    
    DBG_OUT("========== BSP TREE STATISTICS ==========");
    DBG_OUT("Nodes: %d", tree->node_count);
    DBG_OUT("Leaves: %d", tree->leaf_count);
    DBG_OUT("Total Faces: %d", tree->total_faces);
    
    /* Count leaf types and stages */
    int solid_count = 0, empty_count = 0;
    int outside_count = 0, flooded_count = 0;
    int min_faces = INT_MAX, max_faces = 0;
    int total_leaf_faces = 0;
    
    for (int i = 0; i < tree->leaf_count; i++) {
        const BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->contents == CONTENTS_SOLID) {
            solid_count++;
        } else {
            empty_count++;
        }
        
        if (leaf->is_outside) outside_count++;
        if (leaf->flood_filled) flooded_count++;
        
        total_leaf_faces += leaf->face_count;
        if (leaf->face_count < min_faces) min_faces = leaf->face_count;
        if (leaf->face_count > max_faces) max_faces = leaf->face_count;
    }
    
    DBG_OUT("SOLID Leaves: %d (%.1f%%)", 
            solid_count, 100.0f * solid_count / tree->leaf_count);
    DBG_OUT("EMPTY Leaves: %d (%.1f%%)", 
            empty_count, 100.0f * empty_count / tree->leaf_count);
    DBG_OUT("Outside Leaves: %d (%.1f%%)", 
            outside_count, 100.0f * outside_count / tree->leaf_count);
    DBG_OUT("Flooded Leaves: %d (%.1f%%)", 
            flooded_count, 100.0f * flooded_count / tree->leaf_count);
    DBG_OUT("Faces per leaf: min=%d, max=%d, avg=%.1f",
            min_faces, max_faces, (float)total_leaf_faces / tree->leaf_count);
    
    /* PHASE 3: Portal connectivity statistics */
    int portals_with_valid_leaves = 0;
    int total_portal_connections = 0;
    
    for (int i = 0; i < tree->portal_count; i++) {
        if (tree->portals[i].leaf_front >= 0 && tree->portals[i].leaf_back >= 0) {
            portals_with_valid_leaves++;
        }
    }
    
    /* Count average portals per leaf */
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPPortal *buffer[64];
        int count = GetLeafPortals(tree, i, buffer, 64);
        total_portal_connections += count;
    }
    
    DBG_OUT("Portals: %d (capacity: %d)", 
            tree->portal_count, tree->portal_capacity);
    DBG_OUT("Portal connectivity: %d/%d portals have valid leaf indices",
            portals_with_valid_leaves, tree->portal_count);
    DBG_OUT("Average portals per leaf: %.1f",
            tree->leaf_count > 0 ? (float)total_portal_connections / tree->leaf_count : 0.0f);
    DBG_OUT("=========================================");
}
/* Validate tree structure and test point classification */
bool
BSP_Validate(const BSPTree *tree)
{
    if (!tree) {
        DBG_OUT("[Validate] ERROR: NULL tree");
        return false;
    }
    
    DBG_OUT("[Validate] Testing BSP tree...");
    DBG_OUT("[Validate] Tree has %d nodes, %d leaves", 
            tree->node_count, tree->leaf_count);
    
    /* Test 1: Verify node/leaf counts are consistent */
    if (tree->node_count > 0 && tree->leaf_count != tree->node_count + 1) {
        DBG_OUT("[Validate] WARNING: Expected %d leaves for %d nodes (got %d)",
                tree->node_count + 1, tree->node_count, tree->leaf_count);
    }
    
    /* Test 2: Test some known points */
    /* Point clearly in open space */
    Vector3 test_open = {0, 10, 0};
    LeafContents contents_open = BSP_GetPointContents(tree, test_open);
    DBG_OUT("[Validate] Point (%.1f, %.1f, %.1f): %s", 
            test_open.x, test_open.y, test_open.z,
            contents_open == CONTENTS_EMPTY ? "EMPTY" : "SOLID");
    
    /* Point likely inside a wall (you can adjust these coordinates) */
    Vector3 test_wall = {3, 0, 3};
    LeafContents contents_wall = BSP_GetPointContents(tree, test_wall);
    DBG_OUT("[Validate] Point (%.1f, %.1f, %.1f): %s", 
            test_wall.x, test_wall.y, test_wall.z,
            contents_wall == CONTENTS_EMPTY ? "EMPTY" : "SOLID");
    
    /* Test 3: Verify all leaves have valid bounds */
    int invalid_bounds = 0;
    for (int i = 0; i < tree->leaf_count; i++) {
        const BSPLeaf *leaf = &tree->leaves[i];
        if (leaf->bounds_min.x >= leaf->bounds_max.x && leaf->face_count > 0) {
            invalid_bounds++;
        }
    }
    
    if (invalid_bounds > 0) {
        DBG_OUT("[Validate] WARNING: %d leaves with invalid bounds", invalid_bounds);
    }
    
    DBG_OUT("[Validate] Validation complete");
    return true;
}
/* Find which leaf contains the given point */
const BSPLeaf*
BSP_FindLeaf(const BSPTree *tree, Vector3 point)
{
    if (!tree || tree->leaf_count == 0) return NULL;
    
    /* Handle degenerate case: tree is just one leaf */
    if (tree->root_is_leaf) {
        return &tree->leaves[0];
    }
    
    /* Traverse tree from root */
    int node_idx = 0;
    bool is_leaf = false;
    
    while (!is_leaf) {
        const BSPNode *node = &tree->nodes[node_idx];
        
        /* Classify point against partition plane */
        float dist = Vector3DotProduct(point, node->plane_normal) - node->plane_dist;
        
        if (dist >= 0) {
            /* Front side */
            node_idx = node->front_child;
            is_leaf = node->front_is_leaf;
        } else {
            /* Back side */
            node_idx = node->back_child;
            is_leaf = node->back_is_leaf;
        }
    }
    
    /* node_idx now contains leaf index */
    if (node_idx >= 0 && node_idx < tree->leaf_count) {
        return &tree->leaves[node_idx];
    }
    
    return NULL;
}
/* Get the contents (SOLID or EMPTY) of the leaf containing the point */
LeafContents
BSP_GetPointContents(const BSPTree *tree, Vector3 point)
{
    const BSPLeaf *leaf = BSP_FindLeaf(tree, point);
    
    if (!leaf) {
        return CONTENTS_EMPTY; /* Default to empty if lookup fails */
    }
    
    return leaf->contents;
}
void 
BSP_DebugDrawLeafBounds(const BSPTree *tree)
{
    if (!tree) return;
    
    /* Draw all leaf bounding boxes color-coded by state */
    for (int i = 0; i < tree->leaf_count; i++) {
        const BSPLeaf *leaf = &tree->leaves[i];
        
        /* Skip leaves with invalid bounds */
        if (leaf->bounds_min.x >= leaf->bounds_max.x) continue;
        
        Color box_color;
        
        /* Priority: flooded > outside > contents */
        if (leaf->flood_filled) {
            /* Flooded leaves (Stage 3) - CYAN */
            box_color = (Color){0, 255, 255, 128};
        } else if (leaf->is_outside) {
            /* Outside leaves (Stage 2) - YELLOW */
            box_color = (Color){255, 255, 0, 128};
        } else if (leaf->contents == CONTENTS_SOLID) {
            /* SOLID leaves - RED */
            box_color = (Color){255, 0, 0, 128};
        } else {
            /* EMPTY leaves - GREEN */
            box_color = (Color){0, 255, 0, 128};
        }
        
        DrawBoundingBox((BoundingBox){leaf->bounds_min, leaf->bounds_max}, box_color);
        
        /* Draw faces as wireframe (color by brush) */
        for (BSPFace *face = leaf->faces; face; face = face->next) {
            if (face->vertex_count < 3) continue;
            
            Color brush_colors[] = {
                {255, 100, 100, 255},  // Light Red
                {100, 255, 100, 255},  // Light Green  
                {100, 100, 255, 255},  // Light Blue
                {255, 255, 100, 255},  // Yellow
                {255, 100, 255, 255},  // Magenta
                {100, 255, 255, 255},  // Cyan
                {255, 180, 100, 255},  // Orange
                {180, 100, 255, 255},  // Purple
            };
            
            int brush_idx = face->original_face_idx;
            Color color = brush_colors[brush_idx % 8];
            
            for (int v = 0; v < face->vertex_count; v++) {
                int next = (v + 1) % face->vertex_count;
                DrawLine3D(face->vertices[v], face->vertices[next], color);
            }
        }
    }
}

/* Get debug text info (call this in 2D rendering mode for overlay) */
void
BSP_DebugDrawText(const BSPTree *tree)
{
    if (!tree) return;
    
    /* Stage display info */
    DrawText(TextFormat("BSP Tree: %d nodes, %d leaves", 
             tree->node_count, tree->leaf_count), 10, 10, 20, WHITE);
    DrawText(TextFormat("Total faces: %d", tree->total_faces), 10, 35, 20, WHITE);
    
    /* Count leaf types for display */
    int solid_leaves = 0, empty_leaves = 0, outside_leaves = 0, flooded_leaves = 0;
    for (int i = 0; i < tree->leaf_count; i++) {
        if (tree->leaves[i].contents == CONTENTS_SOLID) solid_leaves++;
        else empty_leaves++;
        if (tree->leaves[i].is_outside) outside_leaves++;
        if (tree->leaves[i].flood_filled) flooded_leaves++;
    }
    
    DrawText(TextFormat("SOLID: %d  EMPTY: %d  OUTSIDE: %d  FLOODED: %d", 
                        solid_leaves, empty_leaves, outside_leaves, flooded_leaves), 
             10, 60, 20, WHITE);
    DrawText("RED = SOLID  |  GREEN = EMPTY  |  YELLOW = OUTSIDE  |  CYAN = FLOODED", 
             10, 85, 20, WHITE);
}

/* Debug draw leak path with wireframe spheres and connecting lines */
void
BSP_DebugDrawLeak(const BSPTree *tree)
{
    if (!tree || !tree->has_leak) return;
    
    const float SPHERE_RADIUS = 0.3f;
    const int RINGS = 3;
    const int SLICES = 8;
    
    /* Draw entity position with RED sphere */
    DrawSphereWires(tree->leak_entity_pos, SPHERE_RADIUS, RINGS, SLICES, RED);
    
    /* Draw leak path */
    for (int i = 0; i < tree->leak_path_length; i++) {
        /* Draw sphere at this step */
        Color step_color = (i == tree->leak_path_length - 1) ? YELLOW : ORANGE;
        DrawSphereWires(tree->leak_path[i], SPHERE_RADIUS * 0.75f, RINGS, SLICES, step_color);
        
        /* Draw line from entity to first step, or between steps */
        Vector3 start = (i == 0) ? tree->leak_entity_pos : tree->leak_path[i - 1];
        Vector3 end = tree->leak_path[i];
        DrawLine3D(start, end, MAGENTA);
    }
}

/* Get leak debug text info (call this in 2D rendering mode for overlay) */
void
BSP_DebugDrawLeakText(const BSPTree *tree)
{
    if (!tree || !tree->has_leak) return;
    
    /* Draw text overlay */
    DrawText("LEAK DETECTED!", 10, 110, 30, RED);
    DrawText(TextFormat("Path length: %d steps", tree->leak_path_length), 10, 145, 20, ORANGE);
    DrawText("RED sphere = Entity position", 10, 170, 16, WHITE);
    DrawText("YELLOW sphere = Outside void", 10, 190, 16, WHITE);
    DrawText("MAGENTA lines = Leak path", 10, 210, 16, WHITE);
}

