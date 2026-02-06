#include "mapscene_bsp.h"
#include "mapscene_types.h"
#include "v220_map_parser.h"
#include "kolibri.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#define INITIAL_NODE_CAPACITY 512
#define INITIAL_LEAF_CAPACITY 256
#define MIN_FACES_FOR_SPLIT 4
#define MAX_TREE_DEPTH 32
#define COPLANAR_EPSILON 0.01f
#define MAX_SPLIT_VERTS 128
#define MAX_PORTAL_VERTS 64

/* Portal connecting two leaves */
typedef struct Portal {
	Vector3 *vertices;
	int vertex_count;
	Vector3 normal;
	float plane_dist;
	int leaf_front;
	int leaf_back;
	struct Portal *next;
} Portal;

typedef struct {
	BSPFace **faces;
	int face_count;
	int face_capacity;
} FaceList;

static Vector3 g_player_start_pos = {0, 0, 0};
static Portal *g_portals = NULL;

/* ========================================================================
   PORTAL FUNCTIONS
   ======================================================================== */

static Portal *
Portal_Create(int vertex_count)
{
	Portal *portal;
	
	if (vertex_count <= 0 || vertex_count > MAX_PORTAL_VERTS) {
		fprintf(stderr, "[BSP ERROR] Invalid portal vertex count: %d\n", vertex_count);
		return NULL;
	}
	
	portal = malloc(sizeof(Portal));
	if (!portal)
		return NULL;
	
	portal->vertices = malloc(vertex_count * sizeof(Vector3));
	if (!portal->vertices) {
		free(portal);
		return NULL;
	}
	
	portal->vertex_count = vertex_count;
	portal->normal = (Vector3){0, 0, 0};
	portal->plane_dist = 0.0f;
	portal->leaf_front = -1;
	portal->leaf_back = -1;
	portal->next = NULL;
	
	return portal;
}

static void
Portal_Free(Portal *portal)
{
	if (portal) {
		if (portal->vertices)
			free(portal->vertices);
		free(portal);
	}
}

/* ========================================================================
   HELPER FUNCTIONS
   ======================================================================== */
static bool
IsLeafInExteriorVoid(const BSPLeaf *leaf, 
                     const CompiledFace *all_faces,
                     int face_count,
                     const Vector3 *vertices,
                     const CompiledBrush *brushes,
                     int brush_count)
{
    /* Get leaf center */
    Vector3 center = Vector3Scale(Vector3Add(leaf->bounds_min, leaf->bounds_max), 0.5f);
    
    /* Count how many brush faces have this point in front of them
     * If a point is "outside" the map, most/all faces will have it in front */
    int in_front_count = 0;
    int behind_count = 0;
    
    for (int i = 0; i < face_count; i++) {
        const CompiledFace *face = &all_faces[i];
        
        /* Faces should point OUTWARD from brushes
         * So a point INSIDE the map is BEHIND most faces
         * A point OUTSIDE is IN FRONT of most faces */
        float dist = Vector3DotProduct(center, face->normal) - face->plane_dist;
        
        if (dist > BSP_EPSILON) {
            in_front_count++;
        } else if (dist < -BSP_EPSILON) {
            behind_count++;
        }
    }
    
    /* If point is in front of significantly more faces than behind,
     * it's in the exterior void */
    float front_ratio = (float)in_front_count / (float)(in_front_count + behind_count);
    
    /* Threshold: if >60% of faces have point in front, it's exterior */
    return (front_ratio > 0.6f);
}

/* Check if a face's normal points toward reachable EMPTY space */
static bool
FacePointsTowardPlayableSpace(
    const BSPFace *face, 
    int            leaf_idx, 
    const BSPTree *tree
)
{
    /* Get a point slightly in front of the face (in normal direction) */
    Vector3 center = {0, 0, 0};
    for (int i = 0; i < face->vertex_count; i++) {
        center = Vector3Add(center, face->vertices[i]);
    }
    center = Vector3Scale(center, 1.0f / face->vertex_count);
    
    /* Move slightly along the normal */
    Vector3 test_point = Vector3Add(center, Vector3Scale(face->normal, 0.1f));
    
    /* Find which leaf this point is in */
    const BSPLeaf *test_leaf = BSP_FindLeaf(tree, test_point);
    
    if (!test_leaf)
        return false;
    
    /* Face is visible if it points toward reachable EMPTY space */
    return (test_leaf->contents == CONTENTS_EMPTY && test_leaf->is_reachable);
}

static void
DebugPrintLeafSamples(const BSPTree *tree)
{
    DBG_OUT("[BSP] Sample leaf contents:");
    
    int samples = (tree->leaf_count < 10) ? tree->leaf_count : 10;
    
    for (int i = 0; i < samples; i++) {
        const BSPLeaf *leaf = &tree->leaves[i];
        Vector3 center = Vector3Scale(Vector3Add(leaf->bounds_min, leaf->bounds_max), 0.5f);
        
        const char *contents_str = (leaf->contents == CONTENTS_SOLID) ? "SOLID" : "EMPTY";
        const char *reach_str = leaf->is_reachable ? "reachable" : "unreachable";
        
        DBG_OUT("[BSP]   Leaf %d: center=(%.1f,%.1f,%.1f) %s %s, %d faces",
                i, center.x, center.y, center.z,
                contents_str, reach_str, leaf->face_count);
    }
}

#define OUTSIDE_EPSILON 1.0f  /* How far outside map bounds = exterior */

/* Calculate the bounding box of all brushes */
static void
CalculateMapBounds(const CompiledFace *all_faces, int face_count,
                   const Vector3 *vertices, Vector3 *min_out, Vector3 *max_out)
{
    if (face_count == 0) {
        *min_out = (Vector3){0, 0, 0};
        *max_out = (Vector3){0, 0, 0};
        return;
    }
    
    Vector3 min = {FLT_MAX, FLT_MAX, FLT_MAX};
    Vector3 max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    
    /* Find bounds of all vertices */
    for (int i = 0; i < face_count; i++) {
        const CompiledFace *face = &all_faces[i];
        for (int v = 0; v < face->vertex_count; v++) {
            Vector3 vert = vertices[face->vertex_start + v];
            
            if (vert.x < min.x) min.x = vert.x;
            if (vert.y < min.y) min.y = vert.y;
            if (vert.z < min.z) min.z = vert.z;
            
            if (vert.x > max.x) max.x = vert.x;
            if (vert.y > max.y) max.y = vert.y;
            if (vert.z > max.z) max.z = vert.z;
        }
    }
    
    *min_out = min;
    *max_out = max;
}

/* Check if a point is significantly outside the map bounds */
static bool
IsPointOutsideMap(Vector3 point, Vector3 map_min, Vector3 map_max)
{
    /* Add epsilon to account for leaves that are just barely outside */
    Vector3 min = Vector3Subtract(map_min, (Vector3){OUTSIDE_EPSILON, OUTSIDE_EPSILON, OUTSIDE_EPSILON});
    Vector3 max = Vector3Add(map_max, (Vector3){OUTSIDE_EPSILON, OUTSIDE_EPSILON, OUTSIDE_EPSILON});
    
    return (point.x < min.x || point.x > max.x ||
            point.y < min.y || point.y > max.y ||
            point.z < min.z || point.z > max.z);
}

/* NEW: Mark exterior void leaves as SOLID before flood-fill */
static void
MarkExteriorVoid(BSPTree *tree, const CompiledFace *all_faces, 
                 int face_count, const Vector3 *vertices)
{
    Vector3 map_min, map_max;
    CalculateMapBounds(all_faces, face_count, vertices, &map_min, &map_max);
    
    DBG_OUT("[BSP] Map bounds: (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)",
            map_min.x, map_min.y, map_min.z,
            map_max.x, map_max.y, map_max.z);
    
    int exterior_count = 0;
    
    /* Mark EMPTY leaves outside map bounds as SOLID (exterior void) */
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        /* Only check EMPTY leaves */
        if (leaf->contents != CONTENTS_EMPTY)
            continue;
        
        /* Test leaf center */
        Vector3 center = Vector3Scale(Vector3Add(leaf->bounds_min, leaf->bounds_max), 0.5f);
        
        if (IsPointOutsideMap(center, map_min, map_max)) {
            leaf->contents = CONTENTS_SOLID;  /* Mark as exterior void */
            exterior_count++;
        }
    }
    
    DBG_OUT("[BSP] Marked %d exterior void leaves as SOLID", exterior_count);
}

/* Helper to collect all leaf indices from a subtree */
static void
CollectLeaves(BSPTree *tree, int node_idx, bool is_leaf,
              int *leaf_array, int *count, int max_count)
{
	if (*count >= max_count)
		return;
	
	if (is_leaf) {
		leaf_array[(*count)++] = node_idx;
		return;
	}
	
	const BSPNode *node = &tree->nodes[node_idx];
	CollectLeaves(tree, node->front_child, node->front_is_leaf, leaf_array, count, max_count);
	CollectLeaves(tree, node->back_child, node->back_is_leaf, leaf_array, count, max_count);
}

/* Test if a point is inside a convex brush (defined by planes) */
static bool
IsPointInsideBrush(Vector3 point, const CompiledFace *faces, int face_count,
                   const Vector3 *vertices)
{
    /* A point is inside a convex brush if it's on the BACK side of ALL faces */
    for (int i = 0; i < face_count; i++) {
        const CompiledFace *face = &faces[i];
        
        /* Faces should point OUTWARD from the brush
         * So point is inside if it's BEHIND (negative side) of the plane */
        float dist = Vector3DotProduct(point, face->normal) - face->plane_dist;
        
        /* If point is in front of ANY face, it's outside this brush */
        if (dist > BSP_EPSILON) {
            return false;
        }
    }
    
    /* Point is behind all faces - it's inside the brush */
    return true;
}

/* Test if a leaf's center point is inside ANY brush */
static bool
IsLeafInsideSolid(const BSPLeaf *leaf, const CompiledFace *all_faces,
                  int total_face_count, const Vector3 *vertices,
                  const CompiledBrush *brushes, int brush_count)
{
    /* Get the center point of this leaf */
    Vector3 center = Vector3Scale(
        Vector3Add(leaf->bounds_min, leaf->bounds_max), 0.5f);
    
    /* Check if this point is inside any brush */
    for (int b = 0; b < brush_count; b++) {
        const CompiledBrush *brush = &brushes[b];
        const CompiledFace *brush_faces = &all_faces[brush->face_start];
        
        if (IsPointInsideBrush(center, brush_faces, brush->face_count, vertices)) {
            return true;  /* Inside at least one brush = SOLID */
        }
    }
    
    return false;  /* Not inside any brush = EMPTY (air) */
}

static FaceList *
FaceList_Create(void)
{
	FaceList *list = malloc(sizeof(FaceList));
	if (!list)
		return NULL;
	
	memset(list, 0, sizeof(FaceList));
	list->face_capacity = 64;
	list->face_count = 0;
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
		if (new_capacity < 16)
			new_capacity = 16;
		
		BSPFace **new_faces = realloc(list->faces, new_capacity * sizeof(BSPFace *));
		if (!new_faces) {
			fprintf(stderr, "[BSP ERROR] Failed to reallocate face list\n");
			return;
		}
		list->faces = new_faces;
		list->face_capacity = new_capacity;
	}
	
	if (list->face_count < list->face_capacity) {
		list->faces[list->face_count++] = face;
	}
}

static void
FaceList_Free(FaceList *list)
{
	if (!list)
		return;
	if (list->faces) {
		free(list->faces);
		list->faces = NULL;
	}
	free(list);
}

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
   GEOMETRY OPERATIONS
   ======================================================================== */

PlaneSide
BSP_ClassifyPoint(Vector3 point, Vector3 plane_normal, float plane_dist)
{
	float dist = Vector3DotProduct(point, plane_normal) - plane_dist;
	if (dist > BSP_EPSILON)
		return SIDE_FRONT;
	if (dist < -BSP_EPSILON)
		return SIDE_BACK;
	return SIDE_ON;
}

PlaneSide
BSP_ClassifyPolygon(const Vector3 *verts, int vert_count,
    Vector3 plane_normal, float plane_dist)
{
	int front = 0, back = 0;
	for (int i = 0; i < vert_count; i++) {
		PlaneSide side = BSP_ClassifyPoint(verts[i], plane_normal, plane_dist);
		if (side == SIDE_FRONT)
			front++;
		else if (side == SIDE_BACK)
			back++;
	}
	if (front > 0 && back > 0)
		return SIDE_SPLIT;
	if (front > 0)
		return SIDE_FRONT;
	if (back > 0)
		return SIDE_BACK;
	return SIDE_ON;
}

static void
SplitFace(const BSPFace *face, Vector3 plane_normal, float plane_dist,
    BSPFace **front_out, BSPFace **back_out)
{
	*front_out = *back_out = NULL;
	
	Vector3 front_verts[MAX_SPLIT_VERTS];
	Vector3 back_verts[MAX_SPLIT_VERTS];
	int front_count = 0, back_count = 0;

	for (int i = 0; i < face->vertex_count; i++) {
		Vector3 cur = face->vertices[i];
		Vector3 next = face->vertices[(i + 1) % face->vertex_count];
		float d_cur = Vector3DotProduct(cur, plane_normal) - plane_dist;
		float d_next = Vector3DotProduct(next, plane_normal) - plane_dist;
		
		bool cur_front = d_cur > BSP_EPSILON;
		bool cur_back = d_cur < -BSP_EPSILON;
		bool next_front = d_next > BSP_EPSILON;
		bool next_back = d_next < -BSP_EPSILON;

		if (cur_front) {
			if (front_count < MAX_SPLIT_VERTS - 1)
				front_verts[front_count++] = cur;
		} else if (cur_back) {
			if (back_count < MAX_SPLIT_VERTS - 1)
				back_verts[back_count++] = cur;
		} else {
			if (front_count < MAX_SPLIT_VERTS - 1)
				front_verts[front_count++] = cur;
			if (back_count < MAX_SPLIT_VERTS - 1)
				back_verts[back_count++] = cur;
		}

		if ((cur_front && next_back) || (cur_back && next_front)) {
			if (front_count < MAX_SPLIT_VERTS - 1 && back_count < MAX_SPLIT_VERTS - 1) {
				float t = d_cur / (d_cur - d_next);
				t = fmaxf(0.0f, fminf(1.0f, t));
				Vector3 intersection = Vector3Add(cur,
				    Vector3Scale(Vector3Subtract(next, cur), t));
				front_verts[front_count++] = intersection;
				back_verts[back_count++] = intersection;
			}
		}
	}

	if (front_count >= 3) {
		*front_out = BSPFace_Create(front_count);
		if (*front_out) {
			memcpy((*front_out)->vertices, front_verts, front_count * sizeof(Vector3));
			(*front_out)->normal = face->normal;
			(*front_out)->plane_dist = face->plane_dist;
			(*front_out)->original_face_idx = face->original_face_idx;
		}
	}
	if (back_count >= 3) {
		*back_out = BSPFace_Create(back_count);
		if (*back_out) {
			memcpy((*back_out)->vertices, back_verts, back_count * sizeof(Vector3));
			(*back_out)->normal = face->normal;
			(*back_out)->plane_dist = face->plane_dist;
			(*back_out)->original_face_idx = face->original_face_idx;
		}
	}
}

/* ========================================================================
   BSP TREE CONSTRUCTION
   ======================================================================== */

static bool
AreCoplanar(Vector3 n1, float d1, Vector3 n2, float d2)
{
	float dot = Vector3DotProduct(n1, n2);
	if (fabsf(dot - 1.0f) > 0.01f && fabsf(dot + 1.0f) > 0.01f)
		return false;
	
	float dist_diff = fabsf(d1 - d2);
	if (dot < 0)
		dist_diff = fabsf(d1 + d2);
	
	return dist_diff < BSP_EPSILON;
}

static int
SelectSplittingPlane(FaceList *faces)
{
	if (faces->face_count == 0)
		return -1;
	
	/* Check if all faces are coplanar - if so, can't split */
	if (faces->face_count > 1) {
		BSPFace *first = faces->faces[0];
		bool all_coplanar = true;
		
		for (int i = 1; i < faces->face_count; i++) {
			BSPFace *face = faces->faces[i];
			if (!face || face->vertex_count < 3)
				continue;
			
			if (!AreCoplanar(first->normal, first->plane_dist, 
			                 face->normal, face->plane_dist)) {
				all_coplanar = false;
				break;
			}
		}
		
		if (all_coplanar) {
			return -1;
		}
	}
	
	int best_index = 0;
	int best_score = INT32_MAX;

	for (int i = 0; i < faces->face_count; i++) {
		BSPFace *candidate = faces->faces[i];
		if (!candidate || candidate->vertex_count < 3)
			continue;
			
		int front = 0, back = 0, splits = 0, coplanar = 0;
		
		for (int j = 0; j < faces->face_count; j++) {
			if (i == j || !faces->faces[j])
				continue;
			
			/* Skip coplanar faces in scoring */
			if (AreCoplanar(candidate->normal, candidate->plane_dist,
			                faces->faces[j]->normal, faces->faces[j]->plane_dist)) {
				coplanar++;
				continue;
			}
			
			PlaneSide side = BSP_ClassifyPolygon(
			    faces->faces[j]->vertices,
			    faces->faces[j]->vertex_count,
			    candidate->normal,
			    candidate->plane_dist);
			
			if (side == SIDE_FRONT)
				front++;
			else if (side == SIDE_BACK)
				back++;
			else if (side == SIDE_SPLIT)
				splits++;
		}
		
		/* If this plane doesn't partition anything (all coplanar or all one side), skip */
		if (front == 0 || back == 0)
			continue;
		
		int balance = abs(front - back);
		int score = splits * 8 + balance;
		
		if (score < best_score) {
			best_score = score;
			best_index = i;
		}
	}
	
	/* Final validation */
	BSPFace *best = faces->faces[best_index];
	int front = 0, back = 0;
	for (int j = 0; j < faces->face_count; j++) {
		if (best_index == j || !faces->faces[j])
			continue;
		
		if (AreCoplanar(best->normal, best->plane_dist,
		                faces->faces[j]->normal, faces->faces[j]->plane_dist))
			continue;
		
		PlaneSide side = BSP_ClassifyPolygon(
		    faces->faces[j]->vertices,
		    faces->faces[j]->vertex_count,
		    best->normal,
		    best->plane_dist);
		
		if (side == SIDE_FRONT || side == SIDE_SPLIT)
			front++;
		if (side == SIDE_BACK || side == SIDE_SPLIT)
			back++;
	}
	
	if (front == 0 || back == 0)
		return -1;
	
	return best_index;
}

static int AllocNode(BSPTree *tree);
static int AllocLeaf(BSPTree *tree);

static int
BuildTree_Recursive(BSPTree *tree, FaceList *face_list, int depth, bool *is_leaf)
{
	if (depth > tree->max_tree_depth)
		tree->max_tree_depth = depth;

	/* Base case: create leaf */
	if (face_list->face_count < MIN_FACES_FOR_SPLIT || depth >= MAX_TREE_DEPTH) {
		int leaf_idx = AllocLeaf(tree);
		if (leaf_idx < 0)
			return -1;
		
		BSPLeaf *leaf = &tree->leaves[leaf_idx];
		leaf->leaf_index = leaf_idx;
		leaf->face_count = 0;
		leaf->faces = NULL;
		leaf->bounds_min = (Vector3){FLT_MAX, FLT_MAX, FLT_MAX};
		leaf->bounds_max = (Vector3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
		
		/* Store faces and calculate bounds */
		for (int i = 0; i < face_list->face_count; i++) {
			BSPFace *face = face_list->faces[i];
			if (!face)
				continue;
			face->next = leaf->faces;
			leaf->faces = face;
			leaf->face_count++;
			
			for (int v = 0; v < face->vertex_count; v++) {
				Vector3 vec = face->vertices[v];
				leaf->bounds_min.x = fminf(leaf->bounds_min.x, vec.x);
				leaf->bounds_min.y = fminf(leaf->bounds_min.y, vec.y);
				leaf->bounds_min.z = fminf(leaf->bounds_min.z, vec.z);
				leaf->bounds_max.x = fmaxf(leaf->bounds_max.x, vec.x);
				leaf->bounds_max.y = fmaxf(leaf->bounds_max.y, vec.y);
				leaf->bounds_max.z = fmaxf(leaf->bounds_max.z, vec.z);
			}
		}
		
		/* Default bounds for empty leaves */
		if (leaf->face_count == 0) {
			leaf->bounds_min = (Vector3){-16384, -16384, -16384};
			leaf->bounds_max = (Vector3){16384, 16384, 16384};
		}
		
		/* CRITICAL: In QBSP, leaves start as EMPTY, not SOLID!
		 * Only leaves that are fully enclosed by brush geometry become SOLID.
		 * The outside void is also SOLID, but reachable interior is EMPTY. */
		//leaf->contents = CONTENTS_EMPTY;
		
		*is_leaf = true;
		return leaf_idx;
	}

	/* Select splitting plane */
	int split_idx = SelectSplittingPlane(face_list);
	if (split_idx < 0) {
		/* Can't split - force to leaf */
		return BuildTree_Recursive(tree, face_list, MAX_TREE_DEPTH, is_leaf);
	}
	
	BSPFace *splitter = face_list->faces[split_idx];
	if (!splitter) {
		return BuildTree_Recursive(tree, face_list, MAX_TREE_DEPTH, is_leaf);
	}
	
	int node_idx = AllocNode(tree);
	if (node_idx < 0)
		return -1;
	
	Vector3 split_normal = splitter->normal;
	float split_dist = splitter->plane_dist;
	
	tree->nodes[node_idx].plane_normal = split_normal;
	tree->nodes[node_idx].plane_dist = split_dist;

	FaceList *front_list = FaceList_Create();
	FaceList *back_list = FaceList_Create();
	if (!front_list || !back_list) {
		FaceList_Free(front_list);
		FaceList_Free(back_list);
		return -1;
	}

	bool *should_free_face = calloc(face_list->face_count, sizeof(bool));
	if (!should_free_face) {
		FaceList_Free(front_list);
		FaceList_Free(back_list);
		return -1;
	}

	/* Partition faces - this is the CRITICAL part for QBSP behavior */
	for (int i = 0; i < face_list->face_count; i++) {
		BSPFace *face = face_list->faces[i];
		if (!face) {
			should_free_face[i] = false;
			continue;
		}
		
		/* In QBSP: coplanar faces go to the FRONT child (facing same direction)
		 * or BACK child (facing opposite direction), NOT both sides! */
		if (AreCoplanar(split_normal, split_dist, face->normal, face->plane_dist)) {
			float dot = Vector3DotProduct(split_normal, face->normal);
			if (dot > 0) {
				/* Facing same direction as splitter -> front */
				FaceList_Add(front_list, face);
			} else {
				/* Facing opposite direction -> back */
				FaceList_Add(back_list, face);
			}
			should_free_face[i] = false;
			continue;
		}
		
		PlaneSide side = BSP_ClassifyPolygon(face->vertices, face->vertex_count,
		    split_normal, split_dist);
		
		switch (side) {
		case SIDE_FRONT:
			FaceList_Add(front_list, face);
			should_free_face[i] = false;
			break;
		case SIDE_BACK:
			FaceList_Add(back_list, face);
			should_free_face[i] = false;
			break;
		case SIDE_ON:
			/* This shouldn't happen if coplanar check worked, but handle it */
			{
				float dot = Vector3DotProduct(split_normal, face->normal);
				if (dot > 0) {
					FaceList_Add(front_list, face);
				} else {
					FaceList_Add(back_list, face);
				}
				should_free_face[i] = false;
			}
			break;
		case SIDE_SPLIT: {
			BSPFace *front_piece, *back_piece;
			SplitFace(face, split_normal, split_dist,
			    &front_piece, &back_piece);
			if (front_piece)
				FaceList_Add(front_list, front_piece);
			if (back_piece)
				FaceList_Add(back_list, back_piece);
			should_free_face[i] = true;
			break;
		}
		}
	}

	/* Free original faces that were split */
	for (int i = 0; i < face_list->face_count; i++) {
		if (should_free_face[i])
			BSPFace_Free(face_list->faces[i]);
	}
	free(should_free_face);

	bool front_is_leaf_result = false;
	bool back_is_leaf_result = false;
	
	int front_child = BuildTree_Recursive(tree, front_list, depth + 1, &front_is_leaf_result);
	int back_child = BuildTree_Recursive(tree, back_list, depth + 1, &back_is_leaf_result);
	
	tree->nodes[node_idx].front_child = front_child;
	tree->nodes[node_idx].back_child = back_child;
	tree->nodes[node_idx].front_is_leaf = front_is_leaf_result;
	tree->nodes[node_idx].back_is_leaf = back_is_leaf_result;

	FaceList_Free(front_list);
	FaceList_Free(back_list);
	
	*is_leaf = false;
	return node_idx;
}

static int
AllocNode(BSPTree *tree)
{
	if (tree->node_count >= tree->node_capacity) {
		int new_capacity = tree->node_capacity * 2;
		BSPNode *new_nodes = realloc(tree->nodes, new_capacity * sizeof(BSPNode));
		if (!new_nodes)
			return -1;
		tree->nodes = new_nodes;
		tree->node_capacity = new_capacity;
	}
	memset(&tree->nodes[tree->node_count], 0, sizeof(BSPNode));
	return tree->node_count++;
}

static int
AllocLeaf(BSPTree *tree)
{
	if (tree->leaf_count >= tree->leaf_capacity) {
		int new_capacity = tree->leaf_capacity * 2;
		BSPLeaf *new_leaves = realloc(tree->leaves, new_capacity * sizeof(BSPLeaf));
		if (!new_leaves)
			return -1;
		tree->leaves = new_leaves;
		tree->leaf_capacity = new_capacity;
	}
	memset(&tree->leaves[tree->leaf_count], 0, sizeof(BSPLeaf));
	return tree->leaf_count++;
}

/* ========================================================================
   PORTAL GENERATION - PROPER QBSP ALGORITHM
   ======================================================================== */

static Portal *CreateInitialPortal(Vector3 normal, float dist);
static Portal *ClipPortal(const Portal *portal, Vector3 plane_normal, float plane_dist, bool keep_front);
static void ClipPortalToLeaves(BSPTree *tree, int node_idx, bool is_leaf, Portal *portal, int target_leaf, bool target_is_front);

/* Recursively generate portals by clipping through the BSP tree */
static void
MakeNodePortals_Recursive(BSPTree *tree, int node_idx, Portal *portal)
{
	const BSPNode *node = &tree->nodes[node_idx];
	
	if (!portal)
		return;
	
	/* Clip the portal to the front and back sides of this node's plane */
	Portal *front_portal = ClipPortal(portal, node->plane_normal, node->plane_dist, true);
	Portal *back_portal = ClipPortal(portal, node->plane_normal, node->plane_dist, false);
	
	/* Process front child */
	if (node->front_is_leaf) {
		/* Front is a leaf - this portal connects to it */
		if (front_portal) {
			front_portal->leaf_front = node->front_child;
			/* We'll set leaf_back when we process the back child */
		}
	} else {
		/* Front is a node - recurse with clipped portal */
		if (front_portal) {
			MakeNodePortals_Recursive(tree, node->front_child, front_portal);
			Portal_Free(front_portal);
			front_portal = NULL;
		}
	}
	
	/* Process back child */
	if (node->back_is_leaf) {
		/* Back is a leaf - this portal connects to it */
		if (back_portal) {
			back_portal->leaf_back = node->back_child;
			/* If we also have a front leaf, finalize the portal */
			if (node->front_is_leaf && front_portal) {
				front_portal->leaf_back = node->back_child;
				front_portal->next = g_portals;
				g_portals = front_portal;
			}
			Portal_Free(back_portal);
		}
	} else {
		/* Back is a node - recurse with clipped portal */
		if (back_portal) {
			MakeNodePortals_Recursive(tree, node->back_child, back_portal);
			Portal_Free(back_portal);
		}
	}
}

/* Generate all portals for the tree */
static void
MakeTreePortals(BSPTree *tree)
{
	if (tree->root_is_leaf)
		return;
	
	/* For each node, create a portal at its splitting plane */
	for (int i = 0; i < tree->node_count; i++) {
		const BSPNode *node = &tree->nodes[i];
		
		/* Create a huge portal on this node's plane */
		Portal *portal = CreateInitialPortal(node->plane_normal, node->plane_dist);
		if (!portal)
			continue;
		
		/* If both children are leaves, create direct portal */
		if (node->front_is_leaf && node->back_is_leaf) {
			portal->leaf_front = node->front_child;
			portal->leaf_back = node->back_child;
			portal->next = g_portals;
			g_portals = portal;
		}
		/* If one child is a leaf and other is a node, clip through the node's subtree */
		else if (node->front_is_leaf) {
			portal->leaf_front = node->front_child;
			/* Clip portal through back subtree to find all back leaves */
			ClipPortalToLeaves(tree, node->back_child, false, portal, portal->leaf_front, true);
			Portal_Free(portal);
		}
		else if (node->back_is_leaf) {
			portal->leaf_back = node->back_child;
			/* Clip portal through front subtree to find all front leaves */
			ClipPortalToLeaves(tree, node->front_child, false, portal, portal->leaf_back, false);
			Portal_Free(portal);
		}
		else {
			/* Both children are nodes - collect leaves from both sides and connect them */
			int front_leaves[512];
			int front_count = 0;
			CollectLeaves(tree, node->front_child, false, front_leaves, &front_count, 512);
			
			int back_leaves[512];
			int back_count = 0;
			CollectLeaves(tree, node->back_child, false, back_leaves, &back_count, 512);
			
			/* Create portals between all front/back leaf pairs */
			for (int f = 0; f < front_count; f++) {
				for (int b = 0; b < back_count; b++) {
					Portal *leaf_portal = Portal_Create(portal->vertex_count);
					if (leaf_portal) {
						memcpy(leaf_portal->vertices, portal->vertices, 
						       portal->vertex_count * sizeof(Vector3));
						leaf_portal->vertex_count = portal->vertex_count;
						leaf_portal->normal = portal->normal;
						leaf_portal->plane_dist = portal->plane_dist;
						leaf_portal->leaf_front = front_leaves[f];
						leaf_portal->leaf_back = back_leaves[b];
						leaf_portal->next = g_portals;
						g_portals = leaf_portal;
					}
				}
			}
			Portal_Free(portal);
		}
	}
}

/* Helper: clip a portal through a subtree and create portals to a target leaf */
static void
ClipPortalToLeaves(BSPTree *tree, int node_idx, bool is_leaf, Portal *portal, int target_leaf, bool target_is_front)
{
	if (is_leaf) {
		/* Reached a leaf - create portal between target and this leaf */
		Portal *new_portal = Portal_Create(portal->vertex_count);
		if (new_portal) {
			memcpy(new_portal->vertices, portal->vertices, portal->vertex_count * sizeof(Vector3));
			new_portal->vertex_count = portal->vertex_count;
			new_portal->normal = portal->normal;
			new_portal->plane_dist = portal->plane_dist;
			
			if (target_is_front) {
				new_portal->leaf_front = target_leaf;
				new_portal->leaf_back = node_idx;
			} else {
				new_portal->leaf_front = node_idx;
				new_portal->leaf_back = target_leaf;
			}
			
			new_portal->next = g_portals;
			g_portals = new_portal;
		}
		return;
	}
	
	const BSPNode *node = &tree->nodes[node_idx];
	
	/* Clip portal to front and back of this node */
	Portal *front = ClipPortal(portal, node->plane_normal, node->plane_dist, true);
	Portal *back = ClipPortal(portal, node->plane_normal, node->plane_dist, false);
	
	/* Recurse to children with clipped portals */
	if (front) {
		ClipPortalToLeaves(tree, node->front_child, node->front_is_leaf, front, target_leaf, target_is_front);
		Portal_Free(front);
	}
	if (back) {
		ClipPortalToLeaves(tree, node->back_child, node->back_is_leaf, back, target_leaf, target_is_front);
		Portal_Free(back);
	}
}

/* Forward declaration */
static void CreatePortalsToLeaf(BSPTree *tree, int node_idx, bool is_leaf, int target_leaf, bool target_is_front);

static void
CreatePortalsFromNodeToLeaf(BSPTree *tree, int node_idx, bool is_leaf, 
                             int target_leaf, Vector3 split_normal, float split_dist)
{
	if (is_leaf) {
		/* Create portal between this leaf and target */
		Portal *portal = malloc(sizeof(Portal));
		if (portal) {
			portal->vertices = NULL;
			portal->vertex_count = 0;
			portal->normal = split_normal;
			portal->plane_dist = split_dist;
			portal->leaf_front = target_leaf;
			portal->leaf_back = node_idx;
			portal->next = g_portals;
			g_portals = portal;
		}
		return;
	}
	
	const BSPNode *node = &tree->nodes[node_idx];
	CreatePortalsFromNodeToLeaf(tree, node->front_child, node->front_is_leaf,
	                              target_leaf, split_normal, split_dist);
	CreatePortalsFromNodeToLeaf(tree, node->back_child, node->back_is_leaf,
	                              target_leaf, split_normal, split_dist);
}


static void
MakeTreePortals_Recursive_Fixed(BSPTree *tree, int node_idx)
{
	const BSPNode *node = &tree->nodes[node_idx];
	
	/* Create portals at this node's splitting plane */
	
	/* Case 1: Both children are leaves - direct portal */
	if (node->front_is_leaf && node->back_is_leaf) {
		Portal *portal = malloc(sizeof(Portal));
		if (portal) {
			portal->vertices = NULL;
			portal->vertex_count = 0;
			portal->normal = node->plane_normal;
			portal->plane_dist = node->plane_dist;
			portal->leaf_front = node->front_child;
			portal->leaf_back = node->back_child;
			portal->next = g_portals;
			g_portals = portal;
		}
	}
	/* Case 2: Front is leaf, back is node - connect front leaf to all back leaves */
	else if (node->front_is_leaf && !node->back_is_leaf) {
		CreatePortalsFromNodeToLeaf(tree, node->back_child, false,
		                              node->front_child,
		                              node->plane_normal, node->plane_dist);
	}
	/* Case 3: Back is leaf, front is node - connect back leaf to all front leaves */
	else if (node->back_is_leaf && !node->front_is_leaf) {
		CreatePortalsFromNodeToLeaf(tree, node->front_child, false,
		                              node->back_child,
		                              node->plane_normal, node->plane_dist);
	}
	/* Case 4: Both are nodes - create portals between all leaf pairs */
	else if (!node->front_is_leaf && !node->back_is_leaf) {
		/* This is the tricky case - we need to connect leaves from front subtree
		 * to leaves in back subtree. For a simplified version, we can recursively
		 * handle this by collecting all leaves from each side. */
		
		/* Collect all leaves from front subtree */
		int front_leaves[512];  // Adjust size as needed
		int front_count = 0;
		CollectLeaves(tree, node->front_child, false, front_leaves, &front_count, 512);
		
		/* Collect all leaves from back subtree */
		int back_leaves[512];
		int back_count = 0;
		CollectLeaves(tree, node->back_child, false, back_leaves, &back_count, 512);
		
		/* Create portals between all pairs */
		for (int i = 0; i < front_count; i++) {
			for (int j = 0; j < back_count; j++) {
				Portal *portal = malloc(sizeof(Portal));
				if (portal) {
					portal->vertices = NULL;
					portal->vertex_count = 0;
					portal->normal = node->plane_normal;
					portal->plane_dist = node->plane_dist;
					portal->leaf_front = front_leaves[i];
					portal->leaf_back = back_leaves[j];
					portal->next = g_portals;
					g_portals = portal;
				}
			}
		}
	}
	
	/* Recurse to children */
	if (!node->front_is_leaf)
		MakeTreePortals_Recursive_Fixed(tree, node->front_child);
	if (!node->back_is_leaf)
		MakeTreePortals_Recursive_Fixed(tree, node->back_child);
}

/* Simple portal generation: create a portal wherever a node has at least one leaf child.
 * This ensures every leaf has portal connections to its neighbors. */
static void
MakeTreePortals_Recursive(BSPTree *tree, int node_idx, bool is_leaf)
{
	if (is_leaf)
		return;
	
	const BSPNode *node = &tree->nodes[node_idx];
	
	/* If BOTH children are leaves, create a single portal between them */
	if (node->front_is_leaf && node->back_is_leaf) {
		Portal *portal = malloc(sizeof(Portal));
		if (portal) {
			portal->vertices = NULL;  /* Don't need geometry for flood-fill */
			portal->vertex_count = 0;
			portal->normal = node->plane_normal;
			portal->plane_dist = node->plane_dist;
			portal->leaf_front = node->front_child;
			portal->leaf_back = node->back_child;
			portal->next = g_portals;
			g_portals = portal;
		}
	}
	/* If front is leaf but back is node, need to find all back's descendant leaves */
	else if (node->front_is_leaf && !node->back_is_leaf) {
		/* Front is a leaf - create portals to all leaves in back subtree */
		int front_leaf = node->front_child;
		/* Recursively find all leaves in back subtree and create portals */
		CreatePortalsToLeaf(tree, node->back_child, false, front_leaf, true);
	}
	/* If back is leaf but front is node */
	else if (node->back_is_leaf && !node->front_is_leaf) {
		int back_leaf = node->back_child;
		CreatePortalsToLeaf(tree, node->front_child, false, back_leaf, false);
	}
	
	/* Recurse to children */
	if (!node->front_is_leaf)
		MakeTreePortals_Recursive(tree, node->front_child, false);
	if (!node->back_is_leaf)
		MakeTreePortals_Recursive(tree, node->back_child, false);
}

/* Helper: find all leaves in subtree and create portals to target_leaf */
static void
CreatePortalsToLeaf(BSPTree *tree, int node_idx, bool is_leaf, int target_leaf, bool target_is_front)
{
	if (is_leaf) {
		/* Create portal between this leaf and target */
		Portal *portal = malloc(sizeof(Portal));
		if (portal) {
			portal->vertices = NULL;
			portal->vertex_count = 0;
			portal->normal = (Vector3){0, 0, 0};  /* Don't need for flood-fill */
			portal->plane_dist = 0;
			
			if (target_is_front) {
				portal->leaf_front = target_leaf;
				portal->leaf_back = node_idx;
			} else {
				portal->leaf_front = node_idx;
				portal->leaf_back = target_leaf;
			}
			
			portal->next = g_portals;
			g_portals = portal;
		}
		return;
	}
	
	const BSPNode *node = &tree->nodes[node_idx];
	CreatePortalsToLeaf(tree, node->front_child, node->front_is_leaf, target_leaf, target_is_front);
	CreatePortalsToLeaf(tree, node->back_child, node->back_is_leaf, target_leaf, target_is_front);
}

static Portal *
CreateInitialPortal(Vector3 normal, float dist)
{
	Vector3 abs_normal = {fabsf(normal.x), fabsf(normal.y), fabsf(normal.z)};
	Vector3 up;
	if (abs_normal.x > abs_normal.y && abs_normal.x > abs_normal.z)
		up = (Vector3){0, 0, 1};
	else if (abs_normal.y > abs_normal.z)
		up = (Vector3){0, 0, 1};
	else
		up = (Vector3){0, 1, 0};
	
	Vector3 right = Vector3Normalize(Vector3CrossProduct(up, normal));
	up = Vector3CrossProduct(normal, right);
	
	Vector3 org = Vector3Scale(normal, dist);
	
	const float HUGE_SIZE = 16384.0f;
	Portal *portal = Portal_Create(4);
	if (!portal)
		return NULL;
	
	portal->vertices[0] = Vector3Add(org, Vector3Add(
	    Vector3Scale(right, -HUGE_SIZE), Vector3Scale(up, -HUGE_SIZE)));
	portal->vertices[1] = Vector3Add(org, Vector3Add(
	    Vector3Scale(right, HUGE_SIZE), Vector3Scale(up, -HUGE_SIZE)));
	portal->vertices[2] = Vector3Add(org, Vector3Add(
	    Vector3Scale(right, HUGE_SIZE), Vector3Scale(up, HUGE_SIZE)));
	portal->vertices[3] = Vector3Add(org, Vector3Add(
	    Vector3Scale(right, -HUGE_SIZE), Vector3Scale(up, HUGE_SIZE)));
	
	portal->normal = normal;
	portal->plane_dist = dist;
	return portal;
}

static Portal *
ClipPortal(const Portal *portal, Vector3 plane_normal, float plane_dist, bool keep_front)
{
	Vector3 verts[MAX_PORTAL_VERTS];
	int count = 0;
	
	for (int i = 0; i < portal->vertex_count; i++) {
		Vector3 cur = portal->vertices[i];
		Vector3 next = portal->vertices[(i + 1) % portal->vertex_count];
		float d_cur = Vector3DotProduct(cur, plane_normal) - plane_dist;
		float d_next = Vector3DotProduct(next, plane_normal) - plane_dist;
		
		/* Keep vertices on the correct side */
		bool keep_cur = keep_front ? (d_cur >= -BSP_EPSILON) : (d_cur <= BSP_EPSILON);
		
		if (keep_cur) {
			if (count < MAX_PORTAL_VERTS)
				verts[count++] = cur;
		}
		
		/* Add intersection point if edge crosses plane */
		if ((d_cur > BSP_EPSILON && d_next < -BSP_EPSILON) ||
		    (d_cur < -BSP_EPSILON && d_next > BSP_EPSILON)) {
			if (count < MAX_PORTAL_VERTS) {
				float t = d_cur / (d_cur - d_next);
				t = fmaxf(0.0f, fminf(1.0f, t));
				verts[count++] = Vector3Add(cur,
				    Vector3Scale(Vector3Subtract(next, cur), t));
			}
		}
	}
	
	if (count < 3)
		return NULL;
	
	Portal *clipped = Portal_Create(count);
	if (!clipped)
		return NULL;
	memcpy(clipped->vertices, verts, count * sizeof(Vector3));
	clipped->normal = portal->normal;
	clipped->plane_dist = portal->plane_dist;
	return clipped;
}

/* ========================================================================
   FLOOD FILL - MARK SOLID LEAVES
   ======================================================================== */

/* In QBSP: leaves with faces are SOLID (inside brush geometry).
 * Leaves without faces are EMPTY (air/void).
 * Then flood-fill marks outside void as SOLID too. */
static void
MarkSolidLeaves(BSPTree *tree)
{
	int solid_count = 0, empty_count = 0;
	
	DBG_OUT("[BSP] Classifying leaves based on face content...");
	
	/* QBSP Algorithm:
	 * - Leaves WITH faces = SOLID (inside solid brushes)
	 * - Leaves WITHOUT faces = EMPTY (air/void space)
	 * 
	 * The outside void will be EMPTY, and reachable interior will be EMPTY.
	 * Unreachable interior (enclosed by brushes) stays EMPTY but won't be flood-filled.
	 */
	for (int i = 0; i < tree->leaf_count; i++) {
		BSPLeaf *leaf = &tree->leaves[i];
		
		if (leaf->face_count > 0) {
			/* This leaf has faces = it's inside brush geometry */
			leaf->contents = CONTENTS_SOLID;
			solid_count++;
		} else {
			/* This leaf has no faces = it's air/void */
			leaf->contents = CONTENTS_EMPTY;
			empty_count++;
		}
	}
	
	DBG_OUT("[BSP] Initial classification: %d SOLID (with faces), %d EMPTY (no faces)",
	        solid_count, empty_count);
}

static void
MarkSolidLeaves_Fixed(
    BSPTree             *tree, 
    const CompiledFace  *all_faces,
    int                  total_face_count,
    const Vector3       *vertices,
    const CompiledBrush *brushes, 
    int                  brush_count
)
{
    int solid_count = 0, empty_count = 0;
    
    DBG_OUT("[BSP] Classifying leaves based on inside/outside test...");
    DBG_OUT("[BSP] Testing %d brushes with %d total faces", brush_count, total_face_count);
    
    /* Debug: print first few brushes */
    for (int b = 0; b < 3 && b < brush_count; b++) {
        DBG_OUT("[BSP]   Brush %d: face_start=%d, face_count=%d", 
                b, brushes[b].face_start, brushes[b].face_count);
    }
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        Vector3 center = Vector3Scale(Vector3Add(leaf->bounds_min, leaf->bounds_max), 0.5f);
        
        bool inside = IsLeafInsideSolid(leaf, all_faces, total_face_count, vertices,
                                        brushes, brush_count);
        
        /* Debug first few leaves */
        if (i < 3) {
            DBG_OUT("[BSP]   Leaf %d center=(%.1f, %.1f, %.1f): %s",
                    i, center.x, center.y, center.z, inside ? "SOLID" : "EMPTY");
        }
        
        if (inside) {
            leaf->contents = CONTENTS_SOLID;
            solid_count++;
        } else {
            leaf->contents = CONTENTS_EMPTY;
            empty_count++;
        }
    }
    
    DBG_OUT("[BSP] Initial classification: %d SOLID (inside brushes), %d EMPTY (air)",
            solid_count, empty_count);
}

/* Flood-fill from player start to mark reachable (interior playable) leaves */
static void
FloodFillFromPlayer(BSPTree *tree, int player_leaf_idx)
{
	DBG_OUT("[BSP] Flood-filling from player start (leaf %d)...", player_leaf_idx);
	
	/* Debug: Count how many portals touch the player's starting leaf */
	int player_portal_count = 0;
	for (Portal *p = g_portals; p; p = p->next) {
		if (p->leaf_front == player_leaf_idx || p->leaf_back == player_leaf_idx) {
			player_portal_count++;
		}
	}
	DBG_OUT("[BSP] Player leaf has %d portals connected to it", player_portal_count);
	
	/* Initialize all leaves as unreachable */
	for (int i = 0; i < tree->leaf_count; i++) {
		tree->leaves[i].is_reachable = false;
	}
	
	/* Mark player's leaf as reachable */
	tree->leaves[player_leaf_idx].is_reachable = true;
	
	bool changed = true;
	int iterations = 0;
	
	while (changed && iterations < 1000) {
		changed = false;
		iterations++;
		int marked = 0;
		
		/* For each portal, propagate reachability through EMPTY leaves only */
		for (Portal *portal = g_portals; portal; portal = portal->next) {
			if (portal->leaf_front < 0 || portal->leaf_back < 0)
				continue;
			if (portal->leaf_front >= tree->leaf_count || portal->leaf_back >= tree->leaf_count)
				continue;
			
			BSPLeaf *front = &tree->leaves[portal->leaf_front];
			BSPLeaf *back = &tree->leaves[portal->leaf_back];
			
			/* Only propagate through EMPTY leaves (air) - don't go through SOLID (brushes) */
			if (front->contents == CONTENTS_EMPTY && back->contents == CONTENTS_EMPTY) {
				if (front->is_reachable && !back->is_reachable) {
					back->is_reachable = true;
					changed = true;
					marked++;
				} else if (back->is_reachable && !front->is_reachable) {
					front->is_reachable = true;
					changed = true;
					marked++;
				}
			}
		}
		
		if (marked > 0) {
			DBG_OUT("[BSP]   Iteration %d: marked %d more leaves as reachable", iterations, marked);
		}
	}
	
	/* Count results */
	int reachable_empty = 0, unreachable_empty = 0, solid = 0;
	for (int i = 0; i < tree->leaf_count; i++) {
		if (tree->leaves[i].contents == CONTENTS_SOLID) {
			solid++;
		} else if (tree->leaves[i].is_reachable) {
			reachable_empty++;
		} else {
			unreachable_empty++;
		}
	}
	
	DBG_OUT("[BSP] Flood-fill complete in %d iterations", iterations);
	DBG_OUT("[BSP] Final: %d reachable EMPTY (interior), %d unreachable EMPTY (outside void), %d SOLID (brushes)",
	        reachable_empty, unreachable_empty, solid);
}

/* ========================================================================
   FACE CULLING
   ======================================================================== */

static void
CullHiddenFaces_Correct(BSPTree *tree)
{
    int culled = 0;
    int kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Culling faces not visible from reachable interior...");
    
    /* We need to keep faces that are on the boundary between:
     * - Reachable EMPTY leaves (playable space)
     * - SOLID leaves (brush geometry)
     * 
     * The algorithm:
     * For each leaf with faces:
     *   - If leaf is reachable EMPTY: cull all faces (these are internal to playable space)
     *   - If leaf is unreachable EMPTY: cull all faces (outside the map)
     *   - If leaf is SOLID: keep faces ONLY if this leaf has a portal to a reachable EMPTY leaf
     */
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        bool keep_faces = false;
        
        if (leaf->contents == CONTENTS_SOLID) {
            /* This leaf is inside a brush - keep its faces only if they
             * border reachable playable space */
            for (Portal *p = g_portals; p; p = p->next) {
                int other_leaf_idx = -1;
                
                if (p->leaf_front == i)
                    other_leaf_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_leaf_idx = p->leaf_front;
                
                if (other_leaf_idx >= 0 && other_leaf_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_leaf_idx];
                    
                    /* Keep faces if neighboring leaf is reachable playable space */
                    if (other->contents == CONTENTS_EMPTY && other->is_reachable) {
                        keep_faces = true;
                        break;
                    }
                }
            }
        }
        /* else: EMPTY leaves (reachable or not) don't keep their faces -
         * we only want the boundary surfaces */
        
        /* Now process the faces in this leaf */
        if (keep_faces) {
            /* Count and keep all faces */
            BSPFace *face = leaf->faces;
            while (face) {
                tree->visible_faces++;
                kept++;
                face = face->next;
            }
        } else {
            /* Remove all faces from this leaf */
            BSPFace *face = leaf->faces;
            while (face) {
                BSPFace *next = face->next;
                culled++;
                BSPFace_Free(face);
                face = next;
            }
            leaf->faces = NULL;
            leaf->face_count = 0;
        }
    }
    
    DBG_OUT("[BSP] Kept %d visible faces, culled %d hidden faces", kept, culled);
}

static void
CullHiddenFaces_Fixed(BSPTree *tree)
{
	int culled = 0;
	int kept = 0;
	tree->visible_faces = 0;
	
	DBG_OUT("[BSP] Culling faces not on reachable EMPTY/SOLID boundaries...");
	
	for (int i = 0; i < tree->leaf_count; i++) {
		BSPLeaf *leaf = &tree->leaves[i];
		
		/* Only keep faces in SOLID leaves that border reachable EMPTY space */
		bool keep_faces = false;
		
		if (leaf->contents == CONTENTS_SOLID) {
			/* Check if this SOLID leaf has any portals to reachable EMPTY leaves */
			for (Portal *p = g_portals; p; p = p->next) {
				int other_leaf = -1;
				
				if (p->leaf_front == i)
					other_leaf = p->leaf_back;
				else if (p->leaf_back == i)
					other_leaf = p->leaf_front;
				
				if (other_leaf >= 0 && other_leaf < tree->leaf_count) {
					BSPLeaf *other = &tree->leaves[other_leaf];
					/* Keep if neighboring leaf is EMPTY and reachable (playable interior) */
					if (other->contents == CONTENTS_EMPTY && other->is_reachable) {
						keep_faces = true;
						break;
					}
				}
			}
		}
		/* EMPTY leaves (both reachable and unreachable) should not keep their faces */
		
		/* Process faces in this leaf */
		BSPFace *prev = NULL;
		BSPFace *face = leaf->faces;
		
		while (face) {
			BSPFace *next = face->next;
			
			if (keep_faces) {
				/* Keep this face */
				tree->visible_faces++;
				kept++;
				prev = face;
			} else {
				/* Cull this face */
				culled++;
				if (prev)
					prev->next = next;
				else
					leaf->faces = next;
				BSPFace_Free(face);
			}
			
			face = next;
		}
		
		if (!keep_faces) {
			leaf->faces = NULL;
			leaf->face_count = 0;
		}
	}
	
	DBG_OUT("[BSP] Kept %d faces, culled %d hidden faces", kept, culled);
}

static void
CullHiddenFaces_Conservative(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Culling with conservative visibility test...");
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        bool keep_faces = false;
        
        /* Strategy: Only keep faces that separate two specific leaf types */
        if (leaf->contents == CONTENTS_SOLID) {
            /* Keep if this SOLID leaf neighbors a reachable EMPTY leaf */
            for (Portal *p = g_portals; p; p = p->next) {
                int other_idx = -1;
                
                if (p->leaf_front == i)
                    other_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_idx = p->leaf_front;
                
                if (other_idx >= 0 && other_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_idx];
                    
                    /* Keep only if other is EMPTY and reachable (playable interior) */
                    if (other->contents == CONTENTS_EMPTY && other->is_reachable) {
                        keep_faces = true;
                        break;
                    }
                }
            }
        } 
        else if (leaf->contents == CONTENTS_EMPTY && leaf->is_reachable) {
            /* Alternative: Also check EMPTY leaves for faces that border SOLID */
            for (Portal *p = g_portals; p; p = p->next) {
                int other_idx = -1;
                
                if (p->leaf_front == i)
                    other_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_idx = p->leaf_front;
                
                if (other_idx >= 0 && other_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_idx];
                    
                    /* Keep if this reachable EMPTY borders a SOLID */
                    if (other->contents == CONTENTS_SOLID) {
                        keep_faces = true;
                        break;
                    }
                }
            }
        }
        
        /* Process faces based on decision */
        if (keep_faces) {
            BSPFace *face = leaf->faces;
            while (face) {
                tree->visible_faces++;
                kept++;
                face = face->next;
            }
        } else {
            BSPFace *face = leaf->faces;
            while (face) {
                BSPFace *next = face->next;
                culled++;
                BSPFace_Free(face);
                face = next;
            }
            leaf->faces = NULL;
            leaf->face_count = 0;
        }
    }
    
    DBG_OUT("[BSP] Conservative cull: kept %d, culled %d", kept, culled);
}

static void
CullHiddenFaces_Diagnostic(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    /* Counters for analysis */
    int solid_leaves_with_faces = 0;
    int solid_leaves_with_empty_portals = 0;
    int solid_leaves_no_empty_portals = 0;
    int empty_leaves_with_faces = 0;
    int faces_in_solid = 0;
    int faces_in_empty = 0;
    
    DBG_OUT("[BSP] === DIAGNOSTIC CULLING ===");
    
    /* First pass: count what we have */
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->face_count == 0)
            continue;
        
        if (leaf->contents == CONTENTS_SOLID) {
            solid_leaves_with_faces++;
            faces_in_solid += leaf->face_count;
            
            /* Check for EMPTY portals */
            bool has_empty_portal = false;
            for (Portal *p = g_portals; p; p = p->next) {
                int other_idx = -1;
                
                if (p->leaf_front == i)
                    other_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_idx = p->leaf_front;
                
                if (other_idx >= 0 && other_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_idx];
                    if (other->contents == CONTENTS_EMPTY && other->is_reachable) {
                        has_empty_portal = true;
                        break;
                    }
                }
            }
            
            if (has_empty_portal)
                solid_leaves_with_empty_portals++;
            else
                solid_leaves_no_empty_portals++;
                
        } else if (leaf->contents == CONTENTS_EMPTY) {
            empty_leaves_with_faces++;
            faces_in_empty += leaf->face_count;
        }
    }
    
    DBG_OUT("[BSP] Analysis before culling:");
    DBG_OUT("[BSP]   SOLID leaves with faces: %d", solid_leaves_with_faces);
    DBG_OUT("[BSP]     - with EMPTY portals: %d", solid_leaves_with_empty_portals);
    DBG_OUT("[BSP]     - without EMPTY portals: %d", solid_leaves_no_empty_portals);
    DBG_OUT("[BSP]   EMPTY leaves with faces: %d", empty_leaves_with_faces);
    DBG_OUT("[BSP]   Total faces in SOLID leaves: %d", faces_in_solid);
    DBG_OUT("[BSP]   Total faces in EMPTY leaves: %d", faces_in_empty);
    DBG_OUT("[BSP]   Grand total: %d faces", faces_in_solid + faces_in_empty);
    
    /* Second pass: actually cull */
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        bool keep_faces = false;
        
        /* Only keep faces in SOLID leaves with EMPTY portals */
        if (leaf->contents == CONTENTS_SOLID) {
            for (Portal *p = g_portals; p; p = p->next) {
                int other_idx = -1;
                
                if (p->leaf_front == i)
                    other_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_idx = p->leaf_front;
                
                if (other_idx >= 0 && other_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_idx];
                    if (other->contents == CONTENTS_EMPTY && other->is_reachable) {
                        keep_faces = true;
                        break;
                    }
                }
            }
        }
        /* EMPTY leaves never keep faces */
        
        /* Process faces */
        BSPFace *prev = NULL;
        BSPFace *face = leaf->faces;
        
        while (face) {
            BSPFace *next = face->next;
            
            if (keep_faces) {
                tree->visible_faces++;
                kept++;
                prev = face;
            } else {
                culled++;
                if (prev)
                    prev->next = next;
                else
                    leaf->faces = next;
                BSPFace_Free(face);
            }
            
            face = next;
        }
        
        if (!keep_faces) {
            leaf->faces = NULL;
            leaf->face_count = 0;
        }
    }
    
    DBG_OUT("[BSP] Culling results: kept %d, culled %d", kept, culled);
    
    /* Verify what remains */
    int remaining_in_solid = 0, remaining_in_empty = 0;
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        if (leaf->contents == CONTENTS_SOLID)
            remaining_in_solid += leaf->face_count;
        else
            remaining_in_empty += leaf->face_count;
    }
    
    DBG_OUT("[BSP] After culling:");
    DBG_OUT("[BSP]   Faces in SOLID leaves: %d", remaining_in_solid);
    DBG_OUT("[BSP]   Faces in EMPTY leaves: %d (should be 0!)", remaining_in_empty);
}

static void
CullHiddenFaces_WithDirectionTest(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Culling with direction test...");
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->face_count == 0)
            continue;
        
        /* Process each face individually based on direction */
        BSPFace *prev = NULL;
        BSPFace *face = leaf->faces;
        
        while (face) {
            BSPFace *next = face->next;
            bool keep_face = false;
            
            if (leaf->contents == CONTENTS_SOLID) {
                /* For SOLID leaves, test which direction the face points */
                
                /* Calculate face center */
                Vector3 center = {0, 0, 0};
                for (int v = 0; v < face->vertex_count; v++) {
                    center = Vector3Add(center, face->vertices[v]);
                }
                center = Vector3Scale(center, 1.0f / face->vertex_count);
                
                /* Test point in front of face (normal direction) */
                Vector3 front_test = Vector3Add(center, Vector3Scale(face->normal, 0.1f));
                const BSPLeaf *front_leaf = BSP_FindLeaf(tree, front_test);
                
                /* Test point behind face (negative normal direction) */
                Vector3 back_test = Vector3Subtract(center, Vector3Scale(face->normal, 0.1f));
                const BSPLeaf *back_leaf = BSP_FindLeaf(tree, back_test);
                
                /* Keep face if front is reachable EMPTY and back is SOLID */
                /* (face separates playable space from solid geometry) */
                if (front_leaf && back_leaf) {
                    bool front_is_playable = (front_leaf->contents == CONTENTS_EMPTY && 
                                             front_leaf->is_reachable);
                    bool back_is_solid = (back_leaf->contents == CONTENTS_SOLID);
                    
                    if (front_is_playable && back_is_solid) {
                        keep_face = true;
                    }
                }
            }
            /* EMPTY leaves never keep faces */
            
            if (keep_face) {
                tree->visible_faces++;
                kept++;
                prev = face;
            } else {
                culled++;
                if (prev)
                    prev->next = next;
                else
                    leaf->faces = next;
                BSPFace_Free(face);
            }
            
            face = next;
        }
        
        if (leaf->faces == NULL) {
            leaf->face_count = 0;
        } else {
            /* Recount faces */
            int count = 0;
            for (BSPFace *f = leaf->faces; f; f = f->next)
                count++;
            leaf->face_count = count;
        }
    }
    
    DBG_OUT("[BSP] Direction test: kept %d, culled %d", kept, culled);
}

static void
CullHiddenFaces_ByNormal(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Culling faces using normal direction test...");
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->face_count == 0)
            continue;
        
        /* Process each face individually */
        BSPFace *prev = NULL;
        BSPFace *face = leaf->faces;
        
        while (face) {
            BSPFace *next = face->next;
            bool keep_face = false;
            
            /* Test 1: Is this face in a SOLID leaf? */
            if (leaf->contents == CONTENTS_SOLID) {
                /* Test 2: Does it point toward reachable playable space? */
                if (FacePointsTowardPlayableSpace(face, i, tree)) {
                    keep_face = true;
                }
            }
            /* Faces in EMPTY leaves are never kept (internal) */
            
            if (keep_face) {
                tree->visible_faces++;
                kept++;
                prev = face;
            } else {
                culled++;
                if (prev)
                    prev->next = next;
                else
                    leaf->faces = next;
                BSPFace_Free(face);
                leaf->face_count--;
            }
            
            face = next;
        }
    }
    
    DBG_OUT("[BSP] Normal-based cull: kept %d, culled %d", kept, culled);
}


static void
CullHiddenFaces_PortalOnly(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Culling faces - only keep SOLID↔reachable EMPTY boundaries...");
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        bool keep_faces = false;
        
        /* CRITICAL: Only check SOLID leaves */
        if (leaf->contents == CONTENTS_SOLID) {
            /* Check if any portal connects to reachable EMPTY */
            for (Portal *p = g_portals; p; p = p->next) {
                int other_idx = -1;
                
                if (p->leaf_front == i)
                    other_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_idx = p->leaf_front;
                
                if (other_idx >= 0 && other_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_idx];
                    
                    /* Keep if other side is reachable playable space */
                    if (other->contents == CONTENTS_EMPTY && other->is_reachable) {
                        keep_faces = true;
                        break;
                    }
                }
            }
        }
        /* DO NOT check EMPTY leaves - remove that code block */
        
        /* Process faces */
        if (keep_faces) {
            BSPFace *face = leaf->faces;
            while (face) {
                tree->visible_faces++;
                kept++;
                face = face->next;
            }
        } else {
            BSPFace *face = leaf->faces;
            while (face) {
                BSPFace *next = face->next;
                culled++;
                BSPFace_Free(face);
                face = next;
            }
            leaf->faces = NULL;
            leaf->face_count = 0;
        }
    }
    
    DBG_OUT("[BSP] Portal-only cull: kept %d, culled %d", kept, culled);
}

static void
CullHiddenFaces_Debug(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Debug culling - analyzing all leaves...");
    
    int solid_with_portals = 0;
    int solid_without_portals = 0;
    int empty_reachable = 0;
    int empty_unreachable = 0;
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        bool keep_faces = false;
        
        if (leaf->contents == CONTENTS_SOLID) {
            bool has_empty_portal = false;
            
            for (Portal *p = g_portals; p; p = p->next) {
                int other_idx = -1;
                
                if (p->leaf_front == i)
                    other_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_idx = p->leaf_front;
                
                if (other_idx >= 0 && other_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_idx];
                    
                    if (other->contents == CONTENTS_EMPTY && other->is_reachable) {
                        has_empty_portal = true;
                        keep_faces = true;
                        break;
                    }
                }
            }
            
            if (has_empty_portal)
                solid_with_portals++;
            else
                solid_without_portals++;
                
        } else if (leaf->contents == CONTENTS_EMPTY && leaf->is_reachable) {
            /* Check if this was causing the problem */
            bool has_solid_portal = false;
            
            for (Portal *p = g_portals; p; p = p->next) {
                int other_idx = -1;
                
                if (p->leaf_front == i)
                    other_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_idx = p->leaf_front;
                
                if (other_idx >= 0 && other_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_idx];
                    
                    if (other->contents == CONTENTS_SOLID) {
                        has_solid_portal = true;
                        /* THIS WAS THE BUG - don't set keep_faces = true here! */
                        break;
                    }
                }
            }
            
            if (has_solid_portal)
                empty_reachable++;
        }
        
        /* Process faces */
        if (keep_faces) {
            BSPFace *face = leaf->faces;
            while (face) {
                tree->visible_faces++;
                kept++;
                face = face->next;
            }
        } else {
            BSPFace *face = leaf->faces;
            while (face) {
                BSPFace *next = face->next;
                culled++;
                BSPFace_Free(face);
                face = next;
            }
            leaf->faces = NULL;
            leaf->face_count = 0;
        }
    }
    
    DBG_OUT("[BSP] SOLID leaves: %d with EMPTY portals, %d without",
            solid_with_portals, solid_without_portals);
    DBG_OUT("[BSP] EMPTY reachable leaves with SOLID portals: %d", empty_reachable);
    DBG_OUT("[BSP] Result: kept %d, culled %d", kept, culled);
}

static void
CullHiddenFaces_Hybrid(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Hybrid culling (portal + direction)...");
    
    /* Stats for debugging */
    int passed_portal_test = 0;
    int passed_direction_test = 0;
    int passed_both = 0;
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->face_count == 0)
            continue;
        
        /* Pre-check: Does this SOLID leaf have any portals to reachable EMPTY? */
        bool leaf_borders_playable = false;
        
        if (leaf->contents == CONTENTS_SOLID) {
            for (Portal *p = g_portals; p; p = p->next) {
                int other_idx = -1;
                
                if (p->leaf_front == i)
                    other_idx = p->leaf_back;
                else if (p->leaf_back == i)
                    other_idx = p->leaf_front;
                
                if (other_idx >= 0 && other_idx < tree->leaf_count) {
                    const BSPLeaf *other = &tree->leaves[other_idx];
                    if (other->contents == CONTENTS_EMPTY && other->is_reachable) {
                        leaf_borders_playable = true;
                        break;
                    }
                }
            }
        }
        
        /* Now test each face individually */
        BSPFace *prev = NULL;
        BSPFace *face = leaf->faces;
        
        while (face) {
            BSPFace *next = face->next;
            bool keep_face = false;
            
            /* Test 1: Portal test - is leaf adjacent to playable space? */
            if (leaf_borders_playable) {
                passed_portal_test++;
                
                /* Test 2: Direction test - does face point toward playable space? */
                Vector3 center = {0, 0, 0};
                for (int v = 0; v < face->vertex_count; v++) {
                    center = Vector3Add(center, face->vertices[v]);
                }
                center = Vector3Scale(center, 1.0f / face->vertex_count);
                
                /* Test point in front of face (where normal points) */
                Vector3 front_test = Vector3Add(center, Vector3Scale(face->normal, 0.1f));
                const BSPLeaf *front_leaf = BSP_FindLeaf(tree, front_test);
                
                if (front_leaf) {
                    /* Keep if normal points toward reachable playable space */
                    if (front_leaf->contents == CONTENTS_EMPTY && front_leaf->is_reachable) {
                        passed_direction_test++;
                        keep_face = true;
                        passed_both++;
                    }
                }
            }
            
            if (keep_face) {
                tree->visible_faces++;
                kept++;
                prev = face;
            } else {
                culled++;
                if (prev)
                    prev->next = next;
                else
                    leaf->faces = next;
                BSPFace_Free(face);
            }
            
            face = next;
        }
        
        if (leaf->faces == NULL) {
            leaf->face_count = 0;
        } else {
            int count = 0;
            for (BSPFace *f = leaf->faces; f; f = f->next)
                count++;
            leaf->face_count = count;
        }
    }
    
    DBG_OUT("[BSP] Hybrid test results:");
    DBG_OUT("[BSP]   Passed portal test: %d", passed_portal_test);
    DBG_OUT("[BSP]   Passed direction test: %d", passed_direction_test);
    DBG_OUT("[BSP]   Passed both (kept): %d", passed_both);
    DBG_OUT("[BSP]   Failed (culled): %d", culled);
}

static void
CullHiddenFaces_ImprovedDirection(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Improved direction test with larger epsilon...");
    
    const float TEST_DISTANCE = 0.5f;  /* Increased from 0.1 */
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->face_count == 0)
            continue;
        
        BSPFace *prev = NULL;
        BSPFace *face = leaf->faces;
        
        while (face) {
            BSPFace *next = face->next;
            bool keep_face = false;
            
            if (leaf->contents == CONTENTS_SOLID) {
                /* Calculate face center */
                Vector3 center = {0, 0, 0};
                for (int v = 0; v < face->vertex_count; v++) {
                    center = Vector3Add(center, face->vertices[v]);
                }
                center = Vector3Scale(center, 1.0f / face->vertex_count);
                
                /* Test further out from face */
                Vector3 front_test = Vector3Add(center, 
                    Vector3Scale(face->normal, TEST_DISTANCE));
                const BSPLeaf *front_leaf = BSP_FindLeaf(tree, front_test);
                
                if (front_leaf) {
                    /* Keep if front is reachable playable space */
                    if (front_leaf->contents == CONTENTS_EMPTY && 
                        front_leaf->is_reachable) {
                        keep_face = true;
                    }
                }
            }
            
            if (keep_face) {
                tree->visible_faces++;
                kept++;
                prev = face;
            } else {
                culled++;
                if (prev)
                    prev->next = next;
                else
                    leaf->faces = next;
                BSPFace_Free(face);
            }
            
            face = next;
        }
        
        if (leaf->faces == NULL) {
            leaf->face_count = 0;
        } else {
            int count = 0;
            for (BSPFace *f = leaf->faces; f; f = f->next)
                count++;
            leaf->face_count = count;
        }
    }
    
    DBG_OUT("[BSP] Improved direction: kept %d, culled %d", kept, culled);
}

static void
CullHiddenFaces_CheckBothSides(BSPTree *tree)
{
    int culled = 0, kept = 0;
    tree->visible_faces = 0;
    
    DBG_OUT("[BSP] Testing both sides of faces...");
    
    const float TEST_DIST = 0.2f;
    
    for (int i = 0; i < tree->leaf_count; i++) {
        BSPLeaf *leaf = &tree->leaves[i];
        
        if (leaf->face_count == 0)
            continue;
        
        BSPFace *prev = NULL;
        BSPFace *face = leaf->faces;
        
        while (face) {
            BSPFace *next = face->next;
            bool keep_face = false;
            
            if (leaf->contents == CONTENTS_SOLID) {
                /* Calculate face center */
                Vector3 center = {0, 0, 0};
                for (int v = 0; v < face->vertex_count; v++) {
                    center = Vector3Add(center, face->vertices[v]);
                }
                center = Vector3Scale(center, 1.0f / face->vertex_count);
                
                /* Test BOTH sides of the face */
                Vector3 front_test = Vector3Add(center, 
                    Vector3Scale(face->normal, TEST_DIST));
                Vector3 back_test = Vector3Subtract(center, 
                    Vector3Scale(face->normal, TEST_DIST));
                
                const BSPLeaf *front_leaf = BSP_FindLeaf(tree, front_test);
                const BSPLeaf *back_leaf = BSP_FindLeaf(tree, back_test);
                
                /* Keep if ONE side is reachable EMPTY and other is SOLID */
                bool front_playable = (front_leaf && 
                    front_leaf->contents == CONTENTS_EMPTY && 
                    front_leaf->is_reachable);
                bool back_playable = (back_leaf && 
                    back_leaf->contents == CONTENTS_EMPTY && 
                    back_leaf->is_reachable);
                
                bool front_solid = (front_leaf && 
                    front_leaf->contents == CONTENTS_SOLID);
                bool back_solid = (back_leaf && 
                    back_leaf->contents == CONTENTS_SOLID);
                
                /* Keep if face separates playable from solid */
                if ((front_playable && back_solid) || 
                    (back_playable && front_solid)) {
                    keep_face = true;
                }
            }
            
            if (keep_face) {
                tree->visible_faces++;
                kept++;
                prev = face;
            } else {
                culled++;
                if (prev)
                    prev->next = next;
                else
                    leaf->faces = next;
                BSPFace_Free(face);
            }
            
            face = next;
        }
        
        if (leaf->faces == NULL) {
            leaf->face_count = 0;
        } else {
            int count = 0;
            for (BSPFace *f = leaf->faces; f; f = f->next)
                count++;
            leaf->face_count = count;
        }
    }
    
    DBG_OUT("[BSP] Both-sides test: kept %d, culled %d", kept, culled);
}

static void
CullHiddenFaces(BSPTree *tree)
{
	int culled = 0;
	int tested = 0;
	tree->visible_faces = 0;
	
	DBG_OUT("[BSP] Culling faces not bordering interior...");
	
	for (int i = 0; i < tree->leaf_count; i++) {
		BSPLeaf *leaf = &tree->leaves[i];
		
		bool keep_leaf_faces = false;
		
		/* Keep all faces in EMPTY (interior) leaves */
		if (leaf->contents == CONTENTS_EMPTY) {
			keep_leaf_faces = true;
		}
		/* Also keep faces in SOLID leaves that border EMPTY leaves */
		else if (leaf->contents == CONTENTS_SOLID) {
			for (Portal *p = g_portals; p; p = p->next) {
				if ((p->leaf_front == i && tree->leaves[p->leaf_back].contents == CONTENTS_EMPTY) ||
				    (p->leaf_back == i && tree->leaves[p->leaf_front].contents == CONTENTS_EMPTY)) {
					keep_leaf_faces = true;
					break;
				}
			}
		}
		
		if (keep_leaf_faces) {
			/* Keep all faces in this leaf */
			BSPFace *face = leaf->faces;
			while (face) {
				tested++;
				tree->visible_faces++;
				face = face->next;
			}
		} else {
			/* Cull all faces in this leaf */
			BSPFace *face = leaf->faces;
			while (face) {
				BSPFace *next = face->next;
				tested++;
				culled++;
				BSPFace_Free(face);
				face = next;
			}
			leaf->faces = NULL;
			leaf->face_count = 0;
		}
	}
	
	DBG_OUT("[BSP] Tested %d faces total", tested);
	DBG_OUT("[BSP] Culled %d hidden faces, %d visible remain", culled, tree->visible_faces);
}

/* ========================================================================
   MAIN BUILD FUNCTION
   ======================================================================== */

static bool
FindPlayerStart(const MapData *map, Vector3 *out_pos)
{
	if (!map || !out_pos)
		return false;

	for (int i = 0; i < map->entity_count; i++) {
		const MapEntity *entity = &map->entities[i];
		const char *classname = GetEntityProperty((MapEntity *)entity, "classname");
		
		if (classname && strcmp(classname, "info_player_start") == 0) {
			const char *origin = GetEntityProperty((MapEntity *)entity, "origin");
			if (!origin)
				return false;
			
			float x, y, z;
			if (sscanf(origin, "%f %f %f", &x, &y, &z) != 3)
				return false;
			
			*out_pos = (Vector3){x, y, z};
			DBG_OUT("[BSP] Found info_player_start at (%.1f, %.1f, %.1f)", x, y, z);
			return true;
		}
	}
	return false;
}

BSPTree *
BSP_Build(
    const CompiledFace  *compiled_faces,
    int                  face_count,
    const Vector3       *vertices,
    int                  vertex_count,
    const CompiledBrush *brushes,
    int                  brush_count,
    const MapData       *map_data
)
{
	DBG_OUT("[BSP] Building tree from %d faces...", face_count);

	if (!FindPlayerStart(map_data, &g_player_start_pos)) {
		fprintf(stderr, "[BSP ERROR] No info_player_start found\n");
		return NULL;
	}

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

	FaceList *initial_faces = FaceList_Create();
	if (!initial_faces) {
		free(tree->nodes);
		free(tree->leaves);
		free(tree);
		return NULL;
	}

	for (int i = 0; i < face_count; i++) {
		const CompiledFace *cf = &compiled_faces[i];
		if (!cf->is_visible || cf->vertex_count < 3)
			continue;
		
		BSPFace *bsp_face = BSPFace_Create(cf->vertex_count);
		if (!bsp_face)
			continue;
		
		for (int v = 0; v < cf->vertex_count; v++)
			bsp_face->vertices[v] = vertices[cf->vertex_start + v];
		bsp_face->normal = cf->normal;
		bsp_face->plane_dist = cf->plane_dist;
		bsp_face->original_face_idx = i;
		FaceList_Add(initial_faces, bsp_face);
	}

	DBG_OUT("[BSP] Building BSP tree with %d faces...", initial_faces->face_count);

	if (initial_faces->face_count > 0) {
		BuildTree_Recursive(tree, initial_faces, 0, &tree->root_is_leaf);
	} else {
		tree->root_is_leaf = true;
		AllocLeaf(tree);
		tree->leaves[0].contents = CONTENTS_SOLID;
	}

	FaceList_Free(initial_faces);

	DBG_OUT("[BSP] Generating portals...");
	g_portals = NULL;
	
	if (!tree->root_is_leaf) {
		MakeTreePortals(tree);
	}
	
	int portal_count = 0;
	for (Portal *p = g_portals; p; p = p->next)
		portal_count++;
	DBG_OUT("[BSP] Generated %d portals", portal_count);

	/* QBSP algorithm:
	 * 1. Mark leaves with faces as SOLID (inside brushes)
	 * QBSP-style inside/outside determination:
	 * 1. All leaves start as SOLID (outside/unknown)
	 * 2. Flood-fill from player start to mark reachable leaves as EMPTY (inside)
	 * 3. Keep faces that border EMPTY (inside) space */
/*	
	MarkSolidLeaves_Fixed(
            tree, 
            compiled_faces, 
            face_count, 
            vertices,
            brushes, 
            brush_count
        );
	
	/* Find player start leaf */
/*
	const BSPLeaf *player_leaf = BSP_FindLeaf(tree, g_player_start_pos);
	if (!player_leaf) {
		fprintf(stderr, "[BSP ERROR] Could not find leaf for player start\n");
	} else {
		DBG_OUT("[BSP] Player start is in leaf %d", player_leaf->leaf_index);
		FloodFillFromPlayer(tree, player_leaf->leaf_index);
	}
*/
    /* Step 1: Mark leaves inside brushes as SOLID */
    MarkSolidLeaves_Fixed(tree, compiled_faces, face_count, vertices,
                          brushes, brush_count);
    
    /* Step 2: NEW - Mark exterior void as SOLID before flood-fill */
    MarkExteriorVoid(tree, compiled_faces, face_count, vertices);
    
    /* Step 3: Find player start leaf */
    const BSPLeaf *player_leaf = BSP_FindLeaf(tree, g_player_start_pos);
    if (!player_leaf) {
        fprintf(stderr, "[BSP ERROR] Could not find leaf for player start\n");
    } else {
        DBG_OUT("[BSP] Player start is in leaf %d", player_leaf->leaf_index);
        
        /* Step 4: Verify player is in EMPTY (interior) space */
        if (player_leaf->contents != CONTENTS_EMPTY) {
            fprintf(stderr, "[BSP ERROR] Player start is not in EMPTY space! Contents=%d\n",
                    player_leaf->contents);
        } else {
            /* Step 5: Flood-fill from player to mark reachable interior */
            FloodFillFromPlayer(tree, player_leaf->leaf_index);
            DebugPrintLeafSamples(tree);
        }
    }

    
	int empty_count = 0, solid_count = 0, reachable_count = 0;
	for (int i = 0; i < tree->leaf_count; i++) {
		if (tree->leaves[i].contents == CONTENTS_EMPTY) {
			empty_count++;
			if (tree->leaves[i].is_reachable)
				reachable_count++;
		} else {
			solid_count++;
		}
	}
	DBG_OUT("[BSP] Leaf classification: %d EMPTY (%d reachable, %d unreachable), %d SOLID", 
	        empty_count, reachable_count, empty_count - reachable_count, solid_count);

	CullHiddenFaces(tree);

	while (g_portals) {
		Portal *next = g_portals->next;
		Portal_Free(g_portals);
		g_portals = next;
	}

	DBG_OUT("[BSP] Build complete: %d nodes, %d leaves, %d visible faces",
	    tree->node_count, tree->leaf_count, tree->visible_faces);

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
		PlaneSide side = BSP_ClassifyPoint(point, node->plane_normal, node->plane_dist);
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
	int empty_leaves = 0, solid_leaves = 0, reachable_leaves = 0;
	for (int i = 0; i < tree->leaf_count; i++) {
		if (tree->leaves[i].contents == CONTENTS_EMPTY) {
			empty_leaves++;
			if (tree->leaves[i].is_reachable)
				reachable_leaves++;
		} else {
			solid_leaves++;
		}
	}
	DBG_OUT("=== BSP Tree Statistics ===");
	DBG_OUT("  Nodes: %d", tree->node_count);
	DBG_OUT("  Leaves: %d", tree->leaf_count);
	DBG_OUT("  Total faces: %d", tree->total_faces);
	DBG_OUT("  Visible faces: %d", tree->visible_faces);
	DBG_OUT("  Max depth: %d", tree->max_tree_depth);
	DBG_OUT("  Empty leaves: %d (%d reachable interior, %d unreachable outside)", 
	        empty_leaves, reachable_leaves, empty_leaves - reachable_leaves);
	DBG_OUT("  Solid leaves: %d", solid_leaves);
}

bool
BSP_Validate(const BSPTree *tree)
{
	if (!tree)
		return false;
	for (int i = 0; i < tree->node_count; i++) {
		const BSPNode *node = &tree->nodes[i];
		if (node->front_is_leaf) {
			if (node->front_child < 0 || node->front_child >= tree->leaf_count)
				return false;
		} else {
			if (node->front_child < 0 || node->front_child >= tree->node_count)
				return false;
		}
		if (node->back_is_leaf) {
			if (node->back_child < 0 || node->back_child >= tree->leaf_count)
				return false;
		} else {
			if (node->back_child < 0 || node->back_child >= tree->node_count)
				return false;
		}
	}
	return true;
}
