#include "game.h"
#include "sprite.h"

#include <raylib.h>
#include <raymath.h>


/*
	Media forward declarations
*/
Model
	blast_model;
	
Texture2D
	blast_sprite,
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
			.damage  =  0.0f,
			.speed   = 25.0f,
			.timeout =  3.5f,
		};

SpriteInfo blast_sprite_info;/* = {
		.scale            = 1.0f,
		.time_per_frame   = 1.0f/12.0f,
		.num_frames       = 16,
		.atlas           = blast_sprite,
		.sprite_alignment = SPRITE_ALIGN_CAMERA,
	};*/

Renderable
	blast_Renderable = (Renderable){
			.data        = &blast_sprite_info,
			.Render      = RenderBillboard,
			.transparent = true,
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
	.Free        = NULL,
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
	.collision_shape   = COLLISION_NONE, 
	.solid             = false
};


void
Projectile_MediaInit(void)
{
	blast_model   = LoadModel(  PATH_PREFIX "resources/models/projectiles/blast.obj");
	blast_texture = LoadTexture(PATH_PREFIX "resources/models/projectiles/blast.png");
	blast_sprite  = LoadTexture(PATH_PREFIX "resources/sprites/green_blast.png");
	
	SetTextureFilter(blast_sprite, TEXTURE_FILTER_BILINEAR);
	SetMaterialTexture(&blast_model.materials[0], MATERIAL_MAP_ALBEDO, blast_texture);
	SetTextureFilter(blast_texture, TEXTURE_FILTER_BILINEAR);

	blast_sprite_info = CreateRegularSprite(
			1.0f,
			1.0f/12.0f,
			blast_sprite,
			4, 4,
			SPRITE_ALIGN_CAMERA
		);
	
	DBG_OUT("Projectile_MediaInit() ran.");
}

void
Projectile_new(Vector3 position, Vector3 direction, Entity *template, Scene *scene)
{
	Entity *projectile = Entity_new(
			template, 
			scene,
			sizeof(ProjectileData)
		);

	ProjectileData *data = (ProjectileData*)&projectile->local_data;

	size_t start_frame = rand() % 16;
	data->sprite_data       = (SpriteData){
			.start_frame   = start_frame,
			.current_frame = start_frame,
		};
	data->prev_offset       = V3_ZERO;
	data->elapsed_time      = 0.0f;
	
	projectile->position    = position;
	projectile->visible     = true;
	projectile->active      = true;
	projectile->orientation = QuaternionFromVector3ToVector3(
			V3_FORWARD,
			Vector3Normalize(direction)
		);
	projectile->velocity              = Vector3Scale(
			direction,
			((ProjectileInfo*)template->user_data)->speed
		);
}

void 
projectileSetup(    Entity *self)
{

}

void 
projectileUpdate(   Entity *self, float delta)
{
	ProjectileInfo *info = self->user_data;
	ProjectileData *data = (ProjectileData*)&self->local_data;
	
	if (info && data && info->timeout <= data->elapsed_time) {
		self->visible = false;
		self->active  = false;
		Entity_free(self);
		return;
	}
	data->elapsed_time += delta;
	
	Vector3 old_pos = self->position;
	Entity_move(self, Vector3Scale(self->velocity, delta));

	self->renderable_offset = Vector3Subtract(old_pos, self->position);
	data->prev_offset       = self->renderable_offset;
	
	CollisionResult collision = Scene_raycast(
			Entity_getScene(self), 
			old_pos, 
			self->position
		);

	if (collision.hit) {
		self->visible = false;
		self->active = false;
		Entity_free(self);
	}
}

void
projectileRender(Entity *self, float delta)
{
	ProjectileData *data = (ProjectileData*)&self->local_data;
	self->renderable_offset = Vector3Lerp(
		data->prev_offset,
		V3_ZERO,
		Engine_getTickElapsed(Entity_getEngine(self))
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
	sdata->current_frame = AnimateSprite(
			sinfo,
			sdata,
			Entity_getAge(self),
			16
		);
}

void 
projectileCollision(Entity *self, CollisionResult collision)
{
	self->visible = false;
	self->active  = false;
}

void
projectileFree(Entity *self)
{
}
