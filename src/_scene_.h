#ifndef SCENE_PRIVATE_H
#define SCENE_PRIVATE_H


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


Scene__update(Scene *scene, float delta);


#endif /* SCENE_PRIVATE_H */
