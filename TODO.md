# TODO...

## General/Unknown:
- [ ] Fix whatever prevents being able to re-run the game after quitting to main menu
- [ ] (engine/head) Fix RenderTexture viewports 

## Collision:
- [ ] Finish implementing all collision shape interactions
- [ ] Fix cylinder-AABB collision
- [ ] Fix whatever is causing catching when sliding against another collider
- [ ] Fix tunneling when moving at low tick rates.

## Engine:
- [x] Fix issue where player intermittently spawns unable to move
	- It was caused by an unititialized `Engine.tick_rate` value, which would hold a random garbage value

## Renderer:
- [ ] Implement 2-pass rendering to support transparency.
