#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H


#include "common.h"
#include "scene.h"


#ifndef TERRAIN_NUM_SQUARES
	#define TERRAIN_NUM_SQUARES 256
#endif
#ifndef TERRAIN_SQUARE_SIZE
	#define TERRAIN_SQUARE_SIZE 2.0f
#endif
#define WORLD_SIZE (TERRAIN_NUM_SQUARES * TERRAIN_SQUARE_SIZE)
#ifndef DEFAULT_SCALE
	#define DEFAULT_SCALE 1.0f
#endif
#ifndef TERRAIN_HEIGHT_SCALE
	#define TERRAIN_HEIGHT_SCALE 10.0f
#endif


extern SceneVTable heightmap_Scene_Callbacks;

typedef struct 
{
	float    *heightmap;
	Vector3   sun_angle;
	float     
			  offset,
			  world_size,
			  height_scale;
	uint      cells_wide;
	union {
		struct {
			Color
				hi_color,
				lo_color;
		};
		Color colors[2];
	};
}
Heightmap;


float *genHeightmapXOR();
float *genHeightmapDiamondSquare(size_t cells_wide, float roughness, float decay, size_t seed);


Scene *HeightmapScene_new(Heightmap *heightmap, Engine *engine);


#endif /* HEIGHTMAP_H */
