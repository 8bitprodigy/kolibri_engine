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
typedef void            (*SceneRenderCallback)(   Scene *scene, Head    *head);


typedef struct
SceneVTable
{
    SceneDataCallback            Setup;          /* Called on initialization */
    SceneCallback                Enter;          /* Called on entering the engine */
    SceneUpdateCallback          Update;         /* Called once every frame after updating all the entities and before rendering */
    SceneEntityCallback          EntityEnter;    /* Called every time an Entity enters the scene */
    SceneEntityCallback          EntityExit;     /* Called every time an Entity exits the scene */
    SceneCollisionCallback       CheckCollision; /* Called when checking if an entity would collide if moved */
    SceneCollisionCallback       MoveEntity;     /* Called Every time an Entity moves in order to check if it has collided with the scene */
    SceneRaycastCallback         Raycast;        /* Called Every time a raycast is performed in order to check if has collided with the scene */
    SceneRenderCallback          Render;         /* Called once every frame in order to render the scene */
    SceneCallback                Exit;           /* Called upon Scene exiting the engine */
    SceneDataCallback            Free;           /* Called upon freeing the Scene from memory */
}
SceneVTable;

/* Constructor/Desructor */
Scene          *Scene_new(SceneVTable *scene_type, void *data, Engine *engine);
void            Scene_free(Scene      *scene);

/* Setters/Getters */
Engine         *Scene_getEngine(      Scene *scene);
uint            Scene_getEntityCount( Scene *scene);
EntityList     *Scene_getEntityList(  Scene *scene);
void           *Scene_getMapData(     Scene *scene);

/* Public Methods */
void            Scene_enter(          Scene *scene);
void            Scene_update(         Scene *scene, float    delta);
void            Scene_entityEnter(    Scene *scene, Entity  *entity);
void            Scene_entityExit(     Scene *scene, Entity  *entity);
CollisionResult Scene_checkCollision( Scene *scene, Entity  *entity, Vector3 to);
CollisionResult Scene_checkContinuous(Scene *scene, Entity  *entity, Vector3 movement);
CollisionResult Scene_raycast(        Scene *scene, Vector3  from,   Vector3 to);
void            Scene_render(         Scene *scene, Head    *head);
void            Scene_exit(           Scene *scene);


#endif /* SCENE_H */
