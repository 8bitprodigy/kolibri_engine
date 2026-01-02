#include <raylib.h>
#include <raymath.h>

#include "kolibri.h"
#include "entity.h"
#include "scene.h"
#include "explosion.h"
#include "projectile.h"
#include "sprite.h"


typedef struct
ProjectileInfo
{
	float
		               damage,
		               speed,
		               timeout;
	union {
		float           gravity;
		float           homing_strength;
	};
	ProjectileMotion    motion;
	Renderable          renderable;
	ProjectileCollision Collision;
	ProjectileTimeout   Timeout;
}
ProjectileInfo;



/*
	Callback forward delcarations
*/
static void projectileSetup(    Entity *self);
static void projectileUpdate(   Entity *self, float           delta);
static void projectileRender(   Entity *self, float           delta);
static void projectileCollision(Entity *self, CollisionResult collision);


/*
	Template declarations
*/
static EntityVTable 
projectile_Callbacks = {
	.Setup       = projectileSetup,
	.Enter       = NULL,
	.Update      = projectileUpdate,
	.Render      = projectileRender,
	.OnCollision = projectileCollision,
	.OnCollided  = projectileCollision,
	.Exit        = NULL,
	.Free        = NULL,
};

static Entity
projectile_template = {
	.renderables       = {NULL},
	.lod_distances     = {1024.0f},
	.lod_count         = 1,
	.visibility_radius = 0.25f,
	.bounds            = {0.1f, 0.1f, 0.1f},
	.bounds_offset     = {0.0f, 0.0f, 0.0f},
	.renderable_offset = {0.0f, 0.0f, 0.0f},
	.user_data         = NULL,
	.vtable            = &projectile_Callbacks,
	.position          = V3_ZERO_INIT,
	.orientation       = V4_ZERO_INIT,
	.scale             = V3_ONE_INIT,
	.velocity          = V3_ZERO_INIT,
	.collision         = {.layers = 1, .masks = 1},
	.active            = true,
	.visible           = true,
	.collision_shape   = COLLISION_NONE, 
	.solid             = false
};


/*
	CALLBACKS
*/

static void 
projectileSetup(Entity *self)
{
	(void)self; /* Suppress warnings about unused arguments */
}

static void 
projectileUpdate(Entity *self, float delta)
{
	ProjectileInfo *info = self->user_data;
	ProjectileData *data = (ProjectileData*)&self->local_data;
	
	if (info && data && info->timeout <= data->elapsed_time) {
		if (info->Timeout) info->Timeout(self);
		self->visible = false;
		self->active  = false;
		Entity_free(self);
		return;
	}
	data->elapsed_time += delta;

	switch (info->motion) {
	case PROJECTILE_MOTION_BALLISTIC: {
			Vector3 gravity_vec = Vector3Scale(
					V3_DOWN,
					info->gravity * delta
				);
			self->velocity      = Vector3Add(
					self->velocity,
					gravity_vec
				);
			break;
		}
	case PROJECTILE_MOTION_HOMING: {
			if (!(data->target && data->target->active)) break;
			Vector3
				to_target   = Vector3Subtract(
						data->target->position, 
						self->position
					),
				desired_dir = Vector3Normalize(to_target),
				current_dir = Vector3Normalize(self->velocity),
				new_dir     = Vector3Lerp(
						current_dir, 
						desired_dir, 
						info->homing_strength * delta
					);
			self->velocity = Vector3Scale(
					Vector3Normalize(new_dir),
					info->speed
				);
			break;
		}
	case PROJECTILE_MOTION_STRAIGHT: /* FALLTHROUGH */
	default:
		break;
	}
	Vector3 new_pos = Vector3Add(
			self->position,
			Vector3Scale(self->velocity, delta)
		);
	CollisionResult collision;
	
	collision = Scene_raycast(
			Entity_getScene(self), 
			self->position,
			new_pos
		);

	self->renderable_offset = Vector3Subtract(self->position, new_pos);
	data->prev_offset       = self->renderable_offset;


	if (collision.hit) {
		if (info->Collision) info->Collision(self, collision);
		else {
			self->visible = false;
			self->active = false;
			Entity_free(self);
		}
	}
	else {
		self->position = new_pos;
	}
}

static void
projectileRender(Entity *self, float delta)
{
	(void)delta;
	if (
		!self
		|| !self->active
		|| !self->visible
	) {
		return;
	}
	ProjectileData *data = (ProjectileData*)&self->local_data;
	Engine *engine = Entity_getEngine(self);
	if (!engine) {
		ERR_OUT("ERROR: Engine is NULL!");
		return;
	}
	
	float tick_elapsed_val = Engine_getTickElapsed(engine);
	self->renderable_offset = Vector3Lerp(
		data->prev_offset,
		V3_ZERO,
		tick_elapsed_val
	);
	
	Vector3 velocityDir = Vector3Normalize(self->velocity);
	float spinAngle     = Entity_getAge(self) * 10.0f; 
	// Rotate +Z to velocity direction
	Quaternion align    = QuaternionFromVector3ToVector3(V3_FORWARD, velocityDir);
	// Spin around velocity axis
	Quaternion spin     = QuaternionFromAxisAngle(velocityDir, spinAngle);
	// Combine: first align, then spin around that new forward axis
	self->orientation   = QuaternionMultiply(spin, align);
	
	SpriteInfo *sinfo = (SpriteInfo*)self->renderables[0]->data;
	SpriteData *sdata = &data->sprite_data;
	AnimateSprite(
			sinfo,
			sdata,
			Entity_getAge(self)
		);
}

static void 
projectileCollision(Entity *self, CollisionResult collision)
{
	ProjectileData *data = (ProjectileData*)self->local_data;
	if (collision.entity == data->source) return;
	self->visible = false;
	self->active  = false;
	Entity_free(self);
}

static void
projectileFree(Entity *self)
{
	(void)self;
}


/*
	PUBLIC METHODS
*/
ProjectileInfo *
ProjectileInfo_new(
	float                damage,
	float                speed,
	float                timeout,
	ProjectileMotion     motion,
	float                gravity_or_homing_strength,
	Renderable          *renderable,
	ProjectileCollision  Collision_Callback,
	ProjectileTimeout    Timeout_Callaback
)
{
	ProjectileInfo *info = calloc(1, sizeof(ProjectileInfo));
	if (!info) {
		ERR_OUT("Failed to allocate memory for ProjectileInfo.");
		return NULL;
	}

	info->damage     =  damage;
	info->speed      =  speed;
	info->timeout    =  timeout;
	info->gravity    =  gravity_or_homing_strength;
	info->motion     =  motion;
	info->renderable = *renderable;
	info->Collision  =  Collision_Callback;
	info->Timeout    =  Timeout_Callaback;

	return info;
}

Entity *
Projectile_new(
       ProjectileInfo *info,
       Vector3         position,
       Vector3         direction,
       Entity         *source,
       Entity         *target,
       Scene          *scene
)
{
	Entity *projectile = Entity_new(
			&projectile_template, 
			scene,
			sizeof(ProjectileData)
		);
	if (!projectile) return projectile;

	ProjectileData *data = (ProjectileData*)&projectile->local_data;
	
	data->sprite_data       = (SpriteData){
			.start_frame   = 0,
			.current_frame = 0,
			.playing       = true,
		};
	data->prev_offset       = V3_ZERO;
	data->elapsed_time      = 0.0f;
	data->source            = source;
	data->target            = target;

	projectile->user_data      = info;
	projectile->renderables[0] = &info->renderable;
	projectile->position       = position;
	projectile->visible        = true;
	projectile->active         = true;
	projectile->orientation    = QuaternionFromVector3ToVector3(
			V3_FORWARD,
			Vector3Normalize(direction)
		);
	projectile->velocity              = Vector3Scale(
			direction,
			info->speed
		);

	return projectile;
}
