#ifndef SPATIAL_HASH_PRIVATE_H
#define SPATIAL_HASH_PRIVATE_H


#include "common.h"
#include "entity.h"


typedef struct SpatialEntry SpatialEntry;

typedef struct 
SpatialHash
{
	SpatialEntry *entry_pool; /* pre-allocated entries for performance */
	SpatialEntry *free_entries; /* Free list for recycling */
	int           pool_size;
	int           pool_used;
    int           hash_size; /* Number of hash buckets */
    float         cell_size; /* Size of each cell */
	SpatialEntry *cells[SPATIAL_HASH_SIZE]; /* Variable length array - MUST BE LAST */
}
SpatialHash;


/* Constructor/Destructor */
SpatialHash *SpatialHash__new( void);
void         SpatialHash__free(SpatialHash *hash);

/* Private Methods */
void SpatialHash__clear(      SpatialHash *hash);
void SpatialHash__insert(     SpatialHash *hash, void        *data,   Vector3   center,        Vector3  bounds);
void SpatialHash__queryRegion(SpatialHash *hash, BoundingBox  region, void    **query_results, int     *count);


#endif /* SPATIAL_HASH_PRIVATE_H */
