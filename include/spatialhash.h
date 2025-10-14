#ifndef SPATIAL_HASH_H
#define SPATIAL_HASH_H


#include "common.h"
#include "entity.h"


typedef struct SpatialHash SpatialHash;


/* Constructor/Destructor */
SpatialHash *SpatialHash__new( void);
void         SpatialHash__free(SpatialHash *hash);

/* Private Methods */
void  SpatialHash__clear(      SpatialHash *hash);
void  SpatialHash__insert(     SpatialHash *hash, void        *data,   Vector3   center,        Vector3  bounds);
void *SpatialHash__queryRegion(SpatialHash *hash, BoundingBox  region, void    **query_results, int     *count);


#endif /* SPATIAL_HASH_H */
