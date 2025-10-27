#include "game.h"
#include <raylib.h>
#include <raymath.h>


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
void projectileRender(   Entity *self, float           delta);
void projectileCollision(Entity *self, CollisionResult collision);

/*
	Template declarations
*/
ProjectileInfo
	blast_Info = {
			.damage = 0.0f,
			.speed  = 10.0f,
			.timeout = 3.0f,
		};

Renderable
	blast_Renderable = (Renderable){
			.data   = &blast_model,
			.Render = RenderModel,
		};

EntityVTable 
projectile_Callbacks = (EntityVTable){
	.Setup       = projectileSetup,
	.Enter       = NULL,
	.Update      = projectileUpdate,
	.Render      = projectileRender,
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
	DBG_OUT("Projectile_MediaInit() ran.");
}

void
Projectile_new(Vector3 position, Vector3 direction, Entity *template, Scene *scene)
{
	Entity *projectile = Entity_new(
			template, 
			scene,
			sizeof(Vector3)
		);
		
	*(Vector3*)projectile->local_data = V3_ZERO;
	projectile->position              = position;
	projectile->visible               = true;
	projectile->active                = true;
	projectile->orientation           = QuaternionFromVector3ToVector3(
			V3_FORWARD,
			Vector3Normalize(direction)
		);
	projectile->velocity              = Vector3Scale(
			direction,
			((ProjectileInfo*)template->user_data)->speed
		);
	
	DBG_OUT("Projectile@%p created at ( %f, %f, %f ).", projectile, position.x, position.y, position.z);
}

void 
projectileSetup(    Entity *self)
{
}

void 
projectileUpdate(   Entity *self, float delta)
{
	ProjectileInfo *data = self->user_data;
	DBG_OUT("Projectile user data@%p", data);
	if (data && data->timeout <= Entity_getAge(self)) {
		self->visible = false;
		self->active  = false;
		
		return;
	}
	Vector3 old_pos = self->position;
	Entity_moveAndSlide(self, Vector3Scale(self->velocity, delta), 5);

	self->renderable_offset     = Vector3Subtract(old_pos, self->position);
	*(Vector3*)self->local_data = self->renderable_offset;
}

void
projectileRender(Entity *self, float delta)
{
	self->renderable_offset = Vector3Lerp(
		*(Vector3*)self->local_data,
		V3_ZERO,
		Engine_getTickElapsed(Entity_getEngine(self))
	);
	DBG_OUT("Projectile@%p renderable offset moved to ( %f, %f, %f ).", self, self->renderable_offset.x, self->renderable_offset.y, self->renderable_offset.z);
}
void 
projectileCollision(Entity *self, CollisionResult collision)
{
	self->visible = false;
	self->active  = false;
}
