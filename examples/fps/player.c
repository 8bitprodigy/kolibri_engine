#include "game.h"
#include "scene.h"


/*
	Callback Forward Declarations
*/
void playerSetup( Entity *self);
void playerUpdate(Entity *self, float delta);

/*
	Template Declarations
*/
	
Renderable         
	r_player = {
			.data   = &MAGENTA,
			.Render = NULL,//testRenderableBoxWiresCallback
		};

EntityVTable 
player_Callbacks = {
		.Setup       = playerSetup,
		.Enter       = NULL,
		.Update      = playerUpdate,
		.Render      = NULL,
		.OnCollision = NULL,
		.OnCollided  = NULL,
		.Teleport    = teleportHead,
		.Exit        = NULL,
		.Free        = NULL
	};

Entity
playerTemplate = {
		.renderables       = {&r_player},
		.lod_distances     = {1024.0f},
		.lod_count         = 1,
		.visibility_radius = 4.5f,
		.bounds            = {1.0f, 2.0f, 1.0f},
		.bounds_offset     = {0.0f, 1.0f, 0.0f},
		.renderable_offset = {0.0f, 1.0f, 0.0f},
		.user_data         = NULL,
		.vtable            = &player_Callbacks,
		.position          = V3_ZERO_INIT,
		.orientation       = V4_ZERO_INIT,
		.scale             = V3_ONE_INIT,
		.velocity          = V3_ZERO_INIT,
		.collision         = {.layers = 1, .masks = 1},
		.active            = true,
		.visible           = true,
		.collision_shape   = COLLISION_BOX, 
		.solid             = true,
		.floor_max_angle   = 60.0f,
		.max_slides        = MAX_SLIDES,
	};


/*
	Callback Definitions
*/
/* Player Callbacks */
void
playerSetup(Entity *self)
{
	PlayerData *data = malloc(sizeof(PlayerData));
	if (!data) {
		ERR_OUT("Failed to allocate PlayerData.");
		return;
	}
	
	data->prev_position = self->position;
	data->prev_velocity = V3_ZERO;
	data->move_dir      = V3_ZERO;
	data->direction     = V3_ZERO;
	data->request_jump  = false;

	self->user_data = data;
}


void
playerFree(Entity *self)
{
	if (!self->user_data) return;
	free(self->user_data);
}


void
playerUpdate(Entity *self, float delta)
{
	//DBG_OUT("Entering playerUpdate()");
	PlayerData *data     = self->user_data;
	data->prev_velocity       = self->velocity;
	Vector3    
		*velocity  = &self->velocity,
		 vel       = *velocity,
		*direction = &data->direction,
		 dir       = *direction;
	
	bool is_on_floor = Entity_isOnFloor(self);

	if (is_on_floor) {
		data->frames_since_grounded = 0;
		CollisionResult floor_check = Scene_checkCollision(
				Entity_getScene(self), 
				self, 
				Vector3Add(
						self->position, 
						(Vector3){0, -0.05f, 0}
					)
			);
		if (floor_check.hit) {
			float 
				floor_dot     = Vector3DotProduct(floor_check.normal, V3_UP),
				dot_threshold = cosf(self->floor_max_angle * DEG2RAD);
			if (floor_dot > dot_threshold) {  // Nearly flat ground
				velocity->y = 0;
			}
		}
	}
	if (!is_on_floor) {
		data->frames_since_grounded++;
		velocity->y -= (0.0f < velocity->y)
			? JUMP_GRAVITY * delta 
			: FALL_GRAVITY * delta;

	}
/*
	if (0.0f < velocity->y && data->request_end_jump) {
		velocity->y *= 0.7f;
		data->request_end_jump = false;
	}
*/
	if (data->frames_since_grounded < 5 && data->request_jump) {
		velocity->y = JUMP_VELOCITY;
		data->request_jump = false;
	}
	data->direction = Vector3Lerp(data->direction, data->move_dir, delta * CONTROL);
	
	float   
		friction_per_second = (is_on_floor ? FRICTION : AIR_DRAG),
		decel               = powf(friction_per_second, delta * 60.0f);
	//DBG_OUT("delta=%f decel=%f", delta, decel);
	Vector3 horiz_vel = (Vector3){vel.x * decel, 0.0f, vel.z * decel};
	float   hvel_len  = Vector3Length(horiz_vel);
	
	if (hvel_len < (MAX_SPEED * 0.01f)) horiz_vel = (Vector3){0};
	
	float 
		speed     = Vector3DotProduct(horiz_vel, dir),
		max_speed = MAX_SPEED,
		accel     = Clamp(max_speed - speed, 0.0f, MAX_ACCEL * delta);
	
	horiz_vel.x += dir.x * accel;
	horiz_vel.z += dir.z * accel;
	
	velocity->x = horiz_vel.x;
	velocity->z = horiz_vel.z;
	
	Vector3 intended_movement = Vector3Scale(*velocity, delta);
	data->prev_position       = self->position;
	Entity_moveAndSlide(self, intended_movement);
}
