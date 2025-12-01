#include "infinite_plane_scene.h"
#include "renderer.h"
#include <raylib.h>


#define ENTITY_ON_ENTITY_TEST_VECTOR ((Vector3){0.0f, -0.01f, 0.0f})

/*
	Callback Forward Declarations
*/
void            testSceneRender(         Scene *scene, Head   *head);
CollisionResult testSceneCollision(      Scene *scene, Entity *entity, Vector3 to);
CollisionResult testSceneRaycast(        Scene *scene, Vector3 from,   Vector3 to);
bool            testSceneEntityIsOnFloor(Scene *scene, Entity *entity);



/*
	Template Declarations
*/
SceneVTable scene_Callbacks = {
	.Setup          = NULL, 
	.Enter          = NULL, 
	.Update         = NULL, 
	.EntityEnter    = NULL, 
	.EntityExit     = NULL, 
	.CheckCollision = testSceneCollision, 
	.MoveEntity     = testSceneCollision, 
	.Raycast        = testSceneRaycast,
	.Render         = testSceneRender, 
	.Exit           = NULL, 
	.Free           = NULL
};


void
testSceneRender(Scene *scene, Head *head)
{
	Renderer *renderer = Engine_getRenderer(Scene_getEngine(scene));
	Camera   *camera   = Head_getCamera(head);
#ifndef ON_CONSOLE
	//DrawGrid(100, 1.0f);
#endif
	DrawPlane(
			(Vector3){
					camera->position.x,
					0.0f,
					camera->position.z
				},
			(Vector2){
					128.0f,
					128.0f
				},
			WHITE
		);
	EntityList *ent_list = Scene_getEntityList(scene);
	for (int i = 0; i < ent_list->count; i++) {
		Renderer_submitEntity(renderer, ent_list->entities[i]);
	}
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
		true,
		distance,
		hit_floor_point,
		V3_UP,
		0,
		NULL,
		NULL
	};
}

CollisionResult
testSceneRaycast(Scene *self, Vector3 from, Vector3 to)
{
	CollisionResult result = {0};

	if (from.y <= 0.0f) {
		result.hit      = true;
		result.distance = 0.0f;
		result.position = (Vector3){from.x, 0.0f, from.z};
		result.normal   = V3_UP;

		return result;
	}

	if (0.0f < to.y) return NO_COLLISION;

	float t         = -from.y / (to.y - from.y);
	t               = CLAMP(t, 0.0f, 1.0f);
	Vector3 hit_pos = Vector3Lerp(from, to, t);
	hit_pos.y       = 0.0f;

	result.hit      = true;
	result.distance = Vector3Distance(from, hit_pos);
	result.position = hit_pos;
	result.normal   = V3_UP;
	result.entity   = NULL;

	return result;
}


bool
testSceneEntityIsOnFloor(Scene *self, Entity *entity)
{
	if (entity->position.y <= 0.0f) {
		entity->position.y = 0.0f;
		entity->velocity.y = 0.0f;
		return true;
	}

	return Scene_checkContinuous(self, entity, (Vector3){0.0f, -0.01f,  0.0f}).hit;

	return false;
}
