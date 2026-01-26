#include <raylib.h>
#include <stdbool.h>
#include <string.h>

#include "_collision_.h"
#include "_engine_.h"
#include "_entity_.h"
#include "_scene_.h"
#include "_renderer_.h"
#include "common.h"
#include "dynamicarray.h"


#define HANDLE_SCENE_CALLBACK(scene, method, ...) do{ \
        SceneVTable *vtable = (scene)->vtable; \
        if (vtable && vtable->method) return vtable->method((scene), ##__VA_ARGS__); \
    }while(0)


void insertScene(Scene *scene, Scene *to);
void removeScene(Scene *scene);


/*
    CONSTRUCTOR
*/
Scene *
Scene_new(
    SceneVTable *map_type, 
    void        *info,
    void        *data,
    size_t       data_size, 
    Engine      *engine)
{
    Scene *scene = malloc(sizeof(Scene) + data_size);
    if (!scene) {
        ERR_OUT("Failed to allocate Scene.");
        return NULL;
    }

    scene->prev             = scene;
    scene->next             = scene;
    
    scene->engine           = engine;
    scene->entities         = NULL;
    scene->entity_count     = 0;
	scene->collision_scene  = CollisionScene__new(scene);
    scene->info             = info;
    scene->vtable           = map_type;
    scene->entity_list      = DynamicArray(Entity*, 128);
    
    Engine__insertScene(engine, scene);

    if (data_size) memcpy(scene->data, data, data_size);
    
    if (map_type->Setup) map_type->Setup(scene, scene->data);

    return scene;
} /* Scene_new */
/*
    DESTRUCTOR
*/
void
Scene_free(Scene *scene)
{
    SceneVTable *vtable = scene->vtable; 
    if (vtable && vtable->Free) vtable->Free(scene);
    
    Engine__removeScene(  scene->engine, scene);
	//DestroyMemPool(      &scene->entity_pool);
	EntityNode__freeAll(  scene->entities);
	CollisionScene__free( scene->collision_scene);
    
    free(scene);
} /* Scene_free */


/*
    SETTERS / GETTERS
*/
Engine *
Scene_getEngine(Scene *self)
{
    return self->engine;
}

uint
Scene_getEntityCount(Scene *self)
{
	return self->entity_count;
}

Entity **
Scene_getEntityList(Scene *self)
{/*
	if (!self->dirty_EntityList) {
		return &self->entity_list;
	}

	if (!self->entities || self->entity_count == 0) {
		DynamicArray_clear(self->entity_list.entities);
		self->entity_list.count = 0;
		self->dirty_EntityList = false;
		return &self->entity_list;
	}

	/* Clear and rebuild the dynamic array */
/*
	DynamicArray_clear(self->entity_list.entities);
	
	EntityNode *current = self->entities;
	do {
		Entity *entity = NODE_TO_ENTITY(current);
		DynamicArray_append((void**)&self->entity_list.entities, &entity, 1);
		current = current->next;
	} while (current != self->entities);
	
	self->entity_list.count    = DynamicArray_length(  self->entity_list.entities);
	self->entity_list.capacity = DynamicArray_capacity(self->entity_list.entities);
	self->dirty_EntityList     = false;
*/
	return self->entity_list;
}

void *
Scene_getData(Scene *self)
{
    return &self->data;
}

void *
Scene_getInfo(Scene *self)
{
    return self->info;
}

/*
    PUBLIC METHODS
*/

void
Scene_enter(Scene *self)
{
    SceneVTable *vtable = self->vtable; 
    if (vtable && vtable->Enter) vtable->Enter(self);
}

void
Scene_update(Scene *self, float delta)
{
    CollisionScene__update(     self->collision_scene);
    //EntityNode__updateAll(      self->entities, delta);
    Scene__update(self, delta);
    
    SceneVTable *vtable = self->vtable; 
    if (vtable && vtable->Update) vtable->Update(self, delta);
}

void 
Scene_entityEnter(Scene *self, Entity *entity)
{
    SceneVTable *vtable = self->vtable; 
    if (vtable && vtable->EntityEnter) vtable->EntityEnter(self, entity);
}

void 
Scene_entityExit(Scene *self, Entity *entity)
{
    SceneVTable *vtable = self->vtable; 
    if (vtable && vtable->EntityExit) vtable->EntityExit(self, entity);
}

CollisionResult
Scene_checkCollision(Scene *self, Entity *entity, Vector3 to)
{
    SceneVTable *vtable = self->vtable;
    CollisionResult 
        scene_result  = {0},
        entity_result = {0};
        
    if (vtable && vtable->CheckCollision) {
        scene_result = vtable->CheckCollision(self, entity, to);
    }
    CollisionScene *collision_scene = self->collision_scene;
    if(collision_scene) {
        entity_result = CollisionScene__checkCollision(collision_scene, entity, to);
    }

    if (scene_result.hit && entity_result.hit) {
        return (scene_result.distance <= entity_result.distance) ? scene_result : entity_result;
    }
    else if (scene_result.hit) {
        return scene_result;
    }
    else if (entity_result.hit) {
        return entity_result;
    }
    
    return NO_COLLISION;
}

CollisionResult
Scene_checkContinuous(Scene *self, Entity *entity, Vector3 movement)
{
    SceneVTable     *vtable = self->vtable;
    CollisionResult 
        scene_result  = {0},
        entity_result = {0},
        result;
        
    if (vtable && vtable->MoveEntity) {
        Vector3 to = Vector3Add(entity->position, movement);
        scene_result = vtable->MoveEntity(self, entity, to);
    }
    
    CollisionScene *collision_scene = self->collision_scene;
    if(collision_scene) {
        entity_result = CollisionScene__moveEntity(collision_scene, entity, movement);
    }
    
    if (scene_result.hit && entity_result.hit) {
        result = (scene_result.distance <= entity_result.distance) ? scene_result : entity_result;
    }
    else if (scene_result.hit) {
        result = scene_result;
    }
    else if (entity_result.hit) {
        result = entity_result;
    }
    else {
        result = NO_COLLISION;
    }
    
    return result;
}

CollisionResult
Scene_raycast(Scene *self, Vector3 from, Vector3 to, Entity *ignore)
{
    SceneVTable     *vtable = self->vtable;
    CollisionResult  scene_result;
    if (vtable && vtable->Raycast) scene_result = vtable->Raycast(self, from, to);
    
    Vector3 diff = Vector3Subtract(to, from);
    CollisionResult  entity_result = CollisionScene__raycast(
            self->collision_scene, 
            (K_Ray){
                    .position  = from,
                    .direction = Vector3Normalize(diff),
                    .length    = Vector3Length(diff)
                },
            ignore
        );
        
    if ( scene_result.hit && !entity_result.hit) return scene_result;
    if (!scene_result.hit &&  entity_result.hit) return entity_result;
    if (!scene_result.hit && !entity_result.hit) return NO_COLLISION;

    if (entity_result.distance <= scene_result.distance) return entity_result;
    
    return scene_result;
}

void
Scene_preRender(Scene *self, Head *head)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->PreRender) {
        vtable->PreRender(self, head);
        return;
    }
}

void
Scene_render(Scene *self, Head *head)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->Render) {
        vtable->Render(self, head);
        return;
    }
}

void
Scene_exit(Scene *self)
{
    SceneVTable *vtable = (self)->vtable;
    if (vtable && vtable->Exit) vtable->Exit(self);
}

Entity **
Scene_queryRegion(Scene *scene, BoundingBox  bbox)
{
    CollisionScene *collision_scene = scene->collision_scene;
    
    Entity **result = (Entity**)CollisionScene__queryRegion(collision_scene, bbox);
    
    return result;
}


/*
    Private methods
*/
void
Scene__freeAll(Scene *scene)
{
	Scene *next = scene->next;
	if (next == scene) {
		Scene_free(scene);
		return;
	}
	EntityNode__freeAll(scene->entities);
	Scene_free(scene);
	Scene__freeAll(next);
}

EntityNode *
Scene__getEntities( Scene *self)
{
    return self->entities;
}

void
Scene__insertEntity(Scene *self, EntityNode *node)
{
    if (MAX_NUM_ENTITIES <= self->entity_count) return;
	Entity *entity = NODE_TO_ENTITY(node);
	if (!self->entities) {
		self->entities = node;
	}
	else {
        EntityNode__insert(node, self->entities);
    }
	
	self->entity_count++;
}

void
Scene__removeEntity(Scene *self, EntityNode *node)
{
    if (!self->entity_count) return;
	if (self->entities == node) self->entities = node->next;

	EntityNode__remove(node);
	
	self->entity_count--;
}

void
Scene__render(Scene *self, float delta)
{
	for (int i = DynamicArray_length(self->entity_list) - 1; 0 <= i; i--) {
	    Entity       *entity = self->entity_list[i];
	    
	    if (!entity->visible) continue;
	    
	    EntityVTable *vtable = entity->vtable;
	    
	    if (vtable && vtable->Render) vtable->Render(entity, delta);
	}
}

void
Scene__update(Scene *self, float delta)
{
	for (int i = DynamicArray_length(self->entity_list) - 1; 0 <= i; i--) {
	    Entity     *entity = self->entity_list[i];
	    EntityNode *node   = ENTITY_TO_NODE(entity);

	    if (node->to_delete) {
	        DynamicArray_delete(self->entity_list, i, 1);
	        EntityNode__free(node);
	        continue;
	    }
        if (!entity->active) continue;
        
	    EntityVTable *vtable = entity->vtable;
	    if (vtable && vtable->Update) vtable->Update(entity, delta);
	}
}
