#include "game.h"


EngineVTable engine_Callbacks = {
	NULL, 
	testEngineRun, 
	NULL, 
	testEngineComposite, 
	NULL, 
	NULL, 
	NULL, 
	NULL,
	NULL
};


void
testEngineRun(Engine *engine)
{
	DisableCursor();
}


void
testEngineComposite(Engine *engine)
{
	Head *head = Engine_getHeads(engine);
	RenderTexture *rt = Head_getViewport(head);
	TestHeadData *data = Head_getUserData(head);
	
	Vector2 pos = (Vector2){
			(SCREEN_WIDTH/2)  - (rt->texture.width/2),
			(SCREEN_HEIGHT/2) - (rt->texture.height/2),
		};
	ClearBackground(BLACK);
	DrawTextureRec((*Head_getViewport(head)).texture, Head_getRegion(head), pos, WHITE);
}

