#ifndef INFINITE_PLANE_SCENE
#define INFINITE_PLANE_SCENE

#include "kolibri.h"


#ifndef PATH_PREFIX
	#define PATH_PREFIX "./"
#endif
#ifndef PLANE_SIZE
	#define PLANE_SIZE 256.0f
#endif


extern SceneVTable infinite_Plane_Scene_Callbacks;

void            testSceneRender(   Scene *scene, Head   *head);
CollisionResult testSceneCollision(Scene *scene, Entity *entity, Vector3 to);

#endif /* INFINITE_PLANE_SCENE */
