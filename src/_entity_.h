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
	
	Entity base;
}
EntityNode;


void EntityNode__free(EntityNode *entity_node);
