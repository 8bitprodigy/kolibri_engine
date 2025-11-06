#include "entity.h"
#include "utils.h"


/* Texture loader/releaser */
void *
texture_loader(const char *path, void *data)
{
    (void)data;  // Unused
    Texture2D *texture = malloc(sizeof(Texture2D));
    *texture = LoadTexture(path);
    return texture;
}

void 
texture_releaser(void *asset, void *data)
{
    (void)data;  // Unused
    Texture2D *texture = (Texture2D *)asset;
    UnloadTexture(*texture);
    free(texture);
}

/* Model loader/releaser */
void *
model_loader(const char *path, void *data)
{
    (void)data;  // Unused
    Model *model = malloc(sizeof(Model));
    *model = LoadModel(path);
    return model;
}

void 
model_releaser(void *asset, void *data)
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
	void       *render_data
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
testRenderableBoxCallback(
	Renderable *renderable,
	void       *render_data
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
	void       *render_data
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
	void       *render_data
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

