#include <raylib.h>
#include <stdlib.h>

#include "game.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"


void switchMenu(void *data, void *value);
void runEngine( void *data, void *value);
void closeAll  (void *data, void *value);

bool readyToClose;
int
	tick_rate,
	frame_rate,
	screen_width,
	screen_height;


int
main(int argc, char **argv)
{
	readyToClose = false;
	
	tick_rate     = DEFAULT_TICK_RATE,
	frame_rate    = DEFAULT_FRAME_RATE;
	screen_width  = SCREEN_WIDTH;
	screen_height = SCREEN_HEIGHT;

	SetTargetFPS(frame_rate);
	InitWindow(screen_width, screen_height, "Kolibri Engine Test");
	
	initMouse();
	
	while (!readyToClose) {
		readyToClose = WindowShouldClose();
		BeginDrawing();
			ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
			DrawRectangle( 50, 50, 300, 200, RED);
			if (GuiButton(
					(Rectangle) {
							20,
							20,
							MENU_WIDTH,
							MENU_ITEM_HEIGHT
						},
					"test"
				)
			) {
				
			}
		EndDrawing();
	}

	
	CloseWindow();
	return 0;
}
