#ifndef ENTITY_PRIVATE_H
#define ENTITY_PRIVATE_H


#include <stddef.h>

#include "entity.h"


#define ENTITY_TO_NODE(e) (((EntityNode*)((char*)(e) - offsetof(EntityNode, base))))
#define NODE_TO_ENTITY(p) (&((p)->base))


typedef struct
EntityNode
{
	struct EntityNode
		*prev,
		*next;

    Engine *engine;
    Scene  *scene;
	uint64  unique_ID;
    double  creation_time;
    size_t  size;
	int     current_lod;
    float   last_lod_distance; /* Cache to avoid recalculating every frame */
    union {
        uint8 flags;
        struct {
            bool
                visible_last_frame:1, /* For frustum culling optimizations */
                on_floor          :1, /* Collision States */
                on_wall           :1,
                on_ceiling        :1,
                _flag_4           :1, /* Not yet defined */
                _flag_5           :1,
                _flag_6           :1,
                _flag_7           :1;
        };
    };
	
	Entity  base;
}
EntityNode;

/* Destructor */
void EntityNode__free(     EntityNode *entity_node);
void EntityNode__freeAll(  EntityNode *entity_node);
void EntityNode__insert(   EntityNode *self,         EntityNode *to);


/* Methods */
void EntityNode__updateAll(EntityNode *entity_node, float delta);
void EntityNode__renderAll(EntityNode *entity_node, float delta);


#endif /* ENTITY_PRIVATE_H */
