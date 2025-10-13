#include <raylib.h>
#include <stdlib.h>

#include "game.h"
#define RAYGUI_IMPLEMENTATION
#define MENU_IMPLEMENTATION
#include "../menu.h"


#ifdef __PSP__
	#include <pspsdk.h>
	PSP_MODULE_INFO("TEST", 0, 1, 0);
#endif


void switchMenu(void *data, void *value);
void runEngine( void *data, void *value);
void closeAll  (void *data, void *value);

Engine       *engine;
Entity       *player;
TestHeadData *head_data;
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
	Engine_run((Engine*)data);
}

void 
closeAll(void *data, void *value)
{
	Engine_requestExit((Engine*)data);
	readyToClose = true;
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
	SetTargetFPS(frame_rate);
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Kolibri Engine Test");
	
	initMouse();
	
	engine = Engine_new(&engine_Callbacks, tick_rate);
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
/* 
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
*/
	player    = Entity_new(&playerTemplate, engine);
	player->position = (Vector3){0.0f, 0.0f, 0.0f};
	head_data = (TestHeadData*)Head_getUserData(head);

	head_data->target      = player;
	head_data->target_data = player->user_data;
	head_data->eye_height  = 1.75f;

	mainMenu = Menu( "Main Menu",
			MENU_WIDTH,
			MENU_ITEM_HEIGHT,
			MENU_PADDING,
			MenuButton( "Run",        runEngine,  engine),
			MenuButton( "Options...", switchMenu, &optionsMenu),
			MenuButton( "Exit",       closeAll,   engine)
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
			ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
			
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
