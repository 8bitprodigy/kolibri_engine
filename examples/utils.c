#include "entity.h"
#include "fps/game.h"
#include <raylib.h>
#include "utils.h"


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
	SpriteInfo *data    = (SpriteInfo*)renderable->data;
	Entity     *entity  = (Entity*)render_data;
	SpriteData *sdata   = (SpriteData*)entity->local_data;
	Texture2D   texture = data->frames[sdata->current_frame];
	float       
		scale    = 1.0f,
		rotation = 0.0f;
	Vector3 
		pos     = Vector3Add(entity->position, entity->renderable_offset),
		forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position)),
		right   = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0, 1, 0})),
		camera_local_up = Vector3CrossProduct(right, forward);

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
			camera_local_up,
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

