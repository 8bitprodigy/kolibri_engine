#ifndef INFINITE_PLANE_SCENE
#define INFINITE_PLANE_SCENE

#include "kolibri.h"

extern SceneVTable scene_Callbacks;

void            testSceneRender(   Scene *scene, Head   *head);
CollisionResult testSceneCollision(Scene *scene, Entity *entity, Vector3 to);

#endif /* INFINITE_PLANE_SCENE */
