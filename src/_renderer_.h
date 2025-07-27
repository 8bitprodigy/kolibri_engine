#ifndef RENDERER_H
#define RENDERER_H


#include "common.h"
#include "engine.h"
#include "head.h"


typedef struct Renderer Renderer;

Renderer *Renderer__new( Engine   *engine,   RendererSettings *settings);
void      Renderer__free(Renderer *renderer);

void      Renderer__render(Renderer *renderer, EntityList *entities, Head *head);


#endif /* RENDERER_H */

