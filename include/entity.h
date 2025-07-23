#ifndef ENTITY_H
#define ENTITY_H


#include "common.h"


typedef void (*EntityCallback)(   Entity *entity);
typedef void (*EntityCallback_1e)(Entity *entity, CollisionResult *collision);
typedef void (*EntityCallback_1f)(Entity *entity, float   delta);

typedef struct
EntityVTable
{
    EntityCallback    Setup;
    EntityCallback    Enter;
    EntityCallback_1f Update;
    EntityCallback    Render;
    EntityCallback_1e OnCollision;
    EntityCallback    Exit;
    EntityCallback    Free;
}
EntityVTable;


typedef struct 
Entity 
{
    
    Renderable      renderables[MAX_LOD_LEVELS];
    float           lod_distances[MAX_LOD_LEVELS];
    uint8           lod_count;
    Engine         *engine;
    void           *user_data;

    EntityVTable   *vtable;
    
    union {
        Vector3 transform[4];
        Xform   xform;
        struct {
            Vector3 
                position, 
                rotation,
                scale,
                skew;
        };
    };
    Vector3        bounds;
    
    union {
        uint8 flags;
        struct {
            bool
                visible:1,
                active :1,
                flag_2 :1,
                flag_3 :1,
                flag_4 :1,
                flag_5 :1,
                flag_6 :1,
                flag_7 :1;
        };
    };
} 
Entity;


Entity  *Entity_new(      const Entity *template_entity, Engine *engine);
void     Entity_free(           Entity *entity);
/*
void     Entity_addToScene(     Scene  *scene,           Entity *entity);
void     Entity_removeFromScene(Scene  *scene,           Entity *entity);
void     Entity_updateAll(      float   dt);
*/
uint64   Entity_getUniqueID(    Entity *entity);

#endif /* ENTITY_H */
