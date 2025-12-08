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


static void
setCollisionState(EntityNode *node, CollisionResult *collision)
{
	if (!collision->hit) return;
	float 
		dot_up              = Vector3DotProduct(collision->normal, V3_UP),
		floor_dot_threshold = cosf(node->base.floor_max_angle);
	
	if      (floor_dot_threshold  < dot_up) node->on_floor   = true;
	else if (dot_up < -floor_dot_threshold) node->on_ceiling = true;
	else                                    node->on_wall    = true;
}


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
	node->on_floor   = false;
	node->on_wall    = false;
	node->on_ceiling = false;

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

Renderable *
Entity_getLODRenderable(Entity *entity,  Vector3 camera_position)
{
	float distance = Vector3Distance(entity->position, camera_position);
	
	int lod_level  = -1;
	for (int i = 0; i < entity->lod_count; i++) {
		if (entity->lod_distances[i] < distance) continue;
		lod_level = i;
		break;
	}
	if (lod_level < 0) return NULL; /* Distance is greater than max renderable LOD level, so don't render it. */
	
	return entity->renderables[lod_level];
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
	return ENTITY_TO_NODE(self)->on_floor;
}


CollisionResult
move(Entity *self, Vector3 movement)
{
    if (Vector3Equals(movement, V3_ZERO)) return NO_COLLISION;
    
    Scene *scene = ENTITY_TO_NODE(self)->scene;
    CollisionResult result = Scene_checkContinuous(scene, self, movement);
    
    if (!result.hit) {
        self->position = Vector3Add(self->position, movement);
    } else {
        // Use the position from collision system
        self->position = result.position;
        
        // Trigger collision callbacks
        EntityVTable *vtable = self->vtable;
        if (vtable && vtable->OnCollision) {
            vtable->OnCollision(self, result);
        }
        
        if (result.entity && result.entity->vtable && result.entity->vtable->OnCollided) {
            CollisionResult other_result = result;
            other_result.entity = self;
            result.entity->vtable->OnCollided(result.entity, other_result);
        }
    }
    
    return result;
}


CollisionResult
Entity_move(Entity *self, Vector3 movement)
{
	if (Vector3Length(movement) <= EPSILON) return NO_COLLISION;
    EntityNode *node = ENTITY_TO_NODE(self);
	node->on_floor   = false;
	node->on_wall    = false;
	node->on_ceiling = false;

	CollisionResult result = move(self, movement);
	
	setCollisionState(ENTITY_TO_NODE(self), &result);

	return result;
}

/* Alternative simpler version without energy loss */
CollisionResult
Entity_moveAndSlide(Entity *self, Vector3 movement)
{
	if (Vector3Length(movement) <= EPSILON) return NO_COLLISION;
    if (!self->max_slides) return move(self, movement);
    if (self->max_slides < 0) self->max_slides = 3;
    
    EntityNode *node = ENTITY_TO_NODE(self);
	node->on_floor   = false;
	node->on_wall    = false;
	node->on_ceiling = false;
    
    CollisionResult result = NO_COLLISION;
    Vector3 remaining = movement;
    
    for (int i = 0; i < self->max_slides; i++) {
        if (Vector3Length(remaining) <= EPSILON) break;
        
        CollisionResult test = move(self, remaining);
        
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
        
		setCollisionState(node, &result);
		
        /* Scale remaining movement by how much we didn't move */
		float move_len = Vector3Length(remaining);
		float remaining_len = fmaxf(0.0f, move_len - test.distance);
		if (move_len > 0.0001f)
			remaining = Vector3Scale(remaining, remaining_len / move_len);
    }
    return result;
}
/*
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
	/*
	Renderable *renderable = entity->renderables[lod_level];

	if (renderable && renderable->Render) renderable->Render(renderable, (void*)entity);
}
*/

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
