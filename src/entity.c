#include <raylib.h>
#include <raymath.h>

#include "_engine_.h"
#include "_entity_.h"
#include "_head_.h"
#include "_scene_.h"
#include "common.h"
#define RMEM_IMPLEMENTATION
#include "rmem.h"


#define PUSH_OUT_DISTANCE  0.1f
#define SEPARATION_EPSILON 0.01f


static uint64 Latest_ID = 0;


/******************
	CONSTRUCTOR
******************/
Entity *
Entity_new(const Entity *template, Scene *scene, size_t user_data_size)
{
	if (!scene) return NULL;
	
	EntityNode *node = MemPoolAlloc(
			&scene->entity_pool,
			sizeof(EntityNode) 
				+ user_data_size
		);

	if (!node) {
		ERR_OUT("Failed to allocate memory for EntityNode.");
		return NULL;
	}

	node->next   = node;
	node->prev   = node;
	node->engine = scene->engine;
	node->size   = sizeof(EntityNode) + user_data_size;

	Entity *entity = NODE_TO_ENTITY(node);
	entity->user_data =  NULL;
	*entity           = *template;

	node->unique_ID = Latest_ID++;
	
	Scene__insertEntity(scene, node);
	node->scene  = scene;

	EntityVTable *vtable = entity->vtable;
	if (vtable && vtable->Setup) vtable->Setup(entity);

	node->creation_time = GetTime();
	
	return entity;
}

void
Entity_free(Entity *self)
{
	EntityNode__free(ENTITY_TO_NODE(self));
}


double
Entity_getAge(Entity *self)
{
	EntityNode *node = ENTITY_TO_NODE(self);
	return Engine_getTime(node->engine) - node->creation_time;
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

Engine *
Entity_getEngine(Entity *entity)
{
	return ENTITY_TO_NODE(entity)->engine;
}

Entity *
Entity_getNext(Entity *self)
{
	return &ENTITY_TO_NODE(self)->next->base;
}

Entity *
Entity_getPrev(Entity *self)
{
	return &ENTITY_TO_NODE(self)->prev->base;
}

Scene *
Entity_getScene(Entity *self)
{
	return ENTITY_TO_NODE(self)->scene;
}

uint64
Entity_getUniqueID(Entity *entity)
{
	return ENTITY_TO_NODE(entity)->unique_ID;
}

bool
Entity_isOnFloor(Entity *self)
{
	Scene *scene = Engine_getScene(ENTITY_TO_NODE(self)->engine);
	return Scene_isEntityOnFloor(scene, self);
}


CollisionResult
Entity_move(Entity *self, Vector3 movement)
{
    if (Vector3Equals(movement, V3_ZERO)) return NO_COLLISION;
    
    Scene *scene = Engine_getScene(ENTITY_TO_NODE(self)->engine);
    CollisionResult result = Scene_checkContinuous(scene, self, movement);
    
    // Handle the movement based on collision result
    if (!result.hit) {
        // No collision - move freely
        self->position = Vector3Add(self->position, movement);
    } else {
        // Collision detected - move to safe distance from collision point
        float safe_distance = fmaxf(0.0f, result.distance - SEPARATION_EPSILON);
        Vector3 safe_movement = Vector3Scale(movement, safe_distance);
        self->position = Vector3Add(self->position, safe_movement);
        
        // Trigger collision callbacks
        EntityVTable *vtable = self->vtable;
        if (vtable && vtable->OnCollision) {
            vtable->OnCollision(self, result);
        }
        
        // Notify the other entity if it exists and has a callback
        if (result.entity && result.entity->vtable && result.entity->vtable->OnCollided) {
            CollisionResult other_result = result;
            other_result.entity = self;
            result.entity->vtable->OnCollided(result.entity, other_result);
        }
    }
    
    return result;
}

/* Alternative simpler version without energy loss */
CollisionResult
Entity_moveAndSlide(Entity *entity, Vector3 movement, int max_slides)
{
    if (!max_slides) return Entity_move(entity, movement);
    if (max_slides < 0) max_slides = 3;
    
    CollisionResult result = NO_COLLISION;
    Vector3 remaining = movement;
    
    for (int i = 0; i < max_slides; i++) {
        if (Vector3Length(remaining) <= 0.001f) break;
        
        CollisionResult test = Entity_move(entity, remaining);
        
        if (!test.hit) {
            return test; /* No collision, we're done */
        }
        
        result = test;
        
        /* Calculate remaining movement after collision */
        Vector3 hit_normal = test.normal;
        
        /* Remove the component of remaining movement that goes into the surface */
        float dot = Vector3DotProduct(remaining, hit_normal);
        if (dot < 0) { /* Only remove if moving into surface */
            remaining = Vector3Subtract(remaining, Vector3Scale(hit_normal, dot));
        }
        
        /* Scale remaining movement by how much we didn't move */
        remaining = Vector3Scale(remaining, 1.0f - test.distance);
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
	Entity *entity = NODE_TO_ENTITY(self);
	Scene  *scene  = self->scene;
	
	EntityVTable *vtable = entity->vtable;
	if (vtable && vtable->Free) vtable->Free(entity);
	
	Scene__removeEntity(scene, self);

	MemPoolFree(&scene->entity_pool, self);
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
EntityNode__insert(EntityNode *self, EntityNode *to)
{
	if (!to) return;
	EntityNode 
		*last = to->prev;

	last->next = self;
	to->prev   = self;
	
	self->next = to;
	self->prev = last;
}

void
EntityNode__updateAll(EntityNode *node, float delta)
{
	if (!node) {
		return;
	}
	EntityNode *starting_node = node;
	
	do {
		Entity *entity = NODE_TO_ENTITY(node);
		if (entity->active) {
			EntityVTable *vtable = entity->vtable;
			if (vtable && vtable->Update) vtable->Update(entity, delta);
		}
		node = node->next;
	} while (node != starting_node);
}

void
EntityNode__renderAll(EntityNode *node, float delta)
{
	if (!node) {
		return;
	}
	EntityNode *starting_node = node;
	
	do {
		Entity *entity = NODE_TO_ENTITY(node);
		if (entity->active) {
			EntityVTable *vtable = entity->vtable;
			if (vtable && vtable->Render) vtable->Render(entity, delta);
		}
		node = node->next;
	} while (node != starting_node);
}
