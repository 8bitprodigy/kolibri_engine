#include "common.h"
#include "engine.h"
#include "entity.h"
#include "head.h"
#include "scene.h"
#include <raylib.h>


#define SCREEN_WIDTH 854
#define SCREEN_HEIGHT 480


float
invLerp(float a, float b, float value)
{
	if (a==b) return 0.0f;
	return (value - a) / (b - a);
}


EntityList
testSceneRender(Scene *scene, Head *head)
{
	ClearBackground(WHITE);
	DrawGrid(100, 1.0f);
	return *Engine_getEntityList(Scene_getEngine(scene));
}

CollisionResult /* Infinite plane at 0.0f y-position */
testSceneCollision(Scene *scene, Entity *entity, Vector3 to)
{
	if (entity->position.y > 0.0f && to.y > 0.0f) return NO_COLLISION;

	Vector3
		from = entity->position,
		hit_floor_point;
	float   distance;

	if (0.0f < from.y && to.y <= 0.0f) {
		hit_floor_point = Vector3Lerp(
			from, 
			to,
			invLerp(from.y, to.y, 0.0f)
		);
		distance = Vector3Distance(from, hit_floor_point);
	}
	else {
		hit_floor_point = (Vector3){from.x, 0.0f, from.z};
		distance = fabsf(from.y);
	}

	return (CollisionResult){
		hit_floor_point,
		V3_UP,
		distance,
		0,
		NULL,
		NULL,
		true
	};
}

void
testSceneComposite(Engine *engine)
{
	Head *head = Engine_getHeads(engine);
	DrawTextureRec((*Head_getViewport(head)).texture, Head_getRegion(head), V2_ZERO, WHITE);
}

void
testHeadPostRender(Head *head)
{
	DrawFPS(10, 10);
}

void
testHeadUpdate(Head *head, float delta)
{
	UpdateCamera(Head_getCamera(head), CAMERA_FIRST_PERSON);
}


EngineVTable engine_Callbacks = {
	NULL, 
	NULL, 
	NULL, 
	testSceneComposite, 
	NULL, 
	NULL, 
	NULL, 
	NULL,
	NULL
};

HeadVTable head_Callbacks = {
	NULL, 
	testHeadUpdate, 
	NULL, 
	NULL, 
	testHeadPostRender,
	NULL, 
	NULL, 
	NULL
};

SceneVTable scene_Callbacks = {
	NULL, 
	NULL, 
	NULL, 
	NULL, 
	NULL, 
	testSceneCollision, 
	NULL, 
	testSceneRender, 
	NULL, 
	NULL
};


int
main(void)
{
	Engine *engine = Engine_new(&engine_Callbacks);
	Head   *head   = Head_new(0, &head_Callbacks, engine);

	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Kolibri Engine Test");

	Head_setViewport(head, SCREEN_WIDTH, SCREEN_HEIGHT);
	Camera3D *cam = Head_getCamera(head);
	cam->fovy     = 45.0f;
	cam->up       = V3_UP;
	cam->position = (Vector3){0.0f, 1.75f, 0.0f};
	cam->target   = (Vector3){10.0f, 0.0f, 10.0f};

	Scene_new(&scene_Callbacks, NULL, engine);

	Engine_run(engine);

	return 0;
}
