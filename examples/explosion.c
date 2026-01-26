#include <stddef.h>
#include <stdlib.h>

#include "dynamicarray.h"
#include "entity.h"
#include "explosion.h"
#include "scene.h"
/* MUST COME LAST */
#include <raymath.h>


typedef struct
ExplosionInfo
{
	float
		radius,
		falloff,
		damage,
		impulse;
	SpriteInfo *sprite_info;
	Renderable  renderable;
}
ExplosionInfo;

typedef struct
{
	SpriteData sprite_data;
}
ExplosionData;


/*
	Callback forward delcarations
*/
void explosionSetup(    Entity *self);
void explosionRender(   Entity *self, float delta);


/*
	Template declarations
*/
Renderable
	explosion_Renderable = {
			.data        = NULL,
			.Render      = RenderBillboard,
			.transparent = true,
		};

EntityVTable 
Explosion_Callbacks = {
		.Setup       = NULL,
		.Enter       = NULL,
		.Update      = NULL,
		.Render      = explosionRender,
		.OnCollision = NULL,
		.OnCollided  = NULL,
		.Exit        = NULL,
		.Free        = NULL,
	};

Entity
Explosion_Template = {
		.renderables       = {&explosion_Renderable},
		.lod_distances     = {512.0f},
		.lod_count         = 1,
		.visibility_radius = 0.25f,
		.bounds            = {0.1f, 0.1f, 0.1f},
		.bounds_offset     = {0.0f, 0.0f, 0.0f},
		.renderable_offset = {0.0f, 0.0f, 0.0f},
		.vtable            = &Explosion_Callbacks,
		.position          = V3_ZERO_INIT,
		.orientation       = V3_ZERO_INIT,
		.scale             = V3_ONE_INIT,
		.velocity          = V3_ZERO_INIT,
		.collision         = {.layers = 1, .masks = 1},
		.active            = true,
		.visible           = true,
		.collision_shape   = COLLISION_SPHERE, 
		.solid             = false
	};


/*
	PRIVATE METHODS
*/
float 
power_curve(float curve, float max_distance, float distance) 
{
    if (distance >= max_distance) return 0.0f;
    
    float t = distance / max_distance;
    return 1.0f - powf(t, curve);
}

/*
	CALLBACKS
*/
void 
explosionRender(Entity *self, float delta)
{
	(void)delta;
	ExplosionData *data = (ExplosionData*)&self->local_data;
	
	SpriteInfo *sinfo = (SpriteInfo*)self->renderables[0]->data;
	SpriteData *sdata = &data->sprite_data;
	AnimateSprite(
			sinfo,
			sdata,
			Entity_getAge(self)
		);
}


/*
	PUBLIC METHODS
*/

void
ExplosionComplete(SpriteInfo *info, SpriteData *data) 
{
	(void)info;
	ExplosionData *edata   = (ExplosionData*)(
			(char*)data - offsetof(ExplosionData, sprite_data)
		);
	Entity        *entity = (Entity*)(
			(char*)edata - offsetof(Entity, local_data)
		);
	entity->active  = false;
	entity->visible = false;
	Entity_free(entity);
}


ExplosionInfo *
ExplosionInfo_new(
	float           radius, 
	float           falloff, 
	float           damage, 
	float           impulse,
	float           scale,
	float           time_per_frame,
	Color           color,
	Texture2D       atlas,
	SpriteAlignment alignment,
	size_t          x_num_frames,
	size_t          y_num_frames,
	size_t          total_frames
)
{
	ExplosionInfo *info;
	if (!(info = malloc(sizeof(ExplosionInfo)))) {
		ERR_OUT("Failed to allocate memory for ExplosionInfo.");
		return NULL;
	}

	SpriteInfo *sprite_info = SpriteInfo_newRegular(
			scale,
			time_per_frame,
			color,
			atlas,
			x_num_frames,
			y_num_frames,
			total_frames,
			alignment,
			SPRITE_DIR_FORWARD,
			SPRITE_PLAY_ONESHOT,
			ExplosionComplete,
			NULL
		);
	
	info->radius                   = radius;
	info->falloff                  = falloff;
	info->damage                   = damage;
	info->impulse                  = impulse;
	info->sprite_info              = sprite_info;
	info->renderable               = (Renderable){
			.data        = sprite_info,
			.Render      = RenderBillboard,
			.transparent = true,
		};

	return info;
}


void 
Explosion_new(
	ExplosionInfo *info, 
	Vector3        position,
	Quaternion     orientation,
	Scene         *scene
)
{
	Entity *explosion = Entity_new(
			&Explosion_Template, 
			scene,
			sizeof(ExplosionData)
		);

	if (!explosion) {
		ERR_OUT("Failed to construct Explosion.");
		return;
	}

	ExplosionData *data = (ExplosionData*)&explosion->local_data;
	
	data->sprite_data      = (SpriteData){
			.start_frame   = 0,
			.current_frame = 0,
			.playing       = true,
		};

	info->renderable.Render = RenderBillboard;
	
	explosion->renderables[0]    = &info->renderable;
	explosion->visibility_radius = info->radius;
	explosion->user_data         = info;
	explosion->position          = position;
	explosion->visible           = true;
	explosion->active            = true;
	explosion->solid             = false;
	explosion->orientation       = orientation;

	float radius = info->radius;
	
	Entity_addToScene(explosion, scene);
	
	if (radius <= 0.0f) return;

	Entity **candidates = Scene_queryRegion(
			scene, 
			(BoundingBox){
					.min = (Vector3){
							.x = position.x - radius,
							.y = position.y - radius,
							.z = position.z - radius
						},
					.max = (Vector3){
							.x = position.x + radius,
							.y = position.y + radius,
							.z = position.z + radius
						}
				}
		);

	for (int i = 0; i < DynamicArray_length(candidates); i++) {
		Entity *other = candidates[i];
	
		Vector3 
			diff      = Vector3Subtract(other->position, position),
			direction = Vector3Normalize(diff),
			other_CM  = other->bounds_offset;
		float   
			length    = Vector3Length(diff),
			ease      = power_curve(info->falloff, info->radius, length),
			damage    = info->damage  * ease,
			impulse   = info->impulse * ease;

		if (info->radius < length) continue;
		
		other->velocity = Vector3Add(
				other->velocity, 
				Vector3Scale(
						direction, 
						impulse
					)
			);
	}

}

