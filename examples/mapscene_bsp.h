#ifndef MAPSCENE_BSP_H
#define MAPSCENE_BSP_H

#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>

#include "mapscene.h"
#include "v220_map_parser.h"
#include "mapscene_types.h"

/*
 * BSP_Build - Build BSP tree with leaf classification
 * 
 * Current implementation (Stages 1-2):
 * 1. Builds BSP tree from all faces, partitioning space into leaves
 * 2. Classifies each leaf as SOLID (inside brush) or EMPTY (air/playable)
 * 
 * Returns NULL if memory allocation fails
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

/*
 * BSP_FloodFillAndDetectLeaks - Stage 3: QBSP-style leak detection
 * 
 * Algorithm:
 * 1. Generate portals between adjacent leaves
 * 2. Mark unbounded leaves as "outside" 
 * 3. Flood-fill from outside through EMPTY leaves
 * 4. Check if any point entities are in outside-marked leaves
 * 5. If leak: trace path from entity to outside for debugging
 * 6. If no leak: convert outside leaves to SOLID
 * 
 * Returns: true if no leak, false if leak detected
 * 
 * If leak is detected, call BSP_DrawLeakPath() in your render loop
 * to visualize the path from the leaked entity to the outside void.
 */
bool BSP_FloodFillAndDetectLeaks(BSPTree *tree, const MapData *map_data);

/* Draw the leak path (if one was detected) as a yellow line */
void BSP_DrawLeakPath(void);

/* Clean up Stage 3 resources (portals, leak path) */
void BSP_Stage3_Cleanup(void);

void BSP_Free(BSPTree *tree);

PlaneSide BSP_ClassifyPoint(Vector3 point, Vector3 plane_normal, float plane_dist);

PlaneSide BSP_ClassifyPolygon(
    const Vector3 *verts,
    int vert_count,
    Vector3 plane_normal,
    float plane_dist
);

/* Find which leaf contains the given point */
const BSPLeaf *BSP_FindLeaf(const BSPTree *tree, Vector3 point);

/* Get the contents (SOLID or EMPTY) of the leaf containing the point */
LeafContents BSP_GetPointContents(const BSPTree *tree, Vector3 point);

void BSP_PrintStats(const BSPTree *tree);

bool BSP_Validate(const BSPTree *tree);

/* Debug visualization: Draw leaf bounding boxes color-coded by contents
 * - GREEN: EMPTY leaves (playable space or void)
 * - RED: SOLID leaves (inside walls/floors/ceilings)
 * - BLUE: Reachable EMPTY leaves (Stage 3, not yet implemented)
 * 
 * Call this in your render loop to visualize the BSP classification.
 */
void BSP_DebugDrawLeafBounds(const BSPTree *tree);

#endif /* MAPSCENE_BSP_H */
