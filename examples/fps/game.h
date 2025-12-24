#ifndef GAME_H
#define GAME_H


#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <string.h>

#include "assman.h"
#include "infinite_plane_scene.h"
#include "common.h"
#include "kolibri.h"
#include "sprite.h"
#include "utils.h"


#ifndef WINDOW_TITLE
	#define WINDOW_TITLE "Kolibri Engine FPS Test"
#endif
#ifndef SCREEN_WIDTH
	#define SCREEN_WIDTH 854
#endif
#ifndef SCREEN_HEIGHT
	#define SCREEN_HEIGHT 480
#endif
#define ASPECT_RATIO ((float)SCREEN_WIDTH/(float)SCREEN_HEIGHT)
#ifndef MENU_WIDTH
	#define MENU_WIDTH 220
#endif
#ifndef MENU_ITEM_HEIGHT
	#define MENU_ITEM_HEIGHT 30
#endif
#ifndef MENU_PADDING
	#define MENU_PADDING 10
#endif
#ifdef NO_MOUSE
	#define HandleMouse() SetMousePosition(0, 0)
#else
	#define HandleMouse()
#endif
#ifndef DEFAULT_TICK_RATE
	#define DEFAULT_TICK_RATE 60
#endif
#ifndef DEFAULT_FRAME_RATE
	#define DEFAULT_FRAME_RATE 180
#endif
#ifndef PATH_PREFIX
	#define PATH_PREFIX "./"
#endif


#define NUM_WEAPONS 10


/* Player Constants */
#define GRAVITY             32.0f
#define MAX_SPEED           50.0f

#define JUMP_HEIGHT          3.5f
#define JUMP_TIME_TO_PEAK    0.5f
#define JUMP_TIME_TO_DESCENT 0.4f

#define JUMP_GRAVITY      ((2.0f * JUMP_HEIGHT) / (JUMP_TIME_TO_PEAK    * JUMP_TIME_TO_PEAK))
#define FALL_GRAVITY      ((2.0f * JUMP_HEIGHT) / (JUMP_TIME_TO_DESCENT * JUMP_TIME_TO_DESCENT))
#define JUMP_VELOCITY     ( 1.5f * JUMP_HEIGHT) / JUMP_TIME_TO_PEAK
#define TERMINAL_VELOCITY FALL_GRAVITY * 5

#define MAX_ACCEL 250.0f

#define FRICTION    0.86f
#define AIR_DRAG    0.98f
#define CONTROL    12.5f
#define MAX_SLIDES  3


/*
	main.c
*/
extern char *path_prefix;

/* 
	engine_impl.c
*/
extern EngineVTable engine_Callbacks;

void engineRun(      Engine *engine);
void engineExit(     Engine *engine);
void engineComposite(Engine *engine);

/* 
	entity_impl.c
*/
extern Renderable         r_1,  r_2,  r_3;

extern EntityVTable entity_Callbacks;
extern Entity       entityTemplate;

/* 
	head_impl.c
*/
typedef struct
{
	Model      weapons[NUM_WEAPONS];
	Vector2    look;
	Texture2D  skybox_textures[6];
	Entity    *target;
	void      *target_data;
	float      
			   look_sensitivity,
			   eye_height;
	int     
			   current_weapon,
			   viewport_scale,
			   controller;
}
TestHeadData;

extern HeadVTable head_Callbacks;
void   teleportHead(Entity *entity, Vector3 from, Vector3 to);

/* 
	player.c
*/
/* Player data */
typedef struct
{
	Head *head;
	Vector3
		prev_position,
		prev_velocity,
		move_dir,
		direction;
	int  frames_since_grounded;
	bool request_jump;
}
PlayerData;

extern EntityVTable player_Callbacks;
extern Entity       playerTemplate;

/*
	projectile.c
*/
typedef struct
{
	SpriteInfo sprite_info;
	float 
		damage,
		speed,
		timeout;
}
ProjectileInfo;

void   Projectile_MediaInit(void);
void   Projectile_new(      Vector3 position, Vector3 direction, Entity *template, Scene *scene);
extern EntityVTable projectile_Callbacks;
extern Entity       Blast_Template;

/*
	options.c
*/


#endif /* GAME_H */
