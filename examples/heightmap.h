#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H


#include "common.h"
#include "scene.h"


#ifndef TERRAIN_NUM_SQUARES
	#define TERRAIN_NUM_SQUARES 128
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
HeightmapData
{
	float    *heightmap;
	Vector3   sun_angle;
	uint      size;
	union {
		struct {
			Color
				hi_color,
				lo_color;
		}
		colors[2];
	};
	Mesh terrain_mesh;
	Model terrain_model;
}
HeightmapData;


float *genHeightmapXOR();
float *genHeightmapDiamondSquare(size_t cells_wide, size_t seed);


Scene *Heightmap_new(HeightmapData *heightmap_data);


#endif /* HEIGHTMAP_H */
