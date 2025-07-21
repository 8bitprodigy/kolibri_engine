#ifndef ENTITY_H
#define ENTITY_H


#include "common.h"


enum {
    POSITION,
    ROTATION,
    SCALE,
    SKEW,
}

typedef void (*EntityCallback)(   Entity *entity);
typedef void (*EntityCallback_1e)(Entity *entity, Entity *entity);
typedef void (*EntityCallback_1f)(Entity *entity, float   delta);

typedef struct 
Entity 
{
    
    Model          *model;
    Engine         *engine;
    void           *user_data; /* This is a void *, but could be replaced with a hashmap, perhaps */

    EntityCallback    Setup;
    EntityCallback    Enter;
    EntityCallback_1f Update;
    EntityCallback_1e OnCollision;
    EntityCallback    Exit;
    EntityCallback    Free;
    
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
    Vector3        dimensions;
    
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
void     Entity_addToScene(     Scene  *scene,           Entity *entity);
void     Entity_removeFromScene(Scene  *scene,           Entity *entity);
void     Entity_updateAll(      float   dt);
uint64   Entity_getUniqueID(    Entity *entity);

#endif /* ENTITY_H */
