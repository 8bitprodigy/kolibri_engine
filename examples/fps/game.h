#ifndef GAME_H
#define GAME_H


#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <string.h>

#include "kolibri.h"


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
	#define initMouse() SetMousePosition(0, 0)
#else
	#define initMouse()
#endif
#ifndef DEFAULT_TICK_RATE
	#define DEFAULT_TICK_RATE 60
#endif
#ifndef DEFAULT_FRAME_RATE
	#define DEFAULT_FRAME_RATE 180
#endif


#define NUM_WEAPONS 10


/* Player Constants */
#define GRAVITY             32.0f
#define MAX_SPEED           20.0f

#define JUMP_HEIGHT          3.5f
#define JUMP_TIME_TO_PEAK    0.5f
#define JUMP_TIME_TO_DESCENT 0.4f

#define JUMP_GRAVITY      ((2.0f * JUMP_HEIGHT) / (JUMP_TIME_TO_PEAK    * JUMP_TIME_TO_PEAK))
#define FALL_GRAVITY      ((2.0f * JUMP_HEIGHT) / (JUMP_TIME_TO_DESCENT * JUMP_TIME_TO_DESCENT))
#define JUMP_VELOCITY     ( 1.5f * JUMP_HEIGHT) / JUMP_TIME_TO_PEAK
#define TERMINAL_VELOCITY FALL_GRAVITY * 5

#define MAX_ACCEL 150.0f

#define FRICTION    0.86f
#define AIR_DRAG    0.98f
#define CONTROL    12.5f
#define MAX_SLIDES  3


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
	float      eye_height;
	int     
			   current_weapon,
			   viewport_scale,
			   controller;
}
TestHeadData;

extern HeadVTable head_Callbacks;

/* 
	player.c
*/
/* Player data */
typedef struct
{
	Vector3
		prev_position,
		prev_velocity,
		move_dir,
		direction;
	bool    request_jump;
}
PlayerData;

extern EntityVTable player_Callbacks;
extern Entity       playerTemplate;

/*
	projectile.c
*/
typedef struct
{
	float 
		damage,
		speed;
}
ProjectileInfo;

void   Projectile_MediaInit(void);
extern EntityVTable projectile_Callbacks;
extern Entity       Blast_Template;

/* 
	scene_impl.c
*/
extern SceneVTable scene_Callbacks;

EntityList      testSceneRender(   Scene *scene, Head   *head);
CollisionResult testSceneCollision(Scene *scene, Entity *entity, Vector3 to);

/*
	options.c
*/

/* Renderable Callback */
static void
RenderModel(
	Renderable *renderable,
	void       *render_data
)
{
	Model  *model  = (Model*)renderable->data;
	Entity *entity = (Entity*)render_data;
	Vector3          
		    pos    = entity->position,
		    scale  = entity->scale;
	Matrix  matrix = QuaternionToMatrix(entity->orientation);
	
	rlPushMatrix();
		rlTranslatef(pos.x, pos.y, pos.z);
		rlMultMatrixf(MatrixToFloat(matrix));
		rlScalef(scale.x, scale.y, scale.z);
		
		DrawModel(
				*model,
				V3_ZERO, 
				1.0f, 
				WHITE
			);
	rlPopMatrix();
}

static void
testRenderableBoxCallback(
	Renderable *renderable,
	void       *render_data
)
{
	Entity *entity = render_data;
	Color  *color  = (Color*)renderable->data;
	if (!color) return;
	DrawCubeV(
		Vector3Add(entity->position, entity->renderable_offset),
		entity->bounds,
		*color
	);
		
}

static void
testRenderableBoxWiresCallback(
	Renderable *renderable,
	void       *render_data
)
{
	Entity *entity = render_data;
	Color  *color  = (Color*)renderable->data;
	if (!color) return;
	DrawCubeWiresV(
		Vector3Add(entity->position, entity->renderable_offset),
		entity->bounds,
		*color
	);
		
}

static void
testRenderableCylinderWiresCallback(
	Renderable *renderable,
	void       *render_data
)
{
	Entity *entity = renderable->data;
	Color  *color  = (Color*)renderable->data;
	float               radius = entity->bounds.x;
	if (!color) return;
	DrawCylinderWires(
		entity->position,
		radius,
		radius,
		entity->bounds.y,
		8,
		*color
	);
}


#endif /* GAME_H */
