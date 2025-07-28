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
void      Renderer__render(Renderer *renderer, EntityList *entities, Head *head);
Entity  **Renderer__queryFrustum(Renderer *renderer, Head *head, float max_distance, int *visible_count);


#endif /* RENDERER_H */

