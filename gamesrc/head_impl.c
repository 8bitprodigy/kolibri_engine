#include "game.h"
#include <raymath.h>


#define MOUSE_SENSITIVITY 0.05f
#define MOVE_SPEED 5.0f


/*
	Callback Forward Declarations
*/
void testHeadSetup(     Head *head);
void testHeadPreRender( Head *head);
void testHeadPostRender(Head *head);
void testHeadUpdate(    Head *head, float delta);


/*
	VTable definition
*/
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


/*
	Callback Definitions
*/
void
testHeadSetup(Head *head)
{
	TestHeadData *user_data = malloc(sizeof(TestHeadData));
	if (!user_data) {
		ERR_OUT("Failed to allocate TestHeadData.");
		return;
	}

	user_data->viewport_scale = 1;
	user_data->target         = NULL;
	
	Model *skysphere = &user_data->model;
	
	Mesh mesh = GenMeshSphere(1.0f, 13, 16);
	*skysphere = LoadModelFromMesh(mesh);

	Mesh *skyMesh = &skysphere->meshes[0];
	Matrix rotMatrix = MatrixRotateX(DEG2RAD * -90.0f); /* Adjust angle as needed */

	for (int i = 0; i < skyMesh->vertexCount; i++) {
		/* Fix UV coordinates */
		float u = skyMesh->texcoords[i*2];
		float v = skyMesh->texcoords[i*2+1];
		skyMesh->texcoords[i*2] = 1.0f - v;
		skyMesh->texcoords[i*2+1] = u;
		
		/* Rotate vertex positions */
		Vector3 vertex = {
			skyMesh->vertices[i*3],     /* x */
			skyMesh->vertices[i*3+1],   /* y */
			skyMesh->vertices[i*3+2]    /* z */
		};
		
		Vector3 rotatedVertex = Vector3Transform(vertex, rotMatrix);
		
		skyMesh->vertices[i*3]     = rotatedVertex.x;
		skyMesh->vertices[i*3+1]   = rotatedVertex.y;
		skyMesh->vertices[i*3+2]   = rotatedVertex.z;
	}

	/* Update both vertex and UV buffers */
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
	
	if      (IsKeyPressed(KEY_EQUAL)) {
		if (0 < data->viewport_scale) data->viewport_scale--;
		else return;

		int
			height = SCREEN_HEIGHT - (48 * data->viewport_scale),
			width  = height * ASPECT_RATIO;
		
		Head_setViewport(head, width, height);
		DBG_OUT("New Viewport dimensions: W-%d\tH-%d", width, height);
	}
	else if (IsKeyPressed(KEY_MINUS)) {
		if (data->viewport_scale < 12) data->viewport_scale++;
		else return;

		int
			height  = SCREEN_HEIGHT - (48 * data->viewport_scale),
			width = height * ASPECT_RATIO;
			
		Head_setViewport(head, width, height);
		DBG_OUT("New Viewport dimensions: W-%d\tH-%d", width, height);
	}
	
    Camera *camera  = Head_getCamera(head);
	if (!data->target) {
		UpdateCamera(camera, CAMERA_FREE);
		return;
	}

	CollisionResult collision;
	Entity *player = data->target;
	Engine *engine = Head_getEngine(head);
	Vector2 
		mouse_look = GetMouseDelta(),
		move_dir   = GET_KEY_VECTOR(KEY_S, KEY_W, KEY_A, KEY_D);
	
	UpdateCameraPro(
			camera,
			V3_ZERO,
			(Vector3){
                mouse_look.x * MOUSE_SENSITIVITY,
                mouse_look.y * MOUSE_SENSITIVITY,
                0.0f
            },
            0.0f
        );
		
	Vector3 forward = Vector3Normalize(Vector3Subtract(camera->position, camera->target));
	forward.y       = 0.0f;
	forward         = Vector3Normalize(forward);
	
	Vector3 
		prev_pos   = camera->position,
		difference = V3_ZERO,
		right      = Vector3RotateByAxisAngle(forward, V3_UP, -90.0f),
		movement   = Vector3Add(
				Vector3Scale(forward, move_dir.x * MOVE_SPEED * delta),
				Vector3Scale(right,   move_dir.y * MOVE_SPEED * delta)
			);

	collision  = Entity_move(player, movement);

	camera->position = Vector3Add(     player->position, (Vector3){0.0f, data->eye_height, 0.0f});
	difference       = Vector3Subtract(camera->position, prev_pos);
    camera->target   = Vector3Add(camera->target, difference);
}
