#ifndef EXPLOSION_H
#define EXPLOSION_H


#include "common.h"
#include "sprite.h"


#ifndef EXPLOSION_DEFAULT_DAMAGE
	#define EXPLOSION_DEFAULT_DAMAGE 50.0f
#endif
#ifndef EXPLOSION_DEFAULT_RANGE
	#define EXPLOSION_DEFAULT_RANGE 5.0f
#endif
#ifndef EXPLOSION_DEFAULT_FALLOFF
	#define EXPLOSION_DEFAULT_FALLOFF 0.1f
#endif


typedef struct
{
	float
		radius,
		falloff,
		damage;
	SpriteInfo sprite_info;
}
ExplosionInfo;

typedef struct
{
	SpriteData sprite_data;
	float      elapsed_time;
}
ExplosionData;


void Explosion_new(ExplosionInfo *info, Vector3 position, Scene *scene);


#endif /* EXPLOSION_H */
