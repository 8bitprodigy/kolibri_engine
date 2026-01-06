#ifndef GAME_H
#define GAME_H


#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <string.h>

#include "kolibri.h"
#include "../assman.h"
#include "../explosion.h"
#include "../infinite_plane_scene.h"
#include "../projectile.h"
#include "../sprite.h"
#include "../utils.h"
#include "../weapon.h"


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

#ifndef SKY_PATH
	#define SKY_PATH "resources/sky/SBS_SKY_panorama_%s.png"
#endif


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


typedef enum
{
	PROJECTILE_BLAST,
	PROJECTILE_PELLET,
	PROJECTILE_GOO,
	PROJECTILE_ROCKET,
	PROJECTILE_GRENADE,
	PROJECTILE_PLASMA,
	PROJECTILE_NUM_PROJECTILES
}
Projectiles;

typedef enum
{
	WEAPON_MELEE,
	WEAPON_BLASTER,
	WEAPON_MINIGUN,
	WEAPON_SHOTGUN,
	WEAPON_GOOGUN,
	WEAPON_ROCKET_LAUNCHER,
	WEAPON_GRENADE_LAUNCHER,
	WEAPON_RAILGUN,
	WEAPON_LIGHTNING_GUN,
	WEAPON_NUM_WEAPONS
}
Weapons;


/* 
	game.c
*/
extern ExplosionInfo   *explosion_Info;
extern Renderable      *projectile_renderables;
extern Model           *projectile_models;
extern Texture         *projectile_Sprite_or_Textures;
extern SpriteInfo     **projectile_SpriteInfos;
extern ProjectileInfo **projectile_Infos;

extern WeaponInfo       weapon_Infos[WEAPON_NUM_WEAPONS];

void Game_mediaInit(      void);
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
	WeaponData weapon_data[WEAPON_NUM_WEAPONS];
	Entity    *target;
	void      *target_data;
	Vector2    look;
	float
			   look_sensitivity,
			   eye_height;
	int     
			   viewport_scale,
			   controller;
	uint16_t   owned_weapons;
	uint8_t    current_weapon;
}
FPSHeadData;

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
	int   frames_since_grounded;
	bool  request_jump;
}
PlayerData;

extern EntityVTable player_Callbacks;
extern Entity       playerTemplate;

/*
	projectile.c
*/
extern EntityVTable projectile_Callbacks;

/*
	options.c
*/


#endif /* GAME_H */
