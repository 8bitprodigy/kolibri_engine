#include "_engine_.h"
#include "_entity_.h"
#include "_head_.h"
#include "common.h"
#include <raylib.h>
#include <raymath.h>


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

Engine *
Entity_getEngine(Entity *entity)
{
	return ENTITY_TO_PRIVATE(entity)->engine;
}

BoundingBox
Entity_getBoundingBox(Entity *entity)
{
	Vector3 
		bounds = entity->bounds,
		scale  = entity->scale;
	
	if (entity->collision_shape) {
		bounds.z = bounds.x;
	}
	bounds = Vector3Multiply(bounds, scale);
	
	Vector3 
		bounds_offset = Vector3Multiply(entity->bounds_offset, scale),
		half_bounds   = Vector3Scale(bounds, 0.5f),
		position      = Vector3Add(entity->position, bounds_offset);

	return (BoundingBox){
			Vector3Subtract(position, half_bounds),
			Vector3Add(     position, half_bounds)
		};
}


CollisionResult
Entity_move(Entity *self, Vector3 movement)
{
	if (Vector3Equals(movement, V3_ZERO)) return NO_COLLISION;
    CollisionResult  result;
    Scene           *scene = Engine_getScene(ENTITY_TO_PRIVATE(self)->engine);
    Vector3
        *position = &self->position;
    result = Scene_checkContinuous(scene, self, movement);

    if (result.hit) {
        EntityVTable *entity_vtable = self->vtable;
        Entity *other               = result.entity;
        Vector3 new_position;
        if (other) {
            EntityVTable *other_vtable   = other->vtable;
            if (other_vtable && other_vtable->OnCollided) {
                CollisionResult other_result = result;
                other_result.entity          = self;
                other_vtable->OnCollided(other, other_result);
            }
            if (other->solid) {
                new_position = Vector3Add(*position, Vector3Scale(movement, result.distance));
            }
            else {
                new_position = Vector3Add(*position, movement);
            }
        }
        else {
            new_position = Vector3Add(*position, Vector3Scale(movement, result.distance));
        }
        *position = new_position;
        if (entity_vtable && entity_vtable->OnCollision) entity_vtable->OnCollision(self, result);
    }
    else {
        *position = Vector3Add(*position, movement);
    }
    
    return result;
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
	
	Renderable *renderable = entity->renderables[lod_level];

	if (renderable && renderable->Render) renderable->Render(renderable, (void*)entity);
}


/*
	Private Methods
*/
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
