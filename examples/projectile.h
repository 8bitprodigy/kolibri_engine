#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "common.h"
#include "sprite.h"


typedef enum {
	PROJECTILE_MOTION_STRAIGHT,  /* Straight linear path */
	PROJECTILE_MOTION_BALLISTIC, /* Affected by gravity */
	PROJECTILE_MOTION_HOMING,    /* Tracks a target */
}
ProjectileMotion;

typedef struct ProjectileInfo ProjectileInfo;
typedef struct
{
	SpriteData    sprite_data;
	Entity
				 *source,
				 *target;
	Vector3
			 	 prev_offset;
	float         elapsed_time;
	unsigned char data[];
}
ProjectileData;

typedef void (*ProjectileCollision)(Entity *projectile, CollisionResult collision);
typedef void (*ProjectileTimeout)(  Entity *projectile);


ProjectileInfo *ProjectileInfo_new(
	float                damage,
	float                speed,
	float                timeout,
	ProjectileMotion     motion,
	float                gravity_or_homing_strength,
	Renderable          *renderable,
	ProjectileCollision  Collision_Callback,
	ProjectileTimeout    Timeout_Callaback
);
void ProjectileInfo_free(ProjectileInfo *info);

Entity *Projectile_new(
       ProjectileInfo *info,
       Vector3         position,
       Vector3         direction,
       Entity         *source,
       Entity         *target,
       Scene          *scene,
       size_t          data_size,
       void           *data
);

#endif /* PROJECTILE_H */
