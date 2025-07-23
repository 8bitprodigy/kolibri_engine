#ifndef ENGINE_PRIVATE_H
#define ENGINE_PRIVATE_H

#include "common.h"
#include "engine.h"


Head       *Engine__getHeads(   Engine *engine);
void       *Engine__addEntity(  Engine *engine, EntityNode *node);
EntityNode *Engine__getEntities(Engine *engine);
Scene      *Engine__getScene(   Engine *engine);


#endif /* ENGINE_PRIVATE_H */
