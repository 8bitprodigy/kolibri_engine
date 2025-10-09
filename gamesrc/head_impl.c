#include "game.h"
#include <raylib.h>
#include <raymath.h>


#define MOUSE_SENSITIVITY   0.05f
#define MOVE_SPEED          5.0f
#define VIEWPORT_INCREMENT 48

#ifndef SKY_PATH
	#define SKY_PATH "resources/sky/SBS_sky_panorama_full.png"
#endif


/*
	Callback Forward Declarations
*/
void testHeadSetup(     Head *head);
void testHeadPreRender( Head *head);
void testHeadPostRender(Head *head);
void testHeadUpdate(    Head *head, float delta);
void testHeadResize(    Head *head, uint   width, uint height);


/*
	VTable definition
*/
HeadVTable head_Callbacks = {
	testHeadSetup, 
	testHeadUpdate, 
	testHeadPreRender, 
	testHeadPostRender,
	testHeadResize, 
	NULL, 
	NULL
};

/*
	Misc functions
*/
void
drawCrosshairs(
	int   center_x, 
	int   center_y, 
	int   hair_thickness,
	int   outline_thickness, 
	int   length, 
	int   spread,
	bool  center_dot, 
	Color outline, 
	Color hair
)
{
	if (hair_thickness    < 0) hair_thickness    = 0;
	if (outline_thickness < 0) outline_thickness = 0;
	int
		h_hair_y   = center_y - (hair_thickness/2),
		h_hair_l_x = center_x - length - spread,
		h_hair_r_x = center_x + spread,
		v_hair_x   = center_x - (hair_thickness/2),
		v_hair_u_y = center_y - length - spread,
		v_hair_d_y = center_y + spread,

		h_outline_y   = h_hair_y   - (outline_thickness/2) - 1,
		h_outline_l_x = h_hair_l_x - outline_thickness,
		h_outline_r_x = h_hair_r_x - outline_thickness,
		v_outline_x   = v_hair_x   - (outline_thickness/2) - 1,
		v_outline_u_y = v_hair_u_y - outline_thickness,
		v_outline_d_y = v_hair_d_y - outline_thickness,
		
		outline_width  = hair_thickness + (2 * outline_thickness),
		outline_height = length         + (2 * outline_thickness);

	DrawRectangle(h_outline_l_x,     h_outline_y,       outline_height, outline_width,  outline);
	DrawRectangle(h_outline_r_x + 1, h_outline_y,       outline_height, outline_width,  outline);
	DrawRectangle(v_outline_x,       v_outline_u_y,     outline_width,  outline_height, outline);
	DrawRectangle(v_outline_x,       v_outline_d_y + 1, outline_width,  outline_height, outline);
	if (center_dot)
		DrawRectangle(v_outline_x,     h_outline_y,       outline_width,  outline_width,  outline);

	DrawRectangle(h_hair_l_x,     h_hair_y,       length,         hair_thickness, hair);
	DrawRectangle(h_hair_r_x + 1, h_hair_y,       length,         hair_thickness, hair);
	DrawRectangle(v_hair_x,       v_hair_u_y,     hair_thickness, length,         hair);
	DrawRectangle(v_hair_x,       v_hair_d_y + 1, hair_thickness, length,         hair);
	if (center_dot)
		DrawRectangle(v_hair_x,     h_hair_y,       hair_thickness, hair_thickness, hair);
		
}

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

	Texture2D skyTexture = LoadTexture(SKY_PATH);
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
	TestHeadData *data = Head_getUserData(head);
	Region      region = Head_getRegion(head);
	DrawFPS(10, 10);

	int
		screen_width  = region.width,
		screen_height = region.height,
		half_width    = screen_width/2,
		half_height   = screen_height/2;

	drawCrosshairs(
			region.x + half_width,
			region.y + half_height,
			1,
			1,
			10,
			5+(int)Vector3Length(data->target->velocity),
			true,
			BLACK,
			WHITE
		);
}

void
testHeadResize(Head *head, uint width, uint height)
{
	TestHeadData *data = Head_getUserData(head);
	float aspect_ratio = (float)width/(float)height;

	int
		region_height = height - (VIEWPORT_INCREMENT * data->viewport_scale),
		region_width  = width  - (VIEWPORT_INCREMENT * data->viewport_scale);

		Head_setRegion(head, (Region){
				width/2 - region_width/2, 
				height/2 - region_height/2, 
				region_width, 
				region_height
			});
}

void
testHeadUpdate(Head *head, float delta)
{
    TestHeadData *data = Head_getUserData(head);

	int 
		screen_width   = GetScreenWidth(),
		screen_height  = GetScreenHeight();
	float aspect_ratio = (float)screen_width / (float)screen_height;
    /* Adjust viewport size */
    if      (IsKeyPressed(KEY_EQUAL)) {
        if (0 < data->viewport_scale) data->viewport_scale--;
        else goto PLAYER_INPUT;
    }
    else if (IsKeyPressed(KEY_MINUS)) {
        if (data->viewport_scale < 12) data->viewport_scale++;
        else goto PLAYER_INPUT;
    }
    else {
			goto PLAYER_INPUT;
    } 

		int 
			height = screen_height - (VIEWPORT_INCREMENT * data->viewport_scale),
			width  = height * aspect_ratio;

		Head_setRegion(head, (Region){
				screen_width/2  - width/2,
				screen_height/2 - height/2,
				width,
				height
			});
    
    /* End adjust viewport size */

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
    {
		Engine_pause(engine, true);
	}
	
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
	Vector3
		old_camera_pos = player_data->prev_position,
		new_camera_pos = player->position,
		current_camera_pos = Vector3Add(
				Vector3Lerp(
					old_camera_pos,
					new_camera_pos,
					Engine_getTickTime(engine)
				),
				(Vector3){0.0f, data->eye_height, 0.0f}
			);
	
	moveCamera(
		camera, 
		current_camera_pos
	);
}
