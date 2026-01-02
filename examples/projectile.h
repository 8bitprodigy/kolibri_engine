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


ProjectileInfo *ProjectileInfo_new(
	float             damage,
	float             speed,
	float             timeout,
	ProjectileMotion  motion,
	float             gravity_or_homing_strength,
	Renderable       *renderable
);
void ProjectileInfo_free(ProjectileInfo *info);

Entity *Projectile_new(
       ProjectileInfo *info,
       Vector3 position,
       Vector3 direction,
       Entity *source,
       Entity *target,
       Scene *scene
);

#endif /* PROJECTILE_H */
