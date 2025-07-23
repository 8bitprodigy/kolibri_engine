#include "_engine_.h"
#include "_scene_.h"
#include "common.h"


void insertScene(Scene *scene, Scene *to);
void removeScene(Scene *scene);


/*
    CONSTRUCTOR
*/
Scene *
Scene_new(
    const SceneVTable *map_type, 
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
}
/*
    DESTRUCTOR
*/
void
Scene_free(Scene *scene, Engine *engine)
{
    SceneVTable *vtable = scene->vtable;
    
    if (vtable && vtable->Free) vtable->Free(scene);
    
    Engine__removeScene(scene->engine, scene);
    
    free(scene);
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
    if (vtable && vtable->Enter) vtable->Update(self, delta);
}

void 
Scene_entityEnter(Scene *scene, Entity *entity)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->EntityEnter) vtable->EntityEnter(self, entity);
}

void 
Scene_entityExit(Scene *scene, Entity *entity)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->EntityExit) vtable->EntityExit(self, entity);
}

void
Scene_checkCollision(Scene *self, Entity *entity, Vector to)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->CheckCollision) vtable->CheckCollision(self, entity, to);
}

void
Scene_raycast(Scene *self, Vector from, Vector to)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->Raycast) vtable->Raycast(self, from, to);
}

void
Scene_update(Scene *self, Head *head)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->Render) vtable->Render(self, head);
}

void
Scene_exit(Scene *self)
{
    SceneVTable *vtable = self->vtable;
    if (vtable && vtable->Exit) vtable->Exit(self);
}
