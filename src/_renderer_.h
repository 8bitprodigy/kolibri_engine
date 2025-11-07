#ifndef RENDERER_H
#define RENDERER_H


#include "common.h"
#include "engine.h"
#include "head.h"


typedef struct Renderer Renderer;


/* Constructor/Destructor */
Renderer *Renderer__new( Engine   *engine);
void      Renderer__free(Renderer *renderer);

/* Protected Methods */
Entity  **Renderer__queryFrustum(           Renderer *renderer, Head       *head,       float    max_distance, int     *visible_count);
void      Renderer__render(                 Renderer *renderer, EntityList *entities,   Head    *head);
void      Renderer__renderBruteForceFrustum(Renderer *renderer, EntityList *entities,   Head    *head) ;
void      Renderer__sortTransparent(        Renderer *renderer);
void      Renderer__submitGeometry(         Renderer *renderer, Renderable *renderable, Vector3  position,     Vector3  bounds,         bool is_entity);


#endif /* RENDERER_H */

