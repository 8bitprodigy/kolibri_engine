#ifndef ENGINE_PRIVATE_H
#define ENGINE_PRIVATE_H


#include "_entity_.h"
#include "_head_.h"
#include "_scene_.h"
#include "_renderer_.h"

#include "common.h"
#include "engine.h"


Head           *Engine__getHeads(         Engine *engine);
void            Engine__insertHead(       Engine *engine, Head           *head);
void            Engine__removeHead(       Engine *engine, Head           *head);
Scene          *Engine__getScene(         Engine *engine);
void            Engine__insertScene(      Engine *engine, Scene          *scene);
void            Engine__removeScene(      Engine *engine, Scene          *scene);
Renderer       *Engine__getRenderer(      Engine *engine);


#endif /* ENGINE_PRIVATE_H */
