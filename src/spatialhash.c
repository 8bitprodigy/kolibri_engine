#include <math.h>
#include <raylib.h>
#include <string.h>
#include <stdbool.h>

#include "_spatialhash_.h"
#include "common.h"


#define CELL_ALIGN( value ) ((int)floorf( (value) / CELL_SIZE))
#define GET_CELL_SELECTION( bbox ) \
    ((BoundingBox){ \
            { \
                CELL_ALIGN(bbox.min.x), \
                CELL_ALIGN(bbox.min.y), \
                CELL_ALIGN(bbox.min.z) \
            }, \
            { \
                CELL_ALIGN(bbox.max.x), \
                CELL_ALIGN(bbox.max.y), \
                CELL_ALIGN(bbox.max.z) \
            } \
        }) 


typedef struct
SpatialEntry
{
    BoundingBox          bbox;
    Vector3              position;
	void                *data;
	struct SpatialEntry *next;
}
SpatialEntry;


static inline uint32
spread(uint32 x)
{
    x = (x | (x << 16)) & 0x030000FF;
    x = (x | (x << 8))  & 0x0300F00F;
    x = (x | (x << 4))  & 0x030C30C3;
    x = (x | (x << 2))  & 0x09249249;
    
    return x;
}

static inline uint32
morton3d(uint32 x, uint32 y, uint32 z)
{
    return (spread(x)) | (spread(y) << 1) | (spread(z) << 2);
}

/* Hash function for spatial coordinates */
static inline uint32
hashPosition(float x, float y, float z)
{
    int 
        cell_x = CELL_ALIGN(x),
        cell_y = CELL_ALIGN(y),
        cell_z = CELL_ALIGN(z);
    
    uint32
        ux = ((uint32)cell_x) & 0x3FF,
        uy = ((uint32)cell_y) & 0x3FF,
        uz = ((uint32)cell_z) & 0x3FF;

    return morton3d(ux, uy, uz) % SPATIAL_HASH_SIZE;
}

/* Get a free entry from the pool */
static SpatialEntry *
allocEntry(SpatialHash *hash)
{
    if (!hash->free_entries) {
        ERR_OUT("Spatial hash entry pool exhausted, allocating dynamically!");
        return malloc(sizeof(SpatialEntry));
    }

    SpatialEntry *entry = hash->free_entries;
    hash->free_entries = entry->next;
    hash->pool_used++;
    
    return entry;
}

/* Return entry to free list */
static void
freeEntry(SpatialHash *hash, SpatialEntry *entry)
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

    hash->entry_pool = malloc(sizeof(SpatialEntry) * ENTRY_POOL_SIZE);
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
        SpatialEntry *entry = hash->cells[i];
        while (entry) {
            SpatialEntry *next = entry->next;
            freeEntry(hash, entry);
            entry = next;
        }
        hash->cells[i] = NULL;
    }
}

/* Insert entity */
void
SpatialHash__insert(SpatialHash *hash, void *data, Vector3 center, Vector3 bounds)
{
    BoundingBox 
        bbox = {
                {
                    center.x - bounds.x * 0.5f,
                    center.y - bounds.y * 0.5f,
                    center.z - bounds.z * 0.5f
                },
                {
                    center.x + bounds.x * 0.5f,
                    center.y + bounds.y * 0.5f,
                    center.z + bounds.z * 0.5f
                }
            },
        selection = GET_CELL_SELECTION(bbox);

    for (int x = selection.min.x; x <= selection.max.x; x++) {
        for (int y = selection.min.y; y <= selection.max.y; y++) {
            for (int z = selection.min.z; z <= selection.max.z; z++) {
                uint32 hash_key = hashPosition(x * CELL_SIZE, y * CELL_SIZE, z * CELL_SIZE);

                SpatialEntry *entry   = allocEntry(hash);
                entry->data           = data;
                entry->next           = hash->cells[hash_key];
                entry->position       = center;
                entry->bbox           = bbox;
                hash->cells[hash_key] = entry;
            }
        }
    }
}

/* Query region */
void *
SpatialHash__queryRegion(
    SpatialHash  *hash, 
    BoundingBox   region, 
    void        **query_results, 
    int          *count
)
{
    int size = *count;
    *count = 0;

    BoundingBox selection = GET_CELL_SELECTION(region);

    for (int x = selection.min.x; x <= selection.max.x; x++) {
        for (int y = selection.min.y; y <= selection.max.y; y++) {
            for (int z = selection.min.z; z <= selection.max.z; z++) {
                uint32 hash_key = hashPosition(x * CELL_SIZE, y * CELL_SIZE, z * CELL_SIZE);

                SpatialEntry *entry = hash->cells[hash_key];
                while (entry && *count < size) {
                    bool is_duplicate = false;
                    for (int i = 0; i < *count; i++) {
                        if (query_results[i] == entry->data) {
                            is_duplicate = true;
                            break;
                        }
                    }

                    if (!is_duplicate) {
                        query_results[*count] = entry->data;
                        (*count)++;
                    }

                    entry = entry->next;
                }
            }
        }
    }

    return (void*)query_results;
}
