#ifndef COLLISION_H
#define COLLISION_H


typedef struct
CollisionScene
{
	
}
CollisionScene;


bool Collision__checkAABB(Entity *a, Entity *b);
void Collision__resolveAll(void);

#endif /* COLLISION_H */
