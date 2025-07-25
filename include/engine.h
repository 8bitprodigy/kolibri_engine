#ifndef ENGINE_H
#define ENGINE_H

#include "common.h"


typedef void (*EngineCallback)(      Engine *engine);
typedef void (*EngineUpdateCallback)(Engine *engine, float delta);
typedef void (*EngineResizeCallback)(Engine *engine, uint width, uint height);

typedef struct
EngineVTable
{
    EngineCallback       Setup;
    EngineCallback       Run;
    EngineUpdateCallback Update;
    EngineCallback       Render;
    EngineResizeCallback Resize;
    EngineCallback       Exit;
    EngineCallback       Free;
}
EngineVTable;


/*
	CONSTRUCTOR/DESTRUCTOR
*/
Engine *Engine_new( EngineVTable *vtable);
void    Engine_free(Engine       *engine);

/*
	SETTERS/GETTERS
*/
EntityList *Engine_getEntityList(Engine *engine);
Head       *Engine_getHeads(     Engine *engine);

void          Engine_setVTable(Engine *engine, EngineVTable *vtable);
EngineVTable *Engine_getVTable(Engine *engine);


/*
	METHODS
*/
void      Engine_run(        Engine *engine);
void      Engine_update(     Engine *engine, float dt);
void      Engine_render(     Engine *engine);
void      Engine_resize(     Engine *engine, uint  width,  uint height);
void      Engine_pause(      Engine *engine, bool  paused);
bool      Engine_isPaused(   Engine *engine);
void      Engine_requestExit(Engine *engine);
/*void      Engine_exit(       Engine *engine);*/


#endif /* ENGINE_H */
