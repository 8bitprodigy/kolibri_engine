#include "game.h"
#include <raylib.h>
//#include "menu.h"

void unPause(   void *data);
void exitToMain(void *data);

void engineRun(    Engine *engine);
void enginePause(  Engine *engine);
void engineUnpause(Engine *engine);
void engineExit(   Engine *engine);


EngineVTable engine_Callbacks = {
	.Setup   = NULL, 
	.Run     = engineRun, 
	.Update  = NULL, 
	.Render  = NULL, 
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
unPause(void *data)
{
	paused = false;
	Engine_pause((Engine*)data, false);
}

void
exitToMain(void *data)
{
	paused = false;
	Engine_requestExit((Engine*)data);
}

void
engineRun(Engine *engine)
{
	DisableCursor();
}

void
enginePause(Engine *engine)
{
	bool first_loop  = true;
	paused           = true;
	currentPauseMenu = &pauseMenu;

	pauseMenu = Menu( "Paused",
			(MenuItem){ "Return To Game", unPause,    engine},
			(MenuItem){ "Exit Game",      exitToMain, engine}
		);
	
	EnableCursor();
	initMouse();
	
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
				MENU_WIDTH,
				MENU_ITEM_HEIGHT,
				MENU_PADDING,
				GET_KEY_OR_BUTTON_AXIS_PRESSED(
						0, 
						GAMEPAD_BUTTON_LEFT_FACE_DOWN, 
						KEY_DOWN, 
						GAMEPAD_BUTTON_LEFT_FACE_UP, 
						KEY_UP
					),
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
	DisableCursor();
}

void
engineExit(Engine *engine)
{
	EnableCursor();
}
