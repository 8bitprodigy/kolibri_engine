# TODO...

## FPS Example
- [X] Implement shooting
	- Projectiles now spawn, but do not yet move.
	- Projectiles now move and disappear after a set amount of time.
	- Needs to have them either despawn or be part of a pool that recycles them.
- [ ] Implement enemy AI

## General/Unknown:
- [ ] (engine/head) Fix RenderTexture viewports 
- [x] (`common.h`) Fix not all entities being updated/rendered
	- Forgot to update `MAX_NUM_ENTITIES` in `common.h`

## Collision:
- [x] Fix the is-on-floor check not working when standing on top of another entity
	- Reimplemented is-on-floor -- should be more accurate now.
	- Unfortunately introduced some major bugs with collision, causing teleportation and application lock-up.
	- Working well enough for the most part, Can sometimes fall through entities when moving back into the entity after starting to fall off.
- [ ] Fix OnCollision not notifying all entities collided with
- [ ] Fix collisions with non-solid entities so colliding with them does not hinder movement.
- [ ] Fix whatever is causing catching when sliding against another collider
- [ ] Fix edge cases wherein an entity can fall into another.
- [ ] Fix cylinder-AABB collision
- [ ] Make generic collision check functions public
- [ ] Finish implementing all collision shape interactions
- [ ] Fix tunneling when moving at low tick rates
- [x] Leverage `ray_collision_2d.h` where needed

## Engine:
- [x] Fix whatever prevents being able to re-run the game after quitting to main menu
	- Forgot to re-set `Engine->request_exit` to `false` upon calling `Engine_run()`.
- [x] Fix issue where player intermittently spawns unable to move
	- It was caused by an unititialized `Engine.tick_rate` value, which would hold a random garbage value

## Entities:
- [x] Make use of `rmem.h`'s `MemoryPool` arena to allocate all entities to.
- [x] Re-subordinate `Entity`s to `Scene`s

## Renderer:
- [ ] Implement 2-pass rendering to support renderables with transparency.
