#ifndef MAPSCENE_BSP_H
#define MAPSCENE_BSP_H

#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>

#include "mapscene.h"
#include "v220_map_parser.h"
#include "mapscene_types.h"

/*
 * BSP_Build - Build BSP tree with proper inside/outside determination
 * 
 * Algorithm:
 * 1. Validates info_player_start exists and is in playable space
 * 2. Builds BSP tree from all faces, partitioning space into leaves
 * 3. Classifies each leaf as SOLID or EMPTY
 * 4. Flood-fills from player start through EMPTY leaves
 * 5. Culls faces not bordering reachable EMPTY space
 * 
 * Returns NULL if:
 * - No info_player_start found
 * - Player start is inside solid geometry
 * - Memory allocation fails
 */
BSPTree *BSP_Build(
    const CompiledFace  *compiled_faces,
    int                  face_count,
    const Vector3       *vertices,
    int                  vertex_count,
    const CompiledBrush *brushes,
    int                  brush_count,
    const MapData       *map_data
);

void BSP_Free(BSPTree *tree);

PlaneSide BSP_ClassifyPoint(Vector3 point, Vector3 plane_normal, float plane_dist);

PlaneSide BSP_ClassifyPolygon(
    const Vector3 *verts,
    int vert_count,
    Vector3 plane_normal,
    float plane_dist
);

const BSPLeaf *BSP_FindLeaf(const BSPTree *tree, Vector3 point);

void BSP_PrintStats(const BSPTree *tree);

bool BSP_Validate(const BSPTree *tree);

#endif /* MAPSCENE_BSP_H */
