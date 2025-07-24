#include "head.h"


typedef struct 
Head
{
    Camera3D              camera;
    RenderTexture         viewport;
    int                   controller_id;
    Engine               *engine;
    Entity               *controlled_entity;
    void                 *user_data;
    FreeUserDataCallback *FreeUserData;

    struct Head
		*prev,
		*next;

	HeadVTable *vtable;
} 
Head;

