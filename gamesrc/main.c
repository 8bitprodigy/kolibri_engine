#include "common.h"
#include "engine.h"
#include "entity.h"
#include "head.h"
#include "scene.h"
#include <raylib.h>


#ifndef SCREEN_WIDTH
	#define SCREEN_WIDTH 854
#endif
#ifndef SCREEN_HEIGHT
	#define SCREEN_HEIGHT 480
#endif


typedef struct
RenderableData
{
	Color color;
}
RenderableData;

typedef struct
TestHeadData
{
	Model model;
	int   viewport_scale;
}
TestHeadData;

float
invLerp(float a, float b, float value)
{
	if (a==b) return 0.0f;
	return (value - a) / (b - a);
}


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
testEngineRun(Engine *engine)
{
	DisableCursor();
}

void
testSceneComposite(Engine *engine)
{
	Head *head = Engine_getHeads(engine);
	RenderTexture *rt = Head_getViewport(head);
	TestHeadData *data = Head_getUserData(head);
	
	Vector2 pos = (Vector2){
			(SCREEN_WIDTH/2.0f)  - (rt->texture.width/2.0f),
			(SCREEN_HEIGHT/2.0f) - (rt->texture.height/2.0f),
		};
	ClearBackground(BLACK);
	DrawTextureRec((*Head_getViewport(head)).texture, Head_getRegion(head), pos, WHITE);
}

void
testHeadSetup(Head *head)
{
	TestHeadData *user_data = malloc(sizeof(TestHeadData));
	if (!user_data) {
		ERR_OUT("Failed to allocate TestHeadData.");
		return;
	}

	user_data->viewport_scale = 1;
	
	Model *skysphere = &user_data->model;
	
	Mesh mesh = GenMeshSphere(1.0f, 13, 16);
	*skysphere = LoadModelFromMesh(mesh);

	Mesh *skyMesh = &skysphere->meshes[0];
	Matrix rotMatrix = MatrixRotateX(DEG2RAD * -90.0f); // Adjust angle as needed

	for (int i = 0; i < skyMesh->vertexCount; i++) {
		// Fix UV coordinates
		float u = skyMesh->texcoords[i*2];
		float v = skyMesh->texcoords[i*2+1];
		skyMesh->texcoords[i*2] = 1.0f - v;
		skyMesh->texcoords[i*2+1] = u;
		
		// Rotate vertex positions
		Vector3 vertex = {
			skyMesh->vertices[i*3],     // x
			skyMesh->vertices[i*3+1],   // y  
			skyMesh->vertices[i*3+2]    // z
		};
		
		Vector3 rotatedVertex = Vector3Transform(vertex, rotMatrix);
		
		skyMesh->vertices[i*3]     = rotatedVertex.x;
		skyMesh->vertices[i*3+1]   = rotatedVertex.y;
		skyMesh->vertices[i*3+2]   = rotatedVertex.z;
	}

	// Update both vertex and UV buffers
	UpdateMeshBuffer(*skyMesh, 0, skyMesh->vertices, skyMesh->vertexCount*3*sizeof(float), 0);
	UpdateMeshBuffer(*skyMesh, 1, skyMesh->texcoords, skyMesh->vertexCount*2*sizeof(float), 0);

	Texture2D skyTexture = LoadTexture("resources/sky/SBS_sky_panorama.png");
	if (skyTexture.id == 0) {
		ERR_OUT("Failed to load sky texture!");
		skyTexture = LoadTexture("");
	}
	
	SetMaterialTexture(&skysphere->materials[0], MATERIAL_MAP_ALBEDO, skyTexture);

	Head_setUserData(head, user_data);
}

void
testHeadPreRender(Head *head)
{
	TestHeadData *data = Head_getUserData(head);
	Camera3D     *cam  = Head_getCamera(head);
	ClearBackground(WHITE);
	BeginMode3D(*cam);
		rlDisableDepthMask();
		rlDisableBackfaceCulling();
			DrawModel(data->model, cam->position, 1.0, WHITE);
		rlEnableBackfaceCulling();
		rlEnableDepthMask();
	EndMode3D();
}

void
testHeadPostRender(Head *head)
{
	DrawFPS(10, 10);
}

void
testHeadUpdate(Head *head, float delta)
{
	TestHeadData *data = Head_getUserData(head);
	
	UpdateCamera(Head_getCamera(head), CAMERA_FIRST_PERSON);
	
	if      (IsKeyPressed(KEY_EQUAL)) {
		if (1 < data->viewport_scale) data->viewport_scale--;
		Head_setViewport(
				head, 
				SCREEN_WIDTH/data->viewport_scale, 
				SCREEN_HEIGHT/data->viewport_scale
			);
	}
	else if (IsKeyPressed(KEY_MINUS)) {
		if (data->viewport_scale < 8) data->viewport_scale++;
		Head_setViewport(
				head, 
				SCREEN_WIDTH/data->viewport_scale, 
				SCREEN_HEIGHT/data->viewport_scale
			);
	}
	
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
	testEngineRun, 
	NULL, 
	testSceneComposite, 
	NULL, 
	NULL, 
	NULL, 
	NULL,
	NULL
};

HeadVTable head_Callbacks = {
	testHeadSetup, 
	testHeadUpdate, 
	testHeadPreRender, 
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
		.lod_distances     = {8.0f, 16.0f, 64.0f},
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
	
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Kolibri Engine Test");
	
	Engine *engine = Engine_new(&engine_Callbacks);
	Head   *head   = Head_new(0, &head_Callbacks, engine);

	Head_setViewport(head, SCREEN_WIDTH, SCREEN_HEIGHT);
	RendererSettings *settings = Head_getRendererSettings(head);
//	settings->frustum_culling = false;
	Camera3D *cam = Head_getCamera(head);
	cam->fovy     = 45.0f;
	cam->up       = V3_UP;
	cam->position = (Vector3){0.0f, 1.75f, 0.0f};
	cam->target   = (Vector3){10.0f, 0.0f, 10.0f};

	Scene_new(&scene_Callbacks, NULL, engine);
	
	Entity *ents[21][21];

	for (int x = 0; x < 21; x++) {
		for (int y = 0; y < 21; y++) {
//			for (int z = 0; z < 21; z++) {
				ents[x][y]           = Entity_new(&entityTemplate, engine);
				ents[x][y]->visible  = true;
				ents[x][y]->active   = true;
				ents[x][y]->position = (Vector3){
					(x * 5.0f) - 50.0f,
					0.0f, //(z * 5.0f),
					(y * 5.0f) - 50.0f
				};
//			}
		}
	}

	Engine_run(engine, 0);

	return 0;
}
