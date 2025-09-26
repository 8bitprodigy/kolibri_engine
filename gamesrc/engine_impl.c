#include "game.h"


EngineVTable engine_Callbacks = {
	NULL, 
	testEngineRun, 
	NULL, 
	NULL, 
	NULL, 
	NULL, 
	NULL, 
	testEngineExit,
	NULL
};


void
testEngineRun(Engine *engine)
{
	DisableCursor();
}

void
testEngineExit(Engine *engine)
{
	EnableCursor();
}


