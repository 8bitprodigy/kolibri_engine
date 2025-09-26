#ifndef GAME_H
#define GAME_H


#include <raylib.h>

#include "kolibri.h"
#include "readarg.h"


#ifndef SCREEN_WIDTH
	#define SCREEN_WIDTH 854
#endif
#ifndef SCREEN_HEIGHT
	#define SCREEN_HEIGHT 480
#endif
#define ASPECT_RATIO ((float)SCREEN_WIDTH/(float)SCREEN_HEIGHT)


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
	Model   model;
	int     viewport_scale;
	int     controller;
	float   eye_height;
	Vector2 look;
	Entity *target;
}
TestHeadData;


/* Engine Implementation */
extern EngineVTable engine_Callbacks;

void testEngineRun(      Engine *engine);
void testEngineExit(     Engine *engine);
void testEngineComposite(Engine *engine);

/* Entity Implementation */
extern TestRenderableData rd_1, rd_2, rd_3;
extern Renderable         r_1,  r_2,  r_3;

/* Entity Implementation */
extern EntityVTable entity_Callbacks;
extern Entity       entityTemplate;

/* Head Implementation */
extern HeadVTable head_Callbacks;

/* Player Implementation */
extern EntityVTable player_Callbacks;
extern Entity       playerTemplate;

/* Scene Implementation */
extern SceneVTable scene_Callbacks;

EntityList      testSceneRender(   Scene *scene, Head   *head);
CollisionResult testSceneCollision(Scene *scene, Entity *entity, Vector3 to);


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
