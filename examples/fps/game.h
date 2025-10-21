#ifndef GAME_H
#define GAME_H


#include <math.h>
#include <raylib.h>
#include <raymath.h>

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


/* Player data */
typedef struct
PlayerData
{
	Vector3
		prev_position,
		prev_velocity,
		move_dir,
		direction;
	bool    request_jump;
}
PlayerData;


typedef struct
TestRenderableData
{
	Color color;
}
TestRenderableData;


/* Head Implementation */
extern HeadVTable head_Callbacks;

typedef struct
TestHeadData
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
extern TestRenderableData rd_1, rd_2, rd_3;
extern Renderable         r_1,  r_2,  r_3;

extern EntityVTable entity_Callbacks;
extern Entity       entityTemplate;

/* 
	head_impl.c
*/
extern HeadVTable head_Callbacks;

/* 
	player.c
*/
extern EntityVTable player_Callbacks;
extern Entity       playerTemplate;

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
testRenderableBoxCallback(
	Renderable *renderable,
	void       *render_data
)
{
	Entity             *entity = render_data;
	TestRenderableData *data   = (TestRenderableData*)renderable->data;
	if (!data) return;
	DrawCubeV(
		Vector3Add(entity->position, entity->renderable_offset),
		entity->bounds,
		data->color
	);
		
}

static void
testRenderableBoxWiresCallback(
	Renderable *renderable,
	void       *render_data
)
{
	Entity             *entity = render_data;
	TestRenderableData *data   = (TestRenderableData*)renderable->data;
	if (!data) return; 
	DrawCubeWiresV(
		Vector3Add(entity->position, entity->renderable_offset),
		entity->bounds,
		data->color
	);
		
}

static void
testRenderableCylinderWiresCallback(
	Renderable *renderable,
	void       *render_data
)
{
	Entity             *entity = render_data;
	TestRenderableData *data   = (TestRenderableData*)renderable->data;
	float               radius = entity->bounds.x;
	if (!data) return;
	DrawCylinderWires(
		entity->position,
		radius,
		radius,
		entity->bounds.y,
		8,
		data->color
	);
}


#endif /* GAME_H */
