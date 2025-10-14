#ifndef SPATIAL_HASH_PRIVATE_H
#define SPATIAL_HASH_PRIVATE_H


#include "spatialhash.h"


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
	SpatialEntry *cells[SPATIAL_HASH_SIZE]; 
}
SpatialHash;


#endif /* SPATIAL_HASH_PRIVATE_H */
