#include "_engine_.h"
#include "_scene_.h"
#include "common.h"


void insertScene(Scene *scene, Scene *to);
void removeScene(Scene *scene);


Scene *
Scene_new(
    const MapType *map_type, 
    void          *data, 
    Engine        *engine)
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
removeEntityNode(Scene *scene)
{
	EntityNode *scene_1 = scene->prev;
	EntityNode *nscene_2 = scene->next;

	scene_1->next = scene_2;
	scene_2->prev = scene_1;
}
