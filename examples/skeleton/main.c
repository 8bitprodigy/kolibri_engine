#include <raylib.h>
#include <stdlib.h>

#include "game.h"
#define RAYGUI_IMPLEMENTATION
#define MENU_IMPLEMENTATION
#include "../menu.h"


void switchMenu(void *data, void *value);
void runEngine( void *data, void *value);
void closeAll  (void *data, void *value);


bool          readyToClose;
Menu
	*currentMenu,
	 mainMenu,
	 optionsMenu;
int
	tick_rate,
	frame_rate,
	screen_width,
	screen_height;


void
switchMenu(void *data, void *value)
{
	currentMenu = (Menu*)data;
	currentMenu->selection   = -1;
}

void 
runEngine(void *data, void *value)
{

}

void 
closeAll(void *data, void *value)
{

}

void
handleArgs(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		char *c = &argv[i][2];
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 't':
				tick_rate = atoi(c);
				break;
			case 'f':
				frame_rate = atoi(c);
				if (0 < frame_rate) SetTargetFPS(frame_rate);
				break;
			case 'w':
				screen_width = atoi(c);
				break;
			case 'h':
				screen_height = atoi(c);
				break;
			default:
				printf("Malformed argument %c\n", argv[i][1]);
			}
		}
		else printf("Malformed argument %c\n", argv[i][0]);
	}
}

int
main(int argc, char **argv)
{
	readyToClose = false;
	
	tick_rate     = DEFAULT_TICK_RATE,
	frame_rate    = DEFAULT_FRAME_RATE;
	screen_width  = SCREEN_WIDTH;
	screen_height = SCREEN_HEIGHT;

	handleArgs(argc, argv);

#ifndef ON_CONSOLE
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
#endif /* ON_CONSOLE */

	InitWindow(screen_width, screen_height, WINDOW_TITLE);

	HandleMouse();

	mainMenu = Menu( "Main Menu",
			MENU_WIDTH,
			MENU_ITEM_HEIGHT,
			MENU_PADDING,
			MenuButton( "Run",        runEngine,  NULL),
			MenuButton( "Options...", switchMenu, &optionsMenu),
#ifndef ON_CONSOLE
			MenuButton( "Exit",       closeAll,   NULL)
#endif
		),
	optionsMenu = Menu( "Options",
			MENU_WIDTH,
			MENU_ITEM_HEIGHT,
			MENU_PADDING,
			MenuButton( "Back...", switchMenu, &mainMenu ),
			MenuLabel(  "This is a label." ),
			MenuButton( "Back...", switchMenu, &mainMenu )
		);
	currentMenu = &mainMenu;

	while (!readyToClose) {
		readyToClose = WindowShouldClose();
		BeginDrawing();
			ClearBackground(WHITE);

			Menu_draw(
					currentMenu,
					GetScreenWidth(),
					GetScreenHeight(),
					GET_KEY_OR_BUTTON_AXIS_PRESSED(
							0,
							GAMEPAD_BUTTON_LEFT_FACE_DOWN, 
							KEY_DOWN, 
							GAMEPAD_BUTTON_LEFT_FACE_UP, 
							KEY_UP
						),
					0, 0,
					GET_KEY_OR_BUTTON_PRESSED(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, KEY_ENTER)
				);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
