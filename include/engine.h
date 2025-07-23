#ifndef ENGINE_H
#define ENGINE_H

#include "common.h"


typedef void (*EngineCallback)(   Engine *engine);
typedef void (*EngineCallback_1f)(Engine *engine, float delta);

typedef struct
EngineVTable
{
    EngineCallback     Setup;
    EngineCallback     Run;
    EngineCallback_1f  update;
    EngineCallback     render;
    EngineCallback     Exit;
    EngineCallback     Free;
}
EngineVTable;


/*
	CONSTRUCTOR/DESTRUCTOR
*/
Engine *Engine_new( EngineVTable  vtable);
void    Engine_free(Engine       *engine);

/*
	SETTERS/GETTERS
*/
EntityList *Engine_getEntityList(Engine *engine);

void          Engine_setVTable(Engine *engine, EngineVTable *vtable);
EngineVTable *Engine_getVTable(Engine *engine);


/*
	METHODS
*/
void      Engine_run(        Engine *engine);
void      Engine_update(     Engine *engine, float dt);
void      Engine_render(     Engine *engine);
void      Engine_pause(      Engine *engine, bool  paused);
bool      Engine_isPaused(   Engine *engine);
void      Engine_requestExit(Engine *engine);
/*void      Engine_exit(       Engine *engine);*/


#endif /* ENGINE_H */
