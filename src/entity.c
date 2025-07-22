#include "_engine_.h"
#include "_entity_.h"
#include "common.h"


static uint64 latest_ID = 0;


void insertEntityNode(EntityNode *node, EntityNode *to);
void removeEntityNode(EntityNode *node);


/******************
	CONSTRUCTOR
******************/
Entity *
Entity_new(const Entity *entity, Engine *engine)
{
	EntityNode *node = malloc(sizeof(EntityNode));

	if (!node) {
		ERR_OUT("Failed to allocate memory for EntityNode.");
		return NULL;
	}

	node->next = node;
	node->prev = node;

	Entity *base = PRIVATE_TO_ENTITY(node);
	*base = *entity;

	base->user_data = NULL;
	base->engine    = engine;

	node->unique_ID = latest_ID++;
	
	insertEntityNode(node, Engine__getEntities(engine));

	if (base->Setup) base->Setup(base);
}


void
Entity_free(Entity *self)
{
	EntityNode__free(ENTITY_TO_PRIVATE(self));
}


void
EntityNode__free(EntityNode *self)
{
	Entity *entity = PRIVATE_TO_ENTITY(self);

	entity->Free(entity);

	free(self);
}


void
insertEntityNode(EntityNode *node, EntityNode *to)
{
	if (!node || !to) {
		ERR_OUT("insertEntity() received a NULL pointer as argument");
		return;
	}

	if (!to->prev || !to->next) {
		ERR_OUT("insertEntity() received improperly initialized EntityNode `to`.");
		return;
	}

	EntityNode *last = to->prev;

	last->next = node;
	to->prev   = node;
	
	node->next = to;
	node->prev = last;
}


void
removeEntityNode(EntityNode *node)
{
	EntityNode *node_1 = node->prev;
	EntityNode *node_2 = node->next;

	node_1->next = node_2;
	node_2->prev = node_1;
}


void
Entity_getUniqueID(Entity *entity)
{
	return &ENTITY_TO_PRIVATE(entity)->unique_ID;
}
