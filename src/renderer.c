#include "_head_.h"
#include "_renderer_.h"
#include "_spatial_hash_.h"
#include <raymath.h>


typedef struct
Renderer
{
	SpatialHash *visibility_hash;
	Engine      *engine;
	bool         dirty;
}
Renderer;


/*
	Constructor/Destructor
*/
Renderer *
Renderer__new(Engine *engine)
{
	Renderer *renderer = malloc(sizeof(Renderer));

	if (!renderer) {
		ERR_OUT("Failed to allocate memory for Renderer.");
		return NULL;
	}

	renderer->engine          = engine;
	renderer->visibility_hash = SpatialHash__new();
	renderer->dirty           = true;

	return renderer;
}


void
Renderer__free(Renderer *renderer)
{
	SpatialHash__free(renderer->visibility_hash);
	free(renderer);
}


/*
	Protected Methods
*/
static inline bool
isSphereBehindPlane(const Plane *plane, Vector3 center, float radius)
{
	float distance = Vector3DotProduct(plane->normal, center) + plane->distance;
	return distance < -radius;
}

bool
isSphereInFrustum(
	Vector3  center, 
	float    radius, 
	Frustum *frustum,
	float    dist_sq,
	float    max_distance
)
{ 
	/* Quick distance check using pre-calculated distance */
    float max_dist_check = max_distance + radius;

    /* Test against frustum planes efficiently */
    for (int i = FRUSTUM_LEFT; i <= FRUSTUM_FAR; i++) {
        const Plane *plane = &frustum->planes[i];
        float distance = Vector3DotProduct(plane->normal, center) + plane->distance;
        if (distance < -radius) {
            return false;
        }
    }

    return true;
}


/* Query for entities visible in camera frustum */
Entity **
Renderer__queryFrustum(
	Renderer *renderer,
	Head     *head,
	float     max_distance,
	int      *visible_entity_count
)
{
    static Entity *frustum_results[QUERY_SIZE];
    *visible_entity_count = 0;

    if (!head) return frustum_results;
    
    Camera3D *camera = Head_getCamera(head);
    Frustum  *frustum = Head_getFrustum(head);
    
    // Pre-calculate values used in loop
    Vector3 camera_pos = camera->position;
    float max_dist_sq = max_distance * max_distance;
    
    // Get spatial hash candidates
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera->target, camera_pos));
    Vector3 frustum_center = Vector3Add(camera_pos, Vector3Scale(forward, max_distance * 0.5f));
    float query_radius = max_distance * 0.5f;
    
    Vector3 min_bounds = {
        frustum_center.x - query_radius,
        frustum_center.y - query_radius,
        frustum_center.z - query_radius
    };
    Vector3 max_bounds = {
        frustum_center.x + query_radius,
        frustum_center.y + query_radius,
        frustum_center.z + query_radius
    };

    int candidate_count;
    Entity **candidates = SpatialHash__queryRegion(
        renderer->visibility_hash,
        (BoundingBox){min_bounds, max_bounds},
        &candidate_count
    );

    // Optimized culling loop
    for (int i = 0; i < candidate_count && *visible_entity_count < QUERY_SIZE; i++) {
        Entity *entity = candidates[i];
        
        if (!entity->visible) continue;

        // Single distance calculation
        float dist_sq = Vector3DistanceSqr(entity->position, camera_pos);
        if (dist_sq > max_dist_sq) continue;
        
        // Fast frustum test with pre-calculated distance
        if (isSphereInFrustum(
                entity->position,
                entity->visibility_radius,
                frustum,
                dist_sq,
                max_distance
            )) {
            frustum_results[*visible_entity_count] = entity;
            (*visible_entity_count)++;
        }
    }

    return frustum_results;
}


void
Renderer__render(Renderer *renderer, EntityList *entities, Head *head)
{
	if (!entities || entities->count < 1) return;

	RendererSettings *settings = &head->settings;
	
	/* Clear and populate the Renderer's visible scene */
	if (renderer->dirty) {
		SpatialHash__clear(renderer->visibility_hash);
		
		for (int i = 0; i < entities->count; i++) {
			Entity *entity = entities->entities[i];
			if (entity->active && entity->visible) {
				Vector3 render_center = entity->position;
				if (0 < entity->lod_count) {
					render_center = Vector3Add(entity->position, entity->renderable_offset);
				}
				SpatialHash__insert(renderer->visibility_hash, entity, render_center, entity->bounds);
			}
		}

		renderer->dirty = false;
	}

	/* Get entities visible in frustum */
	int      visible_count;
	Entity **visible_entities;
	
	if (settings->frustum_culling) {
		visible_entities = Renderer__queryFrustum(renderer, head, settings->max_render_distance, &visible_count);
	}
	else {
		/* No culling -- render all entities */
		visible_entities = entities->entities;
		visible_count    = entities->count;
	}

	//DBG_OUT("Visible count after frustum culling: %d", visible_count);
	
	for (int i = 0; i < visible_count; i++) {
		Entity_render(visible_entities[i], head);
	}
}

void 
Renderer__renderBruteForceFrustum(Renderer *renderer, EntityList *entities, Head *head) 
{
    if (!entities || entities->count < 1) return;
    
    RendererSettings *settings = &head->settings;
    Frustum *frustum = Head_getFrustum(head);
    Camera3D *camera = Head_getCamera(head);
    float max_dist_sq = settings->max_render_distance * settings->max_render_distance;
    int rendered = 0;
    
    // Direct frustum culling on all entities - no spatial hash at all
    for (int i = 0; i < entities->count && rendered < QUERY_SIZE; i++) {
        Entity *entity = entities->entities[i];
        
        if (!entity->active || !entity->visible) continue;
        
        // Distance cull first (cheapest test)
        float dist_sq = Vector3DistanceSqr(entity->position, camera->position);
        if (dist_sq > max_dist_sq) continue;
        
        // Frustum cull
        bool in_frustum = true;
        for (int p = FRUSTUM_LEFT; p <= FRUSTUM_FAR; p++) {
            if (p == FRUSTUM_NEAR) continue; // Skip near plane for outdoor scenes
            float distance = Vector3DotProduct(frustum->planes[p].normal, entity->position) + frustum->planes[p].distance;
            if (distance < -entity->visibility_radius) {
                in_frustum = false;
                break;
            }
        }
        
        if (in_frustum) {
            Entity_render(entity, head);
            rendered++;
        }
    }
}
