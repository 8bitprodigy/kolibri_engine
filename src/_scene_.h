#ifndef SCENE_PRIVATE_H
#define SCENE_PRIVATE_H


#include "_collision_.h"
#include "_entity_.h"
#include "scene.h"


typedef struct 
Scene
{
    struct Scene
        *prev,
        *next;
    
    Engine          *engine;
	Entity         **entity_list;
	CollisionScene  *collision_scene;
    SceneVTable     *vtable;
    void            *info;
    
    union {
        uint8 flags;
        struct {
            bool dirty_EntityList:1;
			bool flag_1          :1; /* 1-7 not yet defined */
			bool flag_2          :1;
            bool flag_3          :1; 
			bool flag_4          :1;
			bool flag_5          :1;
			bool flag_6          :1;
			bool flag_7          :1;
        };
    };

    __attribute__((aligned(8))) unsigned char data[];
} 
Scene;


void        Scene__freeAll(     Scene *scene);
void        Scene__render(      Scene *scene, float       delta);
void        Scene__update(      Scene *scene, float       delta);


#endif /* SCENE_PRIVATE_H */
