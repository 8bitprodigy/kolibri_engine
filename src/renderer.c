#include "_head_.h"
#include "_renderer_.h"
#include "_spatialhash_.h"
#include "dynamicarray.h"
#include "scene.h"
/* THIS MUST NECESSARILY COME AFTER ANY <raylib.h> */
#include <raymath.h>


typedef struct
{
    union {
        Entity     *entity;
        Renderable *renderable;
    };
    Vector3 
            position,
            bounds;
    bool    is_entity;
}
RenderableWrapper;

typedef struct
Renderer
{
	SpatialHash *visibility_hash;
	Engine      *engine;
	bool         dirty;

	RenderableWrapper  *wrapper_pool;
	Renderable        **transparent_renderables;
	void              **transparent_render_data; 
	float              *transparent_distances;
}
Renderer;


static void Renderer__sortTransparent(Renderer *renderer);


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

	renderer->engine            = engine;
	renderer->visibility_hash   = SpatialHash_new();
	renderer->dirty             = true;

    renderer->wrapper_pool            = DynamicArray(RenderableWrapper, 512);
	renderer->transparent_renderables = DynamicArray(Renderable*,       256);
	renderer->transparent_render_data = DynamicArray(void*,             256);
	renderer->transparent_distances   = DynamicArray(float,             256);

	if (!renderer->transparent_renderables || !renderer->transparent_distances) {
	    ERR_OUT("Failed to allocate memory for transparent rendering.");
	    SpatialHash_free( renderer->visibility_hash);
	    DynamicArray_free(renderer->wrapper_pool);
	    DynamicArray_free(renderer->transparent_renderables);
	    DynamicArray_free(renderer->transparent_render_data);
	    DynamicArray_free(renderer->transparent_distances);
	    free(renderer);
	    return NULL;
	}

	return renderer;
}


void
Renderer__free(Renderer *renderer)
{
	SpatialHash_free(renderer->visibility_hash);
	DynamicArray_free(renderer->wrapper_pool);
    DynamicArray_free(renderer->transparent_renderables);
	DynamicArray_free(renderer->transparent_render_data);
    DynamicArray_free(renderer->transparent_distances);
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
RenderableWrapper **
Renderer__queryFrustum(
	Renderer *renderer,
	Head     *head,
	float     max_distance,
	int      *visible_count
)
{
    static RenderableWrapper *frustum_results[VIS_QUERY_SIZE];
    *visible_count = 0;

    if (!head) return frustum_results;
    
    Camera3D *camera = Head_getCamera(head);
    Frustum  *frustum = Head_getFrustum(head);
    
    /* Pre-calculate values used in loop */
    Vector3 camera_pos = camera->position;
    float max_dist_sq = max_distance * max_distance;
    
    /* Get spatial hash candidates */
    Vector3 forward = Vector3Normalize(
            Vector3Subtract(
                    camera->target, 
                    camera_pos
                )
        );
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

    int                       candidate_count = VIS_QUERY_SIZE;
    static RenderableWrapper *candidates[VIS_QUERY_SIZE];
    SpatialHash_queryRegion(
        renderer->visibility_hash,
        (BoundingBox){min_bounds, max_bounds},
        (void*)&candidates,
        &candidate_count
    );

    /* Optimized culling loop */
    for (int i = 0; i < candidate_count && *visible_count < VIS_QUERY_SIZE; i++) {
        RenderableWrapper *wrapper = candidates[i];
        
        if (wrapper->is_entity && !wrapper->entity->visible) continue;
        
        /* Single distance calculation */
        float dist_sq = Vector3DistanceSqr(wrapper->position, camera_pos);
        if (dist_sq > max_dist_sq) continue;
        
        /* Fast frustum test with pre-calculated distance */
        if (isSphereInFrustum(
                wrapper->position,
                wrapper->is_entity 
                    ?  wrapper->entity->visibility_radius 
                    : wrapper->bounds.x,
                frustum,
                dist_sq,
                max_distance
            )) {
            frustum_results[*visible_count] = wrapper;
            (*visible_count)++;
        }
    }

    return frustum_results;
}


void
Renderer__render(Renderer *renderer, Head *head)
{
	RendererSettings *settings   = &head->settings;
	Camera3D         *camera     = Head_getCamera(head);
	Vector3           camera_pos = camera->position;
	Scene            *scene      = Engine_getScene(renderer->engine);

    /* Clear everything */
    DynamicArray_clear(renderer->wrapper_pool);
	//DynamicArray_clear(renderer->visibility_hash);
    DynamicArray_clear(renderer->transparent_renderables);
    DynamicArray_clear(renderer->transparent_render_data);
	DynamicArray_clear(renderer->transparent_distances);
	SpatialHash_clear( renderer->visibility_hash);

	/* Step 1: Let scene submit all entities and geometry */
    if (scene) {
        Scene_render(scene, head);
    }

    /* Step 2: Insert stable pointers into spatial hash */
    size_t wrapper_count = DynamicArray_length(renderer->wrapper_pool);
    
    for (size_t i = 0; i < wrapper_count; i++) {
        RenderableWrapper *wrapper       = &renderer->wrapper_pool[i];
        Vector3            render_center = wrapper->position;

        if (wrapper->is_entity && 0 < wrapper->entity->lod_count) {
            render_center = Vector3Add(
                    wrapper->position,
                    wrapper->entity->renderable_offset
                );
        }

        SpatialHash_insert(
                renderer->visibility_hash,
                wrapper,
                render_center,
                wrapper->bounds
            );
    }
    
	/* Step 3: Get items visible in frustum */
	int                 visible_count;
	RenderableWrapper **visible_wrappers = Renderer__queryFrustum(
            renderer,
            head, 
            settings->max_render_distance, 
            &visible_count
        );

    /* PASS 1: Render opaque stuff, collect transparent */
	for (int i = 0; i < visible_count; i++) {
	    RenderableWrapper *wrapper = visible_wrappers[i];

        if (wrapper->is_entity && !wrapper->entity->visible) continue;

        Renderable *renderable  = NULL;
        void       *render_data = NULL;
        Vector3     render_pos  = wrapper->position;

        if (wrapper->is_entity) {
            Entity *entity = wrapper->entity;
            renderable     = Entity_getLODRenderable(entity, camera_pos);
            render_data    = entity;
            render_pos     = entity->position;
        }
        else {
            renderable  = wrapper->renderable;
            render_data = renderable->data;
        }

        if (!renderable) continue;

        if (renderable->transparent) {
            float dist = Vector3Distance(render_pos, camera_pos);
            DynamicArray_add((void**)&renderer->transparent_renderables, &renderable);
            DynamicArray_add((void**)&renderer->transparent_distances,   &dist);
            DBG_OUT("Before append: render_data=%p, &render_data=%p", render_data, &render_data);
            DBG_OUT("Array header: length=%zu, capacity=%zu, datum_size=%zu\n", 
                    DynamicArray_length(renderer->transparent_render_data),
                    DynamicArray_capacity(renderer->transparent_render_data),
                    DynamicArray_datumSize(renderer->transparent_render_data)
                );
            DynamicArray_add(renderer->transparent_render_data,          &render_data);
        }
        else if (renderable->Render) {
            renderable->Render(renderable, render_data, camera);
        }
	}

	/* PASS 2: Sort and render transparent */
	size_t transparent_count = DynamicArray_length(renderer->transparent_renderables);
	if (transparent_count < 0) return;

    Renderer__sortTransparent(renderer);

    //rlDisableDepthMask();
    
	for (size_t i = 0; i < transparent_count; i++) {
	    Renderable *renderable  = renderer->transparent_renderables[i];
        void       *render_data = renderer->transparent_render_data[i];
	    if (renderable->Render) {
	        renderable->Render(renderable, render_data, camera);
	    }
	}

	//rlEnableDepthMask();
}

static void
swap_transparent(Renderer *renderer, int i, int j)
{
    float temp_dist = renderer->transparent_distances[i];
    renderer->transparent_distances[i] = renderer->transparent_distances[j];
    renderer->transparent_distances[j] = temp_dist;

    Renderable *temp_rend = renderer->transparent_renderables[i];
    renderer->transparent_renderables[i] = renderer->transparent_renderables[j];
    renderer->transparent_renderables[j] = temp_rend;
}

static int
partition_transparent(Renderer *renderer, int low, int high)
{
    float pivot = renderer->transparent_distances[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (renderer->transparent_distances[j] > pivot) {
            i++;
            swap_transparent(renderer, i, j);
        }
    }

    swap_transparent(renderer, ++i, high);
    return i;
}

static void
quicksort_transparent(Renderer *renderer, int low, int high)
{
    if (low < high) {
        int pi = partition_transparent(renderer, low, high);
        quicksort_transparent(renderer, low, pi - 1);
        quicksort_transparent(renderer, pi + 1, high);
    }
}

static void
Renderer__sortTransparent(Renderer *renderer)
{
    size_t count = DynamicArray_length(renderer->transparent_renderables);
    if (count <= 1) return;

    quicksort_transparent(renderer, 0, count - 1);
}


void 
Renderer_submitEntity(Renderer *renderer, Entity *entity) {
    RenderableWrapper wrapper;
    wrapper.entity    = entity;
    wrapper.position  = entity->position;
    wrapper.bounds    = entity->bounds;
    wrapper.is_entity = true;

    size_t index = DynamicArray_length(renderer->wrapper_pool);
    DynamicArray_add((void**)&renderer->wrapper_pool, &wrapper);
    SpatialHash_insert(
            renderer->visibility_hash, 
            &renderer->wrapper_pool[index], 
            entity->position, 
            entity->bounds
        );
}

void Renderer_submitGeometry(
    Renderer *renderer, 
    Renderable *renderable, 
    Vector3 position, 
    Vector3 bounds
)
{
    RenderableWrapper wrapper;
    wrapper.renderable = renderable;
    wrapper.position   = position;
    wrapper.bounds     = bounds;
    wrapper.is_entity  = false;

    size_t index = DynamicArray_length(renderer->wrapper_pool);
    DynamicArray_add((void**)&renderer->wrapper_pool, &wrapper);
    SpatialHash_insert(
            renderer->visibility_hash,
            &renderer->wrapper_pool[index],
            position,
            bounds
        );
}
