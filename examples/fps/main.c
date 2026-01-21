#include <raylib.h>
#include <stdlib.h>

#include "game.h"
#include "../heightmap.h"
#define RAYGUI_IMPLEMENTATION
#define MENU_IMPLEMENTATION
#include "../menu.h"


#ifdef __PSP__
	#include <pspsdk.h>
	#include <pspuser.h>
	#include <pspdebug.h>
	#include <pspdisplay.h>
	PSP_MODULE_INFO("fps_test", 0, 1, 1);
	PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
	PSP_MAIN_THREAD_STACK_SIZE_KB(256);
	PSP_HEAP_SIZE_KB(20480);
#endif
#ifdef __Dreamcast__
	#if defined(IDE_SUPPORT) || defined(SDCARD_SUPPORT)
		#include <dc/g1ata.h>
		#include <dc/sd.h>
		#include <fat/fs_fat.h>
	#endif
#endif


void switchMenu(void *data, void *value);
void runEngine( void *data, void *value);
void closeAll  (void *data, void *value);

Engine       *engine;
Entity       *player;
Scene        *scene;
FPSHeadData *head_data;
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

char *path_prefix;

void
switchMenu(void *data, void *value)
{
	(void)value;
	currentMenu = (Menu*)data;
	currentMenu->selection   = -1;
}

void 
runEngine(void *data, void *value)
{
	(void)data;
	(void)value;
	engine = Engine_new(&engine_Callbacks, tick_rate);
	Head *head = Head_new(
			(Region){0,0,screen_width, screen_height}, 
			&head_Callbacks, 
			engine,
			sizeof(FPSHeadData)
		);
		
	RendererSettings *settings = Head_getRendererSettings(head);
	Camera3D *cam = Head_getCamera(head);

	settings->frustum_culling = true;
	cam->fovy     = 45.0f;
	cam->up       = V3_UP;
	cam->position = (Vector3){0.0f, 1.75f, 0.0f};
	cam->target   = (Vector3){10.0f, 0.0f, 10.0f};
		
	/*scene = Scene_new(&infinite_Plane_Scene_Callbacks, NULL, NULL, 0, engine); // */
	HeightmapData heightmap = (HeightmapData){
			.sun_angle            = (Vector3){0.0f, -0.4f, -0.6f},
			.ambient_value        = 0.6f,
			.offset               = 0.0f,
			.height_scale         = 200.0f,
			.cell_size            = 4.0f,
			.chunk_cells          = 16,
			.chunks_wide          = 32,
			.sun_color            = (Color){255, 255, 250, 255},
			.ambient_color        = (Color){115, 115, 155, 255},
			.hi_color             = (Color){110, 141,  70, 255},
			.lo_color             = BEIGE,
			.texture_path         = "resources/textures/grass/00_bw.png",
			.skybox_textures_path = SKY_PATH
		};
	scene = HeightmapScene_new(&heightmap, engine);
	// */
	
	player = Entity_new(&playerTemplate, scene, 0);
	PlayerData *player_data = player->user_data;
	player_data->head       = head;
	float height = HeightmapScene_getHeight(scene, V3_ZERO);
	player->position        = (Vector3){
			0.0f, 
			height + 0.01f,
			0.0f
		};
	player_data->prev_position = player->position;
	
	head_data = (FPSHeadData*)Head_getUserData(head);
	head_data->controller  = 0;
	head_data->target      = player;
	head_data->target_data = player->user_data;
	head_data->eye_height  = 1.75f;

	Vector3 enemy_pos  = (Vector3){32.0f, 0.0f, -32.0f};
	float enemy_height = HeightmapScene_getHeight(scene, enemy_pos) + 0.01f;
	enemy_pos.y = enemy_height;
	/*
	Entity *grunt      = Enemy_new(
			&enemy_Infos[ENEMY_GRUNT], 
			enemy_pos,
			scene
		);

	Entity *ents[21][21][21];
	int z = 0; 
 
	for (int x = 0; x < 21; x++) {
		for (int y = 0; y < 21; y++) {
			for (int z = 0; z < 1; z++) {
				Vector3 position       = (Vector3){
						.x = (x * 5.0f) - 50.0f,
						.y = (z * 5.0f),
						.z = (y * 5.0f) - 50.0
					};
				position.y = HeightmapScene_getHeight(scene, position);
				DBG_OUT("Entity position.y: %.4f", position.y);
				if (position.x == 0.0f && position.z == 0.0f) continue;
				ents[x][y][z]           = Entity_new(&entityTemplate, scene, 0);
				ents[x][y][z]->visible  = true;
				ents[x][y][z]->active   = true;
				ents[x][y][z]->position = position;
			}
		}
	}
	// */
	DBG_OUT("Enemy entity created");

	DBG_OUT("About to call Engine_run()");
		Engine_run(engine);
	DBG_OUT("Engine_run() completed");
}

void 
closeAll(void *data, void *value)
{
	(void)value;
	if (data) Engine_requestExit(engine);
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

void
setupPathPrefix()
{
	ChangeDirectory(PATH_PREFIX);
	
/*
	Thanks Darc, for the help with this.
*/
#if defined(__DREAMCAST__)
	DBG_OUT("Checking for a valid drive...");
	
	if (DirectoryExists("/pc/resources")) {
		ChangeDirectory("/pc/");
		DBG_OUT("PATH PREFIX: \"/pc\"");
		goto path_found;
	}
	
	if (DirectoryExists("/cd/resources")) {
		ChangeDirectory("/cd/");
		DBG_OUT("PATH PREFIX: \"/cd\"");
		goto path_found;
	}

	#if defined(IDE_SUPPORT) || defined(SDCARD_SUPPORT)
	fs_fat_init();
	#endif

	#ifndef PROJECTNAME
		#define PROJECTNAME "fps_example"
	#endif

	#if defined(SDCARD_SUPPORT)
	path_test = fopen("/sd/" PROJECTNAME "/resources/textures/dev/xor.gif", "rb");
	if (DirectoryExists("/sd/" PROJECTNAME "/resources")) {
		ChangeDirectory("/sd/" PROJECTNAME);
		DBG_OUT("PATH PREFIX: \"/sd\"");
		goto path_found;
	}
	#endif
	#if defined(IDE_SUPPORT)
	if (DirectoryExists("/ide/" PROJECTNAME "/resources")) {
		ChangeDirectory("/ide/" PROJECTNAME);
		DBG_OUT("PATH PREFIX: \"/ide\"");
		goto path_found;
	}
	#endif
	DBG_OUT("Couldn't find a valid drive. Quitting now...");
	exit(-1);
path_found:
	#if defined(SDCARD_SUPPORT)
	fs_fat_unmount("/sd");
	sd_shutdown();
	#endif
	#if defined(IDE_SUPPORT)
	fs_fat_unmount("/ide");
	g1_ata_shutdown();
	#endif
	#if defined(IDE_SUPPORT) || defined(SDCARD_SUPPORT)
	fs_fat_shutdown();
	#endif
#endif
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
#endif /* !ON_CONSOLE */
#ifdef __PSP__
	SetConfigFlags(FLAG_VSYNC_HINT);
#endif /* __PSP__ */
	
	InitWindow(screen_width, screen_height, WINDOW_TITLE);
	
	setupPathPrefix();
	
	HandleMouse();
	
	Game_mediaInit();

#if !defined(ON_CONSOLE) || defined(__PSP__)
	mainMenu = Menu( "Main Menu",
			MENU_WIDTH,
			MENU_ITEM_HEIGHT,
			MENU_PADDING,
			MenuButton( "Run",        runEngine,  engine),
			MenuButton( "Options...", switchMenu, &optionsMenu),
			MenuButton( "Exit",       closeAll,   engine)
		),
#else
	mainMenu = Menu( "Main Menu",
			MENU_WIDTH,
			MENU_ITEM_HEIGHT,
			MENU_PADDING,
			MenuButton( "Run",        runEngine,  engine),
			MenuButton( "Options...", switchMenu, &optionsMenu)
		),
#endif
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
					GET_KEY_OR_BUTTON_PRESSED(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN, KEY_ENTER)
				);
		EndDrawing();
	}

	
	CloseWindow();
	return 0;
}
