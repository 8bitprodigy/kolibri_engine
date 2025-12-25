#include <stddef.h>
#include <stdlib.h>

#include "entity.h"
#include "explosion.h"
/* MUST COME LAST */
#include <raymath.h>


typedef struct
ExplosionInfo
{
	float
		radius,
		falloff,
		damage;
	SpriteInfo *sprite_info;
	Renderable  renderable;
}
ExplosionInfo;
/*
	Callback forward delcarations
*/
void explosionSetup(    Entity *self);
void explosionUpdate(   Entity *self, float           delta);
void explosionRender(   Entity *self, float           delta);
void explosionCollision(Entity *self, CollisionResult collision);


/*
	Template declarations
*/
Renderable
	explosion_Renderable = (Renderable){
			.data        = NULL,
			.Render      = RenderBillboard,
			.transparent = true,
		};

EntityVTable 
Explosion_Callbacks = (EntityVTable){
	.Setup       = explosionSetup,
	.Enter       = NULL,
	.Update      = NULL,
	.Render      = explosionRender,
	.OnCollision = NULL,
	.OnCollided  = NULL,
	.Exit        = NULL,
	.Free        = NULL,
};

Entity
Explosion_Template = (Entity){
	.renderables       = {&explosion_Renderable},
	.lod_distances     = {1024.0f},
	.lod_count         = 1,
	.visibility_radius = 0.25f,
	.bounds            = {0.1f, 0.1f, 0.1f},
	.bounds_offset     = {0.0f, 0.0f, 0.0f},
	.renderable_offset = {0.0f, 0.0f, 0.0f},
	.vtable            = &Explosion_Callbacks,
	.position          = V3_ZERO,
	.orientation       = V4_ZERO,
	.scale             = V3_ONE,
	.velocity          = V3_ZERO,
	.collision         = {.layers = 1, .masks = 1},
	.active            = true,
	.visible           = true,
	.collision_shape   = COLLISION_NONE, 
	.solid             = false
};


/*
	PRIVATE METHODS
*/

float 
ease(float t) 
{
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3) / 2.0;
}


/*
	CALLBACKS
*/

void 
explosionSetup(Entity *self)
{
	
}


void 
explosionRender(Entity *self, float delta)
{
	
	ExplosionData *data = (ExplosionData*)&self->local_data;
	
	SpriteInfo *sinfo = (SpriteInfo*)self->renderables[0]->data;
	SpriteData *sdata = &data->sprite_data;
	AnimateSprite(
			sinfo,
			sdata,
			Entity_getAge(self)
		);
}


void 
explosionCollision(Entity *self, CollisionResult collision)
{
	
}


/*
	PUBLIC METHODS
*/

void
ExplosionComplete(SpriteInfo *info, SpriteData *data) 
{
	ExplosionData *edata   = (ExplosionData*)((char*)data - offsetof(ExplosionData, sprite_data));
	Entity        *entity = (Entity*)((char*)edata - offsetof(Entity, local_data));
	Entity_free(entity);
}


ExplosionInfo *
ExplosionInfo_new(
	float     radius, 
	float     falloff, 
	float     damage, 
	float     scale,
	float     time_per_frame,
	Texture2D atlas,
	size_t    x_num_frames,
	size_t    y_num_frames
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
			atlas,
			x_num_frames,
			y_num_frames,
			SPRITE_ALIGN_CAMERA,
			SPRITE_DIR_FORWARD,
			SPRITE_PLAY_ONESHOT,
			ExplosionComplete,
			NULL
		);
	
	info->radius                   = radius;
	info->falloff                  = falloff;
	info->damage                   = damage;
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
	
	explosion->renderables[0] = &info->renderable;
	explosion->user_data      = info;
	explosion->position       = position;
	explosion->visible        = true;
	explosion->active         = true;
	explosion->solid          = false;
	explosion->orientation    = QuaternionIdentity();
}

