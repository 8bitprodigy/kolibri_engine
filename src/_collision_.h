#ifndef COLLISION_H
#define COLLISION_H


#include "common.h"


typedef struct CollisionScene CollisionScene;


/* Constructor/Destructor */
void CollisionScene__new( Engine         *engine);
void CollisionScene__free(CollisionScene *scene);

/* Scene management */
void CollisionScene__markRebuild(CollisionScene *scene);

/* Collision detection functions */
CollisionResult CollisionScene__checkAABB(     Entity         *a,     Entity *b);
CollisionResult CollisionScene__checkCollision(CollisionScene *scene, Entity *a, Entity *b);
CollisionResult CollisionScene__raycast(       CollisionScene *scene, Vector3 from, Vector3 to);

/* System updates */
void Collision__update(CollisionScene *scene);


#endif /* COLLISION_H */
