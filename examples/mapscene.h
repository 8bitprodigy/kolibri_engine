#ifndef MAPSCENE_H
#define MAPSCENE_H

#include "scene.h"
#include "mapscene_bsp.h"
#include "mapscene_types.h"
#include "v220_map_parser.h"



extern SceneVTable MapScene_vtable;
typedef struct MapSceneData MapSceneData;


/* Scene-local state.  Mirrors how infinite_plane_scene keeps debug_texture
 * as a file-scope global — this is the established pattern. */
typedef struct
MapSceneData
{
    MapData       *map;             /* Raw parsed .map data (owned) */

    /* Flat storage arrays */
    Vector3       *all_vertices;
    int            vertex_count;
    CompiledFace  *all_faces;
    int            face_count;
    /* Brush metadata */
    CompiledBrush *brushes;         /* Brush metadata with indices (owned) */
    int            brush_count;

    BSPTree       *bsp_tree;
    
    char           source_path[256];
}
MapSceneData;

/*
	Constructor
*/
Scene *MapScene_new(const char *map_path, Engine *engine);


#endif /* MAPSCENE_H */
