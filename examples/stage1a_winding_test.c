/* Stage 1a: Winding Building Test
 * 
 * Goal: Build windings for a SINGLE brush and visualize them
 * 
 * This is a standalone test function you can call from your main loop.
 * Press a key to cycle through brushes and see their windings.
 */

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "v220_map_parser.h"

#define EPSILON 0.01f
#define MAX_WINDING_POINTS 64

typedef struct {
    int numpoints;
    Vector3 points[MAX_WINDING_POINTS];
} winding_t;

/* Stage 1a: Build winding for ONE face of ONE brush */
static winding_t*
Stage1a_BuildFaceWinding(const MapBrush *brush, int face_idx)
{
    printf("\n[Stage 1a] Building winding for face %d:\n", face_idx);
    
    const MapPlane *face_plane = &brush->planes[face_idx];
    
    printf("  Face plane: normal=(%.2f, %.2f, %.2f), dist=%.2f\n",
           face_plane->normal.x, face_plane->normal.y, face_plane->normal.z,
           face_plane->distance);
    
    /* Step 1: Create a HUGE quad on this face's plane */
    Vector3 normal = face_plane->normal;
    float dist = face_plane->distance;
    
    /* Find perpendicular vectors */
    Vector3 up = {0, 0, 1};
    if (fabsf(normal.z) > 0.9f) {
        up = (Vector3){1, 0, 0};
    }
    
    float d = Vector3DotProduct(up, normal);
    Vector3 tangent1 = Vector3Normalize((Vector3){
        up.x - normal.x * d,
        up.y - normal.y * d,
        up.z - normal.z * d
    });
    
    Vector3 tangent2 = Vector3CrossProduct(normal, tangent1);
    
    Vector3 center = Vector3Scale(normal, dist);
    
    /* Make huge quad */
    float size = 32768.0f;
    winding_t *w = malloc(sizeof(winding_t));
    w->numpoints = 4;
    w->points[0] = Vector3Add(center, Vector3Add(Vector3Scale(tangent1, size), Vector3Scale(tangent2, size)));
    w->points[1] = Vector3Add(center, Vector3Add(Vector3Scale(tangent1, -size), Vector3Scale(tangent2, size)));
    w->points[2] = Vector3Add(center, Vector3Add(Vector3Scale(tangent1, -size), Vector3Scale(tangent2, -size)));
    w->points[3] = Vector3Add(center, Vector3Add(Vector3Scale(tangent1, size), Vector3Scale(tangent2, -size)));
    
    printf("  Created base winding with %d points\n", w->numpoints);
    
    /* Step 2: Clip against all OTHER planes of the brush */
    for (int i = 0; i < brush->plane_count; i++) {
        if (i == face_idx) continue;
        
        const MapPlane *clip_plane = &brush->planes[i];
        
        printf("  Clipping against plane %d: normal=(%.2f, %.2f, %.2f), dist=%.2f\n",
               i, clip_plane->normal.x, clip_plane->normal.y, clip_plane->normal.z,
               clip_plane->distance);
        
        /* QUESTION: Which side of the plane do we keep?
         * 
         * The brush is the INTERSECTION of half-spaces.
         * Each plane's normal points OUTWARD from the brush.
         * So we want to keep points that are BEHIND the plane (inside).
         * 
         * dot(point, normal) - dist < 0 means BEHIND (inside)
         * dot(point, normal) - dist > 0 means IN FRONT (outside)
         */
        
        /* Classify all points */
        int front_count = 0, back_count = 0;
        float dists[MAX_WINDING_POINTS + 1];
        
        for (int p = 0; p < w->numpoints; p++) {
            dists[p] = Vector3DotProduct(w->points[p], clip_plane->normal) - clip_plane->distance;
            
            if (dists[p] > EPSILON) front_count++;
            else if (dists[p] < -EPSILON) back_count++;
        }
        dists[w->numpoints] = dists[0];
        
        printf("    Classification: %d front, %d back\n", front_count, back_count);
        
        /* All points in front (outside) - winding is completely outside this plane */
        if (back_count == 0) {
            printf("    -> Winding completely outside, CLIPPED AWAY\n");
            free(w);
            return NULL;
        }
        
        /* All points behind (inside) - no clipping needed */
        if (front_count == 0) {
            printf("    -> Winding completely inside, no clip needed\n");
            continue;
        }
        
        /* Winding crosses plane - clip it */
        printf("    -> Winding crosses plane, clipping...\n");
        
        winding_t *new_w = malloc(sizeof(winding_t));
        new_w->numpoints = 0;
        
        for (int p = 0; p < w->numpoints; p++) {
            Vector3 p1 = w->points[p];
            Vector3 p2 = w->points[(p + 1) % w->numpoints];
            float d1 = dists[p];
            float d2 = dists[p + 1];
            
            /* p1 is behind - keep it */
            if (d1 <= EPSILON) {
                new_w->points[new_w->numpoints++] = p1;
            }
            
            /* Edge crosses plane - add intersection */
            if ((d1 > EPSILON) != (d2 > EPSILON)) {
                float t = d1 / (d1 - d2);
                Vector3 mid = {
                    p1.x + t * (p2.x - p1.x),
                    p1.y + t * (p2.y - p1.y),
                    p1.z + t * (p2.z - p1.z)
                };
                new_w->points[new_w->numpoints++] = mid;
            }
        }
        
        free(w);
        w = new_w;
        
        printf("    -> Result: %d points\n", w->numpoints);
        
        if (w->numpoints < 3) {
            printf("    -> Degenerate winding, CLIPPED AWAY\n");
            free(w);
            return NULL;
        }
    }
    
    printf("  FINAL WINDING: %d points\n", w->numpoints);
    return w;
}

/* Visualize a single brush's windings */
void Stage1a_VisualizeBrush(const MapBrush *brush, int brush_idx)
{
    printf("\n=== Stage 1a: Testing Brush %d ===\n", brush_idx);
    printf("Brush has %d faces\n", brush->plane_count);
    
    BeginDrawing();
    ClearBackground(BLACK);
    
    /* Draw each face winding */
    for (int f = 0; f < brush->plane_count; f++) {
        winding_t *w = Stage1a_BuildFaceWinding(brush, f);
        
        if (w) {
            /* Draw winding as wireframe */
            Color colors[] = {RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN};
            Color color = colors[f % 6];
            
            for (int i = 0; i < w->numpoints; i++) {
                int next = (i + 1) % w->numpoints;
                DrawLine3D(w->points[i], w->points[next], color);
            }
            
            /* Draw face normal */
            Vector3 center = {0, 0, 0};
            for (int i = 0; i < w->numpoints; i++) {
                center.x += w->points[i].x;
                center.y += w->points[i].y;
                center.z += w->points[i].z;
            }
            center.x /= w->numpoints;
            center.y /= w->numpoints;
            center.z /= w->numpoints;
            
            Vector3 normal_end = Vector3Add(center, Vector3Scale(brush->planes[f].normal, 0.5f));
            DrawLine3D(center, normal_end, WHITE);
            
            free(w);
        }
    }
    
    /* Draw text info */
    DrawText(TextFormat("Brush %d: %d faces", brush_idx, brush->plane_count), 10, 10, 20, WHITE);
    DrawText("Press SPACE for next brush, ESC to exit", 10, 40, 20, WHITE);
    
    EndDrawing();
}

/* Test harness - call this from your main loop */
void Stage1a_Test(const MapData *map_data)
{
    static int current_brush = 0;
    
    if (IsKeyPressed(KEY_SPACE)) {
        current_brush = (current_brush + 1) % map_data->world_brush_count;
    }
    
    if (current_brush < map_data->world_brush_count) {
        Stage1a_VisualizeBrush(&map_data->world_brushes[current_brush], current_brush);
    }
}
