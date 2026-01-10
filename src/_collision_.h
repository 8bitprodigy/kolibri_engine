#ifndef COLLISION_H
#define COLLISION_H


#include "_entity_.h"
#include "common.h"


typedef struct CollisionScene CollisionScene;


/* Constructor/Destructor */
CollisionScene   *CollisionScene__new(           Scene          *scene);
void              CollisionScene__free(          CollisionScene *scene);

/* Scene management */
void              CollisionScene__insertEntity(  CollisionScene *scene, Entity *entity);
void              CollisionScene__clear(         CollisionScene *scene);

/* Collision detection functions */

Entity          **CollisionScene__queryRegion(   CollisionScene *scene, BoundingBox  bbox);
CollisionResult   CollisionScene__checkCollision(CollisionScene *scene, Entity      *entity, Vector3  to);
CollisionResult   CollisionScene__moveEntity(    CollisionScene *scene, Entity      *entity, Vector3  movement);
CollisionResult   CollisionScene__raycast(       CollisionScene *scene, K_Ray        ray);


/* System updates */
void              CollisionScene__update(        CollisionScene *scene);


#endif /* COLLISION_H */
