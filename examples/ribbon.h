#ifndef RIBBON_H
#define RIBBON_H

#include "common.h"


typedef struct
RibbonInfo
{
	float      
		width,
		lifetime,
		y_scale;
	Color      color;
	Texture2D  atlas;
	size_t     num_frames;
}
RibbonInfo;

typedef struct
RibbonData
{
	Vector3    start;
	Vector3    end;
	float      width_delta;
	float      scroll_offset;
	Color     *colors;     /* NULL = use info->color, else per-node colors  */
	Vector2   *offsets;    /* NULL = single quad, else subdivision nodes    */
	size_t     current_frame;
	bool       
		playing,
		flip_u,
		flip_v;
}
RibbonData;


RibbonInfo *RibbonInfo_new(
	float      width,
	float      lifetime,
	Color      color,
	Texture2D  atlas,
	size_t     num_frames
);

void RenderRibbon(
	Renderable *renderable,
	void       *render_data,
	Vector3     position,
	Camera3D   *camera
);


#endif /* RIBBON_H */
