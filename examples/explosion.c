#include "explosion.h"
#include "entity.h"
#include <raymath.h>


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
	.Setup       = NULL,
	.Enter       = NULL,
	.Update      = NULL,
	.Render      = NULL,
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
explosionUpdate(Entity *self, float delta)
{
	SpriteData *data = (SpriteData*)&self->local_data;

	if (!data->playing) Entity_free(self);
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

	ExplosionData *data = (ExplosionData*)&explosion->local_data;

	size_t start_frame = 0;
	data->sprite_data      = (SpriteData){
			.start_frame   = start_frame,
			.current_frame = start_frame,
		};
	data->elapsed_time      = 0.0f;
	
	explosion->position    = position;
	explosion->visible     = true;
	explosion->active      = true;
	explosion->orientation = QuaternionIdentity();
}
