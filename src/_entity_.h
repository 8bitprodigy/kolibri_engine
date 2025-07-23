#include "entity.h"


#define ENTITY_TO_PRIVATE(e) (((EntityNode*)((char*)(e) - offsetof(EntityNode, base)))
#define PRIVATE_TO_ENTITY(p) ((p)->base)


typedef struct
EntityNode
{
	struct EntityNode
		*prev,
		*next;

	uint64 unique_ID;
	int    current_lod;
    float  last_lod_distance; /* Cache to avoid recalculating every frame */
    bool   visible_last_frame; /* For frustum culling optimizations */
	
	Entity base;
}
EntityNode;

void EntityNode__insert(EntityNode *entity_node, EntityNode *to);
void EntityNode__remove(EntityNode *entity_node);
void EntityNode__free(  EntityNode *entity_node);
