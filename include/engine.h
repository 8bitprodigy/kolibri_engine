#ifndef ENGINE_H
#define ENGINE_H

#include "common.h"


typedef void (*EngineCallback)(   Engine *engine);
typedef void (*EngineCallback_1f)(Engine *engine, float delta);

/*
	CONSTRUCTOR/DESTRUCTOR
*/
Engine   *Engine_new(
	EngineCallback    Setup_Callback, 
	EngineCallback    Run_Callback, 
	EngineCallback_1f Update_Callback, 
	EngineCallback    Render_Callback,
	EngineCallback    Exit_Callback,
	EngineCallback    Free_Callback
);
void      Engine_free(       Engine *engine);

/*
	SETTERS/GETTERS
*/
EntityList *Engine_getEntityList(Engine *engine);

void Engine_setCallbacks(
    Engine            *engine,
    EngineCallback     Setup,
    EngineCallback     Run,
    EngineCallback_1f  update,
    EngineCallback     render,
    EngineCallback     Exit,
    EngineCallback     Free
);
void Engine_setCallbacksConditional(
    Engine            *engine,
    EngineCallback     Setup,
    EngineCallback     Run,
    EngineCallback_1f  update,
    EngineCallback     render,
    EngineCallback     Exit,
    EngineCallback     Free
);
#define Engine_setSetupCallback(  engine, callback) \
    Engine_setCallbacksConditional((engine), (callback), NULL, NULL, NULL, NULL, NULL, NULL)
#define Engine_setUpdateCallback( engine, callback) \
    Engine_setCallbacksConditional((engine), NULL, (callback), NULL, NULL, NULL, NULL, NULL)
#define Engine_setPreRenderCallback( engine, callback) \
    Engine_setCallbacksConditional((engine), NULL, NULL, (callback), NULL, NULL, NULL, NULL)
#define Engine_setRenderCallback( engine, callback) \
    Engine_setCallbacksConditional((engine), NULL, NULL, NULL, (callback), NULL, NULL, NULL)
#define Engine_setPostRenderCallback(engine, callback) \
    Engine_setCallbacksConditional((engine), NULL, NULL, NULL, NULL, (callback), NULL, NULL)
#define Engine_setExitCallback(   engine, callback) \
    Engine_setCallbacksConditional((engine), NULL, NULL, NULL, NULL, NULL, (callback), NULL)
#define Engine_setFreeCallback(   engine, callback) \
    Engine_setCallbacksConditional((engine), NULL, NULL, NULL, NULL, NULL, NULL, (callback))


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
