#include "entity.h"
#include "fps/game.h"
#include <raylib.h>
#include "utils.h"



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

/* Texture loader/releaser */
void *
texture_loader(const char *path, void *data, Camera3D *camera)
{
    (void)data;  // Unused
    Texture2D *texture = malloc(sizeof(Texture2D));
    *texture = LoadTexture(path);
    return texture;
}

void 
texture_releaser(void *asset, void *data, Camera3D *camera)
{
    (void)data;  // Unused
    Texture2D *texture = (Texture2D *)asset;
    UnloadTexture(*texture);
    free(texture);
}

/* Model loader/releaser */
void *
model_loader(const char *path, void *data, Camera3D *camera)
{
    (void)data;  // Unused
    Model *model = malloc(sizeof(Model));
    *model = LoadModel(path);
    return model;
}

void 
model_releaser(void *asset, void *data, Camera3D *camera)
{
    (void)data;  // Unused
    Model *model = (Model *)asset;
    UnloadModel(*model);
    free(model);
}


/* Renderable Callback */
void
RenderModel(
	Renderable *renderable,
	void       *render_data, 
	Camera3D    *camera
)
{
	Model  *model  = (Model*)renderable->data;
	Entity *entity = (Entity*)render_data;
	Vector3          
		    pos    = Vector3Add(entity->position, entity->renderable_offset),
		    scale  = entity->scale;
	Matrix  matrix = QuaternionToMatrix(entity->orientation);
	
	rlPushMatrix();
		rlTranslatef(pos.x, pos.y, pos.z);
		rlMultMatrixf(MatrixToFloat(matrix));
		rlScalef(scale.x, scale.y, scale.z);
		
		DrawModel(
				*model,
				V3_ZERO, 
				1.0f, 
				WHITE
			);
	rlPopMatrix();
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
	Texture2D   texture = info->frames[data->current_frame];
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

	Rectangle source = (Rectangle){
			0.0f,
			0.0f,
			(float)texture.width,
			(float)texture.height,
		};
	Vector2 size = (Vector2){
			scale * fabsf((float)source.width/source.height),
			scale
		};

	
	
	DrawBillboardPro(
			*camera, 
			texture, 
			source,
			entity->position,
			up,
			size,
			Vector2Scale(size, 0.5f),
			rotation, 
			WHITE
		);
}

void
testRenderableBoxCallback(
	Renderable *renderable,
	void       *render_data, 
	Camera3D   *camera
)
{
	Entity *entity = render_data;
	Color  *color  = (Color*)renderable->data;
	if (!color) return;
	DrawCubeV(
		Vector3Add(entity->position, entity->renderable_offset),
		entity->bounds,
		*color
	);
		
}

void
testRenderableBoxWiresCallback(
	Renderable *renderable,
	void       *render_data, 
	Camera3D   *camera
)
{
	Entity *entity = render_data;
	Color  *color  = (Color*)renderable->data;
	if (!color) return;
	DrawCubeWiresV(
		Vector3Add(entity->position, entity->renderable_offset),
		entity->bounds,
		*color
	);
}

void
testRenderableCylinderWiresCallback(
	Renderable *renderable,
	void       *render_data, 
	Camera3D   *camera
)
{
	Entity *entity = renderable->data;
	Color  *color  = (Color*)renderable->data;
	float               radius = entity->bounds.x;
	if (!color) return;
	DrawCylinderWires(
		entity->position,
		radius,
		radius,
		entity->bounds.y,
		8,
		*color
	);
}

