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
*/
typedef struct CompiledFace
{
    int     vertex_start;
    int     vertex_count;
    Vector3 normal;
    float   plane_dist;
    bool    is_visible;
    int     brush_idx;
}
CompiledFace;

typedef struct CompiledBrush
{
    int face_start;
    int face_count;
}
CompiledBrush;


typedef struct
MapSceneData
{
    MapData        *map;            /* Parsed .map (owned)                   */

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

    /*
        Leak state
        ----------
        has_leak is set by build_bsp_from_mapdata when ProcessWorldModel
        detects that the map is not sealed.

        leak_entity is the engine-space origin of the entity whose position
        was outside the sealed volume (the flood fill start point).

        leak_path is an optional malloc'd array of engine-space waypoints
        tracing the path from the leaking entity back to the void.
        It may be NULL if GBSPLib does not expose path data.
        leak_path_count is the number of Vector3 entries in that array.

        mapscene_Free releases leak_path.
    */
    bool            has_leak;
    Vector3         leak_entity;
    Vector3        *leak_path;
    int             leak_path_count;

    char            source_path[256];
}
MapSceneData;


void   DiagnoseBrushNormals(const CompiledBrush *brushes, int brush_count,
                             const CompiledFace  *faces,
                             const Vector3       *vertices);



Scene *MapScene_new(const char *map_path, Engine *engine);


#endif /* MAPSCENE_H */
