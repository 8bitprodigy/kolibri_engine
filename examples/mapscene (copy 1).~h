#ifndef MAPSCENE_H
#define MAPSCENE_H

#include "scene.h"
#include "v220_map_parser.h"
#include "bsp.h"


#define CYAN ((Color){0, 255, 255, 255})


extern SceneVTable MapScene_vtable;
typedef struct MapSceneData MapSceneData;


/*
    CompiledFace / CompiledBrush
        Flat-array brush geometry used by the wireframe renderer.
        Previously defined in mapscene_types.h.
*/
typedef struct CompiledFace
{
    int     vertex_start;
    int     vertex_count;
    Vector3 normal;
    float   plane_dist;
    bool    is_visible;
    int     brush_idx;
} CompiledFace;

typedef struct CompiledBrush
{
    int face_start;
    int face_count;
} CompiledBrush;


typedef struct
MapSceneData
{
    MapData        *map;            /* Parsed .map (owned) */

    /* Flat geometry for the brush wireframe renderer */
    Vector3        *all_vertices;
    int             vertex_count;
    CompiledFace   *all_faces;
    int             face_count;
    CompiledBrush  *brushes;
    int             brush_count;

    /* GBSPLib output — model points into BSPModels[0], not owned */
    GBSP_Model     *model;
    bool            bsp_built;
    bool            has_leak;

    char            source_path[256];
}
MapSceneData;


void   DiagnoseBrushNormals(const CompiledBrush *brushes, int brush_count,
                             const CompiledFace  *faces,
                             const Vector3       *vertices);

Scene *MapScene_new(const char *map_path, Engine *engine);


#endif /* MAPSCENE_H */
