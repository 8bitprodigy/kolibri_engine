#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H


#include "common.h"
#include "scene.h"
#include <raylib.h>


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
	Vector3   sun_angle;
	float     
			  ambient_value,
			  offset,
			  height_scale,
			  cell_size;
	size_t      
			  chunk_cells,
			  chunks_wide;
	union {
		struct {
			Color
				sun_color,
				ambient_color;
		};
		Color light_colors[2];
	};
	union {
		struct {
			Color
				hi_color,
				lo_color;
		};
		Color terrain_colors[2];
	};
	Texture2D texture;
}
HeightmapData;


Scene *HeightmapScene_new(         HeightmapData *heightmap_data, Engine  *engine);

HeightmapData *HeightmapScene_getData(     Scene *scene);
Color          HeightmapScene_sampleShadow(Scene *scene,          Vector3  pos);
float          HeightmapScene_getWorldSize(Scene *scene);
float          HeightmapScene_getHeight(   Scene *scene,          Vector3  pos);


#endif /* HEIGHTMAP_H */
