#include <GL/gl.h>
#include <raylib.h>
#include <raymath.h>
#include <string.h>

#include "game.h"
#include "../heightmap.h"
#include "../reticle.h"
#include "../skybox.h"
#define WEAPON_IMPLEMENTATION
#include "../weapon.h"


#define MOUSE_SENSITIVITY   0.05f
#define MOVE_SPEED          5.0f
#define VIEWPORT_INCREMENT 48

#ifndef SKY_PATH
	#define SKY_PATH "resources/sky/SBS_SKY_panorama_%s.png"
#endif

#define MOD(a, b) (((a) % (b) + (b)) % (b))


/*
	Callback Forward Declarations
*/
void fpsHeadSetup(     Head *head);
void fpsHeadPreRender( Head *head);
void fpsHeadPostRender(Head *head);
void fpsHeadUpdate(    Head *head, float delta);
void fpsHeadResize(    Head *head, uint   width, uint height);


/*
	VTable definition
*/
HeadVTable head_Callbacks = {
	fpsHeadSetup, 
	fpsHeadUpdate, 
	fpsHeadPreRender, 
	fpsHeadPostRender,
	fpsHeadResize, 
	NULL, 
	NULL
};


/*
	Private Methods
*/
static void
cycleWeapon(Head *head, int dir)
{
	FPSHeadData *data = Head_getLocalData(head);
	
	int 
		direction = (dir < 0)? -1 : 1,
		next_slot = MOD(data->current_weapon + direction, WEAPON_NUM_WEAPONS),
		i         = 0;
	
	while (!(data->owned_weapons & 1<<next_slot )) {
		next_slot = MOD(next_slot + direction, WEAPON_NUM_WEAPONS);
		if (WEAPON_NUM_WEAPONS < i++) return;
	}
	
	data->current_weapon = next_slot;
}

static void
selectWeapon(Head *head, uint8_t selected)
{
	FPSHeadData *data = Head_getLocalData(head);

	if (!(data->owned_weapons & 1<<selected )) return;

	data->current_weapon = selected;
}


/*
	Callback Definitions
*/
void
teleportHead(Entity *entity, Vector3 from, Vector3 to)
{
	PlayerData *data = entity->user_data;
	Head       *head = data->head;
	Camera     *cam  = Head_getCamera(head);

	Vector3 
		player_offset = Vector3Subtract(from, data->prev_position),
		cam_offset    = Vector3Subtract(cam->position, from),
		new_cam_pos   = Vector3Add(to, cam_offset);
	data->prev_position = Vector3Add(to, player_offset);
	moveCamera(cam, new_cam_pos);
} /* teleportHead */

void
fpsHeadSetup(Head *head)
{
	FPSHeadData *user_data = Head_getLocalData(head);
	if (!user_data) {
		ERR_OUT("Failed to allocate FPSHeadData.");
		return;
	}
	RendererSettings *settings = Head_getRendererSettings(head);
	settings->frustum_culling = false;
	
	char 
		sky_path[256],
		weapon_path[256],
		weapon_texture_path[256];

	user_data->viewport_scale   = 1;
	user_data->target           = NULL;
	user_data->target_data      = NULL;
	user_data->controller       = 0;
	user_data->look_sensitivity = 50.0f;
	user_data->owned_weapons    = (1<<WEAPON_NUM_WEAPONS) - 1;
	user_data->current_weapon   = 1;
	
	memset(user_data->weapon_data, 0, sizeof(WeaponData) * WEAPON_NUM_WEAPONS);
	user_data->weapon_data[WEAPON_MINIGUN].data = (Any)1.0f;

	snprintf(sky_path, sizeof(sky_path), "%s%s", path_prefix, SKY_PATH);

	for (int i = 0; i < 6; i++) {
		static char filename[256];
		snprintf(filename, sizeof(filename), sky_path, SkyBox_names[i]);
		user_data->skybox_textures[i] = LoadTexture(filename);
		SetTextureFilter(user_data->skybox_textures[i], TEXTURE_FILTER_BILINEAR);
		SetTextureWrap(  user_data->skybox_textures[i], TEXTURE_WRAP_CLAMP);
	}
	
	Head_setUserData(head, user_data);
} /* fpsHeadSetup */

void
fpsHeadPreRender(Head *head)
{
	FPSHeadData *data = Head_getUserData(head);
	Camera3D     *cam  = Head_getCamera(head);
	rlClearScreenBuffers();
	BeginMode3D(*cam);
		SkyBox_draw(cam, data->skybox_textures, V4_ZERO);  
	EndMode3D();
	glClear(GL_DEPTH_BUFFER_BIT);   
}

void
fpsHeadPostRender(Head *head)
{
	/*
		Draw viewmodel
	*/
	FPSHeadData *data        = Head_getUserData(head);
	PlayerData   *target_data = data->target_data;
	Camera3D     *cam         = Head_getCamera(head);
	Region        region      = Head_getRegion(head);
	Engine       *engine      = Head_getEngine(head);
	Scene        *scene       = Engine_getScene(engine);
	
	BeginMode3D(*cam);
		rlPushMatrix();
			glClear(GL_DEPTH_BUFFER_BIT);   
			Vector3 
				cam_pos     = cam->position,
				look_dir    = Vector3Normalize(Vector3Subtract(cam->target, cam->position));
			float   
				yaw_angle   = RAD2DEG * atan2f(look_dir.x, look_dir.z),
				pitch_angle = RAD2DEG * asinf(-look_dir.y);
			
			rlTranslatef(cam_pos.x, cam_pos.y , cam_pos.z);
			rlRotatef(yaw_angle,   0.0f, 1.0f, 0.0f);
			rlTranslatef(0.0f, 0.0f, 0.25f);
			rlRotatef(pitch_angle, 1.0f, 0.0f, 0.0f);
			rlTranslatef(-0.5f, -0.5f, 1.0f);
			
			DrawModel(
					weapon_Infos[data->current_weapon].model, 
					V3_ZERO, 
					0.25f, 
					HeightmapScene_sampleShadow(scene, cam_pos)
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
} /* fpsHeadPostRender */

void
fpsHeadResize(Head *head, uint width, uint height)
{
	FPSHeadData *data = Head_getUserData(head);

	int
		region_height = height - (VIEWPORT_INCREMENT * data->viewport_scale),
		region_width  = width  - (VIEWPORT_INCREMENT * data->viewport_scale);

		Head_setRegion(head, (Region){
				width/2 - region_width/2, 
				height/2 - region_height/2, 
				region_width, 
				region_height
			});
} /* fpsHeadResize */

void
fpsHeadUpdate(Head *head, float delta)
{
	(void)delta;
    FPSHeadData *data = Head_getUserData(head);

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

PLAYER_INPUT: {
		Camera *camera  = Head_getCamera(head);
		if (!data->target) {
			UpdateCamera(camera, CAMERA_FREE);
			return;
		}
		
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
					GAMEPAD_BUTTON_LEFT_FACE_UP,
					KEY_W, 
					GAMEPAD_BUTTON_LEFT_FACE_DOWN,
					KEY_S, 
					GAMEPAD_BUTTON_LEFT_FACE_RIGHT,
					KEY_D, 
					GAMEPAD_BUTTON_LEFT_FACE_LEFT,
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
				GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, 
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

#ifndef ON_CONSOLE
		int  dir = (int)GetMouseWheelMoveV().y;
#else
		int dir = IsGamepadButtonPressed(
				controller_num,
				GAMEPAD_BUTTON_RIGHT_FACE_UP
			) - IsGamepadButtonPressed(
				controller_num,
				GAMEPAD_BUTTON_RIGHT_FACE_LEFT
			);
#endif
		if (dir) {
			cycleWeapon(head, dir);
		}
		bool fire_button = (
				IsMouseButtonDown(MOUSE_BUTTON_LEFT)
				|| IsGamepadButtonDown(
						controller_num, 
						GAMEPAD_BUTTON_RIGHT_FACE_DOWN
					)
			);
		Weapon_fire(
				&weapon_Infos[data->current_weapon],
				&data->weapon_data[data->current_weapon],
				player,
				camera->target,
				Vector3Normalize(
						Vector3Subtract(
								camera->target, 
								camera->position
							)
					), 
				fire_button
			);
	}
} /* fpsHeadUpdate */
