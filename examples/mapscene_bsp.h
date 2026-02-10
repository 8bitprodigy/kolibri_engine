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
 * TODO (Stages 3-4):
 * 3. Validate info_player_start exists and is in EMPTY space
 * 4. Flood-fill from player start through EMPTY leaves to mark reachable
 * 5. Cull faces not bordering reachable EMPTY space
 * 
 * Returns NULL if memory allocation fails
 */
BSPTree *BSP_Build(
/*
    const CompiledFace  *compiled_faces,
    int                  face_count,
    const Vector3       *vertices,
    int                  vertex_count,
    const CompiledBrush *brushes,
    int                  brush_count,
*/
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

/* Find which leaf contains the given point */
const BSPLeaf *BSP_FindLeaf(const BSPTree *tree, Vector3 point);

/* Get the contents (SOLID or EMPTY) of the leaf containing the point */
LeafContents BSP_GetPointContents(const BSPTree *tree, Vector3 point);

void BSP_PrintStats(const BSPTree *tree);

bool BSP_Validate(const BSPTree *tree);

/* Debug visualization: Draw leaf bounding boxes color-coded by contents
 * - GREEN: EMPTY leaves (playable space or void)
 * - RED: SOLID leaves (inside walls/floors/ceilings)
 * - YELLOW: Outside leaves (touching world boundary)
 * - CYAN: Flooded leaves (void-connected)
 * 
 * Call this in 3D rendering mode.
 */
void BSP_DebugDrawLeafBounds(const BSPTree *tree);

/* Debug visualization: Draw text overlay with BSP stats
 * Shows node/leaf counts, SOLID/EMPTY/OUTSIDE/FLOODED counts, and color legend.
 * 
 * Call this in 2D rendering mode (after EndMode3D, before EndDrawing).
 */
void BSP_DebugDrawText(const BSPTree *tree);

/* Debug visualization: Draw leak path with wireframe spheres
 * - RED sphere: Leaked entity position
 * - YELLOW sphere: Outside void destination
 * - ORANGE spheres: Intermediate steps
 * - MAGENTA lines: Path connections
 * 
 * Only draws if tree->has_leak is true. Call this in 3D rendering mode.
 */
void BSP_DebugDrawLeak(const BSPTree *tree);

/* Debug visualization: Draw leak text overlay
 * Shows "LEAK DETECTED!" warning and path information.
 * 
 * Only draws if tree->has_leak is true. Call this in 2D rendering mode.
 */
void BSP_DebugDrawLeakText(const BSPTree *tree);

#endif /* MAPSCENE_BSP_H */
