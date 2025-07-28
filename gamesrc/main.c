#include "common.h"
#include "engine.h"
#include "entity.h"
#include "head.h"
#include "scene.h"
#include <raylib.h>


#ifndef SCREEN_WIDTH
	#define SCREEN_WIDTH 1280
#endif
#ifndef SCREEN_HEIGHT
	#define SCREEN_HEIGHT 720
#endif


typedef struct
RenderableData
{
	Color color;
}
RenderableData;


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


void
testRenderableCallback(
	Renderable *renderable,
	Entity     *entity
)
{
	RenderableData *data = (RenderableData*)renderable->data;
	if (!data) return;
	DrawCubeV(
		Vector3Add(entity->position, entity->renderable_offset),
		entity->bounds,
		data->color
	);
		
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


static RenderableData rd_1, rd_2, rd_3;
static Renderable     r_1,  r_2,  r_3;
static Entity         entityTemplate;


int
main(void)
{

	entityTemplate = (Entity){
		.renderables       = {&r_1,  &r_2,  &r_3},
		.lod_distances     = {8.0f, 16.0f, 32.0f},
		.lod_count         = 3,
		.renderable_offset = {0.0f, 0.5f, 0.0f},
		.visibility_radius = 1.0f,
		.bounds            = V3_ONE,
		.user_data         = NULL,
		.vtable            = NULL,
		.position          = V3_ZERO,
		.rotation          = V3_ZERO,
		.scale             = V3_ONE,
		.visible           = true,
		.active            = true,
		.physical          = true, 
	};
	
	rd_1 = (RenderableData){.color = RED};
	rd_2 = (RenderableData){.color = GREEN};
	rd_3 = (RenderableData){.color = BLUE};

	r_1 = (Renderable){
		.data   = &rd_1,
		.Render = testRenderableCallback
	};
	r_2 = (Renderable){
		.data   = &rd_2,
		.Render = testRenderableCallback
	};
	r_3 = (Renderable){
		.data   = &rd_3,
		.Render = testRenderableCallback
	};

	
	Engine *engine = Engine_new(&engine_Callbacks);
	Head   *head   = Head_new(0, &head_Callbacks, engine);

	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Kolibri Engine Test");

	Head_setViewport(head, SCREEN_WIDTH, SCREEN_HEIGHT);
	RendererSettings *settings = Head_getRendererSettings(head);
//	settings->frustum_culling = false;
	Camera3D *cam = Head_getCamera(head);
	cam->fovy     = 45.0f;
	cam->up       = V3_UP;
	cam->position = (Vector3){0.0f, 1.75f, 0.0f};
	cam->target   = (Vector3){10.0f, 0.0f, 10.0f};

	Scene_new(&scene_Callbacks, NULL, engine);
	
	Entity 
		*ent_1 = Entity_new(&entityTemplate, engine),
		*ent_2 = Entity_new(&entityTemplate, engine),
		*ent_3 = Entity_new(&entityTemplate, engine),
		*ent_4 = Entity_new(&entityTemplate, engine);
	
	ent_1->visible = true; ent_1->active = true;
	ent_2->visible = true; ent_2->active = true;
	ent_3->visible = true; ent_3->active = true;
	ent_4->visible = true; ent_4->active = true;

	ent_1->position = (Vector3){ 10.0f, 0.0f,  10.0f};
	ent_2->position = (Vector3){-10.0f, 0.0f,  10.0f};
	ent_3->position = (Vector3){ 10.0f, 0.0f, -10.0f};
	ent_4->position = (Vector3){-10.0f, 0.0f, -10.0f};

	Engine_run(engine, 0);

	return 0;
}
