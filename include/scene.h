#ifndef SCENE_H
#define SCENE_H


#include "common.h"


typedef struct Scene Scene;


typedef void            (*SceneCallback)(         Scene *scene);
typedef void            (*SceneDataCallback)(     Scene *scene, void    *map_data);
typedef void            (*SceneUpdateCallback)(   Scene *scene, float    delta);
typedef void            (*SceneEntityCallback)(   Scene *scene, Entity  *entity);
typedef CollisionResult (*SceneCollisionCallback)(Scene *scene, Entity  *entity, Vector3 to);
typedef CollisionResult (*SceneRaycastCallback)(  Scene *scene, Vector3  from,   Vector3 to);
typedef EntityList      (*SceneRenderCallback)(   Scene *scene, Head    *head);


typedef struct
SceneVTable
{
    SceneDataCallback       Setup;
    SceneCallback           Enter;
    SceneUpdateCallback     Update;
    SceneEntityCallback     EntityEnter;
    SceneEntityCallback     EntityExit;
    SceneCollisionCallback  CheckCollision; 
    SceneRaycastCallback    Raycast; 
    SceneRenderCallback     Render;
    SceneCallback           Exit;
    SceneDataCallback       Free;
}
SceneVTable;

/* Constructor */
Scene          *Scene_new(SceneVTable *scene_type, void *data, Engine *engine);
/* Destructor */
void            Scene_free(     Scene       *scene);

/* Setters/Getters */
Engine         *Scene_getEngine(     Scene *scene);
void           *Scene_getMapData(    Scene *scene);

/* Public Methods */
void            Scene_enter(         Scene *scene);
void            Scene_update(        Scene *scene, float    delta);
void            Scene_entityEnter(   Scene *scene, Entity  *entity);
void            Scene_entityExit(    Scene *scene, Entity  *entity);
CollisionResult Scene_checkCollision(Scene *scene, Entity  *entity, Vector3 to);
CollisionResult Scene_raycast(       Scene *scene, Vector3  from,   Vector3 to);
void            Scene_render(        Scene *scene, Head    *head);
void            Scene_exit(          Scene *scene);


#endif /* SCENE_H */
