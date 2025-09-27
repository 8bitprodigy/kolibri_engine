#include "game.h"
#include "menu.h"
#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"


Entity *player;
TestHeadData *head_data;


void 
runEngine(void *data)
{
	Engine_run((Engine*)data, 0);
}

void 
closeAll(void *data)
{
	EndDrawing();
	Engine_requestExit((Engine*)data);
	CloseWindow();
}



int
main(void)
{
	
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Kolibri Engine Test");
	
	Engine *engine = Engine_new(&engine_Callbacks);
	Head   *head   = Head_new(
			0, 
			(Region){0,0,SCREEN_WIDTH, SCREEN_HEIGHT}, 
			&head_Callbacks, 
			engine
		);
	
	RendererSettings *settings = Head_getRendererSettings(head);
//	settings->frustum_culling = false;
	Camera3D *cam = Head_getCamera(head);
	cam->fovy     = 45.0f;
	cam->up       = V3_UP;
	cam->position = (Vector3){0.0f, 1.75f, 0.0f};
	cam->target   = (Vector3){10.0f, 0.0f, 10.0f};

	Scene_new(&scene_Callbacks, NULL, engine);
	
	Entity *ents[21][21][2];
	int z = 0;
	for (int x = 0; x < 21; x++) {
		for (int y = 0; y < 21; y++) {
//			for (int z = 0; z < 21; z++) {
				ents[x][y][z]           = Entity_new(&entityTemplate, engine);
				ents[x][y][z]->visible  = true;
				ents[x][y][z]->active   = true;
				ents[x][y][z]->position = (Vector3){
					(x * 5.0f) - 50.0f,
					(z * 5.0f),
					(y * 5.0f) - 50.0f
				};
//			}
		}
	}

	player    = Entity_new(&playerTemplate, engine);
	player->position = (Vector3){2.0f, 0.0f, 2.0f};
	head_data = (TestHeadData*)Head_getUserData(head);

	head_data->target     = player;
	head_data->eye_height = 1.75f;
	
	MenuItem mainMenu[] = {
			{ "Run",     runEngine, engine},
			{ "Options", NULL,        NULL},
			{ "Exit",    CloseWindow, NULL}
		};
	const size_t numMenuItems = sizeof(mainMenu)/sizeof(mainMenu[0]);
	int selection = -1;
	
	while (!WindowShouldClose()) {
		BeginDrawing();
			ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

			int axis;
			if (
				axis = GET_KEY_OR_BUTTON_AXIS_PRESSED(
						0, 
						GAMEPAD_BUTTON_LEFT_FACE_DOWN, 
						KEY_DOWN, 
						GAMEPAD_BUTTON_LEFT_FACE_UP, 
						KEY_UP
					)
			)
			{
				selection += axis;
				if (selection    <  0)         selection = numMenuItems - 1;
				if (numMenuItems <= selection) selection = 0;
			}
			
			Menu(
				mainMenu,
				numMenuItems,
				(Vector2i){SCREEN_WIDTH, SCREEN_HEIGHT},
				(Vector2i){220, 30},
				10,
				&selection,
				GET_KEY_OR_BUTTON_PRESSED(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, KEY_ENTER)
			);
		EndDrawing();
	}
	

	CloseWindow();
	return 0;
}
