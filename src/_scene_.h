#include "scene.h"


typedef struct 
Scene
{
    struct Scene
        *prev,
        *next;
    void      *map_data;
    SceneType *vtable;
} 
Scene;
