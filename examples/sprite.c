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
	SpriteAlignment sprite_alignment
)
{
	SpriteInfo result;
	
	result.scale            = scale;
	result.time_per_frame   = time_per_frame;
	result.atlas            = atlas;
	result.num_frames       = x_num_frames * y_num_frames;
	result.sprite_alignment = sprite_alignment;
	
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
	SpriteAlignment  sprite_alignment
)
{
	SpriteInfo result;

	result.scale            = scale;
	result.time_per_frame   = time_per_frame;
	result.atlas            = atlas;
	result.num_frames       = num_frames;
	result.frames           = frames_array;
	result.sprite_alignment = sprite_alignment;

	return result;
}

size_t 
AnimateSprite(
	SpriteInfo *info, 
	SpriteData *data, 
	float       age, 
	size_t      num_frames
)
{
	return (size_t)(
			data->start_frame 
			+ (age/info->time_per_frame)
		) 
		% num_frames;
}

void
RenderBillboard(
	Renderable *renderable,
	void       *render_data, 
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
	
	Vector3 
		pos     = Vector3Add(entity->position, entity->renderable_offset),
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
			entity->position,
			up,
			size,
			Vector2Scale(size, 0.5f),
			rotation, 
			WHITE
		);
}
