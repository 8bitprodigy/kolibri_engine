#ifndef SCENE_H
#define SCENE_H


#include "common.h"


typedef void       (*SceneCallback)(         Scene *scene);
typedef void       (*SceneDataCallback)(     Scene *scene, void    *map_data);
typedef void       (*SceneUpdateCallback)(   Scene *scene, float   *delta);
typedef void       (*SceneEntityCallback)(   Scene *scene, Entity  *entity);
typedef void       (*SceneCollisionCallback)(Scene *scene, Entity  *entity, Vector3 to);
typedef void       (*SceneRaycastCallback)(  Scene *scene, Vector3  from,   Vector3 to);
typedef VisibleSet (*SceneRenderCallback)(   Scene *scene, Head    *head);

typedef struct Scene Scene;
typedef struct
SceneVTable
{
    SceneDataCallback       Setup;
    SceneCallback           Enter;
    SceneUpdateCallback     Update;
    SceneEntityCallback     EntityEnter;
    SceneEntityCallback     EntityLeave;
    SceneCollisionCallback  CheckCollision; 
    SceneRaycastCallback    Raycast; 
    SceneRenderCallback     Render;
    SceneCallback           Exit;
    SceneDataCallback       Free;
}
SceneVTable;

/* Constructor */
Scene          *Scene_new(const SceneVTable *scene_type, void *data, Engine *engine);
/* Destructor */
void            Scene_free(     Scene   *scene);

/* Public Methods */
void            Scene_enter(         Scene *scene);
void            Scene_update(        Scene *scene, float    delta);
CollisionResult Scene_checkCollision(Scene *scene, Entity  *entity, Vector3 to);
CollisionResult Scene_raycast(       Scene *scene, Vector3  from,   Vector3 to);
void            Scene_render(        Scene *scene, Head    *head);
void            Scene_exit(          Scene *scene);


#endif /* SCENE_H */
