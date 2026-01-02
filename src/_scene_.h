#ifndef SCENE_PRIVATE_H
#define SCENE_PRIVATE_H


#include "_collision_.h"
#include "_entity_.h"
#include "../examples/rmem.h"
#include "scene.h"


typedef struct 
Scene
{
    struct Scene
        *prev,
        *next;
    
    Engine         *engine;
    MemPool         entity_pool;
	EntityNode     *entities;
	CollisionScene *collision_scene;
    SceneVTable    *vtable;
    void           *info;
	EntityList      entity_list;
    uint            entity_count;
    
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
EntityNode *Scene__getEntities( Scene *scene);
void        Scene__insertEntity(Scene *scene, EntityNode *node);
void        Scene__removeEntity(Scene *scene, EntityNode *node);
void        Scene__update(      Scene *scene, float       delta);


#endif /* SCENE_PRIVATE_H */
