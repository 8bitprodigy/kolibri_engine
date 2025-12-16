#ifndef RENDERER_H
#define RENDERER_H

#include "common.h"

typedef struct Renderer Renderer;


bool
isSphereInFrustum(
        Vector3  center, 
        float    radius, 
        Frustum *frustum,
        float    dist_sq,
        float    max_distance
    );
bool
isAABBInFrustum(
        Vector3  center,
        Vector3  extents,
        Frustum *frustum,
        float    dist_sq,
        float    max_distance
    );

void Renderer_submitEntity(  Renderer *renderer, Entity     *entity);
void Renderer_submitGeometry(Renderer *renderer, Renderable *renderable, Vector3 pos, Vector3 bounds);


#endif /* RENDERER_H */
