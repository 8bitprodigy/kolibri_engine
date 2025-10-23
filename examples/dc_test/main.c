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
    InitWindow(640, 480, "Test");
    SetTargetFPS(60);
    
    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);
            DrawRectangle(50, 50, 300, 200, RED);
            DrawCircle(400, 300, 50, GREEN);
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}
