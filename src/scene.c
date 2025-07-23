#include "_engine_.h"
#include "_scene_.h"
#include "common.h"


void insertScene(Scene *scene, Scene *to);
void removeScene(Scene *scene);


typedef struct 
Scene
{
    struct Scene
        *prev,
        *next;
    void      *map_data;
    SceneType *vtable;
} 
Scene;


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
    scene->map_data = data;
    scene->vtable   = map_type;

    insertScene(scene, Engine__getScene(engine));

    if (map_type->Setup) map_type->Setup(scene, data);
}
/*
    DESTRUCTOR
*/
void
Scene_free(Scene *scene)
{
    removeScene(scene);
    if (scene->vtable->Free) scene->vtable->Free(scene);
    free(scene);
}


/*
    PRIVATE METHODS
*/
void
insertScene(Scene *scene, Scene *to)
{
	if (!scene || !to) {
		ERR_OUT("insertEntity() received a NULL pointer as argument");
		return;
	}
    
	if (!to->prev || !to->next) {
		ERR_OUT("insertEntity() received improperly initialized EntityNode `to`.");
		return;
	}
    
	Scene *last = to->prev;
    
	last->next = scene;
	to->prev   = scene;
	
	scene->next = to;
	scene->prev = last;
}


void
removeScene(Scene *scene)
{
	EntityNode *scene_1 = scene->prev;
	EntityNode *nscene_2 = scene->next;

	scene_1->next = scene_2;
	scene_2->prev = scene_1;
}


/*
    PUBLIC METHODS
*/
void
Scene_enter(Scene *self)
{
    if (self->vtable->Enter) self->vtable->Enter(self);
}

void
Scene_update(Scene *self, float delta)
{
    if (self->vtable->Enter) self->vtable->Update(self, delta);
}

void
Scene_checkCollision(Scene *self, Entity *entity, Vector to)
{
    if (self->vtable->CheckCollision) self->vtable->CheckCollision(self, entity, to);
}

void
Scene_raycast(Scene *self, Vector from, Vector to)
{
    if (self->vtable->Raycast) self->vtable->Raycast(self, from, to);
}

void
Scene_update(Scene *self, Head *head)
{
    if (self->vtable->Render) self->vtable->Render(self, head);
}

void
Scene_exit(Scene *self)
{
    if (self->vtable->Exit) self->vtable->Exit(self);
}
