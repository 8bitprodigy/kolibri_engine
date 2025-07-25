#include "_engine_.h"
#include "_entity_.h"
#include "_head_.h"
#include "common.h"


static uint64 latest_ID = 0;


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
	
	Engine__insertEntity(engine, node);

	if (base->Setup) base->Setup(base);
}


void
Entity_free(Entity *self)
{
	EntityNode__free(ENTITY_TO_PRIVATE(self));
}


Entity *
Entity_getNext(Entity *self)
{
	return ENTITY_TO_PRIVATE(self)->next->base;
}


Entity *
Entity_getPrev(Entity *self)
{
	return ENTITY_TO_PRIVATE(self)->prev->base;
}


uint64
Entity_getUniqueID(Entity *entity)
{
	return ENTITY_TO_PRIVATE(entity)->unique_ID;
}


void
Entity_render(Entity *entity, Head *head)
{
	if (!entity->visible) return;

	Camera3D *camera   = head->camera;
	float     distance = Vector3Distance(entity->position, camera->position);

	int lod_level = -1;
	for (int i = 0; i < entity->lod_count; i++) {
		if (entity->lod_distances[i] < distance) continue;
		lod_level = i;
		break;
	}
	if (lod_level < 0) return; /* Distance is greater than max renderable LOD level, so don't render it. */

	Renderable *renderable = &entity->renderables[lod_level];
	if (renderable && renderable->Render) {
		renderable->Render(
			renderable, 
			entity->position,
			entity->rotation,
			entity->scale
		);
	}
}


void
EntityNode__free(EntityNode *self)
{
	Entity       *entity = PRIVATE_TO_ENTITY(self);
	
	EntityVTable *vtable = entity->vtable;
	if (vtable && vtable->Free) vtable->Free(entity);
	
	Engine__removeEntity(self->engine, node);

	free(self);
}


void
EntityNode__insert(EntityNode *node, EntityNode *to)
{
	if (!node || !to) {
		ERR_OUT("EntityNode__insert() received a NULL pointer in arguments");
		return;
	}

	if (!to->prev || !to->next) {
		ERR_OUT("EntityNode__insert() received improperly initialized EntityNode `to`.");
		return;
	}

	EntityNode *last = to->prev;

	last->next = node;
	to->prev   = node;
	
	node->next = to;
	node->prev = last;
}


void
EntityNode__remove(EntityNode *node)
{
	if (!node) {
		ERR_OUT("EntityNode__remove() received a NULL pointer.");
		return;
	}
	if (!node->prev || !node->next) {
		ERR_OUT("EntityNode__remove() received an EntityNode with missing prev/next pointers.");
		return;
	}
	EntityNode *node_1 = node->prev;
	EntityNode *node_2 = node->next;

	node_1->next = node_2;
	node_2->prev = node_1;
}


void
EntityNode__updateAll(EntityNode *node)
{
	EntityNode *starting_node = node;
	
	do {
		Entity *entity = PRIVATE_TO_ENTITY(node)
		if (entity->Update) entity->Update(entity, delta);

		node = node->next;
	} while (node != starting_node);
}
