# TODO...

## FPS Example
- [ ] Implement shooting
- [ ] Implement enemy AI

## General/Unknown:
- [ ] (engine/head) Fix RenderTexture viewports 
- [ ] (unknown) Fix whatever is causing a segfault on exit
- [x] (`common.h`) Fix not all entities being updated/rendered
	- Forgot to update `MAX_NUM_ENTITIES` in `common.h`

## Collision:
- [ ] Fix the is-on-floor check not working when standing on top of another entity
- [ ] Fix whatever is causing catching when sliding against another collider
- [ ] Fix cylinder-AABB collision
- [ ] Fix tunneling when moving at low tick rates.
- [ ] Finish implementing all collision shape interactions

## Engine:
- [x] Fix whatever prevents being able to re-run the game after quitting to main menu
	- Forgot to re-set `Engine->request_exit` to `false` upon calling `Engine_run()`.
- [x] Fix issue where player intermittently spawns unable to move
	- It was caused by an unititialized `Engine.tick_rate` value, which would hold a random garbage value

## Entities:
- [x] Make use of `rmem.h`'s `MemoryPool` arena to allocate all entities to.
- [x] Re-subordinate `Entity`s to `Scene`s

## Renderer:
- [ ] Implement 2-pass rendering to support transparency.
