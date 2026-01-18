#include "entity.h"
#include "fps/game.h"
#include <raylib.h>
#include "utils.h"


/* Texture loader/releaser */
void *
texture_loader(const char *path, void *data, Camera3D *camera)
{
    (void)data;  // Unused
    (void)camera;
    Texture2D *texture = malloc(sizeof(Texture2D));
    *texture = LoadTexture(path);
    return texture;
}

void 
texture_releaser(void *asset, void *data, Camera3D *camera)
{
    (void)data;  // Unused
    (void)camera;
    Texture2D *texture = (Texture2D *)asset;
    UnloadTexture(*texture);
    free(texture);
}

/* Model loader/releaser */
void *
model_loader(const char *path, void *data, Camera3D *camera)
{
    (void)data;  // Unused
    (void)camera;
    Model *model = malloc(sizeof(Model));
    *model = LoadModel(path);
    return model;
}

void 
model_releaser(void *asset, void *data, Camera3D *camera)
{
    (void)data;  // Unused
    (void)camera;
    Model *model = (Model *)asset;
    UnloadModel(*model);
    free(model);
}


/* Renderable Callback */
void
RenderModel(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D    *camera
)
{
    (void)camera;
	Model  *model  = (Model*)renderable->data;
	Entity *entity = (Entity*)render_data;
	Vector3          
		    pos    = Vector3Add(position, entity->renderable_offset),
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
RenderAnimatedModel(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D    *camera
)
{
    (void)camera;
	AnimatedModel *anim_model = (AnimatedModel*)renderable->data;
	Entity        *entity     = (Entity*)render_data;
	
	if (!anim_model) {
		DBG_OUT("anim_model is NULL!");
		return;
	}
	DBG_OUT("Model bone count: %d", anim_model->model.boneCount);
	DBG_OUT("Model mesh count: %d", anim_model->model.meshCount);
	
	Vector3          
		    pos               = Vector3Add(position, entity->renderable_offset),
		    scale             = entity->scale;
	Matrix  matrix            = QuaternionToMatrix(entity->orientation);
	
	if (
		entity->current_anim < 0 
		|| entity->current_anim >= anim_model->anim_count
	) {
		DBG_OUT(
				"Invalid anim: %d (max %d)", 
				entity->current_anim, 
				anim_model->anim_count
			);
		/* Just draw without animation */
		goto DRAW_ONLY;
	}
	if (
		0 <= entity->current_anim 
		&& anim_model->animations
	) {
		ModelAnimation *anim  = &anim_model->animations[entity->current_anim];
		int             frame = entity->anim_frame % anim->frameCount;
		if (frame < 0) frame = 0;
		
		DBG_OUT("Animation bone count: %d", anim->boneCount);
		DBG_OUT("Animation frame count: %d", anim->frameCount);
		DBG_OUT(
				"Updating animation %d, frame %d/%d", 
				entity->current_anim, frame, 
				anim->frameCount
			);
		
		UpdateModelAnimation(
			anim_model->model,
			*anim,
			entity->anim_frame
		);
//*/
	}
	
DRAW_ONLY:
	rlPushMatrix();
		rlTranslatef(pos.x, pos.y, pos.z);
		rlMultMatrixf(MatrixToFloat(matrix));
		rlScalef(scale.x, scale.y, scale.z);
		
		DrawModel(
				anim_model->model,
				V3_ZERO, 
				1.0f, 
				WHITE
			);
	rlPopMatrix();
}

void
testRenderableBoxCallback(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
)
{
    (void)camera;
	Entity *entity = render_data;
	Color  *color  = (Color*)renderable->data;
	if (!color) return;
	DrawCubeV(
		Vector3Add(position, entity->renderable_offset),
		entity->bounds,
		*color
	);
		
}

void
testRenderableBoxWiresCallback(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
)
{
    (void)camera;
	Entity *entity = render_data;
	Color  *color  = (Color*)renderable->data;
	if (!color) return;
	DrawCubeWiresV(
		Vector3Add(position, entity->renderable_offset),
		entity->bounds,
		*color
	);
}

void
testRenderableCylinderWiresCallback(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
)
{
	(void)render_data;
    (void)camera;
	Entity *entity = renderable->data;
	Color  *color  = (Color*)renderable->data;
	float               radius = entity->bounds.x;
	if (!color) return;
	DrawCylinderWires(
		position,
		radius,
		radius,
		entity->bounds.y,
		8,
		*color
	);
}

