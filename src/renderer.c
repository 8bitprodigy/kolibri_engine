#include "_collision_.h"
#include "_renderer_.h"
#include "_renderer_h"


typedef struct
Renderer
{
	CollisionScene   *potentially_visible_set;
	Engine           *engine;
}
Renderer;


/*
	Constructor/Destructor
*/
Renderer *
Renderer__new(Engine *engine, RendererSettings *settings)
{
	Renderer *renderer = malloc(sizeof(Renderer));

	if (!renderer) {
		ERR_OUT("Failed to allocate memory for Renderer.");
		return NULL;
	}

	renderer->engine                  = engine;
	renderer->potentially_visible_set = CollisionScene_new(NULL);
}


void
Renderer__free(Renderer *renderer)
{
	CollisionScene__free(renderer->potentially_visible_set);
	free(renderer);
}


/*
	Protected Methods
*/
void
Renderer__render(Renderer *renderer, EntityList *entities, Head *head)
{
	if (!entities || entities->count < 1) return;

	RendererSettings *settings = &head->settings;
	CollisionScene   *pvs      = renderer->potentially_visible_set;
	
	/* Clear and populate the Renderer's visible scene */
	CollisionScene__clear(renderer->potentially_visible_set);

	for (int i = 0; i < entities->count; i++) {
		Entity *entity = entities->entities[i];
		if (entity->active && entity->visible) {
			CollisionScene__insertEntity(pvs, entity);
		}
	}

	/* Get entities visible in frustum */
	int      visible_count;
	Entity **visible_entities;

	if (settings->frustum_culling) {
		Camera3D *camera = Head_getCamera(head);
		visible_entities = CollisionScene__queryFrustum(pvs, camera, settings->max_render_distance, &visible_count);
	}
	else {
		/* No culling -- render all entities */
		visible_entities = entities->entities;
		visible_count    = entities->count;
	}

	for (int i = 0; i < visible_count; i++) {
		entity_render(visible_entities[i], head);
	}
}
