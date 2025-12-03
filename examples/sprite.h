#ifndef SPRITE_H
#define SPRITE_H

#include "common.h"


/*
	Typedefs
*/
typedef enum
{
	SPRITE_ALIGN_X,
	SPRITE_ALIGN_Y,
	SPRITE_ALIGN_Z,
	SPRITE_ALIGN_LOCAL_AXIS,
	SPRITE_ALIGN_CAMERA,
}
SpriteAlignment;

typedef struct
{
	float            
	                 scale,
	                 time_per_frame;
	size_t           num_frames;
	Texture2D        atlas;
	Rectangle       *frames;
	SpriteAlignment  sprite_alignment;
}
SpriteInfo;

typedef struct
{
	size_t 
		start_frame,
		current_frame;
}
SpriteData;


SpriteInfo CreateRegularSprite(
		float           scale, 
		float           time_per_frame, 
		Texture2D       atlas, 
		size_t          x_num_frames, 
		size_t          y_num_frames, 
		SpriteAlignment sprite_alignment
	);
SpriteInfo CreateIrrecularSprite(
		float            scale,
		float            time_per_frame,
		Texture2D        atlas,
		size_t           num_frames,
		Rectangle       *frames_array,
		SpriteAlignment  sprite_alignment
	);

size_t AnimateSprite(  SpriteInfo *info,       SpriteData *data,        float     age,   size_t num_frames);
void   RenderBillboard(Renderable *renderable, void       *render_data, Camera3D *camera);


#endif /* SPRITE_H */
