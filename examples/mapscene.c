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


#define PLANE_EPSILON  0.01f
#define CLIP_BOX_HALF  16384.0f
#define CLIP_BUF_SIZE  256
#define MAX_FACE_VERTS 32


typedef struct
CompiledFace
{
    Vector3 verts[MAX_FACE_VERTS];
    int     vert_count;
    Vector3 normal;
}
CompiledFace;

typedef struct
CompiledBrush
{
    CompiledFace *faces;        /* Heap-allocated array */
    int           face_count;
}
CompiledBrush;

/* Scene-local state.  Mirrors how infinite_plane_scene keeps debug_texture
 * as a file-scope global — this is the established pattern. */
typedef struct
MapSceneData
{
    MapData       *map;             /* Raw parsed .map data (owned) */
    CompiledBrush *brushes;         /* Compiled world brushes (owned) */
    int            brush_count;
    char           source_path[256];
}
MapSceneData;


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
        Takes a single MapBrush and produces a CompiledBrush whose faces array
        is heap-allocated. Returns true on success. A face with fewer than 3 
        surviving vertices is degenerate and is silently dropped.
*/
static int g_compile_brush_index = 0;  /* temporary: tracks which brush we're on */

static bool
compile_brush(const MapBrush *brush, CompiledBrush *out)
{
    int is_debug = (g_compile_brush_index == 0);
    g_compile_brush_index++;

    CompiledFace temp_faces[MAX_BRUSH_PLANES];
    int          face_count = 0;

    if (is_debug) {
        printf("[DBG] brush 0: %d planes\n", brush->plane_count);
        for (int i = 0; i < brush->plane_count; i++)
            printf("[DBG]   plane %d: n=(%.3f %.3f %.3f) d=%.3f\n", i,
                   brush->planes[i].normal.x,
                   brush->planes[i].normal.y,
                   brush->planes[i].normal.z,
                   brush->planes[i].distance);
    }

    for (int f = 0; f < brush->plane_count; f++) {
        const MapPlane *face_plane = &brush->planes[f];

        int      count = make_face_seed_polygon(face_plane->normal, face_plane->distance, clip_buf_a);
        Vector3 *src   = clip_buf_a;
        Vector3 *dst   = clip_buf_b;

        if (is_debug && f == 0)
            printf("[DBG] face 0: seed count=%d\n", count);

        for (int p = 0; p < brush->plane_count; p++) {
            if (p == f) continue;

            count = clip_polygon_by_plane(
                src, count, dst,
                brush->planes[p].normal,
                brush->planes[p].distance
            );

            if (is_debug && f == 0)
                printf("[DBG]   clipped by plane %d: count=%d\n", p, count);

            Vector3 *tmp = src; src = dst; dst = tmp;

            if (count == 0) break;
        }

        if (count < 3) continue;

        CompiledFace *face   = &temp_faces[face_count];
        face->normal         = face_plane->normal;
        face->vert_count     = (count < 32) ? count : 32;
        memcpy(face->verts, src, face->vert_count * sizeof(Vector3));

        face_count++;
    }

    if (0 < face_count) {
        out->faces      = (CompiledFace *)malloc(face_count * sizeof(CompiledFace));
        out->face_count = face_count;
        if (out->faces)
            memcpy(out->faces, temp_faces, face_count * sizeof(CompiledFace));
        else
            out->face_count = 0;
    }
    else {
        out->faces      = NULL;
        out->face_count = 0;
    }

    return (0 < face_count);
}


/*
    free_compiled_brush
*/
static void
free_compiled_brush(CompiledBrush *brush)
{
    if (brush->faces) {
        free(brush->faces);
        brush->faces      = NULL;
        brush->face_count = 0;
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

        allocates a MapSceneData, parses the .map, compiles all world brushes, 
        and stores the result into the Scene's data slot.
*/
static void
mapscene_Setup(Scene *scene, void *map_data)
{
    (void)scene;
    MapSceneData *sd = (MapSceneData *)map_data;

    /*
        Parse the .map file
    */
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

    /*
        Compile world brushes
    */
    int world_count = sd->map->world_brush_count;
    sd->brush_count = world_count;
    sd->brushes     = NULL;

    if (0 < world_count) {
        sd->brushes = (CompiledBrush*)calloc(
                world_count, 
                sizeof(CompiledBrush)
            );
        if (!sd->brushes) {
            ERR_OUT("[MapScene] Error: Failed to allocate CompiledBrush array");
            sd->brush_count = 0;
        }
        else {
            int compiled = 0;
            for (int i = 0; i < world_count; i++) {
                if (compile_brush(&sd->map->world_brushes[i], &sd->brushes[i]))
                    compiled++;
            }
            DBG_OUT(
                    "[MapScene] Compiled %d / %d world brushes", 
                    compiled, 
                    world_count
                );
        }
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
    mapscene_Render
        SceneRenderCallback - called once per frame.
        Iterates every compiled brush and draws each face as a wireframe polygon
        using raylib's 3D line primitives.
*/
static void
mapscene_Render(Scene *scene, Head *head)
{
    Renderer     *renderer = Engine_getRenderer(Scene_getEngine(scene));
    MapSceneData *sd       = (MapSceneData*)Scene_getData(scene);
    Camera   *camera   = Head_getCamera(head);
    (void)camera; // A secret tool for later
    
    /* World geometry wireframe */
    if (sd && sd->brushes) {
        Color wire_color = MAGENTA;

        for (int b = 0; b < sd->brush_count; b++) {
            CompiledBrush *brush = &sd->brushes[b];
            if (!brush->faces) continue;

            for (int f = 0; f < brush->face_count; f++) {
                CompiledFace *face = &brush->faces[f];
                if (face->vert_count < 3) continue;
                
                for (int v = 0; v < face->vert_count; v++) {
                    int next = (v+1) % face->vert_count;
                    
                    DrawLine3D(face->verts[v], face->verts[next], wire_color);
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


static void
mapscene_Free(Scene *scene)
{
    MapSceneData *sd = (MapSceneData*)Scene_getData(scene);
    if (!sd) return;

    if (sd->brushes) {
        for (int i = 0; i < sd->brush_count; i++)
            free_compiled_brush(&sd->brushes[i]);
        free(sd->brushes);
        sd->brushes     = NULL;
        sd->brush_count = 0;
    }

    FreeMapData(sd->map);
    sd->map = NULL;
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
        .Render         = mapscene_Render,
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
