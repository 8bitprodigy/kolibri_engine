#include "_engine_.h"
#include "_entity_.h"
#include "_head_.h"
#include "common.h"


static uint64 Latest_ID = 0;


/******************
	CONSTRUCTOR
******************/
Entity *
Entity_new(const Entity *entity, Engine *engine)
{
	if (!engine) return NULL;
	
	EntityNode *node = malloc(sizeof(EntityNode));

	if (!node) {
		ERR_OUT("Failed to allocate memory for EntityNode.");
		return NULL;
	}

	node->next   = node;
	node->prev   = node;
	node->engine = engine;

	Entity *base = PRIVATE_TO_ENTITY(node);
	*base = *entity;

	base->user_data = NULL;
	node->unique_ID = Latest_ID++;
	
	Engine__insertEntity(engine, node);

	EntityVTable *vtable = base->vtable;
	if (vtable && vtable->Setup) vtable->Setup(base);

	return base;
}


void
Entity_free(Entity *self)
{
	EntityNode__free(ENTITY_TO_PRIVATE(self));
}


Entity *
Entity_getNext(Entity *self)
{
	return &ENTITY_TO_PRIVATE(self)->next->base;
}


Entity *
Entity_getPrev(Entity *self)
{
	return &ENTITY_TO_PRIVATE(self)->prev->base;
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

	Camera3D *camera   = &head->camera;
	float     distance = Vector3Distance(entity->position, camera->position);

	int lod_level = -1;
	for (int i = 0; i < entity->lod_count; i++) {
		if (entity->lod_distances[i] < distance) continue;
		lod_level = i;
		break;
	}
	if (lod_level < 0) return; /* Distance is greater than max renderable LOD level, so don't render it. */
	
	//DBG_OUT("Rendering entity at position: (%.2f, %.2f, %.2f)", entity->position.x, entity->position.y, entity->position.z);
	//DBG_OUT("LOD level: %d\n", lod_level);
	
	Renderable *renderable = entity->renderables[lod_level];

	//DBG_OUT("Renderable pointer: %p", (void*)renderable);
	//DBG_OUT("Renderable->Render function pointer: %p", (void*)renderable->Render);
	//DBG_OUT("Entity->renderables[%d] address: %p", lod_level, (void*)entity->renderables[lod_level]);
	
	
	if (renderable && renderable->Render) {
		//DBG_OUT("About to call Render function...");
		renderable->Render(
			renderable,
			entity
		);
		//DBG_OUT("Render function completed successfully");
	}
	/*
	DBG_EXPR(
		else {
			DBG_OUT("ERROR: renderable or renderable->Render is NULL!");
		}
	);
	*/
}


void
EntityNode__free(EntityNode *self)
{
	Entity       *entity = PRIVATE_TO_ENTITY(self);
	
	EntityVTable *vtable = entity->vtable;
	if (vtable && vtable->Free) vtable->Free(entity);
	
	Engine__removeEntity(self->engine, self);

	free(self);
}

void
EntityNode__freeAll(EntityNode *self)
{
	EntityNode *next = self->next;
	if (next == self) {
		EntityNode__free(self);
		return;
	}
	
	EntityNode__free(self);
	EntityNode__freeAll(next);
}

void
EntityNode__updateAll(EntityNode *node, float delta)
{
	if (!node) return;
	EntityNode *starting_node = node;
	
	do {
		Entity *entity = PRIVATE_TO_ENTITY(node);
		EntityVTable *vtable = entity->vtable;
		if (vtable && vtable->Update) vtable->Update(entity, delta);

		node = node->next;
	} while (node != starting_node);
}
