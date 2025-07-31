#ifndef SPATIAL_HASH_PRIVATE_H
#define SPATIAL_HASH_PRIVATE_H


#include "common.h"
#include "entity.h"


typedef struct CollisionEntry CollisionEntry;

typedef struct 
SpatialHash
{
	CollisionEntry *entry_pool; /* pre-allocated entries for performance */
	CollisionEntry *free_entries; /* Free list for recycling */
	int             pool_size;
	int             pool_used;
    int             hash_size; /* Number of hash buckets */
    float           cell_size; /* Size of each cell */
	CollisionEntry *cells[SPATIAL_HASH_SIZE]; /* Variable length array - MUST BE LAST */
}
SpatialHash;


/* Constructor/Destructor */
SpatialHash *SpatialHash__new(void);
void         SpatialHash__free(SpatialHash *hash);

/* Private Methods */
void     SpatialHash__clear(SpatialHash *hash);
void     SpatialHash__insert(SpatialHash *hash, Entity *entity, Vector3 center, Vector3 bounds);
Entity **SpatialHash__queryRegion(SpatialHash *hash, Vector3 min, Vector3 max, int *count);


#endif /* SPATIAL_HASH_PRIVATE_H */
