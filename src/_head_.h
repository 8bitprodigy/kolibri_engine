#ifndef HEAD_PRIVATE_H
#define HEAD_PRIVATE_H


#include "head.h"
#include <raylib.h>


typedef struct 
Head
{
    Camera3D                  camera;
    Vector3
        prev_position,
        prev_target;
    float                     prev_fovy;
    int
        prev_width,
        prev_height;
    RenderTexture             viewport;
    Region                    region;
	RendererSettings          settings;
    Frustum                   frustum;
    Engine                   *engine;
    void                     *user_data;

    struct Head
		*prev,
		*next;

	HeadVTable *vtable;
} 
Head;


void Head__freeAll(Head *head);
void Head__updateAll(Head *head, float delta);


#endif /* HEAD_PRIVATE_H */
