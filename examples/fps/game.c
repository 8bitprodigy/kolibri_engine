#include <raylib.h>

#include "game.h"


Texture        explosion_Sprite;
ExplosionInfo *explosion_Info;

Renderable
	sprite_Renderable = {
			.data        = NULL,
			.Render      = RenderBillboard,
			.transparent = true,
		},
	model_Renderable = {
			.data        = NULL,
			.Render      = RenderModel,
			.transparent = false,
		},
	*projectile_renderables;
		
Model           *projectile_models;
Texture         *projectile_Sprite_or_Textures;
SpriteInfo     **projectile_SpriteInfos;
ProjectileInfo **projectile_Infos;

WeaponInfo       weapon_Infos[WEAPON_NUM_WEAPONS];


/*
	Projectile callbacks
*/
void
grenadeTimeout(Entity *projectile)
{
	Explosion_new(
			explosion_Info, 
			projectile->position,
			Entity_getScene(projectile)
		);
}

void
rocketCollision(
	Entity          *projectile, 
	CollisionResult  collision
)
{
	(void)collision;
	//if (!collision.hit) return;
	Explosion_new(
			explosion_Info, 
			projectile->position,
			Entity_getScene(projectile)
		);
}

void
grenadeCollision(
	Entity          *projectile, 
	CollisionResult  collision
)
{
    ProjectileData *data = (ProjectileData*)&projectile->local_data;
    
    // Check if it's terrain (no entity) or another entity
    if (!collision.entity) {
        // Bounce off terrain
        float restitution = 0.5f; // Energy retained (70%)
        static const float min_velocity = 2.0f; // Stop bouncing below this speed
        
        // Reflect velocity around normal
        Vector3 reflected = Vector3Reflect(projectile->velocity, collision.normal);
        projectile->velocity = Vector3Scale(reflected, restitution);
        
        float speed = Vector3Length(projectile->velocity);
        if (speed > min_velocity) {
            // Move slightly away from surface to prevent getting stuck
            projectile->position = Vector3Add(
                collision.position,
                Vector3Scale(collision.normal, 0.15f)
            );
        }
    } else {
    	if (data->source == collision.entity) return;
        projectile->visible = false;
        projectile->active = false;
        grenadeTimeout(projectile);
        Entity_free(projectile);
    }
}


/*
	Weapon Callbacks
*/	

void
fireHitscan(
	WeaponInfo *info, 
	WeaponData *data, 
	Entity     *source, 
	Vector3     position, 
	Vector3     direction
)
{
	CollisionResult result = Scene_raycast(
			Entity_getScene(source),
			position,
			Vector3Add(position, Vector3Scale(direction, info->distance))
		);

	if (result.hit && result.entity)
		DBG_OUT("Stabbed entity @%p", (void*)result.entity);
}

void
fireProjectile(
	WeaponInfo *info, 
	WeaponData *data, 
	Entity     *source, 
	Vector3     position, 
	Vector3     direction
)
{
	Projectile_new(
			info->projectile, 
			position, 
			direction, 
			source, 
			NULL, 
			Entity_getScene(source)
		);
}


/*
	Resource Inits
*/
void
Explosion_mediaInit(void)
{
	char explosion_path[256];

	snprintf(
			explosion_path, 
			sizeof(explosion_path), 
			"%s%s", 
			path_prefix,  
			"resources/sprites/explosion.png"
		);

	explosion_Sprite = LoadTexture(explosion_path);
	SetTextureFilter(explosion_Sprite, TEXTURE_FILTER_BILINEAR);
	
	explosion_Info = ExplosionInfo_new(
			2.5f,
			0.5f,
			10.0f,
			2.0f,
			1.0f/15.0f,
			explosion_Sprite,
			4, 4
		);
}

void
Projectile_mediaInit(void)
{
	projectile_renderables        = DynamicArray(Renderable,      PROJECTILE_NUM_PROJECTILES);
	projectile_models             = DynamicArray(Model,           PROJECTILE_NUM_PROJECTILES);
	projectile_Sprite_or_Textures = DynamicArray(Texture,         PROJECTILE_NUM_PROJECTILES);
	projectile_SpriteInfos        = DynamicArray(SpriteInfo*,     PROJECTILE_NUM_PROJECTILES);
	projectile_Infos              = DynamicArray(ProjectileInfo*, PROJECTILE_NUM_PROJECTILES);
	
	char load_path[256];

	/* Blast */
	snprintf(
			load_path, 
			sizeof(load_path), 
			"%s%s", 
			path_prefix, 
			"resources/models/projectiles/projectile.obj"
		);
	projectile_models[PROJECTILE_BLAST] = LoadModel(load_path);
	snprintf(
			load_path, 
			sizeof(load_path), 
			"%s%s",
			path_prefix, 
			"resources/models/projectiles/projectile.png"
		);
	projectile_Sprite_or_Textures[PROJECTILE_BLAST] = LoadTexture(load_path);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_BLAST], 
			TEXTURE_FILTER_BILINEAR
		);
	SetMaterialTexture(
			&projectile_models[PROJECTILE_BLAST].materials[0], 
			MATERIAL_MAP_ALBEDO, 
			projectile_Sprite_or_Textures[PROJECTILE_BLAST]
		);
	projectile_renderables[PROJECTILE_BLAST]      = model_Renderable;
	projectile_renderables[PROJECTILE_BLAST].data = &projectile_models[PROJECTILE_BLAST]; 
	projectile_Infos[PROJECTILE_BLAST]       = ProjectileInfo_new(
			5.0f,
			25.0f,
			5.0f,
			PROJECTILE_MOTION_STRAIGHT,
			10.0f,
			&projectile_renderables[PROJECTILE_BLAST],
			NULL,
			NULL
		);
	
	/* Goo */
	snprintf(
			load_path, 
			sizeof(load_path), 
			"%s%s", 
			path_prefix,  
			"resources/sprites/glob.png"
		);
	projectile_Sprite_or_Textures[PROJECTILE_GOO] = LoadTexture(load_path);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_GOO], 
			TEXTURE_FILTER_BILINEAR
		);
	projectile_SpriteInfos[PROJECTILE_GOO] = SpriteInfo_newRegular(
			0.5f,
			1.0f/24.0f,
			projectile_Sprite_or_Textures[PROJECTILE_GOO],
			4, 4,
			16,
			SPRITE_ALIGN_CAMERA,
			SPRITE_DIR_PINGPONG,
			SPRITE_PLAY_LOOP,
			NULL,
			NULL
		);
	projectile_renderables[PROJECTILE_GOO] = *SpriteInfo_getRenderable(
			projectile_SpriteInfos[PROJECTILE_GOO]
		);
	projectile_Infos[PROJECTILE_GOO] = ProjectileInfo_new(
			5.0f,
			25.0f,
			5.0f,
			PROJECTILE_MOTION_BALLISTIC,
			10.0f,
			SpriteInfo_getRenderable(projectile_SpriteInfos[PROJECTILE_GOO]),
			NULL,
			NULL
		);
	
	/* Rocket */
	snprintf(
			load_path, 
			sizeof(load_path), 
			"%s%s", 
			path_prefix, 
			"resources/models/projectiles/rocket.obj"
		);
	projectile_models[PROJECTILE_ROCKET] = LoadModel(load_path);
	snprintf(
			load_path, 
			sizeof(load_path), 
			"%s%s",
			path_prefix, 
			"resources/models/projectiles/rocket.png"
		);
	projectile_Sprite_or_Textures[PROJECTILE_ROCKET] = LoadTexture(load_path);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_ROCKET], 
			TEXTURE_FILTER_BILINEAR
		);
	SetMaterialTexture(
			&projectile_models[PROJECTILE_ROCKET].materials[0], 
			MATERIAL_MAP_ALBEDO, 
			projectile_Sprite_or_Textures[PROJECTILE_ROCKET]
		);
	projectile_renderables[PROJECTILE_ROCKET] = model_Renderable;
	projectile_renderables[PROJECTILE_ROCKET].data = &projectile_models[PROJECTILE_ROCKET]; 
	projectile_Infos[PROJECTILE_ROCKET] = ProjectileInfo_new(
			5.0f,
			25.0f,
			5.0f,
			PROJECTILE_MOTION_STRAIGHT,
			10.0f,
			&projectile_renderables[PROJECTILE_ROCKET],
			rocketCollision,
			NULL
		);
	
	/* Grenade */
	snprintf(
			load_path, 
			sizeof(load_path), 
			"%s%s",
			path_prefix, 
			"resources/sprites/grenade.png"
		);
	projectile_Sprite_or_Textures[PROJECTILE_GRENADE] = LoadTexture(load_path);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_GRENADE], 
			TEXTURE_FILTER_BILINEAR
		);
	projectile_SpriteInfos[PROJECTILE_GRENADE] = SpriteInfo_newRegular(
			0.5f,
			1.0f/48.0f,
			projectile_Sprite_or_Textures[PROJECTILE_GRENADE],
			4, 2,
			8,
			SPRITE_ALIGN_CAMERA,
			SPRITE_DIR_FORWARD,
			SPRITE_PLAY_LOOP,
			NULL,
			NULL
		);
	projectile_renderables[PROJECTILE_GRENADE] = *SpriteInfo_getRenderable(
			projectile_SpriteInfos[PROJECTILE_GRENADE]
		);
	projectile_Infos[PROJECTILE_GRENADE] = ProjectileInfo_new(
			5.0f,
			35.0f,
			5.0f,
			PROJECTILE_MOTION_BALLISTIC,
			25.0f,
			&projectile_renderables[PROJECTILE_GRENADE],
			grenadeCollision,
			grenadeTimeout
		);
	
	/* Plasma */
	snprintf(
			load_path, 
			sizeof(load_path), 
			"%s%s",
			path_prefix, 
			"resources/sprites/plasma_ball.png"
		);
	projectile_Sprite_or_Textures[PROJECTILE_PLASMA] = LoadTexture(load_path);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_PLASMA], 
			TEXTURE_FILTER_BILINEAR
		);
	projectile_SpriteInfos[PROJECTILE_PLASMA] = SpriteInfo_newRegular(
			0.5f,
			1.0f/24.0f,
			projectile_Sprite_or_Textures[PROJECTILE_PLASMA],
			4, 4,
			16,
			SPRITE_ALIGN_CAMERA,
			SPRITE_DIR_RANDOM,
			SPRITE_PLAY_LOOP,
			NULL,
			NULL
		);
	projectile_renderables[PROJECTILE_PLASMA] = *SpriteInfo_getRenderable(
			projectile_SpriteInfos[PROJECTILE_PLASMA]
		);
	projectile_Infos[PROJECTILE_PLASMA] = ProjectileInfo_new(
			5.0f,
			15.0f,
			5.0f,
			PROJECTILE_MOTION_STRAIGHT,
			10.0f,
			&projectile_renderables[PROJECTILE_PLASMA],
			NULL,
			NULL
		);
}

void
Weapon_init(void)
{
	char 
		weapon_path[256],
		weapon_texture_path[256];
	
	/*
		Construct Weapons
	*/
	weapon_Infos[WEAPON_MELEE] = (WeaponInfo){
		.projectile        = NULL,
		.refractory_period = 1.0f,
		.Fire              = fireHitscan,
		.action_type       = ACTION_AUTOMATIC,
	};
	weapon_Infos[WEAPON_BLASTER] = (WeaponInfo){
		.projectile        = projectile_Infos[PROJECTILE_BLAST],
		.refractory_period = 0.35f,
		.Fire              = fireProjectile,
		.action_type       = ACTION_SEMIAUTO,
	};
	weapon_Infos[WEAPON_MINIGUN] = (WeaponInfo){
		.projectile        = projectile_Infos[PROJECTILE_BLAST],
		.refractory_period = 0.125f,
		.Fire              = fireProjectile,
		.action_type       = ACTION_AUTOMATIC,
	};
	weapon_Infos[WEAPON_SHOTGUN] = (WeaponInfo){
		.projectile        = projectile_Infos[PROJECTILE_BLAST],
		.refractory_period = 1.0f,
		.Fire              = fireProjectile,
		.action_type       = ACTION_MANUAL,
	};
	weapon_Infos[WEAPON_GOOGUN] = (WeaponInfo){
		.projectile        = projectile_Infos[PROJECTILE_GOO],
		.refractory_period = 0.3f,
		.Fire              = fireProjectile,
		.action_type       = ACTION_SEMIAUTO,
	};
	weapon_Infos[WEAPON_ROCKET_LAUNCHER] = (WeaponInfo){
		.projectile        = projectile_Infos[PROJECTILE_ROCKET],
		.refractory_period = 1.0f,
		.Fire              = fireProjectile,
		.action_type       = ACTION_MANUAL,
	};
	weapon_Infos[WEAPON_GRENADE_LAUNCHER] = (WeaponInfo){
		.projectile        = projectile_Infos[PROJECTILE_GRENADE],
		.refractory_period = 0.5f,
		.Fire              = fireProjectile,
		.action_type       = ACTION_SEMIAUTO,
	};
	weapon_Infos[WEAPON_RAILGUN] = (WeaponInfo){
		.projectile        = NULL,
		.refractory_period = 1.5f,
		.Fire              = fireHitscan,
		.action_type       = ACTION_MANUAL,
	};
	weapon_Infos[WEAPON_LIGHTNING_GUN] = (WeaponInfo){
		.projectile        = NULL,
		.refractory_period = 0.0f,
		.Fire              = NULL,
		.action_type       = ACTION_AUTOMATIC,
	};
	/*
		Load Models
	*/
	for (int i = 0; i < WEAPON_NUM_WEAPONS; i++) {
		snprintf(
				weapon_path, 
				sizeof(weapon_path), 
				"%s%s%d%s", 
				path_prefix, 
				"resources/models/weapons/weapon", 
				i+1, 
				".obj"
			);
		DBG_OUT("Weapon Path: %s", weapon_path);
		weapon_Infos[i].model = LoadModel(weapon_path);
		
		snprintf(
				weapon_texture_path, 
				sizeof(weapon_texture_path), 
				"%s%s%d%s", 
				path_prefix, 
				"resources/models/weapons/weapon", 
				i+1, 
				".png"
			);
		DBG_OUT("Weapon Texture Path: %s", weapon_texture_path);
		Texture2D weaponTexture = LoadTexture(weapon_texture_path);
		SetMaterialTexture(
				&weapon_Infos[i].model.materials[0], 
				MATERIAL_MAP_ALBEDO, 
				weaponTexture
			);  
		SetTextureFilter(weaponTexture, TEXTURE_FILTER_BILINEAR);
	}
}


void
Game_mediaInit()
{
	Explosion_mediaInit();
	Projectile_mediaInit();
	Weapon_init();
}
