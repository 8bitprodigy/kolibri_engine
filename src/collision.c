#include <math.h>
#include <raylib.h>
#include <raymath.h>
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

/* Insert entity into spatial hash */
static void
insertEntity(CollisionScene *scene, Entity *entity)
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

/* Query entities in a region */
static Entity **
queryRegion(
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


/* 
	Constructor
*/
void
CollisionScene__new(Engine *engine)
{
	CollisionScene *scene = malloc(sizeof(CollisionScene));
	if (!scene) {
		ERR_OUT("Failed to allocate CollisionScene.");
		return NULL;
	}

	initSpatialHash(&scene->spatial_hash);

	scene->entities_ref = Engine__getEntities(engine);
	
	Engine__setCollisionScene(engine, scene);
}

void
CollisionScene__free(CollisionScene *scene)
{
	if (!scene) return;

	clearHash(&scene->spatial_hash);
	free(scene->spatial_hash.entry_pool);
	free(scene);
}

/* Add entity to scene */
void
CollisionScene__markRebuild(CollisionScene *scene)
{
	scene->needs_rebuild = true;
}

/* AABB Collision */
CollisionResult
CollisionScene__checkAABB(Entity *a, Entity *b);
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
Collision__checkCollision(CollisionScene *scene, Entity *entity, Vector3 to)
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

		if (CollisionScene__checkAABB(&temp_entity, other)) {
			result.hit      = true;
			result.entity   = other;
			result.position = to;

			/* Calculate simple collision normal (toward entity center) */
			Vector3 direcction = Vector3Subtract(entity->positon, other->position);
			result.normal      = Vector3Normalize(direction);
			result.distance    = Vector3Distance(entity->position, to);

			break; /* Return first collision found */
		}
	}
	
	return result;
}

/* Simple raycast */
CollisionResult
Collision__raycast(CollisionScene *scene, Vector3 from, Vector3 to)
{
	CollisionResult result = {0};
	result.hit             = false;

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

	float closest_distance = INFINITY;

	for (int i = 0; i < candidate_count; i++) {
		Entity *entity = candidates[i];
		if (!entity->physical) continue;

		/* Simple ray-AABB intersection test */
		Vector3
			entity_min = {
				entity->position.x - entity->bounds.x * 0.5f,
				entity->position.y,
				entity->position.z - entity->bounds.z * 0.5f
			},
			entity_max = {
				entity->position.x + entity->bounds.x * 0.5f,
				entity->position.y + entity->bounds.y,
				entity->position.z + entity->bounds.z * 0.5f
			};

		/*
			TODO: Implement proper ray-AABB intersection
			for now, just check if ray endpoints are near entity
		*/
		float distance = Vector3Distance(from, entity->position);
		if (distance < closest_distance) {
			result.hit       = true;
			result.entity    = entity;
			result.positon   = entity->position;
			result.distance  = distance;
			closest_distance = distance;
		}
	}

	return result;
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
