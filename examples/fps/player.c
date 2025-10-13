#include "game.h"


/*
	Callback Forward Declarations
*/
void playerSetup( Entity *self);
void playerUpdate(Entity *self, float delta);

/*
	Template Declarations
*/
TestRenderableData rd_player = (TestRenderableData){.color = MAGENTA};
	
Renderable         
	r_player = (Renderable){
			.data   = &rd_player,
			.Render = testRenderableBoxWiresCallback
		};

EntityVTable player_Callbacks = (EntityVTable){
	.Setup       = playerSetup,
	.Enter       = NULL,
	.Update      = playerUpdate,
	.Render      = NULL,
	.OnCollision = NULL,
	.OnCollided  = NULL,
	.Exit        = NULL,
	.Free        = NULL
};

Entity playerTemplate = (Entity){
	.renderables       = {&r_player},
	.lod_distances     = {48.0f},
	.lod_count         = 1,
	.visibility_radius = 4.5f,
	.bounds            = {1.0f, 2.0f, 1.0f},
	.bounds_offset     = {0.0f, 1.0f, 0.0f},
	.renderable_offset = {0.0f, 1.0f, 0.0f},
	.user_data         = NULL,
	.vtable            = &player_Callbacks,
	.position          = V3_ZERO,
	.orientation       = V4_ZERO,
	.scale             = V3_ONE,
	.velocity          = V3_ZERO,
	.collision         = {.layers = 1, .masks = 1},
	.active            = true,
	.visible           = true,
	.collision_shape   = COLLISION_BOX, 
	.solid             = true
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

	data->prev_position = V3_ZERO;
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
	PlayerData *data     = self->user_data;
	data->prev_velocity       = self->velocity;
	Vector3    
		*position  = &self->position,
		*velocity  = &self->velocity,
		 vel       = *velocity,
		*direction = &data->direction,
		 dir       = *direction;
	
	bool is_on_floor = Entity_isOnFloor(self);
	
	if (is_on_floor && data->request_jump) {
		velocity->y = JUMP_VELOCITY;
		data->request_jump = false;
	}
	if (!is_on_floor) 
		velocity->y -= (0.0f < velocity->y)
			? JUMP_GRAVITY * delta 
			: FALL_GRAVITY * delta;
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
	Entity_moveAndSlide(self, intended_movement, 3);
}
