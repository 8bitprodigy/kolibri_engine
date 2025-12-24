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

typedef enum
{
	SPRITE_PLAY_ONESHOT = 0,
	SPRITE_PLAY_LOOP,
}
SpritePlayback;

typedef enum
{
    SPRITE_DIR_FORWARD,
    SPRITE_DIR_BACKWARD,
    SPRITE_DIR_PINGPONG,
    SPRITE_DIR_RANDOM,
}
SpriteDirection;

typedef struct
{
	float            
	                 scale,
	                 time_per_frame;
	size_t           num_frames;
	Texture2D        atlas;
	Rectangle       *frames;
	SpriteAlignment  sprite_alignment;
	SpritePlayback   sprite_playback;
	SpriteDirection  sprite_direction;
}
SpriteInfo;

typedef struct
{
	size_t 
		start_frame,
		current_frame;
	bool playing;
}
SpriteData;


SpriteInfo CreateRegularSprite(
		float           scale, 
		float           time_per_frame, 
		Texture2D       atlas, 
		size_t          x_num_frames, 
		size_t          y_num_frames, 
		SpriteAlignment sprite_alignment,
		SpriteDirection sprite_direction,
		SpritePlayback  sprite_playback
	);
SpriteInfo CreateIrrecularSprite(
		float            scale,
		float            time_per_frame,
		Texture2D        atlas,
		size_t           num_frames,
		Rectangle       *frames_array,
		SpriteAlignment  sprite_alignment,
		SpriteDirection  sprite_direction,
		SpritePlayback   sprite_playback
	);

void AnimateSprite(  SpriteInfo *info,       SpriteData *data,        float   age);
void RenderBillboard(Renderable *renderable, void       *render_data, Vector3 position, Camera3D *camera);


#endif /* SPRITE_H */
