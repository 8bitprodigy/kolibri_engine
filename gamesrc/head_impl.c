#include "game.h"
#include <raylib.h>
#include <raymath.h>


#define MOUSE_SENSITIVITY 0.05f
#define MOVE_SPEED        5.0f


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
	user_data->target_data    = NULL;
	
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

	Texture2D skyTexture = LoadTexture("resources/sky/SBS_sky_panorama_full.png");
	if (skyTexture.id == 0) {
		ERR_OUT("Failed to load sky texture!");
		skyTexture = LoadTexture("");
	}
	
	SetMaterialTexture(&skysphere->materials[0], MATERIAL_MAP_ALBEDO, skyTexture);
	SetTextureFilter(skyTexture, TEXTURE_FILTER_BILINEAR);

	Head_setUserData(head, user_data);
}

void
testHeadPreRender(Head *head)
{
	TestHeadData *data = Head_getUserData(head);
	Camera3D     *cam  = Head_getCamera(head);
	//ClearBackground(WHITE);
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

    /* Adjust viewport size */
    if      (IsKeyPressed(KEY_EQUAL)) {
        if (0 < data->viewport_scale) data->viewport_scale--;
        else goto PLAYER_INPUT;

        int
            height = SCREEN_HEIGHT - (48 * data->viewport_scale),
            width  = height * ASPECT_RATIO;
        
        Head_setRegion(head, (Region){GetScreenWidth()/2 - width/2 , GetScreenHeight()/2 - height/2 ,width, height});
    }
    else if (IsKeyPressed(KEY_MINUS)) {
        if (data->viewport_scale < 12) data->viewport_scale++;
        else goto PLAYER_INPUT;

        int
            height  = SCREEN_HEIGHT - (48 * data->viewport_scale),
            width = height * ASPECT_RATIO;
            
        Head_setRegion(head, (Region){GetScreenWidth()/2 - width/2 , GetScreenHeight()/2 - height/2 ,width, height});
    } /* End adjust viewport size */

PLAYER_INPUT:
    Camera *camera  = Head_getCamera(head);
    if (!data->target) {
        UpdateCamera(camera, CAMERA_FREE);
        return;
    }

    CollisionResult collision;
    Entity     *player      = data->target;
    PlayerData *player_data = data->target_data;
    Engine     *engine      = Head_getEngine(head);

	if (
		GET_KEY_OR_BUTTON_PRESSED(0, GAMEPAD_BUTTON_MIDDLE, KEY_ESCAPE)
    )
		Engine_pause(engine, true);
	
    Vector2 
        mouse_look = GetMouseDelta(),
        move_dir   = GET_KEY_OR_BUTTON_VECTOR(
				0, 
				GAMEPAD_BUTTON_RIGHT_FACE_UP,
				KEY_W, 
				GAMEPAD_BUTTON_RIGHT_FACE_DOWN,
				KEY_S, 
				GAMEPAD_BUTTON_RIGHT_FACE_RIGHT,
				KEY_D, 
				GAMEPAD_BUTTON_RIGHT_FACE_LEFT,
				KEY_A
			);
    
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

	/* Calculate player inputs */
    /* FIX: Calculate forward direction FROM camera TO target */
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
    forward.y       = 0.0f;  /* Keep movement on horizontal plane */
    forward         = Vector3Normalize(forward);
    
    Vector3 
        side      = Vector3CrossProduct(forward, V3_UP),  /* Cross product is cleaner than rotation */
        movement   = Vector3Add(
                Vector3Scale(forward, move_dir.x),
                Vector3Scale(side,   move_dir.y)
            );

	player_data->move_dir     = movement;
	if (
		GET_KEY_OR_BUTTON_PRESSED(
			0, 
			GAMEPAD_BUTTON_LEFT_TRIGGER_1, 
			KEY_SPACE
		)
	)
	{
		player_data->request_jump = true;
	}
	/* End calculate player inputs */
	
    moveCamera(
			camera, 
			Vector3Add(
					player->position, 
					(Vector3){0.0f, data->eye_height, 0.0f}
				)
		);

	DBG_OUT("End of Head Update function.");
}
