#include <raylib.h>
#include <stdlib.h>

#include "game.h"
#include "heightmap.h"
#define RAYGUI_IMPLEMENTATION
#define MENU_IMPLEMENTATION
#include "../menu.h"


#ifdef __PSP__
	#include <pspsdk.h>
	PSP_MODULE_INFO("TEST", 0, 1, 0);
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

char *path_prefix;

void
switchMenu(void *data, void *value)
{
	currentMenu = (Menu*)data;
	currentMenu->selection   = -1;
}

void 
runEngine(void *data, void *value)
{
	engine = Engine_new(&engine_Callbacks, tick_rate);
	Head *head = Head_new(
			(Region){0,0,screen_width, screen_height}, 
			&head_Callbacks, 
			engine,
			sizeof(TestHeadData)
		);
		
	RendererSettings *settings = Head_getRendererSettings(head);
//	settings->frustum_culling = false;
	Camera3D *cam = Head_getCamera(head);
	cam->fovy     = 45.0f;
	cam->up       = V3_UP;
	cam->position = (Vector3){0.0f, 1.75f, 0.0f};
	cam->target   = (Vector3){10.0f, 0.0f, 10.0f};

	char texture_path[256];
	snprintf(
			texture_path, 
			sizeof(texture_path),
			"%s%s", path_prefix, 
			"resources/textures/grass/00.png"
		);
	DBG_OUT("\n\tTEXTURE PATH: %s\n", texture_path);
	/*scene = Scene_new(&infinite_Plane_Scene_Callbacks, NULL, NULL, 0, engine); // */
	HeightmapData heightmap = (HeightmapData){
			.sun_angle     = (Vector3){0.0f, -0.4f, -0.6f},
			.ambient_value = 0.6f,
			.offset        = 0.0f,
			.height_scale  = 200.0f,
			.cell_size     = 4.0f,
			.chunk_cells   = 16,
			.chunks_wide   = 32,
			.hi_color      = WHITE,
			.lo_color      = WHITE,
			.texture       = LoadTexture(texture_path),
			.lod_distances = {48.0f, 64.0f, 128.0f, 192.0f},
		};
	scene = HeightmapScene_new(&heightmap, engine);
	// */
	
	player = Entity_new(&playerTemplate, scene, 0);
	player->position = (Vector3){0.0f, 8.0f, 0.0f};
	
	head_data = (TestHeadData*)Head_getUserData(head);
	head_data->controller  = 0;
	head_data->target      = player;
	head_data->target_data = player->user_data;
	head_data->eye_height  = 1.75f;
/*	
	Entity *ents[21][21][21];
	int z = 0; 
 
	for (int x = 0; x < 21; x++) {
		for (int y = 0; y < 21; y++) {
			for (int z = 0; z < 1; z++) {
				Vector3 position       = (Vector3){
						(x * 5.0f) - 50.0f,
						(z * 5.0f),
						(y * 5.0f) - 50.0
					};
				if (Vector3Equals(position, V3_ZERO)) continue;
				ents[x][y][z]           = Entity_new(&entityTemplate, scene, 0);
				ents[x][y][z]->visible  = true;
				ents[x][y][z]->active   = true;
				ents[x][y][z]->position = position;
			}
		}
	}
	// */
	Engine_run(engine);
}

void 
closeAll(void *data, void *value)
{
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
	path_prefix = PATH_PREFIX;

/*
	Thanks, Darc, for the help with this.
*/
#ifdef __DREAMCAST__
	DBG_OUT("Checking for a valid drive...");
	
	FILE *path_test;
	path_test = fopen("/pc/resources/textures/dev/xor.gif", "rb");
	if (path_test) {
		path_prefix = "/pc/";
		DBG_OUT("PATH PREFIX: \"/pc\"");
		goto path_found;
	}

	path_test = fopen("/cd/resources/textures/dev/xor.gif", "rb");
	if (path_test) {
		path_prefix = "/cd/";
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
	if (path_test) {
		path_prefix = "/sd/" PROJECTNAME;
		DBG_OUT("PATH PREFIX: \"/sd\"");
		goto path_found;
	}
	#endif
	#if defined(IDE_SUPPORT)
	path_test = fopen("/ide/" PROJECTNAME "/resources/textures/dev/xor.gif", "rb");
	if (path_test) {
		path_prefix = "/ide/" PROJECTNAME;
		DBG_OUT("PATH PREFIX: \"/ide\"");
		goto path_found;
	}
	#endif
	DBG_OUT("Couldn't find a valid drive. Quitting now...");
	exit(-1);
path_found:
	fclose(path_test);
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
#endif /* ON_CONSOLE */
	setupPathPrefix();
	
	InitWindow(screen_width, screen_height, WINDOW_TITLE);
	
	HandleMouse();
	
	Projectile_MediaInit();

	mainMenu = Menu( "Main Menu",
			MENU_WIDTH,
			MENU_ITEM_HEIGHT,
			MENU_PADDING,
			MenuButton( "Run",        runEngine,  engine),
			MenuButton( "Options...", switchMenu, &optionsMenu),
#ifndef ON_CONSOLE
			MenuButton( "Exit",       closeAll,   engine)
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
