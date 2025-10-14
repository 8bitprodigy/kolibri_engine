#ifndef ENGINE_H
#define ENGINE_H

#include "common.h"


typedef void (*EngineCallback)(      Engine *engine);
typedef void (*EngineUpdateCallback)(Engine *engine, float delta);
typedef void (*EngineResizeCallback)(Engine *engine, uint width, uint height);

typedef struct
EngineVTable
{
    EngineCallback       Setup;   /* Called upon creating a new Engine */
    EngineCallback       Run;     /* Called at the beginning of Engine_run() */
    EngineUpdateCallback Update;  /* Called once every frame after updating all Entities and Heads */
    EngineCallback       Render;  /* Called once every frame after rendering the scene */
    EngineResizeCallback Resize;  /* Called upon window resize */
    EngineCallback       Pause;   /* Called on pausing the Engine */
    EngineCallback       Unpause; /* Called on unpausing the Engine */ 
    EngineCallback       Exit;    /* Called at end of Engine_run(), after exiting the main loop */
    EngineCallback       Free;    /* Called upon freeing Engine from Memory  with Engine_free()*/
}
EngineVTable;


/*
	CONSTRUCTOR/DESTRUCTOR
*/
Engine *Engine_new( EngineVTable *vtable, int tick_rate);
void    Engine_free(Engine       *engine);

/*
	SETTERS/GETTERS
*/
float       Engine_getDeltaTime(  Engine *engine);
uint64      Engine_getFrameNumber(Engine *engine);
float       Engine_getTickElapsed(Engine *engine);
void        Engine_setTickRate(   Engine *engine, int tick_rate);
EntityList *Engine_getEntityList( Engine *engine);
Head       *Engine_getHeads(      Engine *engine);
Scene      *Engine_getScene(      Engine *engine);

void          Engine_setVTable(   Engine *engine, EngineVTable *vtable);
EngineVTable *Engine_getVTable(   Engine *engine);


/*
	METHODS
*/
void      Engine_run(        Engine *engine);
void      Engine_update(     Engine *engine);
void      Engine_render(     Engine *engine);
void      Engine_resize(     Engine *engine, uint  width,  uint height);
void      Engine_pause(      Engine *engine, bool  paused);
bool      Engine_isPaused(   Engine *engine);
void      Engine_requestExit(Engine *engine);


#endif /* ENGINE_H */
