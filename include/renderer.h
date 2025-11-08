#ifndef RENDERER_H
#define RENDERER_H

#include "common.h"

typedef struct Renderer Renderer;


void Renderer_submitEntity(  Renderer *renderer, Entity     *entity);
void Renderer_submitGeometry(Renderer *renderer, Renderable *renderable, Vector3 pos, Vector3 bounds);


#endif /* RENDERER_H */
