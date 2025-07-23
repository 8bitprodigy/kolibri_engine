#include "scene.h"


typedef struct 
Scene
{
    struct Scene
        *prev,
        *next;
    Engine    *engine;
    void      *map_data;
    SceneType *vtable;
} 
Scene;
