#include <raylib.h>

#include "game.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"


#ifdef __PSP__
	#include <pspsdk.h>
	PSP_MODULE_INFO("TEST", 0, 1, 0);
#endif


void switchMenu(void *data);
void runEngine( void *data);
void closeAll  (void *data);

Engine       *engine;
Entity       *player;
TestHeadData *head_data;
bool          readyToClose;

Menu
	*currentMenu,
	 mainMenu,
	 optionsMenu;


void
switchMenu(void *data)
{
	currentMenu = (Menu*)data;
	currentMenu->selection   = -1;
}

void 
runEngine(void *data)
{
	Engine_run((Engine*)data);
}

void 
closeAll(void *data)
{
	Engine_requestExit((Engine*)data);
	readyToClose = true;
}



int
main(void)
{
	readyToClose = false;


	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
	//SetTargetFPS(180);
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Kolibri Engine Test");
	
	initMouse();
	
	engine = Engine_new(&engine_Callbacks);
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
	player->position = (Vector3){2.0f, 0.0f, 2.0f};
	head_data = (TestHeadData*)Head_getUserData(head);

	head_data->target      = player;
	head_data->target_data = player->user_data;
	head_data->eye_height  = 1.75f;

	mainMenu = Menu( "Main Menu",
			(MenuItem){ "Run",     runEngine,  engine},
			(MenuItem){ "Options...", switchMenu, &optionsMenu},
			(MenuItem){ "Exit",    closeAll,   engine}
		),
	optionsMenu = Menu( "Options",
			{ "Back...", switchMenu, &mainMenu }
		);
	currentMenu = &mainMenu;
	
	while (!readyToClose) {
		BeginDrawing();
			ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
			
			Menu_draw(
				currentMenu,
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
	}

	
	CloseWindow();
	return 0;
}
