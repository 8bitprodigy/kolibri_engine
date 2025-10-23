#include "game.h"
#include <raylib.h>


/*
	Media forward declarations
*/
Model
	blast_model;
	
Texture2D
	blast_texture;

/*
	Callback forward delcarations
*/
void projectileSetup(    Entity *self);
void projectileUpdate(   Entity *self, float           delta);
void projectileCollision(Entity *self, CollisionResult collision);

/*
	Template declarations
*/
ProjectileInfo
	blast_Info = {
			.damage = 0.0f,
			.speed  = 10.0f,
		};

Renderable
	blast_Renderable = (Renderable){
			.data   = &blast_model,
			.Render = RenderModel,
		};

EntityVTable 
projectile_Callbacks = (EntityVTable){
	.Setup       = NULL,
	.Enter       = NULL,
	.Update      = projectileUpdate,
	.Render      = NULL,
	.OnCollision = projectileCollision,
	.OnCollided  = projectileCollision,
	.Exit        = NULL,
	.Free        = NULL
};

Entity
Blast_Template = (Entity){
	.renderables       = {&blast_Renderable},
	.lod_distances     = {1024.0f},
	.lod_count         = 1,
	.visibility_radius = 0.25f,
	.bounds            = {0.1f, 0.1f, 0.1f},
	.bounds_offset     = {0.0f, 0.0f, 0.0f},
	.renderable_offset = {0.0f, 0.0f, 0.0f},
	.user_data         = &blast_Info,
	.vtable            = &projectile_Callbacks,
	.position          = V3_ZERO,
	.orientation       = V4_ZERO,
	.scale             = V3_ONE,
	.velocity          = V3_ZERO,
	.collision         = {.layers = 1, .masks = 1},
	.active            = true,
	.visible           = true,
	.collision_shape   = COLLISION_BOX, 
	.solid             = false
};


void
Projectile_MediaInit(void)
{
	blast_model   = LoadModel(  "./resources/models/projectiles/projectile.obj");
	blast_texture = LoadTexture("./resources/models/projectiles/blast.png");
	SetMaterialTexture(&blast_model.materials[0], MATERIAL_MAP_ALBEDO, blast_texture);
	SetTextureFilter(blast_texture, TEXTURE_FILTER_BILINEAR);
}

void 
projectileSetup(    Entity *self)
{
	
}

void 
projectileUpdate(   Entity *self, float           delta)
{
	
}

void 
projectileCollision(Entity *self, CollisionResult collision)
{
	self->visible = false;
	self->active  = false;
	DBG_OUT("Projectile collided with Entity@%p", collision.entity);
}
