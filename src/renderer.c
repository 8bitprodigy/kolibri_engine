#include "_collision_.h"
#include "_head_.h"
#include "_renderer_.h"
#include "_spatial_hash_.h"


typedef struct
Renderer
{
	SpatialHash *visibility_hash;
	Engine      *engine;
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
bool
isSphereInFrustum(
	Vector3  sphere_center, 
	float    sphere_radius, 
	Head    *head,
	float    max_distance
)
{ 

	Camera3D         *camera   = &head->camera;
    RendererSettings *settings = &head->settings;

    Vector3 
		forward   = Vector3Normalize(Vector3Subtract(camera->target, camera->position)),
		to_sphere = Vector3Subtract(sphere_center, camera->position);

    float distance = Vector3Length(to_sphere);

    //DBG_OUT("\tSphere at (%.2f, %.2f, %.2f), radius: %.2f", sphere_center.x, sphere_center.y, sphere_center.z, sphere_radius);
    //DBG_OUT("\tDistance to camera: %.2f, max_distance: %.2f", distance, max_distance);
    
    if (distance > max_distance + sphere_radius) {
        //DBG_OUT("  FAIL: Too far from camera");
    	return false;
    }
	
    float forward_dot = Vector3DotProduct(to_sphere, forward);
    //DBG_OUT("\tForward dot product: %.2f", forward_dot);
    
    if (forward_dot < -sphere_radius) {
       // DBG_OUT("\tFAIL: Behind camera");
    	return false; /* Behind camera */
    }

    /* Build right and up vectors from forward */ 
    Vector3 
		up    = camera->up,
		right = Vector3Normalize(Vector3CrossProduct(forward, up));
		up    = Vector3Normalize(Vector3CrossProduct(right, forward)); /* Re-orthogonalize */

	RenderTexture *viewport = Head_getViewport(head);
    float 
		horizontal  = Vector3DotProduct(to_sphere, right),
		vertical    = Vector3DotProduct(to_sphere, up),
		/* Get angles from camera forward direction */
		horiz_angle = atanf(horizontal / fmaxf(fabsf(forward_dot), 0.001f)),
		vert_angle  = atanf(vertical / fmaxf(fabsf(forward_dot), 0.001f)),
		/* Get actual FOV and aspect */
		vfov_rad    = DEG2RAD * camera->fovy,
		aspect      = (float)viewport->texture.width / (float)viewport->texture.height,
		hfov_rad    = 2.0f * atanf(tanf(vfov_rad * 0.5f) * aspect),
		horiz_limit = hfov_rad * 0.5f,
		vert_limit  = vfov_rad * 0.5f,
		/* Account for sphere radius as a loose bound */
		angle_pad   = asinf(CLAMP(sphere_radius / distance, 0.0f, 1.0f));

    //DBG_OUT("\tHorizontal angle: %.2f, limit: %.2f", horiz_angle, horiz_limit + angle_pad);
    //DBG_OUT("\tVertical angle: %.2f, limit: %.2f", vert_angle, vert_limit + angle_pad);

	bool in_frustum =  fabsf(horiz_angle) <= (horiz_limit + angle_pad)
					&& fabsf(vert_angle)  <= (vert_limit  + angle_pad);
	
    //DBG_OUT("  Result: %s", in_frustum ? "PASS" : "FAIL");
    
    return in_frustum;
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
	
	Camera3D *camera = &head->camera;
	Vector3 
		/* Calculate frustum bounds for broad-phase culling */
		camera_pos     = camera->position,
		camera_target  = camera->target,
		forward        = Vector3Normalize(Vector3Subtract(camera_target, camera_pos)),
		/* Create rough bounding box around frustum for spatial hash query */
		frustum_center = Vector3Add(camera_pos, Vector3Scale(forward, max_distance * 0.5f));
	float query_radius = max_distance * 0.5f; /* add some padding */
	
	//DBG_OUT("Camera pos: (%.2f, %.2f, %.2f)", camera_pos.x, camera_pos.y, camera_pos.z);
	//DBG_OUT("Camera target: (%.2f, %.2f, %.2f)", camera_target.x, camera_target.y, camera_target.z);
	//DBG_OUT("Max distance: %.2f", max_distance);
	
	Vector3
		min_bounds = {
			frustum_center.x - query_radius,
			frustum_center.y - query_radius,
			frustum_center.z - query_radius
		},
		max_bounds = {
			frustum_center.x + query_radius,
			frustum_center.y + query_radius,
			frustum_center.z + query_radius
		};
/*
	EntityList *all_entities = Engine_getEntityList(renderer->engine);
	Entity **candidates = all_entities->entities;
*/
	int candidate_count;
	Entity **candidates = SpatialHash__queryRegion(
		renderer->visibility_hash,
		min_bounds,
		max_bounds,
		&candidate_count
	);

	
	DBG_OUT("Candidate count from spatial hash: %d", candidate_count);

	/* Fine-grained frustum culling */
	for (int i = 0; i < candidate_count && *visible_entity_count < QUERY_SIZE; i++) {
		Entity *entity = candidates[i];
		
		if (!entity->visible) {
			//DBG_OUT("Entity %d: not visible", i);
			continue;
		}

		float dist_sq = Vector3DistanceSqr(entity->position, camera->position);
		if (dist_sq > max_distance * max_distance) continue;
		
		//DBG_OUT("Entity %d pos: (%.2f, %.2f, %.2f), visibility_radius: %.2f", i, entity->position.x, entity->position.y, entity->position.z, entity->visibility_radius);

		if (
			isSphereInFrustum(
				entity->position,
				entity->visibility_radius,
				head,
				max_distance
			)
		) {
			//DBG_OUT("Entity %d: PASSED frustum test", i);
			frustum_results[*visible_entity_count] = entity;
			(*visible_entity_count)++;
		}
		DBG_EXPR(
			else {
				//DBG_OUT("Entity %d: FAILED frustum test", i);
			}
		)
	}

	return frustum_results;
}


void
Renderer__render(Renderer *renderer, EntityList *entities, Head *head)
{
	if (!entities || entities->count < 1) return;

	RendererSettings *settings = &head->settings;
	
	/* Clear and populate the Renderer's visible scene */
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

	/* Get entities visible in frustum */
	int      visible_count;
	Entity **visible_entities;
	
	//DBG_OUT("Entities count: %d", entities->count);
	
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
