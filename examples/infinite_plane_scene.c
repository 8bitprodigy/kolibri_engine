#include "infinite_plane_scene.h"
#include "renderer.h"
#include <raylib.h>


#define ENTITY_ON_ENTITY_TEST_VECTOR ((Vector3){0.0f, -0.01f, 0.0f})

/*
	Callback Forward Declarations
*/
void            infinitePlaneSceneSetup(    Scene *scene, void   *map_data);
void            infinitePlaneSceneRender(   Scene *scene, Head   *head);
CollisionResult infinitePlaneSceneCollision(Scene *scene, Entity *entity,   Vector3 to);
CollisionResult infinitePlaneSceneRaycast(  Scene *scene, Vector3 from,     Vector3 to);
void            infinitePlaneSceneFree(     Scene *scene, void   *map_data);


Texture debug_texture;
/*
	Template Declarations
*/
SceneVTable infinite_Plane_Scene_Callbacks = {
	.Setup          = infinitePlaneSceneSetup, 
	.Enter          = NULL, 
	.Update         = NULL, 
	.EntityEnter    = NULL, 
	.EntityExit     = NULL, 
	.CheckCollision = infinitePlaneSceneCollision, 
	.MoveEntity     = infinitePlaneSceneCollision, 
	.Raycast        = infinitePlaneSceneRaycast,
	.Render         = infinitePlaneSceneRender, 
	.Exit           = NULL, 
	.Free           = infinitePlaneSceneFree,
};

void 
DrawInfinitePlane(Camera *cam, float tileScale)
{
    float 
		size = PLANE_SIZE,
		cx   = cam->position.x,
		cz   = cam->position.z;

    rlBegin(RL_QUADS);
		rlSetTexture(debug_texture.id);
		rlTexCoord2f((cx - size) * tileScale, (cz - size) * tileScale);
		rlVertex3f(cx - size, 0.0f, cz - size);

		rlTexCoord2f((cx - size) * tileScale, (cz + size) * tileScale);
		rlVertex3f(cx - size, 0.0f, cz + size);

		rlTexCoord2f((cx + size) * tileScale, (cz + size) * tileScale);
		rlVertex3f(cx + size, 0.0f, cz + size);

		rlTexCoord2f((cx + size) * tileScale, (cz - size) * tileScale);
		rlVertex3f(cx + size, 0.0f, cz - size);

    rlEnd();
    rlSetTexture(0);
}

void 
infinitePlaneSceneSetup(Scene *scene, void *map_data)
{
	debug_texture = LoadTexture(
			PATH_PREFIX 
			"resources/textures/dev/Prototype_Grid_Gray_08-128x128.png"
		);
	GenTextureMipmaps(&debug_texture);
	SetTextureFilter(debug_texture, TEXTURE_FILTER_TRILINEAR);
}

void
infinitePlaneSceneRender(Scene *scene, Head *head)
{
	Renderer *renderer = Engine_getRenderer(Scene_getEngine(scene));
	Camera   *camera   = Head_getCamera(head);
#ifndef ON_CONSOLE
	//DrawGrid(100, 1.0f);
#endif
	DrawInfinitePlane(camera, 1.0f);
	EntityList *ent_list = Scene_getEntityList(scene);
	for (int i = 0; i < ent_list->count; i++) {
		Renderer_submitEntity(renderer, ent_list->entities[i]);
	}
}

CollisionResult /* Infinite plane at 0.0f y-position */
infinitePlaneSceneCollision(Scene *scene, Entity *entity, Vector3 to)
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
infinitePlaneSceneRaycast(Scene *self, Vector3 from, Vector3 to)
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

void 
infinitePlaneSceneFree(Scene *scene, void *map_data)
{
	UnloadTexture(debug_texture);
}
