#include <raylib.h>
#include <stdbool.h>

#include "_engine_.h"
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
    void              *data, 
    Engine            *engine)
{
    Scene *scene = malloc(sizeof(Scene));
    if (!scene) {
        ERR_OUT("Failed to allocate Scene.");
        return NULL;
    }

    scene->prev     = scene;
    scene->next     = scene;
    
    scene->engine           = engine;
    scene->entities         = NULL;
	scene->collision_scene  = CollisionScene__new(scene);
    scene->map_data         = data;
    scene->vtable           = map_type;

    scene->dirty_EntityList     = true;
    scene->entity_list.entities = DynamicArray_new(sizeof(Entity*), 128);
    scene->entity_list.count    = 0;
    scene->entity_list.capacity = 0;

    Engine__insertScene(engine, scene);

    if (map_type->Setup) map_type->Setup(scene, data);

    return scene;
} /* Scene_new */
/*
    DESTRUCTOR
*/
void
Scene_free(Scene *scene)
{
    SceneVTable *vtable = scene->vtable; 
    if (vtable && vtable->Free) vtable->Free(scene, scene->map_data);
    
    Engine__removeScene( scene->engine, scene);
    EntityNode__freeAll( scene->entities);
	CollisionScene__free(scene->collision_scene);
    
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

EntityList *
Scene_getEntityList(Scene *self)
{
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
	return &self->entity_list;
}

void *
Scene_getMapData(Scene *self)
{
    return self->map_data;
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
    EntityNode__updateAll(      self->entities, delta);
    CollisionScene__markRebuild(self->collision_scene);
    CollisionScene__update(     self->collision_scene);
    
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
Scene_checkContinuous(Scene *self, Entity *entity, Vector3 to)
{
    SceneVTable     *vtable = self->vtable;
    CollisionResult 
        scene_result  = {0},
        entity_result = {0},
        result;
        
    if (vtable && vtable->MoveEntity) {
        scene_result = vtable->MoveEntity(self, entity, to);
    }
    CollisionScene *collision_scene = self->collision_scene;
    if(collision_scene) {
        entity_result = CollisionScene__moveEntity(collision_scene, entity, to);
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

bool
Scene_isEntityOnFloor(Scene *self, Entity  *entity)
{
    bool scene_on_floor = false;
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->EntityOnFloor) 
        scene_on_floor = vtable->EntityOnFloor(self, entity);

    CollisionResult collision = {0};
    collision.hit = false;
    collision = Scene_checkContinuous(
            self, 
            entity, 
            Vector3Add(entity->position, (Vector3){0.0f, -0.001f, 0.0f})
        ); 
        
    return scene_on_floor || collision.hit;
}

CollisionResult
Scene_raycast(Scene *self, Vector3 from, Vector3 to)
{
    HANDLE_SCENE_CALLBACK(self, Raycast, from, to);
    return NO_COLLISION;
}

void
Scene_render(Scene *self, Head *head)
{
    SceneVTable *vtable    = self->vtable;
    EntityList entity_list = {0};
    if (vtable && vtable->Render) entity_list = vtable->Render(self, head);
    else {
        EntityList *all_entities = Scene_getEntityList(self);
        if (all_entities) {
            entity_list = *all_entities;
        }
    }
    Renderer__render(Engine__getRenderer(self->engine), &entity_list, head);
}

void
Scene_exit(Scene *self)
{
    HANDLE_SCENE_CALLBACK(self, Exit);
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
	if (!self->entities) {
		self->entities = node;
		self->entity_count++;
		return;
	}
	EntityNode 
		*to   = self->entities,
		*last = to->prev;

	last->next = node;
	to->prev   = node;
	
	node->next = to;
	node->prev = last;
	
	self->entity_count++;
}

void
Scene__removeEntity(Scene *self, EntityNode *node)
{
    if (!self->entity_count) return;
	EntityNode *node_1 = node->prev;
	EntityNode *node_2 = node->next;

	node_1->next = node_2;
	node_2->prev = node_1;

	if (self->entities == node) self->entities = node_2;
	
	self->entity_count--;
}
