#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include "kolibri.h"
#include "v220_map_parser.h"
#include "mapscene.h"
#include "mapscene_bsp.h"


#define PLANE_EPSILON  0.01f
#define CLIP_BOX_HALF  16384.0f
#define CLIP_BUF_SIZE  256
#define MAX_FACE_VERTS 32


/*
    TempCompiledFace - temporary structure used during brush compilation
    before data is copied into the flat arrays
*/
typedef struct
TempCompiledFace
{
    Vector3 verts[MAX_FACE_VERTS];
    int     vert_count;
    Vector3 normal;
    float   plane_dist;
}
TempCompiledFace;


/*
    faces_are_coplanar_and_opposite
        Returns true if f1 and f2 lie on the same plane but face opposite
        directions (indicating they're internal faces between touching brushes).

        Two faces are considered coplanar-opposite if:
            1. Their normals point in opposite directions (dot ~= -1)
            2. They lie on the same geometric plane (within PLANE_EPSILON)
*/
static bool
faces_are_coplanar_and_opposite(const CompiledFace *f1, const CompiledFace *f2)
{
    float dot = Vector3DotProduct(f1->normal, f2->normal);
    if (-0.99f < dot) 
        return false;

    float dist_sum = fabsf(f1->plane_dist + f2->plane_dist);
    if (PLANE_EPSILON < dist_sum)
        return false;

    return true;
}

/*
    mark_hidden_faces
        Iterates all compiled brushes and marks faces as hidden if they are
        coplanar and opposite-facing with another face (indicating internal
        geometry between touching brushes).

        Complexity: O(F^2) where F = total face count
        For typical Valve maps (< 10K faces), this is fast enough.
*/
static void
mark_hidden_faces(MapSceneData *sd)
{
    if (!sd || !sd->all_faces) return;

    int hidden_faces = 0;

    /* Initialize all faces as visible */
    for (int i = 0; i < sd->face_count; i++) {
        sd->all_faces[i].is_visible = true;
    }

    /* Check each pair of faces for coplanar-opposite condition */
    for (int i = 0; i < sd->face_count; i++) {
        if (!sd->all_faces[i].is_visible) continue;

        for (int j = i + 1; j < sd->face_count; j++) {
            if (!sd->all_faces[j].is_visible) continue;

            if (faces_are_coplanar_and_opposite(&sd->all_faces[i], &sd->all_faces[j])) {
                sd->all_faces[j].is_visible = false;
                hidden_faces++;
            }
        }
    }

    DBG_OUT("[MapScene] Marked %d hidden faces (%.1f%%)", 
            hidden_faces, 
            (100.0f * hidden_faces) / (sd->face_count ? sd->face_count : 1));
}

/*
    BRUSH COMPILER

    A Valve .map brush is a convex solid defined as the intersection of N
    half-spaces (one per plane). The parser gives us (normal, distance) for each
    plane; we need to recover the actual polygon vertices for every face.

    Algorithm
    For each face iin the brush:
        1. Start with a large polygon that lies on F's plane (clipped to the
        bounding box so it is finite).
        2. For every OTHER plane P in the same brush, clip the polygon to the
        half-space defined by P (keep the side that points inward, i.e.
        dot(P.normal, vertex) <= P.distance).
        3. Whatever polygon survives is the rendered face.

    The clipping loop is standard Sutherland-HOdgman style loop over the vertex
    list.
*/
static Vector3 clip_buf_a[CLIP_BUF_SIZE];
static Vector3 clip_buf_b[CLIP_BUF_SIZE];


/*
    clip_polygon_by_plane
        Clips the polygon in "in" (length "in_count") against the half-space
        defined by (normal, distance). Vertices in the NEGATIVE side of the
        plane (dot(normal, v) <= distance + epsilon) are kept. Results are
        written to "out"; returns the new vertex count.

        This is one iteration of Sutherland-Hodgman.
*/
static int
clip_polygon_by_plane(
    const Vector3 *in,
          int      in_count,
          Vector3 *out,
          Vector3  normal,
          float    distance
)
{
    int out_count = 0;

    if (in_count == 0) return 0;

    for (int i = 0; i < in_count; i++) {
        Vector3
            cur  = in[i],
            next = in[(i+1) % in_count];

        float
            d_cur  = Vector3DotProduct(normal, cur)  - distance,
            d_next = Vector3DotProduct(normal, next) - distance;

        bool
            cur_inside  = (d_cur  <= PLANE_EPSILON),
            next_inside = (d_next <= PLANE_EPSILON);

        if (cur_inside) {
            if (out_count < CLIP_BUF_SIZE) out[out_count++] = cur;

            if (!next_inside) {
                float   t = d_cur / (d_cur - d_next);
                Vector3 intersection = Vector3Add(
                        cur,
                        Vector3Scale(
                                Vector3Subtract(next, cur),
                                t
                            )
                    );
                    
                if (out_count < CLIP_BUF_SIZE) out[out_count++] = intersection;
            }
        }
        else if (next_inside)
        {
            float   t            = d_cur / (d_cur - d_next);
            Vector3 intersection = Vector3Add(
                    cur, 
                    Vector3Scale(
                            Vector3Subtract(
                                    next, 
                                    cur
                                ), 
                            t
                        )   
                );
            if (out_count < CLIP_BUF_SIZE)
                out[out_count++] = intersection;
        }
    }

    return out_count;
}


/*
    make_face_seed_polygon
        Given a plane (normal, distance), produce a large convex polygon that 
        lies exactly on that plane and is contained within the clip box.

        We pick two orthogonal tangent vectors on the plane and lay out a large
        square. The box clipping that follows will trim it.
*/
static int
make_face_seed_polygon(Vector3 normal, float distance, Vector3 *out)
{
    Vector3 
        up        = (fabsf(normal.y) < 0.9f) ? V3_UP : V3_FORWARD,
        tangent_u = Vector3Normalize(Vector3CrossProduct(normal, up)),
        tangent_v = Vector3CrossProduct(normal, tangent_u),

        center    = Vector3Scale(normal, distance);

    float s = CLIP_BOX_HALF;
    out[0] = Vector3Add(
            center, 
            Vector3Add(
                    Vector3Scale(tangent_u,  s), 
                    Vector3Scale(tangent_v,  s)
                )
        );
    out[1] = Vector3Add(
            center, 
            Vector3Add(
                    Vector3Scale(tangent_u, -s), 
                    Vector3Scale(tangent_v,  s)
                )
        );
    out[2] = Vector3Add(
            center, 
            Vector3Add(
                    Vector3Scale(tangent_u, -s), 
                    Vector3Scale(tangent_v, -s)
                )
        );
    out[3] = Vector3Add(
            center, 
            Vector3Add(
                    Vector3Scale(tangent_u,  s), 
                    Vector3Scale(tangent_v, -s)
                )
        );

    return 4;
}


/*
    compile_brush
        Takes a single MapBrush and produces temporary face data (with embedded
        vertices). Returns the number of valid faces compiled. The caller is
        responsible for copying this data into the flat arrays.
        
        temp_faces: output array (must have space for at least MAX_BRUSH_PLANES faces)
        Returns: number of valid faces written to temp_faces
*/
static int g_compile_brush_index = 0;  /* temporary: tracks which brush we're on */

static int
compile_brush(const MapBrush *brush, TempCompiledFace *temp_faces)
{
    int face_count = 0;

    for (int f = 0; f < brush->plane_count; f++) {
        const MapPlane *face_plane = &brush->planes[f];

        int      count = make_face_seed_polygon(face_plane->normal, face_plane->distance, clip_buf_a);
        Vector3 *src   = clip_buf_a;
        Vector3 *dst   = clip_buf_b;

        for (int p = 0; p < brush->plane_count; p++) {
            if (p == f) continue;

            count = clip_polygon_by_plane(
                src, count, dst,
                brush->planes[p].normal,
                brush->planes[p].distance
            );

            Vector3 *tmp = src; src = dst; dst = tmp;

            if (count == 0) break;
        }

        if (count < 3) continue;

        TempCompiledFace *face = &temp_faces[face_count];
        face->normal           = face_plane->normal;
        face->plane_dist       = Vector3DotProduct(face_plane->normal, src[0]);
        face->vert_count       = (count < MAX_FACE_VERTS) ? count : MAX_FACE_VERTS;
        memcpy(face->verts, src, face->vert_count * sizeof(Vector3));

        face_count++;
    }

    return face_count;
}


void DiagnoseBrushNormals(const CompiledBrush *brushes, int brush_count,
                         const CompiledFace *faces, const Vector3 *vertices)
{
    DBG_OUT("[DIAG] === Testing Brush Normal Directions ===");
    
    for (int b = 0; b < 3 && b < brush_count; b++) {  // Test first 3 brushes
        const CompiledBrush *brush = &brushes[b];
        
        DBG_OUT("[DIAG] Brush %d: %d faces", b, brush->face_count);
        
        // Calculate brush center (average of all vertices)
        Vector3 brush_center = {0, 0, 0};
        int total_verts = 0;
        
        for (int f = 0; f < brush->face_count; f++) {
            const CompiledFace *face = &faces[brush->face_start + f];
            for (int v = 0; v < face->vertex_count; v++) {
                Vector3 vert = vertices[face->vertex_start + v];
                brush_center.x += vert.x;
                brush_center.y += vert.y;
                brush_center.z += vert.z;
                total_verts++;
            }
        }
        
        if (total_verts > 0) {
            brush_center.x /= total_verts;
            brush_center.y /= total_verts;
            brush_center.z /= total_verts;
        }
        
        DBG_OUT("[DIAG]   Brush center: (%.1f, %.1f, %.1f)",
                brush_center.x, brush_center.y, brush_center.z);
        
        // Test each face: center should be on BACK side if normals point outward
        int correct_normals = 0;
        int wrong_normals = 0;
        
        for (int f = 0; f < brush->face_count; f++) {
            const CompiledFace *face = &faces[brush->face_start + f];
            
            // Distance from brush center to face plane
            float dist = Vector3DotProduct(brush_center, face->normal) - face->plane_dist;
            
            if (dist < -0.01f) {
                // Center is on BACK side - correct if normals point outward!
                correct_normals++;
            } else if (dist > 0.01f) {
                // Center is on FRONT side - WRONG, normal points inward!
                wrong_normals++;
                DBG_OUT("[DIAG]   Face %d: WRONG DIRECTION! dist=%.3f", f, dist);
                DBG_OUT("[DIAG]     Normal: (%.3f, %.3f, %.3f)",
                        face->normal.x, face->normal.y, face->normal.z);
            }
        }
        
        DBG_OUT("[DIAG]   Result: %d correct, %d wrong", correct_normals, wrong_normals);
        
        if (wrong_normals > 0) {
            DBG_OUT("[DIAG]   *** NORMALS ARE INVERTED! ***");
        }
    }
}

/*****************************
    SCENE VTABLE CALLBACKS
*****************************/

/*
    mapscene_Setup
        SceneDataCallback - called by Scene_new.
        "map_data" is the raw "data" pointer passed to Scene_new; we treat it as
        a null-terminated path string.

        Allocates a MapSceneData, parses the .map, compiles all world brushes 
        into flat arrays (two-pass: count first, then allocate and copy), and 
        stores the result into the Scene's data slot.
*/
static void
mapscene_Setup(Scene *scene, void *map_data)
{
    (void)scene;
    MapSceneData *sd = (MapSceneData *)map_data;

    /* Parse the .map file */
    sd->map = ParseValve220Map(sd->source_path, AXIS_REMAP_RAYLIB);
    if (!sd->map)
    {
        DBG_OUT(
                "[MapScene] Error: ParseValve220Map failed for \"%s\"", 
                sd->source_path
            );
        free(sd);
        return;
    }

    int world_count = sd->map->world_brush_count;
    if (world_count == 0) {
        DBG_OUT("[MapScene] No world brushes found");
        sd->brush_count = 0;
        sd->brushes = NULL;
        sd->all_faces = NULL;
        sd->all_vertices = NULL;
        sd->face_count = 0;
        sd->vertex_count = 0;
        return;
    }

    /*
        PASS 1: Compile brushes into temporary buffers and count totals
    */
    TempCompiledFace *temp_brush_faces[world_count];  /* Array of pointers to temp face arrays */
    int               brush_face_counts[world_count];
    
    int total_faces = 0;
    int total_verts = 0;

    for (int i = 0; i < world_count; i++) {
        temp_brush_faces[i] = (TempCompiledFace*)malloc(MAX_BRUSH_PLANES * sizeof(TempCompiledFace));
        if (!temp_brush_faces[i]) {
            ERR_OUT("[MapScene] Failed to allocate temp face buffer for brush");
            brush_face_counts[i] = 0;
            continue;
        }

        int face_count = compile_brush(&sd->map->world_brushes[i], temp_brush_faces[i]);
        brush_face_counts[i] = face_count;
        total_faces += face_count;

        /* Count vertices */
        for (int f = 0; f < face_count; f++) {
            total_verts += temp_brush_faces[i][f].vert_count;
        }
    }

    DBG_OUT("[MapScene] Compiled %d brushes: %d faces, %d vertices", 
            world_count, total_faces, total_verts);

    if (total_faces == 0) {
        DBG_OUT("[MapScene] Warning: No valid faces compiled");
        for (int i = 0; i < world_count; i++) {
            free(temp_brush_faces[i]);
        }
        sd->brush_count = 0;
        sd->brushes = NULL;
        sd->all_faces = NULL;
        sd->all_vertices = NULL;
        sd->face_count = 0;
        sd->vertex_count = 0;
        return;
    }

    /*
        PASS 2: Allocate flat arrays and copy data
    */
    sd->all_vertices = (Vector3*)malloc(total_verts * sizeof(Vector3));
    sd->all_faces    = (CompiledFace*)malloc(total_faces * sizeof(CompiledFace));
    sd->brushes      = (CompiledBrush*)malloc(world_count * sizeof(CompiledBrush));

    if (!sd->all_vertices || !sd->all_faces || !sd->brushes) {
        ERR_OUT("[MapScene] Failed to allocate flat arrays");
        free(sd->all_vertices);
        free(sd->all_faces);
        free(sd->brushes);
        for (int i = 0; i < world_count; i++) {
            free(temp_brush_faces[i]);
        }
        sd->brush_count = 0;
        sd->brushes = NULL;
        sd->all_faces = NULL;
        sd->all_vertices = NULL;
        sd->face_count = 0;
        sd->vertex_count = 0;
        return;
    }

    sd->brush_count = world_count;
    sd->face_count = total_faces;
    sd->vertex_count = total_verts;

    int face_idx = 0;
    int vert_idx = 0;

    for (int b = 0; b < world_count; b++) {
        sd->brushes[b].face_start = face_idx;
        sd->brushes[b].face_count = brush_face_counts[b];

        for (int f = 0; f < brush_face_counts[b]; f++) {
            TempCompiledFace *temp_face = &temp_brush_faces[b][f];
            CompiledFace     *face      = &sd->all_faces[face_idx];

            face->vertex_start = vert_idx;
            face->vertex_count = temp_face->vert_count;
            face->normal       = temp_face->normal;
            face->plane_dist   = temp_face->plane_dist;
            face->is_visible   = true;
            face->brush_idx    = b; /* Track which brush each face belongs to */

            /* Copy vertices */
            memcpy(&sd->all_vertices[vert_idx], temp_face->verts, 
                   temp_face->vert_count * sizeof(Vector3));

            vert_idx += temp_face->vert_count;
            face_idx++;
        }

        /* Free temporary face buffer for this brush */
        free(temp_brush_faces[b]);
    }

    /*
        PASS 3: Build BSP tree
        The BSP builder will determin which faces are actually visible
        (those bordering empty space) and handle app splitting/clipping
    */
    DBG_OUT("[MapScene] Building BSP Tree...");
    sd->bsp_tree = BSP_Build(
/*
            sd->all_faces, 
            sd->face_count, 
            sd->all_vertices,
            sd->vertex_count,
            sd->brushes,
            sd->brush_count,
*/
            sd->map
        );

    if (sd->bsp_tree) {
        BSP_PrintStats(sd->bsp_tree);
        BSP_Validate(sd->bsp_tree);
        DiagnoseBrushNormals(
                sd->brushes, 
                sd->brush_count, 
                sd->all_faces, 
                sd->all_vertices
            );
    }
    else {
        ERR_OUT("[MapScene] Failed to build BSP tree");
    }
}



static void
mapscene_Enter(Scene *scene)
{
    (void)scene;
}

static void
mapscene_Exit(Scene *scene)
{
    (void)scene;
}


/*
    mapscene_RenderMap
        Renders wireframe of the MapData. Called once per frame.
        Iterates every compiled brush, accesses its faces through indices,
        and draws each visible face as a wireframe polygon using raylib's 
        3D line primitives.
*/
static void
mapscene_RenderMap(Scene *scene, Head *head)
{
    Renderer     *renderer = Engine_getRenderer(Scene_getEngine(scene));
    MapSceneData *sd       = (MapSceneData*)Scene_getData(scene);
    Camera   *camera   = Head_getCamera(head);
    (void)camera; // A secret tool for later
    
    /* World geometry wireframe */
    if (sd && sd->brushes && sd->all_faces && sd->all_vertices) {
        Color wire_color = MAGENTA;

        for (int b = 0; b < sd->brush_count; b++) {
            CompiledBrush *brush = &sd->brushes[b];

            for (int f = 0; f < brush->face_count; f++) {
                CompiledFace *face = &sd->all_faces[brush->face_start + f];
                
                if (face->vertex_count < 3) continue;
                if (!face->is_visible) continue;
                
                /* Get pointer to this face's vertices in the flat array */
                Vector3 *face_verts = &sd->all_vertices[face->vertex_start];
                
                /* Draw wireframe edges */
                for (int v = 0; v < face->vertex_count; v++) {
                    int next = (v + 1) % face->vertex_count;
                    DrawLine3D(face_verts[v], face_verts[next], wire_color);
                }
            }
        }
    }

    /*
        Entity submission
    */
    Entity **entities = Scene_getEntities(scene);
    for (size_t i = 0; i < DynamicArray_length(entities); i++) {
        Renderer_submitEntity(renderer, entities[i]);
    }
}

/*
    mapscene_Render
        SceneRenderCallback - called once per frame.
        
        Now renders from the BSP tree instead of the flat face arrays.
        This shows only the visible faces (those in EMPTY leaves) and
        prepares us for future PVS-based culling.
*/
static void
mapscene_RenderBSP(Scene *scene, Head *head)
{
    Renderer     *renderer = Engine_getRenderer(Scene_getEngine(scene));
    MapSceneData *sd       = (MapSceneData*)Scene_getData(scene);
    Camera       *camera   = Head_getCamera(head);

    if (sd && sd->bsp_tree) {
        Color    wire_color = MAGENTA;
        BSPTree *tree       = sd->bsp_tree;

        /* Iterate ALL leaves - faces are in SOLID leaves after culling */
        for (int i = 0; i < tree->leaf_count; i++) {
            const BSPLeaf *leaf = &tree->leaves[i];

            if (leaf->face_count == 0) 
                continue;

            const BSPFace *face = leaf->faces;
            while (face) {
                if (face->vertex_count >= 3) {
                    for (int v = 0; v < face->vertex_count; v++) {
                        int next = (v+1) % face->vertex_count;
                        //DrawLine3D(face->vertices[v], face->vertices[next], wire_color);
                    }
                }
                face = face->next;
            }
        }
    }
    
    BSP_DebugDrawLeafBounds(sd->bsp_tree);
//    BSP_DrawLeakPath();
    BSP_DebugDrawLeak(sd->bsp_tree);   
    
    /* Entity Submission */
    Entity **entities = Scene_getEntities(scene);
    for (size_t i = 0; i < DynamicArray_length(entities); i++) {
        Renderer_submitEntity(renderer, entities[i]);
    }
}


static void
mapscene_Free(Scene *scene)
{
    MapSceneData *sd = (MapSceneData*)Scene_getData(scene);
    if (!sd) return;

    if (sd->bsp_tree) {
        BSP_Free(sd->bsp_tree);
        sd->bsp_tree = NULL;
    }

    /* Free flat arrays */
    if (sd->all_vertices) {
        free(sd->all_vertices);
        sd->all_vertices = NULL;
        sd->vertex_count = 0;
    }

    if (sd->all_faces) {
        free(sd->all_faces);
        sd->all_faces = NULL;
        sd->face_count = 0;
    }

    /* Free brush metadata */
    if (sd->brushes) {
        free(sd->brushes);
        sd->brushes = NULL;
        sd->brush_count = 0;
    }

    /* Free parsed map data */
    if (sd->map) {
        FreeMapData(sd->map);
        sd->map = NULL;
    }
}


/*
    PUBLIC VTABLE DEFINITION
*/

SceneVTable MapScene_vtable = {
        .Setup          = mapscene_Setup,
        .Enter          = NULL,
        .Update         = NULL,
        .EntityEnter    = NULL,
        .EntityExit     = NULL,
        .CheckCollision = NULL,
        .MoveEntity     = NULL,
        .Raycast        = NULL,
        .PreRender      = NULL,
        .Render         = mapscene_RenderBSP,
        .Exit           = NULL,
        .Free           = mapscene_Free,
    };


/*
    CONSTRUCTOR
*/
Scene *
MapScene_new(const char *map_path, Engine *engine)
{
    MapSceneData seed;
    memset(&seed, 0, sizeof(seed));
    strncpy(seed.source_path, map_path, sizeof(seed.source_path) - 1);
    seed.source_path[sizeof(seed.source_path) - 1] = '\0';

    return Scene_new(
            &MapScene_vtable,
            NULL,
            &seed,
            sizeof(MapSceneData),
            engine
        );
}
