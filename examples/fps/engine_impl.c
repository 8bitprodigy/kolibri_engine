#include "game.h"
#include "../menu.h"


void unPause(   void *data, void *value);
void exitToMain(void *data, void *value);

void engineRun(    Engine *engine);
void engineRender( Engine *engine);
void enginePause(  Engine *engine);
void engineUnpause(Engine *engine);
void engineExit(   Engine *engine);


EngineVTable engine_Callbacks = {
	.Setup   = NULL, 
	.Run     = engineRun, 
	.Update  = NULL, 
	.Render  = engineRender, 
	.Resize  = NULL, 
	.Pause   = enginePause, 
	.Unpause = engineUnpause, 
	.Exit    = engineExit,
	.Free    = NULL
};

bool paused;
int pauseSelection;
Menu 
	*currentPauseMenu,
	 pauseMenu;


void
unPause(void *data, void *value)
{
	(void)value;
	paused = false;
	Engine_pause((Engine*)data, false);
}

void
exitToMain(void *data, void *value)
{
	(void)value;
	paused = false;
	Engine_requestExit((Engine*)data);
	Engine_pause((Engine*)data, paused);
}

void
engineRun(Engine *engine)
{
	(void)engine;
	DisableCursor();
}

void
engineRender(Engine *engine)
{
	(void)engine;
	DrawFPS(10, 10);
}

void
enginePause(Engine *engine)
{
	bool first_loop  = true;
	paused           = true;
	currentPauseMenu = &pauseMenu;

	pauseMenu = Menu( "Paused",
			MENU_WIDTH,
			MENU_ITEM_HEIGHT,
			MENU_PADDING,
			MenuButton( "Return To Game", unPause,    engine),
			MenuButton( "Exit Game",      exitToMain, engine)
		);
	
	EnableCursor();
	HandleMouse();
	
	while(paused) {
		BeginDrawing();
			if (
				!first_loop
				&& GET_KEY_OR_BUTTON_PRESSED(0, GAMEPAD_BUTTON_MIDDLE, KEY_ESCAPE)
			)
			{
				paused = false;
				Engine_pause(engine, false);
			}
			
			Engine_render(engine);
			Menu_draw(
				currentPauseMenu,
				GetScreenWidth(),
				GetScreenHeight(),
				GET_KEY_OR_BUTTON_AXIS_PRESSED(
						0, 
						GAMEPAD_BUTTON_LEFT_FACE_DOWN, 
						KEY_DOWN, 
						GAMEPAD_BUTTON_LEFT_FACE_UP, 
						KEY_UP
					),
				0,0,
				GET_KEY_OR_BUTTON_PRESSED(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, KEY_ENTER)
			);
		EndDrawing();
		first_loop = false;
	}
	DisableCursor();
}

void
engineUnpause(Engine *engine)
{
	(void)engine;
	DisableCursor();
}

void
engineExit(Engine *engine)
{
	(void)engine;
	Engine_free(engine);
	EnableCursor();
	HandleMouse();
}
