#ifndef RENDERER_H
#define RENDERER_H


#include "engine.h"
#include "head.h"
#include "renderer.h"

typedef struct Renderer Renderer;

/* Constructor/Destructor */
Renderer *Renderer__new( Engine   *engine);
void      Renderer__free(Renderer *renderer);

/* Protected Methods */
//RenderableWrapper **Renderer__queryFrustum(           Renderer *renderer,  Head       *head,       float    max_distance, int     *visible_count);
void Renderer__render(         Renderer *renderer,  Head       *head);


#endif /* RENDERER_H */

