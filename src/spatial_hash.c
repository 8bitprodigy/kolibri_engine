#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "_spatial_hash_.h"
#include "common.h"


#define CELL_ALIGN( value ) ((int)floorf( (value) / CELL_SIZE))


typedef struct
CollisionEntry
{
	Entity                *entity;
	struct CollisionEntry *next;
}
CollisionEntry;


/* Hash function for spatial coordinates */
static inline uint32
hashPosition(float x, float y, float z)
{
    int 
        cell_x = CELL_ALIGN(x),
        cell_y = CELL_ALIGN(y),
        cell_z = CELL_ALIGN(z);

    uint32 hash = (
        (uint32)cell_x * 73856093u
        ^ (uint32)cell_y * 19349663u
        ^ (uint32)cell_z * 83492791u
    );
    
    return hash % SPATIAL_HASH_SIZE;
}

static inline void
getEntityCells(
    Vector3 center,
    Vector3 bounds,
    int *min_x, int *max_x,
    int *min_y, int *max_y,
    int *min_z, int *max_z
)
{
    Vector3
        min_bounds = {
            center.x - bounds.x * 0.5f,
            center.y,
            center.z - bounds.z * 0.5f
        },
        max_bounds = {
            center.x + bounds.x * 0.5f,
            center.y + bounds.y,
            center.z + bounds.z * 0.5f
        };

    *min_x = CELL_ALIGN(min_bounds.x);
    *max_x = CELL_ALIGN(max_bounds.x);
    *min_y = CELL_ALIGN(min_bounds.y);
    *max_y = CELL_ALIGN(max_bounds.y);
    *min_z = CELL_ALIGN(min_bounds.z);
    *max_z = CELL_ALIGN(max_bounds.z);
}

/* Get a free entry from the pool */
static CollisionEntry *
allocEntry(SpatialHash *hash)
{
    if (!hash->free_entries) {
        ERR_OUT("Spatial hash entry pool exhausted, allocating dynamically!");
        return malloc(sizeof(CollisionEntry));
    }

    CollisionEntry *entry = hash->free_entries;
    hash->free_entries = entry->next;
    hash->pool_used++;
    
    return entry;
}

/* Return entry to free list */
static void
freeEntry(SpatialHash *hash, CollisionEntry *entry)
{
    if (!(hash->entry_pool <= entry && entry < hash->entry_pool + hash->pool_size)) {
        free(entry);
        return;
    }
    
    entry->next = hash->free_entries;
    hash->free_entries = entry;
    hash->pool_used--;
}

/* Constructor */
SpatialHash *
SpatialHash__new(void)
{
    SpatialHash *hash = malloc(sizeof(SpatialHash));
    if (!hash) {
        ERR_OUT("Failed to allocate SpatialHash.");
        return NULL;
    }

    memset(hash->cells, 0, sizeof(hash->cells));

    hash->entry_pool = malloc(sizeof(CollisionEntry) * ENTRY_POOL_SIZE);
    hash->pool_size  = ENTRY_POOL_SIZE;
    hash->pool_used  = 0;

    hash->free_entries = hash->entry_pool;
    for (int i = 0; i < ENTRY_POOL_SIZE - 1; i++) {
        hash->entry_pool[i].next = &hash->entry_pool[i + 1];
    }
    hash->entry_pool[ENTRY_POOL_SIZE - 1].next = NULL;

    return hash;
}

/* Destructor */
void
SpatialHash__free(SpatialHash *hash)
{
    if (!hash) return;
    
    SpatialHash__clear(hash);
    free(hash->entry_pool);
    free(hash);
}

/* Clear all entries */
void
SpatialHash__clear(SpatialHash *hash)
{
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

/* Insert entity */
void
SpatialHash__insert(SpatialHash *hash, Entity *entity, Vector3 center, Vector3 bounds)
{
    int min_x, max_x, min_y, max_y, min_z, max_z;
    getEntityCells(center, bounds, &min_x, &max_x, &min_y, &max_y, &min_z, &max_z);

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

/* Query region */
Entity **
SpatialHash__queryRegion(SpatialHash *hash, Vector3 min, Vector3 max, int *count)
{
    static Entity *query_results[QUERY_SIZE];
    *count = 0;

    int min_x = CELL_ALIGN(min.x);
    int max_x = CELL_ALIGN(max.x);
    int min_y = CELL_ALIGN(min.y);
    int max_y = CELL_ALIGN(max.y);
    int min_z = CELL_ALIGN(min.z);
    int max_z = CELL_ALIGN(max.z);

    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {
            for (int z = min_z; z <= max_z; z++) {
                uint32 hash_key = hashPosition(x * CELL_SIZE, y * CELL_SIZE, z * CELL_SIZE);

                CollisionEntry *entry = hash->cells[hash_key];
                while (entry && *count < QUERY_SIZE) {
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
