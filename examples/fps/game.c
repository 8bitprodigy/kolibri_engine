#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>

#include "game.h"
#define LOADING_SCREEN_IMPLEMENTATION
#include "../loading_screen.h"


Texture        explosion_Sprite;
ExplosionInfo *explosion_Info;
Texture        impact_Sprite;
ExplosionInfo *impact_Info;
Texture        blood_Sprite;
ExplosionInfo *blood_Info;

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
	animatedModel_Renderable = {
			.data        = NULL,
			.Render      = RenderAnimatedModel,
			.transparent = false,
		},
	*projectile_renderables;
		
Model           *projectile_models;
Texture         *projectile_Sprite_or_Textures;
SpriteInfo     **projectile_SpriteInfos;
ProjectileInfo **projectile_Infos;

WeaponInfo       weapon_Infos[WEAPON_NUM_WEAPONS];

AnimatedModel  *enemy_Models;
Texture        *enemy_Textures;
Renderable     *enemy_Renderables;
EnemyInfo      *enemy_Infos;



/*
	Projectile callbacks
*/
void
projectileCollision(
	Entity          *projectile, 
	CollisionResult  collision
)
{
	projectile->visible = false;
	projectile->active = false;
	
	if (!collision.hit) {
		Entity_free(projectile);
		return;
	}
	if (!collision.entity) {
		Explosion_new(
				impact_Info, 
				Vector3Add(
						collision.position,
						(Vector3){0.0f, 0.55f, 0.0f}
					),
				QuaternionFromAxisAngle(collision.normal, 0.0f),
				Entity_getScene(projectile)
			);
	}
	else {
		Explosion_new(
				blood_Info, 
				collision.position,
				QuaternionFromAxisAngle(collision.normal, 0.0f),
				Entity_getScene(projectile)
			);
	}
	
	Entity_free(projectile);
}

void
grenadeTimeout(Entity *projectile)
{
	Explosion_new(
			explosion_Info, 
			projectile->position,
			QuaternionIdentity(),
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
	projectile->visible = false;
	projectile->active = false;
	Explosion_new(
			explosion_Info, 
			projectile->position,
			QuaternionIdentity(),
			Entity_getScene(projectile)
		);
    Entity_free(projectile);
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

void
pelletCollision(
	Entity          *projectile, 
	CollisionResult  collision
)
{
    ProjectileData *data         = (ProjectileData*)&projectile->local_data;
    int            *bounces_left = (int*)data->data;
    
	if (*bounces_left == 0) goto NO_BOUNCE;
	if (0 < *bounces_left ) *bounces_left--;
	
    data->source = NULL;
    // Check if it's terrain (no entity) or another entity
    if (!collision.entity) {
        // Reflect velocity around normal
        projectile->velocity = Vector3Reflect(projectile->velocity, collision.normal);
        return;
    } 
    
NO_BOUNCE:
	if (data->source == collision.entity) return;
	projectile->visible = false;
	projectile->active = false;

	if (!collision.entity) {
		Explosion_new(
				impact_Info, 
				Vector3Add(
						collision.position,
						(Vector3){0.0f, 0.55f, 0.0f}
					),
				QuaternionFromAxisAngle(collision.normal, 0.0f),
				Entity_getScene(projectile)
			);
	}
	else {
		Explosion_new(
				blood_Info, 
				collision.position,
				QuaternionFromAxisAngle(collision.normal, 0.0f),
				Entity_getScene(projectile)
			);
	}
	
	Entity_free(projectile);
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
	Scene *scene = Entity_getScene(source);
	
	CollisionResult result = Scene_raycast(
			scene,
			position,
			Vector3Add(position, Vector3Scale(direction, info->distance)),
			source
		);

	if (result.hit) {
		if (result.entity)
			DBG_OUT("Hitscan fired at entity @%p", (void*)result.entity);
		else {
			Explosion_new(
					impact_Info, 
					result.position,
					QuaternionFromAxisAngle(result.normal, 0.0f),
					scene
				);
		}
	}
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
			Entity_getScene(source),
			0,
			NULL
		);
}

void
fireMinigun(
	WeaponInfo *info, 
	WeaponData *data, 
	Entity     *source, 
	Vector3     position, 
	Vector3     direction
)
{
	double time = Engine_getTime(Entity_getEngine(source));
	float  *spread = &data->data.f;
	
	const float 
		MIN_SPREAD    = 0.25f,
		MAX_SPREAD    = 2.5f,
		WARMUP_TIME   = 6.0f, 
		COOLDOWN_TIME = 16.0f,
		RANGE         = MAX_SPREAD - MIN_SPREAD,
		WARMUP_RATE   = (RANGE / 5.0F),
		COOLDOWN_RATE = (RANGE / 8.0F);
	
    if (!data->trigger_was_down) {
		float 
			elapsed = time - data->trigger_up,
			cooled  = (elapsed / COOLDOWN_TIME) * RANGE;
        *spread = CLAMP(*spread - cooled, MIN_SPREAD, MAX_SPREAD);
    } else {
        float 
			elapsed = time - data->trigger_down,
			delta   = (elapsed / WARMUP_TIME) * RANGE;
        *spread = fminf(*spread + delta, MAX_SPREAD);
    }
	
	Vector3 random_axis = Vector3Normalize(
			(Vector3){
				(float)rand() / RAND_MAX - 0.5f,
				(float)rand() / RAND_MAX - 0.5f,
				(float)rand() / RAND_MAX - 0.5f
			}
		);
	float spread_angle = ((float)rand() / RAND_MAX) * DEG2RAD * *spread;
	Vector3 pellet_direction = Vector3Normalize(
			Vector3RotateByAxisAngle(
				direction,
				random_axis,
				spread_angle
			)
		);
	Projectile_new(
			info->projectile, 
			position, 
			pellet_direction, 
			source, 
			NULL, 
			Entity_getScene(source),
			0,
			NULL
		);
}

void
fireShotgun(
	WeaponInfo *info, 
	WeaponData *data, 
	Entity     *source, 
	Vector3     position, 
	Vector3     direction
)
{
	const int  num_bounces = 1;
	int        max         = 8;
	double     time        = Engine_getTime(Entity_getEngine(source));
	Scene     *scene       = Entity_getScene(source);

	/*
		Mechanic: 
		If you don't fire for 5 seconds, one of your pellets will always
		fire straight ahead.
	*/
	if (5.0f < (time - data->next_shot)) { 
		Projectile_new(
				info->projectile, 
				position, 
				direction, 
				source, 
				NULL, 
				scene,
				sizeof(int),
				&num_bounces
			);
		max--;
	}
	
	for (int i = 0; i < max; i++) {
		Vector3 random_axis = Vector3Normalize(
				(Vector3){
					(float)rand() / RAND_MAX - 0.5f,
					(float)rand() / RAND_MAX - 0.5f,
					(float)rand() / RAND_MAX - 0.5f
				}
			);
		float spread_angle = ((float)rand() / RAND_MAX) * DEG2RAD * 4.0f;
		Vector3 pellet_direction = Vector3Normalize(
				Vector3RotateByAxisAngle(
					direction,
					random_axis,
					spread_angle
				)
			);

		Projectile_new(
				info->projectile,
				position,
				pellet_direction,
				source,
				NULL,
				scene,
				sizeof(int),
				&num_bounces
			);
	}
}

void
fireLightning(
	WeaponInfo *info, 
	WeaponData *data, 
	Entity     *source, 
	Vector3     position, 
	Vector3     direction
)
{
	
}


/*
	Resource Inits
*/
void
Explosion_mediaInit(void)
{
	LoadingScreen_draw(0, "resources/sprites/explosion.png");
	
	explosion_Sprite = LoadTexture( "resources/sprites/explosion.png");
	SetTextureFilter(explosion_Sprite, TEXTURE_FILTER_BILINEAR);
	
	explosion_Info = ExplosionInfo_new(
			5.0f,
			0.5f,
			10.0f,
			100.0f,
			4.0f,
			1.0f/15.0f,
			WHITE,
			explosion_Sprite,
			SPRITE_ALIGN_CAMERA,
			4, 4, 16
		);

	LoadingScreen_draw(2.5, "resources/sprites/impact.png");
	
	impact_Sprite = LoadTexture("resources/sprites/impact.png");
	SetTextureFilter(impact_Sprite, TEXTURE_FILTER_BILINEAR);
	
	impact_Info = ExplosionInfo_new(
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			2.0f,
			1.0f/30.0f,
			BEIGE,
			impact_Sprite,
			SPRITE_ALIGN_Y,
			4, 4, 16
		);

	LoadingScreen_draw(2.5, "resources/sprites/blood_impact.png");
	
	blood_Sprite = LoadTexture("resources/sprites/blood_impact.png");
	SetTextureFilter(blood_Sprite, TEXTURE_FILTER_BILINEAR);
	
	blood_Info = ExplosionInfo_new(
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			0.5f,
			1.0f/12.5f,
			WHITE,
			blood_Sprite,
			SPRITE_ALIGN_CAMERA,
			4, 1, 4
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
	LoadingScreen_draw(5, "resources/models/projectiles/projectile.obj");
	projectile_models[PROJECTILE_BLAST] = LoadModel(
			"resources/models/projectiles/projectile.obj"
		);
	
	LoadingScreen_draw(10,"resources/models/projectiles/projectile.png");
	projectile_Sprite_or_Textures[PROJECTILE_BLAST] = LoadTexture(
			"resources/models/projectiles/projectile.png"
		);
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
			200.0f,
			5.0f,
			PROJECTILE_MOTION_STRAIGHT,
			10.0f,
			&projectile_renderables[PROJECTILE_BLAST],
			projectileCollision,
			NULL
		);

	/* Pellet */
	projectile_Infos[PROJECTILE_PELLET]       = ProjectileInfo_new(
			5.0f,
			200.0f,
			5.0f,
			PROJECTILE_MOTION_STRAIGHT,
			10.0f,
			&projectile_renderables[PROJECTILE_BLAST],
			pelletCollision,
			NULL
		);
	
	/* Goo */
	LoadingScreen_draw(15, "resources/sprites/glob.png");
	projectile_Sprite_or_Textures[PROJECTILE_GOO] = LoadTexture(
			"resources/sprites/glob.png"
		);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_GOO], 
			TEXTURE_FILTER_BILINEAR
		);
	projectile_SpriteInfos[PROJECTILE_GOO] = SpriteInfo_newRegular(
			0.5f,
			1.0f/24.0f,
			WHITE,
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
	LoadingScreen_draw(20, "resources/models/projectiles/rocket.obj");
	projectile_models[PROJECTILE_ROCKET] = LoadModel(
			"resources/models/projectiles/rocket.obj"
		);
		
	LoadingScreen_draw(25, "resources/models/projectiles/rocket.png");
	projectile_Sprite_or_Textures[PROJECTILE_ROCKET] = LoadTexture(
			"resources/models/projectiles/rocket.png"
		);
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
			50.0f,
			5.0f,
			PROJECTILE_MOTION_STRAIGHT,
			10.0f,
			&projectile_renderables[PROJECTILE_ROCKET],
			rocketCollision,
			NULL
		);
	
	/* Grenade */
	LoadingScreen_draw(30, "resources/sprites/grenade.png");
	projectile_Sprite_or_Textures[PROJECTILE_GRENADE] = LoadTexture(
			"resources/sprites/grenade.png"
		);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_GRENADE], 
			TEXTURE_FILTER_BILINEAR
		);
	projectile_SpriteInfos[PROJECTILE_GRENADE] = SpriteInfo_newRegular(
			0.5f,
			1.0f/48.0f,
			WHITE,
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
	
	LoadingScreen_draw(35, "resources/sprites/plasma_ball.png");
	projectile_Sprite_or_Textures[PROJECTILE_PLASMA] = LoadTexture(
			"resources/sprites/plasma_ball.png"
		);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_PLASMA], 
			TEXTURE_FILTER_BILINEAR
		);
	projectile_SpriteInfos[PROJECTILE_PLASMA] = SpriteInfo_newRegular(
			0.5f,
			1.0f/24.0f,
			WHITE,
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
	
	/* Tracer */
	
	LoadingScreen_draw(35, "resources/sprites/tracer.png");
	projectile_Sprite_or_Textures[PROJECTILE_TRACER] = LoadTexture(
			"resources/sprites/tracer.png"
		);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_TRACER], 
			TEXTURE_FILTER_BILINEAR
		);
	projectile_SpriteInfos[PROJECTILE_TRACER] = SpriteInfo_newRegular(
			0.5f,
			1.0f/60.0f,
			WHITE,
			projectile_Sprite_or_Textures[PROJECTILE_TRACER],
			4, 2,
			7,
			SPRITE_ALIGN_CAMERA,
			SPRITE_DIR_PINGPONG,
			SPRITE_PLAY_LOOP,
			NULL,
			NULL
		);
	projectile_renderables[PROJECTILE_TRACER] = *SpriteInfo_getRenderable(
			projectile_SpriteInfos[PROJECTILE_TRACER]
		);
	projectile_Infos[PROJECTILE_TRACER] = ProjectileInfo_new(
			5.0f,
			200.0f,
			5.0f,
			PROJECTILE_MOTION_STRAIGHT,
			10.0f,
			&projectile_renderables[PROJECTILE_TRACER],
			projectileCollision,
			NULL
		);

	/* Green */
	
	LoadingScreen_draw(5, "resources/models/projectiles/blast.obj");
	projectile_models[PROJECTILE_GREEN] = LoadModel(
			"resources/models/projectiles/blast.obj"
		);
		
	LoadingScreen_draw(10, "resources/models/projectiles/blast.png");
	projectile_Sprite_or_Textures[PROJECTILE_GREEN] = LoadTexture(
			"resources/models/projectiles/blast.png"
		);
	SetTextureFilter(
			projectile_Sprite_or_Textures[PROJECTILE_GREEN], 
			TEXTURE_FILTER_BILINEAR
		);
	SetMaterialTexture(
			&projectile_models[PROJECTILE_GREEN].materials[0], 
			MATERIAL_MAP_ALBEDO, 
			projectile_Sprite_or_Textures[PROJECTILE_GREEN]
		);
	projectile_renderables[PROJECTILE_GREEN]      = model_Renderable;
	projectile_renderables[PROJECTILE_GREEN].data = &projectile_models[PROJECTILE_GREEN]; 
	projectile_Infos[PROJECTILE_GREEN]       = ProjectileInfo_new(
			5.0f,
			200.0f,
			5.0f,
			PROJECTILE_MOTION_STRAIGHT,
			10.0f,
			&projectile_renderables[PROJECTILE_GREEN],
			projectileCollision,
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
		.projectile        = projectile_Infos[PROJECTILE_TRACER],
		.refractory_period = 0.125f,
		.Fire              = fireMinigun,
		.action_type       = ACTION_AUTOMATIC,
	};
	weapon_Infos[WEAPON_SHOTGUN] = (WeaponInfo){
		.projectile        = projectile_Infos[PROJECTILE_PELLET],
		.refractory_period = 1.0f,
		.Fire              = fireShotgun,
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
	int increment = 50 / WEAPON_NUM_WEAPONS;
	for (int i = 0; i < WEAPON_NUM_WEAPONS; i++) {
		snprintf(
				weapon_path, 
				sizeof(weapon_path), 
				"%s%d%s", 
				"resources/models/weapons/weapon", 
				i+1, 
				".obj"
			);
		LoadingScreen_draw(50 + (i * increment), (const char *)&weapon_path);
		DBG_OUT("Weapon Path: %s", weapon_path);
		weapon_Infos[i].model = LoadModel(weapon_path);
		
		snprintf(
				weapon_texture_path, 
				sizeof(weapon_texture_path), 
				"%s%d%s", 
				"resources/models/weapons/weapon", 
				i+1, 
				".png"
			);
		LoadingScreen_draw(50 + (i * increment), (const char *)&weapon_texture_path);
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
Enemy_mediaInit()
{
	enemy_Models      = DynamicArray(AnimatedModel, ENEMY_NUM_ENEMIES);
	enemy_Textures    = DynamicArray(Texture,       ENEMY_NUM_ENEMIES);
	enemy_Renderables = DynamicArray(Renderable,    ENEMY_NUM_ENEMIES);
	enemy_Infos       = DynamicArray(EnemyInfo,     ENEMY_NUM_ENEMIES);

	enemy_Models[ENEMY_GRUNT].model      = LoadModel(
			"resources/models/grunt/model.m3d"
		);
	enemy_Models[ENEMY_GRUNT].animations = LoadModelAnimations(
			"resources/models/grunt/model.m3d", 
			&enemy_Models[ENEMY_GRUNT].anim_count
		);
	DBG_OUT(
			"Loaded %d animations for grunt", 
			enemy_Models[ENEMY_GRUNT].anim_count
		);
	
	enemy_Textures[ENEMY_GRUNT] = LoadTexture(
			"resources/models/grunt/texture.png"
		);
	SetMaterialTexture(
		&enemy_Models[ENEMY_GRUNT].model.materials[0],
		MATERIAL_MAP_ALBEDO,
		enemy_Textures[ENEMY_GRUNT]
	);
	SetTextureFilter(enemy_Textures[ENEMY_GRUNT], TEXTURE_FILTER_BILINEAR);
	
	enemy_Renderables[ENEMY_GRUNT]      = animatedModel_Renderable;
	enemy_Renderables[ENEMY_GRUNT].data = &enemy_Models[ENEMY_GRUNT];

	enemy_Infos[ENEMY_GRUNT] = (EnemyInfo){
			.renderables      = {enemy_Renderables[ENEMY_GRUNT]},
			.lod_distances    = {128.0f},
			.num_renderables  = 1,
			.projectile_info  = projectile_Infos[PROJECTILE_GREEN],
			.health           = 100.0f, /* health */
			.speed            = 5.0f,   /* speed */
			.turn_speed       = 5.0f,
			.melee_damage     = 15.0f,  /* melee_damage */
			.melee_range      = 2.0f,   /* melee_range */
			.projectile_range = 96.0f,  /* projectile_range */
			.sight_range      = 128.0f, /* sight_range */
		};
}

void
Game_mediaInit()
{
	LoadingScreen_draw(0, NULL);
		Explosion_mediaInit();
	LoadingScreen_draw(5, NULL);
		Projectile_mediaInit();
		Enemy_mediaInit();
	LoadingScreen_draw(50, NULL);
		Weapon_init();
	LoadingScreen_draw(100, NULL);
}
