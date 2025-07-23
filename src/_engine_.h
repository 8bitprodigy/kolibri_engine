#ifndef ENGINE_PRIVATE_H
#define ENGINE_PRIVATE_H

#include "common.h"
#include "engine.h"


Head       *Engine__getHeads(    Engine *engine);
void       *Engine__insertHead(  Engine *engine, Head       *head);
void       *Engine__removeHead(  Engine *engine, Head       *head);
EntityNode *Engine__getEntities( Engine *engine);
void       *Engine__insertEntity(Engine *engine, EntityNode *node);
void       *Engine__removeEntity(Engine *engine, EntityNode *node);
Scene      *Engine__getScene(    Engine *engine);
void       *Engine__insertScene( Engine *engine, Scene      *scene);
void       *Engine__removeScene( Engine *engine, Scene      *scene);


#endif /* ENGINE_PRIVATE_H */
