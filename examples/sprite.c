#include "dynamicarray.h"
#include "entity.h"
#include "sprite.h"


SpriteInfo
CreateRegularSprite(
	float           scale, 
	float           time_per_frame, 
	Texture2D       atlas, 
	size_t          x_num_frames, 
	size_t          y_num_frames, 
	SpriteAlignment sprite_alignment,
	SpriteDirection sprite_direction,
	SpritePlayback  sprite_playback
)
{
	SpriteInfo result;
	
	result.scale            = scale;
	result.time_per_frame   = time_per_frame;
	result.atlas            = atlas;
	result.num_frames       = x_num_frames * y_num_frames;
	result.sprite_alignment = sprite_alignment;
	result.sprite_playback  = sprite_playback;
	result.sprite_direction = sprite_direction;
	
	float
		frame_width   = atlas.width  / x_num_frames,
		frame_height  = atlas.height / y_num_frames;
	Rectangle *frames = DynamicArray(Region, result.num_frames);
	for (int y = 0; y < y_num_frames; y++) { 
		for (int x = 0; x < x_num_frames; x++) {
			frames[y * x_num_frames + x] = (Rectangle){
					.x      = x * frame_width,
					.y      = y * frame_height,
					.width  = frame_width,
					.height = frame_height,
				};
		}
	}
	result.frames = frames;

	return result;
}

SpriteInfo 
CreateIrrecularSprite(
	float            scale,
	float            time_per_frame,
	Texture2D        atlas,
	size_t           num_frames,
	Rectangle       *frames_array,
	SpriteAlignment  sprite_alignment,
	SpriteDirection  sprite_direction,
	SpritePlayback   sprite_playback
)
{
	SpriteInfo result;

	result.scale            = scale;
	result.time_per_frame   = time_per_frame;
	result.atlas            = atlas;
	result.num_frames       = num_frames;
	result.frames           = frames_array;
	result.sprite_alignment = sprite_alignment;
	result.sprite_playback  = sprite_playback;
	result.sprite_direction = sprite_direction;

	return result;
}

void 
AnimateSprite(
	SpriteInfo *info, 
	SpriteData *data, 
	float       age
)
{
	if (!data->playing) return;
	
	size_t num_frames = info->num_frames;
	size_t frame_idx  = (size_t)(age / info->time_per_frame);
	bool   is_oneshot = (info->sprite_playback == SPRITE_PLAY_ONESHOT);
	
	switch (info->sprite_direction) {
	case SPRITE_DIR_FORWARD: {
		size_t result = data->start_frame + frame_idx;
		if (is_oneshot && result >= num_frames) {
			data->current_frame = num_frames - 1;
			data->playing = false;
		}
		else {
			data->current_frame = (data->start_frame + frame_idx) % num_frames;
		}
		return;
	}
	
	case SPRITE_DIR_BACKWARD: {
		if (is_oneshot && frame_idx >= num_frames) {
			data->current_frame = data->start_frame;
			data->playing = false;
		}
		else {
			data->current_frame = (data->start_frame + num_frames - (frame_idx % num_frames)) % num_frames;
		}
		return;
	}
	
	case SPRITE_DIR_PINGPONG: {
		size_t cycle_len = (num_frames - 1) * 2;
		size_t total_frames_to_play = (num_frames - 1 - data->start_frame) + num_frames;
		
		if (is_oneshot && frame_idx >= total_frames_to_play) {
			data->current_frame = 0;
			data->playing = false;
		}
		else {
			size_t pos = (data->start_frame + frame_idx) % cycle_len;
			data->current_frame = (pos < num_frames) ? pos : (cycle_len - pos);
		}
		return;
	}
	
	case SPRITE_DIR_RANDOM: {
		if (is_oneshot && frame_idx >= num_frames) {
			data->playing = false;
			return;
		}
		
		// Deterministic integer hash
		size_t r = frame_idx;
		r ^= r >> 16;
		r *= 0x7feb352d;
		r ^= r >> 15;
		r *= 0x846ca68b;
		r ^= r >> 16;

		data->current_frame = (data->start_frame + r) % num_frames;
		return;
	}
	
	default:
		data->current_frame = (data->start_frame + frame_idx) % num_frames;
		return;
	}
}


void
RenderBillboard(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
)
{
	SpriteInfo *info    = (SpriteInfo*)renderable->data;
	Entity     *entity  = (Entity*)render_data;
	SpriteData *data   = (SpriteData*)entity->local_data;
	Texture2D   texture = info->atlas;
	Rectangle   region  = info->frames[data->current_frame];
	float       
		scale    = info->scale,
		rotation = 0.0f;

	scale = (scale == 0.0f)? 1.0f : scale;
	
    // Debug for wrapped positions
    if (fabsf(position.x) > 1000 || fabsf(position.z) > 1000) {
        printf("RenderBillboard: pos=(%.1f,%.1f,%.1f), cam=(%.1f,%.1f,%.1f)\n",
               position.x, position.y, position.z,
               camera->position.x, camera->position.y, camera->position.z);
    }
    
	Vector3 
		pos     = Vector3Add(position, entity->renderable_offset),
		forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position)),
		right   = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0, 1, 0})),
		up;

	switch (info->sprite_alignment) {
	case SPRITE_ALIGN_X:
		up = V3_LEFT;
		break;
	case SPRITE_ALIGN_Y:
		up = V3_UP;
		break;
	case SPRITE_ALIGN_Z:
		up = V3_FORWARD;
		break;
	case SPRITE_ALIGN_LOCAL_AXIS:
		up = Vector3Normalize(*(Vector3*)&entity->orientation);
		break;
	case SPRITE_ALIGN_CAMERA:
		up = Vector3CrossProduct(right, forward);
	}
	
	Vector2 size = (Vector2){
			scale * fabsf(region.width/region.height),
			scale
		};
	
	DrawBillboardPro(
			*camera, 
			texture, 
			region,
			pos,
			up,
			size,
			Vector2Scale(size, 0.5f),
			rotation, 
			WHITE
		);
    
    if (fabsf(position.x) > 1000 || fabsf(position.z) > 1000) {
        printf("  Drew billboard at final pos=(%.1f,%.1f,%.1f)\n", pos.x, pos.y, pos.z);
    }
}
