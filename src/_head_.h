#ifndef HEAD_PRIVATE_H
#define HEAD_PRIVATE_H


#include "head.h"


typedef struct 
Head
{
    Camera3D                  camera;
    RenderTexture             viewport;
    Rectangle                 region;
    int                       controller_id;
    Engine                   *engine;
    Entity                   *controlled_entity;
    void                     *user_data;
    HeadFreeUserDataCallback  FreeUserData;
	RendererSettings         *settings;

    struct Head
		*prev,
		*next;

	HeadVTable *vtable;
} 
Head;


void Head__freeAll(Head *head);
void Head__updateAll(Head *head, float delta);


#endif /* HEAD_PRIVATE_H */
