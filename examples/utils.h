#ifndef UTILS_H
#define UTILS_H

#include "common.h"


/* Texture loader/releaser */
void *texture_loader(const char *path,  void *data);
void texture_releaser(void      *asset, void *data);
/* Model loader/releaser */
void *model_loader(const char *path,  void *data);
void model_releaser(void      *asset, void *data);


void
RenderModel(
	Renderable *renderable,
	void       *render_data
);

void
testRenderableBoxCallback(
	Renderable *renderable,
	void       *render_data
);

void
testRenderableBoxWiresCallback(
	Renderable *renderable,
	void       *render_data
);

void
testRenderableCylinderWiresCallback(
	Renderable *renderable,
	void       *render_data
);

#endif /* UTILS_H */
