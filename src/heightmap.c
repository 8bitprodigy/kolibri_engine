#include "common.h"
#include <raylib.h>
#include "heightmap.h"


typedef struct
HeightMap
{
	float *heightmap;
	Vector3 sun_angle;
	uint size;
	union {
		struct {
			Color
				hi_color,
				lo_color;
		}
		colors[2];
	};
}
HeightMap


Vector3
calculateVertexNormal(int x, int z, float scale, float height_scale)
{
	float 
		left   = (x > 0) ? heightmap[x-1][z]                   : heightmap[x][z],
		right  = (x < TERRAIN_NUM_SQUARES) ? heightmap[x+1][z] : heightmap[x][z],
		up     = (z > 0) ? heightmap[x][z-1]                   : heightmap[x][z],
		down   = (z < TERRAIN_NUM_SQUARES) ? heightmap[x][z+1] : heightmap[x][z];

	// Calculate normal using central differences
	Vector3 normal = {
		(left - right) * height_scale,
		2.0f * scale,
		(up - down) * height_scale
	};

	return Vector3Normalize(normal);
}
