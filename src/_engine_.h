#ifndef ENGINE_PRIVATE_H
#define ENGINE_PRIVATE_H

#include "common.h"
#include "engine.h"


void    Engine__addHead(    Engine *engine, Head *head);
Entity *Engine__getEntities(Engine *engine);
Scene  *Engine__getScene(   Engine *engine);


#endif /* ENGINE_PRIVATE_H */
