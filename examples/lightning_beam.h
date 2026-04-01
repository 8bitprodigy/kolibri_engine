#ifndef LIGHTNING_BEAM_H
#define LIGHTNING_BEAM_H

#include "common.h"
#include "ribbon.h"


typedef struct LightningBeamInfo LightningBeamInfo;


LightningBeamInfo *LightningBeamInfo_new(
	float      width,
	Color      color,
	Texture2D  atlas,
	size_t     num_frames
);

Entity *LightningBeam_new(
	LightningBeamInfo *info,
	Vector3            muzzle,
	Vector3            endpoint,
	Engine            *engine,
	Scene             *scene
);

void LightningBeam_fire(
	LightningBeamInfo  *info,
	Entity            **beam,
	Vector3             muzzle,
	Vector3             endpoint,
	Engine             *engine,
	Scene              *scene
);


#endif /* LIGHTNING_BEAM_H */
