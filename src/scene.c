#include "_collision_.h"
#include "_engine_.h"
#include "_scene_.h"
#include "_renderer_.h"
#include "common.h"
#include <raylib.h>


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
    
    scene->engine   = engine;
    scene->map_data = data;
    scene->vtable   = map_type;

    Engine__insertScene(engine, scene);

    if (map_type->Setup) map_type->Setup(scene, data);

    return scene;
}
/*
    DESTRUCTOR
*/
void
Scene_free(Scene *scene)
{
    SceneVTable *vtable = scene->vtable;
    
    if (vtable && vtable->Free) vtable->Free(scene, scene->map_data);
    
    Engine__removeScene(scene->engine, scene);
    
    free(scene);
}

void
Scene__freeAll(Scene *scene)
{
	Scene *next = scene->next;
	if (next == scene) {
		Scene_free(scene);
		return;
	}
	
	Scene_free(scene);
	Scene__freeAll(next);
}


/*
    SETTERS / GETTERS
*/
Engine *
Scene_getEngine(Scene *self)
{
    return self->engine;
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
    CollisionScene *collision_scene = Engine__getCollisionScene(self->engine);
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
    CollisionScene *collision_scene = Engine__getCollisionScene(self->engine);
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

CollisionResult
Scene_moveEntity(Scene *scene, Entity *entity, Vector3 to)
{
    CollisionResult result;
    Vector3
        *position = &entity->position;
    result = Scene_checkContinuous(scene, entity, to);

    if (result.hit) {
        EntityVTable *entity_vtable = entity->vtable;
        Entity *other               = result.entity;
        Vector3 new_position;
        if (other) {
            EntityVTable *other_vtable   = other->vtable;
            if (other_vtable && other_vtable->OnCollided) {
                CollisionResult other_result = result;
                other_result.entity          = entity;
                other_vtable->OnCollided(other, other_result);
            }
            if (other->solid) {
                new_position = Vector3Add(*position, Vector3Scale(to, result.distance));
            }
            else {
                new_position = Vector3Add(*position, to);
            }
        }
        else {
            new_position = Vector3Add(*position, Vector3Scale(to, result.distance));
        }
        *position = new_position;
        if (entity_vtable && entity_vtable->OnCollision) entity_vtable->OnCollision(entity, result);
    }
    else {
        *position = Vector3Add(*position, to);
    }
    
    return result;
}

CollisionResult
Scene_raycast(Scene *self, Vector3 from, Vector3 to)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->Raycast) return vtable->Raycast(self, from, to);
    return NO_COLLISION;
}

void
Scene_render(Scene *self, Head *head)
{
    SceneVTable *vtable    = self->vtable;
    EntityList entity_list = {0};
    if (vtable && vtable->Render) entity_list = vtable->Render(self, head);
    else {
        EntityList *all_entities = Engine_getEntityList(self->engine);
        if (all_entities) {
            entity_list = *all_entities;
        }
    }
    Renderer__render(Engine__getRenderer(self->engine), &entity_list, head);
}

void
Scene_exit(Scene *self)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->Exit) vtable->Exit(self);
}
