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

typedef struct ExplosionInfo ExplosionInfo;

typedef struct
{
	SpriteData sprite_data;
}
ExplosionData;

ExplosionInfo *ExplosionInfo_new(
	float     radius, 
	float     falloff, 
	float     damage, 
	float     scale,
	float     time_per_frame,
	Texture2D atlas,
	size_t    x_num_frames,
	size_t    y_num_frames
);

void Explosion_new(    ExplosionInfo *info, Vector3     position, Scene *scene);
void ExplosionComplete(SpriteInfo    *info, SpriteData *data);

#endif /* EXPLOSION_H */
