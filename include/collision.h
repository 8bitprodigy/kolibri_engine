#ifndef COLLISION_H
#define COLLISION_H


#include "common.h"


CollisionResult   Collision_checkAABB(          Entity         *a,     Entity  *b);
CollisionResult   Collision_checkRayAABB(       K_Ray           ray,   Entity  *entity);
CollisionResult   Collision_checkDiscreet(      Entity         *a,     Entity   *b);
CollisionResult   Collision_checkContinuous(    Entity         *a,     Entity   *b,     Vector3  movement);

#endif /* COLLISION_H */
