#ifndef ENGINE_H
#define ENGINE_H

#include "common.h"


typedef void (*EngineCallback)(   Engine *engine);
typedef void (*EngineCallback_1f)(Engine *engine, float delta);

Engine   *Engine_new(
	EngineCallback    Setup_Callback, 
	EngineCallback    Run_Callback, 
	EngineCallback_1f Update_Callback, 
	EngineCallback    Render_Callback,
	EngineCallback    Exit_Callback,
	EngineCallback    Free_Callback
);
void      Engine_free(       Engine *engine);
void      Engine_run(        Engine *engine);
void      Engine_update(     Engine *engine, float dt);
void      Engine_render(     Engine *engine);
void      Engine_pause(      Engine *engine, bool  paused);
bool      Engine_isPaused(   Engine *engine);
void      Engine_requestExit(Engine *engine);
/*void      Engine_exit(       Engine *engine);*/


#endif /* ENGINE_H */
