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
CollisionResult   Collision__checkAABB(          Entity         *a,     Entity  *b);
CollisionResult   Collision__checkRayAABB(       Vector3         from,  Vector3  to,   Entity  *entity);

Entity          **CollisionScene__queryRegion(   CollisionScene *scene, Vector3  min,  Vector3  max, int *count);
CollisionResult   CollisionScene__checkCollision(CollisionScene *scene, Entity  *a,    Entity  *b);
CollisionResult   CollisionScene__raycast(       CollisionScene *scene, Vector3  from, Vector3  to);

/* System updates */
void Collision__update(CollisionScene *scene);


#endif /* COLLISION_H */
