#ifndef ENTITY_H
#define ENTITY_H


#include "common.h"


typedef void (*EntityCallback)(         Entity *entity);
typedef void (*EntityCollisionCallback)(Entity *entity, CollisionResult collision);
typedef void (*EntityUpdateCallback)(   Entity *entity, float           delta);

typedef struct
EntityVTable
{
    EntityCallback          Setup;
    EntityCallback          Enter;
    EntityUpdateCallback    Update;
    EntityCallback          Render;
    EntityCollisionCallback OnCollision;
    EntityCollisionCallback OnCollided;
    EntityCallback          Exit;
    EntityCallback          Free;
}
EntityVTable;


typedef struct 
Entity 
{
    
    Renderable     *renderables[MAX_LOD_LEVELS];
    float           lod_distances[MAX_LOD_LEVELS];
    uint8           lod_count;
    float           visibility_radius;
    Vector3         
                    bounds,
                    bounds_offset,
                    renderable_offset;
    void           *user_data;
    EntityVTable   *vtable;
    
    union {
        Transform transform;
        struct {
            Vector3    position;
            Quaternion rotation;
            Vector3    scale;
        };
    };
    
    struct {
        union {
            uint8 layers;
            struct {
                bool
                    layer_0:1,
                    layer_1:1,
                    layer_2:1,
                    layer_3:1,
                    layer_4:1,
                    layer_5:1,
                    layer_6:1,
                    layer_7:1;
            };
        };
        union {
            uint8 masks;
            struct {
                bool
                    mask_0:1,
                    mask_1:1,
                    mask_2:1,
                    mask_3:1,
                    mask_4:1,
                    mask_5:1,
                    mask_6:1,
                    mask_7:1;
            };
        };
    }
    collision;
    
    union {
        uint8 flags;
        struct {
            bool
                active         :1,
                visible        :1;
            CollisionShape collision_shape:2; /* 0 = None | 1 = AABB | 2 = Cylinder | 3 = Sphere */
            bool
                solid          :1, /* Solid or area collision */
                _flag_5        :1,
                _flag_6        :1,
                _flag_7        :1;
        };
    };
} 
Entity;

/*
    Constructor/Destructor
*/
Entity  *Entity_new(      const Entity *template_entity, Engine *engine);
void     Entity_free(           Entity *entity);

/*
    Setters/Getters
*/
BoundingBox  Entity_getBoundingBox(Entity *entity);
Engine      *Entity_getEngine(     Entity *entity);
Entity      *Entity_getNext(       Entity *entity);
Entity      *Entity_getPrev(       Entity *entity);
uint64       Entity_getUniqueID(   Entity *entity);

/*
    Methods
*/
CollisionResult Entity_move(  Entity *entity, Vector3  movement);    
void            Entity_render(Entity *entity, Head    *head);

#endif /* ENTITY_H */
