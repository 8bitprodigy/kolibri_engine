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
	int     current_lod;
    float   last_lod_distance; /* Cache to avoid recalculating every frame */
    double  creation_time;
    size_t  size;
    bool    visible_last_frame; /* For frustum culling optimizations */
	
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
