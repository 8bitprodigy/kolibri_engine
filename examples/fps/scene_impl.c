#include "game.h"


#define ENTITY_ON_ENTITY_TEST_VECTOR ((Vector3){0.0f, -0.01f, 0.0f})

/*
	Callback Forward Declarations
*/
EntityList      testSceneRender(         Scene *scene, Head   *head);
CollisionResult testSceneCollision(      Scene *scene, Entity *entity, Vector3 to);
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
	.MoveEntity     = NULL, 
	.EntityOnFloor  = testSceneEntityIsOnFloor,
	.Raycast        = NULL,
	.Render         = testSceneRender, 
	.Exit           = NULL, 
	.Free           = NULL
};


EntityList
testSceneRender(Scene *scene, Head *head)
{
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
		true,
		distance,
		hit_floor_point,
		V3_UP,
		0,
		NULL,
		NULL
	};
}


bool
testSceneEntityIsOnFloor(Scene *self, Entity *entity)
{
	if (entity->position.y <= 0.0f) {
		entity->position.y = 0.0f;
		entity->velocity.y = 0.0f;
		return true;
	}

	//return testSceneCollision(self, entity, ENTITY_ON_ENTITY_TEST_VECTOR).hit;

	return false;
}
