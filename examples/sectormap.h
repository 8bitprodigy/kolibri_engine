#ifndef SECTOR_MAP_H
#define SECTOR_MAP_H

#include "common.h"


typedef struct
{
	size_t
		verts[2],
		next_wall,
		next_sector;
	float
		x, y,
		scale_x,
		scale_y;
	union {
		size_t flags;
		struct {
			/* Todo... */
		};
	}
}
Wall;

typedef struct
{
	size_t
		wall_start,
		wall_count;
	float
		floor_z,
		floor_light,
		ceiling_z,
		ceiling_light;
	union {
		size_t floor_flags;
	}
	union {
		size_t ceiling_flags;
	}
}
Sector;

typedef struct
{
	Vector2 *vertices;
	Wall    *walls;
	Sector  *sectors;
	size_t
		vertex_count,
		wall_count,
		sector_count;
}
SectorMap;


extern SceneVTable sectormap_Scene_Callbacks;


Scene *SectorMapScene_new(        SectorMap *map,    Engine *engine);
SectorMap *ScectorMapScene_getMap(Scene     *scene);

#endif /* SECTOR_MAP_H */
