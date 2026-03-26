#ifndef RAILTRAIL_H
#define RAILTRAIL_H

#include "common.h"
#include "ribbon.h"


typedef struct RailTrailInfo RailTrailInfo;


RailTrailInfo *RailTrailInfo_new(
	float      width,
	float      lifetime,
	float      scroll_speed,
	Color      color,
	Texture2D  atlas,
	size_t     num_frames
);

void RailTrail_new(
	RailTrailInfo *info,
	Vector3        muzzle,
	Vector3        endpoint,
	Engine        *engine,
	Scene         *scene
);


#endif /* RAILTRAIL_H */
