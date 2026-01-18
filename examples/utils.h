#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include <string.h>




/* Texture loader/releaser */
void *texture_loader(const char *path,  void *data, Camera3D *camera);
void texture_releaser(void      *asset, void *data, Camera3D *camera);
/* Model loader/releaser */
void *model_loader(const char *path,  void *data, Camera3D *camera);
void model_releaser(void      *asset, void *data, Camera3D *camera);


void
RenderModel(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
);

void
RenderAnimatedModel(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
);

void
testRenderableBoxCallback(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
);

void
testRenderableBoxWiresCallback(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
);

void
testRenderableCylinderWiresCallback(
	Renderable *renderable,
	void       *render_data, 
	Vector3     position,
	Camera3D   *camera
);

#endif /* UTILS_H */
