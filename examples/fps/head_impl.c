#include <GL/gl.h>
#include <raylib.h>
#include <raymath.h>

#include "../reticle.h"
#include "game.h"
#include "../skybox.h"


#define MOUSE_SENSITIVITY   0.05f
#define MOVE_SPEED          5.0f
#define VIEWPORT_INCREMENT 48

#ifndef SKY_PATH
	#define SKY_PATH "resources/sky/SBS_SKY_panorama_%s.png"
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
	Callback Definitions
*/
void
testHeadSetup(Head *head)
{
	TestHeadData *user_data = Head_getLocalData(head);
	if (!user_data) {
		ERR_OUT("Failed to allocate TestHeadData.");
		return;
	}
	char 
		sky_path[256],
		weapon_path[256],
		weapon_texture_path[256];

	user_data->viewport_scale = 1;
	user_data->target         = NULL;
	user_data->target_data    = NULL;
	user_data->controller     = 0;
	user_data->look_sensitivity = 50.0f;

	user_data->current_weapon = 0;

	snprintf(sky_path, sizeof(sky_path), "%s%s", path_prefix, SKY_PATH);

	for (int i = 0; i < 6; i++) {
		static char filename[256];
		snprintf(filename, sizeof(filename), sky_path, SkyBox_names[i]);
		DBG_OUT("Sky Texture loaded: %s", filename);
		user_data->skybox_textures[i] = LoadTexture(filename);
		SetTextureFilter(user_data->skybox_textures[i], TEXTURE_FILTER_BILINEAR);
		SetTextureWrap(   user_data->skybox_textures[i], TEXTURE_WRAP_CLAMP);
	}

	/*
		Handle HUD Weapons
	*/
	Model *weapons = &user_data->weapons[0];
	snprintf(weapon_path, sizeof(weapon_path), "%s%s", path_prefix, "resources/models/weapons/weapon5.obj");
	weapons[0] = LoadModel(weapon_path);
	snprintf(weapon_path, sizeof(weapon_path), "%s%s", path_prefix, "resources/models/weapons/weapon5.png");
	Texture2D weaponTexture = LoadTexture(weapon_path);
	SetMaterialTexture(&weapons->materials[0], MATERIAL_MAP_ALBEDO, weaponTexture);
	SetTextureFilter(weaponTexture, TEXTURE_FILTER_BILINEAR);

	Head_setUserData(head, user_data);
}

void
testHeadPreRender(Head *head)
{
	TestHeadData *data = Head_getUserData(head);
	Camera3D     *cam  = Head_getCamera(head);
	rlClearScreenBuffers();
	BeginMode3D(*cam);
		SkyBox_draw(cam, data->skybox_textures, V4_ZERO);  
	EndMode3D();
	glClear(GL_DEPTH_BUFFER_BIT);   
}

void
testHeadPostRender(Head *head)
{
	/*
		Draw viewmodel
	*/
	TestHeadData *data        = Head_getUserData(head);
	PlayerData   *target_data = data->target_data;
	Camera3D     *cam  = Head_getCamera(head);
	Region        region      = Head_getRegion(head);
	Engine       *engine      = Head_getEngine(head);
	BeginMode3D(*cam);
		rlPushMatrix();
			glClear(GL_DEPTH_BUFFER_BIT);   
			Vector3 
				weap_offset = (Vector3){-0.25f, -0.5f, 1.0f},
				cam_pos     = cam->position,
				look_dir    = Vector3Normalize(Vector3Subtract(cam->target, cam->position));
			float   
				yaw_angle   = RAD2DEG * atan2f(look_dir.x, look_dir.z),
				pitch_angle = RAD2DEG * asinf(-look_dir.y);
			
			rlTranslatef(cam_pos.x, cam_pos.y , cam_pos.z);
			rlRotatef(yaw_angle,   0.0f, 1.0f, 0.0f);
			rlTranslatef(0.0f, 0.0f, 0.25f);
			rlRotatef(pitch_angle, 1.0f, 0.0f, 0.0f);
			rlTranslatef(-0.5f, -0.5f, 1.25f);
			
			DrawModel(
					data->weapons[0], 
					V3_ZERO, 
					0.25f, 
					WHITE
				);
		rlPopMatrix();
	EndMode3D();

	/*
		Draw Reticle
	*/
	int
		screen_width  = region.width,
		screen_height = region.height,
		half_width    = screen_width/2,
		half_height   = screen_height/2,
		center_x      = region.x + half_width,
		center_y      = region.y + half_height,
		spread_offset = (int)(Lerp(
				Vector3Length(target_data->prev_velocity),
				Vector3Length(data->target->velocity),
				Engine_getTickElapsed(engine)
			) * 1.5f);

	drawReticle(
			center_x,
			center_y,
			3,
			12,
			4 + spread_offset,
			BLACK,
			RETICLE_CENTER_DOT
				//| RETICLE_CIRCLE
				| RETICLE_CROSSHAIRS
		);
	drawReticle(
			center_x,
			center_y,
			1,
			10,
			5 + spread_offset,
			WHITE,
			RETICLE_CENTER_DOT
				| RETICLE_CROSSHAIRS
		);
	/*drawReticle(
			center_x,
			center_y,
			2,
			10,
			5 + spread_offset,
			WHITE,
			RETICLE_CIRCLE
		);*/
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
		controller_num = data->controller,
		screen_width   = GetScreenWidth(),
		screen_height  = GetScreenHeight();
	float 
		look_sensitivity = data->look_sensitivity,
		aspect_ratio = (float)screen_width / (float)screen_height;
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
		GET_KEY_OR_BUTTON_PRESSED(controller_num, GAMEPAD_BUTTON_MIDDLE_RIGHT, KEY_ESCAPE)
    )
    {
		Engine_pause(engine, true);
	}
	
Vector2 look_delta;
#ifndef ON_CONSOLE
	look_delta = GetMouseDelta();
#else
	look_delta = (Vector2){
			GetGamepadAxisMovement(
					controller_num, 
					GAMEPAD_AXIS_LEFT_X
				)*look_sensitivity,
			GetGamepadAxisMovement(
					controller_num,
					GAMEPAD_AXIS_LEFT_Y
				)*look_sensitivity
		};
#endif
    Vector2 
        mouse_look = look_delta,
        move_dir   = GET_KEY_OR_BUTTON_VECTOR(
				controller_num, 
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
    Vector3 forward = Vector3Subtract(camera->target, camera->position);
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
			controller_num, 
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
					Engine_getTickElapsed(engine)
				),
				(Vector3){0.0f, data->eye_height, 0.0f}
			);
	
	moveCamera(
		camera, 
		current_camera_pos
	);

	
	if (
		IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
		|| IsGamepadButtonPressed(controller_num, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)
		|| IsGamepadButtonPressed(controller_num, GAMEPAD_BUTTON_RIGHT_TRIGGER_2)
	) {
		Projectile_new(
				camera->target,
				Vector3Subtract(camera->target, camera->position),
				&Blast_Template,
				Engine_getScene(engine)
			);
	}
}
