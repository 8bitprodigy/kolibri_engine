#ifndef SPATIAL_HASH_H
#define SPATIAL_HASH_H


#include "common.h"
#include "entity.h"


typedef struct SpatialHash SpatialHash;


/* Constructor/Destructor */
SpatialHash *SpatialHash_new( void);
void         SpatialHash_free(SpatialHash *hash);

/* Methods */
void  SpatialHash_clear(      SpatialHash *hash);
void  SpatialHash_insert(     SpatialHash *hash, void        *data,   Vector3   center,        Vector3  bounds);
void *SpatialHash_queryRegion(SpatialHash *hash, BoundingBox  region, void    **query_results, int     *count);


#endif /* SPATIAL_HASH_H */
