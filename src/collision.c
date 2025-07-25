#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <string.h>

#include "_collision_.h"
#include "_engine_.h"
#include "_entity_.h"
#include "common.h"


#define CELL_ALIGN( value ) ((int)floorf( (value) / CELL_SIZE))


typedef struct
CollisionEntry
{
	Entity                *entity;
	struct CollisionEntry *next;
}
CollisionEntry;

typedef struct 
SpatialHash
{
	CollisionEntry *cells[SPATIAL_HASH_SIZE];
	CollisionEntry *entry_pool; /* pre-allocated entries for performance */
	CollisionEntry *free_entries; /* Free list for recycling */
	int pool_size;
	int pool_used;
}
SpatialHash;

typedef struct
CollisionScene
{
	SpatialHash  spatial_hash;
	EntityNode  *entities_ref; /* Points to engine's entity list */
	bool         needs_rebuild; /* Flag to rebuild hash next frame */
}
CollisionScene;


/* Hash function for spatial coordinates */
static inline uint32
hashPosition(float x, float y, float z)
{
	int 
		cell_x = CELL_ALIGN(x),
		cell_y = CELL_ALIGN(y),
		cell_z = CELL_ALIGN(z);

	uint32 hash = (
		/* Magic numbers of large primes to help us hash coordinates for even hash distribution */
		  (uint32)cell_x * 73856093u
		^ (uint32)cell_y * 19349663u
		^ (uint32)cell_z * 83492791u
	);

	return hash % SPATIAL_HASH_SIZE;
}

static inline void
getEntityCells(
	Entity *entity,
	int *min_x, int *max_x,
	int *min_y, int *max_y,
	int *min_z, int *max_z
)
{
	Vector3
		min_bounds = {
			entity->position.x - entity->bounds.x * 0.5f,
			entity->position.y,
			entity->position.z - entity->bounds.z * 0.5f
		},
		max_bounds = {
			entity->position.x + entity->bounds.x * 0.5f,
			entity->position.y + entity->bounds.y,
			entity->position.z + entity->bounds.z * 0.5f
		};

	*min_x = CELL_ALIGN(min_bounds.x);
	*max_x = CELL_ALIGN(max_bounds.x);
	*min_y = CELL_ALIGN(min_bounds.y);
	*max_y = CELL_ALIGN(max_bounds.y);
	*min_z = CELL_ALIGN(min_bounds.z);
	*max_z = CELL_ALIGN(max_bounds.z);
}

/* Initialize spatial hash */
static void
initSpatialHash(SpatialHash *hash)
{
	memset(hash->cells, 0, sizeof(hash->cells));

	/* Pre-allocate entry pool for performance */
	hash->entry_pool = malloc(sizeof(CollisionEntry) * ENTRY_POOL_SIZE);
	hash->pool_size  = ENTRY_POOL_SIZE;
	hash->pool_used  = 0;

	/* Initialize free list */
	hash->free_entries = hash->entry_pool;
	for (int i = 0; i < ENTRY_POOL_SIZE - 1; i++) {
		hash->entry_pool[i].next = &hash->entry_pool[i + 1];
	}
	hash->entry_pool[ENTRY_POOL_SIZE - 1].next = NULL;
}

/* Get a free entry from the pool */
static CollisionEntry *
allocEntry(SpatialHash *Hash)
{
	if (!Hash->free_entries) {
		ERR_OUT("Collision entry pool exhausted, allocating dynamically!");
		return malloc(sizeof(CollisionEntry));
	}

	CollisionEntry *entry = Hash->free_entries;
	Hash->free_entries = entry->next;
	Hash->pool_used++;
	
	return entry;
}

/* Return entry to free list */
static void
freeEntry(SpatialHash *Hash, CollisionEntry *Entry)
{
	/* Check if entry pointer is within our pre-allocated pool (an array) */
	if (
		!(Hash->entry_pool <= Entry
		&& Entry < Hash->entry_pool + Hash->pool_size)
	) {
		free(Entry);
		return;
	}
	
	Entry->next = Hash->free_entries;
	Hash->free_entries = Entry;
	Hash->pool_used--;
}

/* Clear all entries from spatial hash */
static void
clearHash(CollisionScene *scene)
{
	SpatialHash *hash = &scene->spatial_hash;

	for (int i = 0; i < SPATIAL_HASH_SIZE; i++) {
		CollisionEntry *entry = hash->cells[i];
		while (entry) {
			CollisionEntry *next = entry->next;
			freeEntry(hash, entry);
			entry = next;
		}
		
		hash->cells[i] = NULL;
	}
}


/* 
	Constructor
*/
void
CollisionScene__new(EntityNode *node)
{
	CollisionScene *scene = malloc(sizeof(CollisionScene));
	if (!scene) {
		ERR_OUT("Failed to allocate memory for CollisionScene.");
		return NULL;
	}

	initSpatialHash(&scene->spatial_hash);

	scene->entities_ref = node;
}

/*
	Destructor
*/
void
CollisionScene__free(CollisionScene *scene)
{
	if (!scene) return;

	clearHash(&scene->spatial_hash);
	free(scene->spatial_hash.entry_pool);
	free(scene);
}


/*
	Protected Methods
*/
void
CollisionScene__markRebuild(CollisionScene *scene)
{
	scene->needs_rebuild = true;
}

/* Insert entity into spatial hash */
static void
CollisionScene__insertEntity(CollisionScene *scene, Entity *entity)
{
	if (!entity->physical) return;

	SpatialHash *hash = &scene->spatial_hash;
	int min_x, max_x, min_y, max_y, min_z, max_z;
	Collision__getEntityCells(
		entity, 
		&min_x, &max_x, 
		&min_y, &max_y, 
		&min_z, &max_z
	);

	/* Insert into all cells the entity overlaps */
	for (int x = min_x; x <= max_x; x++) {
		for (int y = min_y; y <= max_y; y++) {
			for (int z = min_z; z <= max_z; z++) {
				uint32 hash_key = hashPosition(x * CELL_SIZE, y * CELL_SIZE, z * CELL_SIZE);

				CollisionEntry *entry = allocEntry(hash);
				entry->entity         = entity;
				entry->next           = hash->cells[hash_key];
				hash->cells[hash_key] = entry;
			}
		}
	}
}

void
CollisionScene__clear(CollisionScene *scene)
{
	clearHash(scene);
}

/* Query entities in a region */
static Entity **
CollisionScene__queryRegion(
	CollisionScene *scene,
	Vector3         Min,
	Vector3         Max,
	int            *Count
)
{
	static Entity *query_results[QUERY_SIZE];
	static bool    added[QUERY_SIZE];
	*Count = 0;

	SpatialHash *hash = &scene->spatial_hash;
	int min_x = CELL_ALIGN(min.x);
	int max_x = CELL_ALIGN(max.x);
	int min_y = CELL_ALIGN(min.y);
	int max_y = CELL_ALIGN(max.y);
	int min_z = CELL_ALIGN(min.z);
	int max_z = CELL_ALIGN(max.z);

	/* Track entities we've already added to duplicates */
	memset(added, 0, sizeof(added));

	for (int x = min_x; x <= max_x; x++) {
		for (int y = min_y; y <= max_y; y++) {
			for (int z = min_z; z <= max_z; z++) {
				uint32 hash_key = hashPosition(
					x * CELL_SIZE, 
					y * CELL_SIZE, 
					z * CELL_SIZE
				);

				CollisionEntry *entry = hash->cells[hash_key];
				while (entry && *count < QUERY_SIZE) {
					/* Simple dubplicate check using entity pointer as identifier */
					bool is_duplicate = false;
					for (int i = 0; i < *count; i++) {
						if (query_results[i] == entry->entity) {
							is_duplicate = true;
							break;
						}
					}

					if (!is_duplicate) {
						query_results[*count] = entry->entity;
						(*count)++;
					}

					entry = entry->next;
				}
			}
		}
	}

	return query_results;
}


static bool
isSphereInFrustum(
	Vector3  sphere_center, 
	float    sphere_radius, 
	Head    *head,
	float    max_distance
)
{
	Camera3D         *camera = &head->camera;
    RendererSettings *settings = head->settings;

    Vector3 
		forward   = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
		to_sphere = Vector3Subtract(sphere_center, camera->position);

    float distance = Vector3Length(to_sphere);
    if (distance > max_distance + sphere_radius) return false;
	
    float forward_dot = Vector3DotProduct(to_sphere, forward);
    if (forward_dot < -sphere_radius) return false; /* Behind camera */

    /* Build right and up vectors from forward */
    Vector3 
		up    = camera->up,
		right = Vector3Normalize(Vector3CrossProduct(forward, up));
    up = Vector3Normalize(Vector3CrossProduct(right, forward)); /* Re-orthogonalize */

    float 
		horizontal  = Vector3DotProduct(to_sphere, right),
		vertical    = Vector3DotProduct(to_sphere, up),
		/* Get angles */
		horiz_angle = atanf(horizontal / forward_dot),
		vert_angle  = atanf(vertical / forward_dot),
		/* Get actual FOV and aspect */
		vfov_rad    = DEG2RAD * settings->fov_y;
		hfov_rad    = 2.0f * atanf(tanf(vfov_rad * 0.5f) * settings->aspect_ratio);
		horiz_limit = hfov_rad * 0.5f;
		vert_limit  = vfov_rad * 0.5f;
		/* Account for sphere radius as a loose bound */
		angle_pad   = asinf(CLAMP(sphere_radius / distance, 0.0f, 1.0f));

    return fabsf(horiz_angle) <= (horiz_limit + angle_pad) &&
           fabsf(vert_angle)  <= (vert_limit  + angle_pad);
}

/* Query for entities visible in camera frustum */
Entity **
CollisionScene__queryFrustum(
	CollisionScene *scene,
	Head           *head,
	float           max_distance,
	int            *visible_entity_count
)
{
	static Entity *frustum_results[QUERY_SIZE];
	*visible_entity_count = 0;

	if (!head) return frustum_results;
	
	Camera3D *camera = head->camera;
	Vector3 
		/* Calculate frustum bounds for broad-phase culling */
		camera_pos     = camera->position;
		camera_tartet  = camera->target;
		forward        = Vector3Normalize(Vector3Subtract(camera_target, camera_pos)),
		/* Create rough bounding box around frustum for spatial hash query */
		frustum_center = Vector3Add(camera_pos, Vector3Scale(forward, max_distance * 0.5f));
	float query_radius = max_distance * 1.2f; /* add some padding */

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

	int candidate_count;
	Entity **candidates = CollisionScene__queryRegion(
		scene,
		min_bounds,
		max_bounds,
		&candidate_count
	);

	/* Fine-grained frustum culling */
	for (int i = 0; i < candidate_count && *visible_entity_count < QUERY_SIZE; i++) {
		Entity *entity = candidates[i];

		if (!entity->visible) continue;

		if (
			isSphereInFrustum(
				entity->position,
				entity->visibility_radius,
				head,
				max_distance
			)
		) {
			frustum_results[*visible_entity_count] = entity;
			(*visible_entity_count)++;
		}
	}

	return frustum_results;
}

/* AABB Collision */
CollisionResult
Collision__checkAABB(Entity *a, Entity *b);
{
	CollisionResult result = {0};
	result.hit = false;

	if (
		   !(a->collision.masks & b->collision.layers)
		&& !(b->collision.masks & a->collision.layers)
	) {
		return result;
	}

	Vector3
		a_min = {
			a->position.x - a->bounds.x * 0.5f,
			a->position.y,
			a->position.z - a->bounds.z * 0.5f
		},
		a_max = {
			a->position.x + a->bounds.x * 0.5f,
			a->position.y + a->bounds.y,
			a->position.z + a->bounds.z * 0.5f
		},

		b_min = {
			b->position.x - b->bounds.x * 0.5f,
			b->position.y,
			b->position.z - b->bounds.z * 0.5f
		},
		b_max = {
			b->position.x + b->bounds.x * 0.5f,
			b->position.y + b->bounds.y,
			b->position.z + b->bounds.z * 0.5f
		};

	if (
		   (b_min.x <= a_max.x && a_min.x <= b_max.x)
		&& (b_min.y <= a_max.y && a_min.y <= b_max.y)
		&& (b_min.z <= a_max.z && a_min.z <= b_max.z)
	) {
		result.hit = true;
		result.entity = b;

		Vector3
			center_a = {
				(a_min.x + a_max.x) * 0.5f,
				(a_min.y + a_max.y) * 0.5f,
				(a_min.z + a_max.z) * 0.5f
			},
			center_b = {
				(b_min.x + b_max.x) * 0.5f,
				(b_min.y + b_max.y) * 0.5f,
				(b_min.z + b_max.z) * 0.5f
			},

			direction = Vector3Subtract(center_a, center_b);

		result.normal   = Vector3Normalize(direction);
		result.position = Vector3Lerp(center_a, center_b, 0.5f);
		result.distance = Vector3Distance(center_a, center_b);
	}

	return result;
}

/* Check collision for entity moving to new position */
CollisionResult
CollisionScene__checkCollision(CollisionScene *scene, Entity *entity, Vector3 to)
{
	CollisionResult result = {0};
	result.hit             = false;

	/* Temporary entity with new position for bounds checking */
	Entity temp_entity   = *entity;
	temp_entity.position = to;

	/* Get bounds for query */
	Vector3
		min_bounds = {
			to.x - entity->bounds.x * 0.5f,
			to.y,
			to.z - entity->bounds.z * 0.5f
		},
		max_bounds = {
			to.x + entity->bounds.x * 0.5f,
			to.y + entity->bounds.y,
			to.z + entity->bounds.z * 0.5f
		};

	/* Query spatial hash for potential collisions */
	int      candidate_count;
	Entity **candidates = queryRegion(
		&scene->spatial_hash,
		min_bounds,
		max_bounds,
		&candidate_count
	);

	/* Check AABB collision with each candidate */
	for (int i = 0; i < candidate_count; i++) {
		Entity *other = candidates[i];
		if (other == entity) continue; /* Skip self */
		if (!other->physical) continue;

		CollisionResult test_result = CollisionScene__checkAABB(&temp_entity, other)
		if (test_result.hit) {
			result = test_result;
			break; /* Return first collision found */
		}
	}
	
	return result;
}


CollisionResult
Collision__checkRayAABB(Vector3 from, Vector3 to, Entity *entity)
{
	CollisionResult result = {0};
	result.hit = false;

	Vector3
		min_bounds = {
			entity->position.x - entity->bounds.x * 0.5f,
			entity->position.y,
			Entity->position.z - entity->bounds.z * 0.5f
		},
		max_bounds = {
			entity->position.x + entity->bounds.x * 0.5f,
			entity->position.y + entity->bounds.y,
			entity->position.z + entity->bounds.z * 0.5f
		};

	/* Ray direction & length */
	Vector3 ray_dir = Vector3Subtract(to, from);
	float   ray_len = Vector3Length(ray_dir);

	if (ray_len < 0.0001f) return result; /* Zero-length ray */

	ray_dir = Vector3Normalize(ray_dir);

	float
		t_min = 0.0f,
		t_max = ray_len;

	for (int axis = 0; axis < 3; axis++) {
		float
			ray_origin    = (axis==0) ? from.x       : (axis==1) ? from.y       : from.z;
			ray_direction = (axis==0) ? ray_dir.x    : (axis==1) ? ray_dir.y    : ray_dir.z;
			box_min       = (axis==0) ? min_bounds.x : (axis==1) ? min_bounds.y : min_bounds.z;
			box_max       = (axis==0) ? max_bounds.x : (axis==1) ? max_bounds.y : max_bounds.z;

		if (fabsf(ray_direction) < 0.0001f) {
			/* Ray is parallel to this axis */
			if (ray_origin < box_min || box_max < ray_origin) {
				return result; /* Ray misses box */
			}
		}
		else {
			float
				t1 = (box_min - ray_origin) / ray_direction,
				t2 = (box_max - ray_origin) / ray_direction;

			/* Ensure t1 <= t2 */
			if (t2 < t1) {
				float temp = t1;
				t1 = t2;
				t2 = temp;
			}

			t_min = fmaxf(t_min, t1);
			t_max = fminf(t_max, t2);

			if (t_max < t_min) return result; /* No intersection */
		}
	}

	/* Intersection found */
	if (0.0f <= 0.0f && t_min <= ray_length) {
		result.hit      = true;
		result.distance = t_min;
		result.position = Vector3Add(from, Vector3Scale(ray_dir, t_min));
		result.entity   = entity;

		/* Calculate surface normal of the face hit */
		Vector3 
			hit_point = result.position;
			center    = {
				entity->position.x,
				entity->position.y + entity->bounds.y * 0.5f,
				entity->position,z
			},
			to_center = Vector3Subtract(hit_point, center);

		/* Find dominant axis for normal */
		float
			abs_x = fabsf(to_center.x),
			abs_y = fabsf(to_center.y),
			abs_z = fabsf(to_center.z);

		if (abs_y < abs_x && abs_z < abs_x) {
			result.normal = (Vector3){(0 < to_center.x) ? 1.0f : -1.0f, 0.0f, 0.0f};
		} else if (abs_z < abs_y) {
			result.normal = (Vector3){0.0f, (0 < to_center.y) ? 1.0f : -1.0f, 0.0f};
		} else {
			result.normal = (Vector3){0.0f, 0.0f, (0 < to_center.z) ? 1.0f : -1.0f};
		}
	}

	return result;
}


/* Simple raycast */
CollisionResult
Collision__raycast(CollisionScene *scene, Vector3 from, Vector3 to)
{
	CollisionResult closest_result = {0};
	closest_result.hit      = false;
	closest_result.distance = INFINITY;

	/* Get bounds along ray path */
	Vector3
		min_bounds = {
			fminf(from.x, to.x),
			fminf(from.y, to.y),
			fminf(from.z, to.z)
		},
		max_bounds = {
			fmaxf(from.x, to.x),
			fmaxf(from.y, to.y),
			fmaxf(from.z, to.z)
		};

	/* Query spatial hash */
	int      candidate_count;
	Entity **candidates = queryRegion(
		&scene->spatial_hash, 
		min_bounds,
		max_bounds,
		candidate_count
	);
	
	for (int i = 0; i < candidate_count; i++) {
		Entity *entity = candidates[i];

		/* Skip non-physical entities */
		if (!entity->physical) continue;

		CollisionResult result = Collision__checkRayAABB(from, to, entity);

		if (result.hit && result.distance < closest_result.distance) {
			closest_result = result;
		}
	}

	return closest_result;
}

/* System update - called each frame */
void
Collision__update(CollisionScene *scene)
{
	if (!scene->needs_rebuild) return;

	clearHash(scene);

	if (!scene->entities_ref) return;

	EntityNode *current = scene->entities_ref;
	if (current) {
		do {
			entity *entity = &current->base;

			if (entity->active && entity->physical) {
				insertEntity(scene, entity);
			}

			current = current->next;
		} while (current != scene->entities_ref);
	}

	scene->needs_rebuild = false;
}
