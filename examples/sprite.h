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

typedef struct SpriteInfo SpriteInfo;
typedef struct SpriteData SpriteData;

typedef void (*SpriteCallback)(SpriteInfo *info, SpriteData *data);

typedef struct
SpriteData
{
	size_t 
		start_frame,
		current_frame;
	bool playing;
}
SpriteData;


SpriteInfo *SpriteInfo_newRegular(
		float            scale, 
		float            time_per_frame, 
		Texture2D        atlas, 
		size_t           x_num_frames, 
		size_t           y_num_frames, 
		SpriteAlignment  sprite_alignment,
		SpriteDirection  sprite_direction,
		SpritePlayback   sprite_playback,
		SpriteCallback   sprite_callback,
		void            *user_data
	);
SpriteInfo *SpriteInfo_newIrregular(
		float            scale,
		float            time_per_frame,
		Texture2D        atlas,
		size_t           num_frames,
		Rectangle       *frames_array,
		SpriteAlignment  sprite_alignment,
		SpriteDirection  sprite_direction,
		SpritePlayback   sprite_playback,
		SpriteCallback  sprite_callback,
		void            *user_data
	);

void AnimateSprite(  SpriteInfo *info,       SpriteData *data,        float   age);
void RenderBillboard(Renderable *renderable, void       *render_data, Vector3 position, Camera3D *camera);


#endif /* SPRITE_H */
