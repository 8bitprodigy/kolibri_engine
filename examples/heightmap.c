#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "dynamicarray.h"
#include "engine.h"
#include "entity.h"
#include "head.h"
#include "heightmap.h"
#include "renderer.h"


#define INDEX(array, width, x, y) (array[y * width + x])

typedef struct Heightmap Heightmap;

typedef struct
{
	Heightmap *heightmap;
	union {
		struct {
			int
				x,
				z;
		} idx;
		struct {
			int
				chunk_x,
				chunk_z;
		};
		int idxa[2];
	};
	Vector3
		position,
		bounds;
	float *corners[4];
}
ChunkData;

typedef struct
Heightmap
{
	HeightmapData data;

	float      
		       lod_distances[MAX_LOD_LEVELS],
		       world_size;
	size_t     cells_wide;
	struct {
		size_t vertex_count;
		size_t triangle_count;
		size_t cells_per_edge;
		size_t step;
	} lod_info[MAX_LOD_LEVELS];
	
	float     *heightmap;
	Color     
		      *shadowmap,
		      *colormap;
	Vector3   *normalmap;
	ChunkData *chunks;
}
Heightmap;


/*
	Callback Forward Declarations
*/
void            heightmapSceneSetup(    Scene *scene, void   *map_data);
void            heightmapSceneRender(   Scene *scene, Head   *head);
CollisionResult heightmapSceneCollision(Scene *scene, Entity *entity,   Vector3 to);
CollisionResult heightmapSceneRaycast(  Scene *scene, Vector3 from,     Vector3 to);
void            heightmapSceneFree(     Scene *scene);


/*
	Template Declarations
*/
SceneVTable heightmap_Scene_Callbacks = {
	.Setup          = heightmapSceneSetup, 
	.Enter          = NULL, 
	.Update         = NULL, 
	.EntityEnter    = NULL, 
	.EntityExit     = NULL, 
	.CheckCollision = heightmapSceneCollision, 
	.MoveEntity     = heightmapSceneCollision, 
	.Raycast        = heightmapSceneRaycast,
	.Render         = heightmapSceneRender, 
	.Exit           = NULL, 
	.Free           = heightmapSceneFree,
};


const Vector3 SUN_ANGLE = {
		.x =  0.3f, 
		.y = -0.8f, 
		.z =  0.3f
	};


int
getChunkLOD(float dist_sq, Heightmap *map)
{
	for (int lod = 0; lod < MAX_LOD_LEVELS; lod++) {
		float threshold = map->lod_distances[lod];
		if (dist_sq < threshold * threshold) {
			return lod;
		}
	}

	return MAX_LOD_LEVELS - 1;
}


void
RenderHeightmapChunk(
	ChunkData *chunk, 
	Vector3    cam_pos, 
	int        lod
)
{
	Heightmap     *map   = (Heightmap*)chunk->heightmap;
	HeightmapData *data  = (HeightmapData*)&map->data;
	
	float 
		offset       = data->offset,
		height_scale = data->height_scale,
		cell_size    = data->cell_size;
		
	int
		step = map->lod_info[lod].step,
		
		chunk_cells = data->chunk_cells,
		chunks_wide = data->chunks_wide,
		
		chunk_x = chunk->idx.x,
		chunk_z = chunk->idx.z,
		
		heightmap_grid = data->chunks_wide * data->chunk_cells;

	float
		lod_threshold_sq = map->lod_distances[lod] * map->lod_distances[lod],
		chunk_size = data->chunk_cells * data->cell_size,
		(*heightmap)[heightmap_grid]   = (float(*)[heightmap_grid])map->heightmap;

	bool 
		stitch_north = false,
		stitch_south = false,
		stitch_west  = false,
		stitch_east  = false;

	if (lod < MAX_LOD_LEVELS - 1) {
		float 
			dx_nw = (chunk->position.x - chunk_size*0.5f) - cam_pos.x,
			dz_nw = (chunk->position.z - chunk_size*0.5f) - cam_pos.z,
			dx_ne = (chunk->position.x + chunk_size*0.5f) - cam_pos.x,
			dz_ne = (chunk->position.z - chunk_size*0.5f) - cam_pos.z,
			dx_sw = (chunk->position.x - chunk_size*0.5f) - cam_pos.x,
			dz_sw = (chunk->position.z + chunk_size*0.5f) - cam_pos.z,
			dx_se = (chunk->position.x + chunk_size*0.5f) - cam_pos.x,
			dz_se = (chunk->position.z + chunk_size*0.5f) - cam_pos.z;
		
		bool
			nw_outside = (dx_nw*dx_nw + dz_nw*dz_nw) > lod_threshold_sq,
			ne_outside = (dx_ne*dx_ne + dz_ne*dz_ne) > lod_threshold_sq,
			sw_outside = (dx_sw*dx_sw + dz_sw*dz_sw) > lod_threshold_sq,
			se_outside = (dx_se*dx_se + dz_se*dz_se) > lod_threshold_sq;
		
		stitch_north = (nw_outside && ne_outside);
		stitch_south = (sw_outside && se_outside);
		stitch_west  = (nw_outside && sw_outside);
		stitch_east  = (ne_outside && se_outside);
	}
	
	Color (*colormap)[heightmap_grid]    = (Color(*)[heightmap_grid])map->colormap;
	Vector3 (*normalmap)[heightmap_grid] = (Vector3(*)[heightmap_grid])map->normalmap;

	int
		start_x = chunk_x * chunk_cells,
		start_z = chunk_z * chunk_cells;

	float
		chunk_offset_x = (chunk_x - chunks_wide / 2.0f) * chunk_cells * cell_size,
		chunk_offset_z = (chunk_z - chunks_wide / 2.0f) * chunk_cells * cell_size;
/*
	DBG_EXPR(
			DrawCubeWires(
					chunk->position, 
					chunk->bounds.x, 
					chunk->bounds.y, 
					chunk->bounds.z, 
					ORANGE
				);
		);
//*/
	rlPushMatrix();
	
	rlBegin(RL_TRIANGLES);

	if (data->texture.id != 0) {
		rlSetTexture(data->texture.id);
	}
	
	for (int z = 0; z < chunk_cells; z += step) {
		for (int x = 0; x < chunk_cells; x += step) {
			int 
				hm_x = (start_x + x) % heightmap_grid,
				hm_z = (start_z + z) % heightmap_grid,
				next_x = (x + step <= chunk_cells) ? x + step : chunk_cells,
				next_z = (z + step <= chunk_cells) ? z + step : chunk_cells,
				hm_next_x = (start_x + next_x) % heightmap_grid,
				hm_next_z = (start_z + next_z) % heightmap_grid;

			float
				y_tl = heightmap[hm_z][hm_x] * height_scale + offset,
				y_tr = heightmap[hm_z][hm_next_x] * height_scale + offset,
				y_bl = heightmap[hm_next_z][hm_x] * height_scale + offset,
				y_br = heightmap[hm_next_z][hm_next_x] * height_scale + offset;

			Color 
				c_tl = colormap[hm_z][hm_x],
				c_tr = colormap[hm_z][hm_next_x],
				c_bl = colormap[hm_next_z][hm_x],
				c_br = colormap[hm_next_z][hm_next_x];

			if (lod < MAX_LOD_LEVELS - 1) {
				// North edge (z == 0)
				if (stitch_north && z == 0) {
					// TL vertex
					if ((x % (step * 2)) == step) {
						y_tl = 0.5f * (
							heightmap[hm_z][hm_x - step] +
							heightmap[hm_z][hm_x + step]
						) * height_scale + offset;
						
						c_tl = ColorLerp(
							colormap[hm_z][hm_x - step],
							colormap[hm_z][hm_x + step],
							0.5f
						);
					}

					// TR vertex
					if ((next_x % (step * 2)) == step) {
						y_tr = 0.5f * (
							heightmap[hm_z][hm_next_x - step] +
							heightmap[hm_z][hm_next_x + step]
						) * height_scale + offset;
						
						c_tr = ColorLerp(
							colormap[hm_z][hm_next_x - step],
							colormap[hm_z][hm_next_x + step],
							0.5f
						);
					}
				}

				// South edge (next_z == chunk_cells)
				if (stitch_south && next_z == chunk_cells) {
					// BL vertex
					if ((x % (step * 2)) == step) {
						y_bl = 0.5f * (
							heightmap[hm_next_z][hm_x - step] +
							heightmap[hm_next_z][hm_x + step]
						) * height_scale + offset;
						
						c_bl = ColorLerp(
							colormap[hm_next_z][hm_x - step],
							colormap[hm_next_z][hm_x + step],
							0.5f
						);
					}

					// BR vertex
					if ((next_x % (step * 2)) == step) {
						y_br = 0.5f * (
							heightmap[hm_next_z][hm_next_x - step] +
							heightmap[hm_next_z][hm_next_x + step]
						) * height_scale + offset;
						
						c_br = ColorLerp(
							colormap[hm_next_z][hm_next_x - step],
							colormap[hm_next_z][hm_next_x + step],
							0.5f
						);
					}
				}

				// West edge (x == 0)
				if (stitch_west && x == 0) {
					// TL vertex
					if ((z % (step * 2)) == step) {
						y_tl = 0.5f * (
							heightmap[hm_z - step][hm_x] +
							heightmap[hm_z + step][hm_x]
						) * height_scale + offset;
						
						c_tl = ColorLerp(
							colormap[hm_z - step][hm_x],
							colormap[hm_z + step][hm_x],
							0.5f
						);
					}

					// BL vertex
					if ((next_z % (step * 2)) == step) {
						y_bl = 0.5f * (
							heightmap[hm_next_z - step][hm_x] +
							heightmap[hm_next_z + step][hm_x]
						) * height_scale + offset;
						
						c_bl = ColorLerp(
							colormap[hm_next_z - step][hm_x],
							colormap[hm_next_z + step][hm_x],
							0.5f
						);
					}
				}

				// East edge (next_x == chunk_cells)
				if (stitch_east && next_x == chunk_cells) {
					// TR vertex
					if ((z % (step * 2)) == step) {
						y_tr = 0.5f * (
							heightmap[hm_z - step][hm_next_x] +
							heightmap[hm_z + step][hm_next_x]
						) * height_scale + offset;
						
						c_tr = ColorLerp(
							colormap[hm_z - step][hm_next_x],
							colormap[hm_z + step][hm_next_x],
							0.5f
						);
					}

					// BR vertex
					if ((next_z % (step * 2)) == step) {
						y_br = 0.5f * (
							heightmap[hm_next_z - step][hm_next_x] +
							heightmap[hm_next_z + step][hm_next_x]
						) * height_scale + offset;
						
						c_br = ColorLerp(
							colormap[hm_next_z - step][hm_next_x],
							colormap[hm_next_z + step][hm_next_x],
							0.5f
						);
					}
				}
			}

			Vector3
				v_tl = {
					x * cell_size + chunk_offset_x,
					y_tl,
					z * cell_size + chunk_offset_z
				},
				v_tr = {
					next_x * cell_size + chunk_offset_x,
					y_tr,
					z * cell_size + chunk_offset_z
				},
				v_bl = {
					x * cell_size + chunk_offset_x,
					y_bl,
					next_z * cell_size + chunk_offset_z
				},
				v_br = {
					next_x * cell_size + chunk_offset_x,
					y_br,
					next_z * cell_size + chunk_offset_z
				};


			Vector3
				n_tl = normalmap[hm_z][hm_x],
				n_tr = normalmap[hm_z][hm_next_x],
				n_bl = normalmap[hm_next_z][hm_x],
				n_br = normalmap[hm_next_z][hm_next_x];

			Vector2
				uv_tl = {0.0f, 0.0f},
				uv_tr = {(float)step, 0.0f},
				uv_bl = {0.0f, (float)step},
				uv_br = {(float)step, (float)step};

			/* First triangle (top-left -> bottom-left -> top-right) */
            rlColor4ub(  c_tl.r,  c_tl.g, c_tl.b, c_tl.a);
            rlNormal3f(  n_tl.x,  n_tl.y, n_tl.z);
            rlTexCoord2f(uv_tl.x, uv_tl.y);
            rlVertex3f(  v_tl.x,  v_tl.y, v_tl.z);
            
            rlColor4ub(  c_bl.r,  c_bl.g, c_bl.b, c_bl.a);
            rlNormal3f(  n_bl.x,  n_bl.y, n_bl.z);
            rlTexCoord2f(uv_bl.x, uv_bl.y);
            rlVertex3f(  v_bl.x,  v_bl.y, v_bl.z);
            
            rlColor4ub(  c_tr.r,  c_tr.g, c_tr.b, c_tr.a);
            rlNormal3f(  n_tr.x,  n_tr.y, n_tr.z);
            rlTexCoord2f(uv_tr.x, uv_tr.y);
            rlVertex3f(  v_tr.x,  v_tr.y, v_tr.z);

            /* Second triangle (top-right -> bottom-left -> bottom-right) */
            rlColor4ub(  c_tr.r,  c_tr.g, c_tr.b, c_tr.a);
            rlNormal3f(  n_tr.x,  n_tr.y, n_tr.z);
            rlTexCoord2f(uv_tr.x, uv_tr.y);
            rlVertex3f(  v_tr.x,  v_tr.y, v_tr.z);
            
            rlColor4ub(  c_bl.r,  c_bl.g, c_bl.b, c_bl.a);
            rlNormal3f(  n_bl.x,  n_bl.y, n_bl.z);
            rlTexCoord2f(uv_bl.x, uv_bl.y);
            rlVertex3f(  v_bl.x,  v_bl.y, v_bl.z);
            
            rlColor4ub(  c_br.r,  c_br.g, c_br.b, c_br.a);
            rlNormal3f(  n_br.x,  n_br.y, n_br.z);
            rlTexCoord2f(uv_br.x, uv_br.y);
            rlVertex3f(  v_br.x,  v_br.y, v_br.z);
		}
	}
	
	rlEnd();
	
	if (data->texture.id != 0) {
		rlSetTexture(0);
	}
    rlPopMatrix();
}


/**********************
	PRIVATE METHODS
**********************/

float 
randomRange(float range) 
{
    return ((float)rand() / RAND_MAX) * 2.0f * range - range;
}

// Set height at position with bounds checking
void 
setHeight(float *heightmap_data, size_t cells_wide, int x, int y, float height) 
{
    float (*heightmap)[cells_wide] = (float(*)[cells_wide])heightmap_data;
	
    x = ((x % cells_wide) + cells_wide) % cells_wide;
    y = ((y % cells_wide) + cells_wide) % cells_wide;

    heightmap[y][x] = height;
}

// Get height at position with bounds checking
float 
getHeight(float *heightmap_data, size_t cells_wide, int x, int y) 
{
    float (*heightmap)[cells_wide] = (float(*)[cells_wide])heightmap_data;
	
    x = ((x % cells_wide) + cells_wide) % cells_wide;
    y = ((y % cells_wide) + cells_wide) % cells_wide;

    return heightmap[y][x];
}


void
diamond(
	float  *heightmap_data, 
	size_t  cells_wide,
	int     x, 
	int     y, 
	int     size,
	float   roughness
)
{
	int half = size / 2;
	float avg = (
			getHeight(  heightmap_data, cells_wide, x - half, y - half)
			+ getHeight(heightmap_data, cells_wide, x + half, y - half)
			+ getHeight(heightmap_data, cells_wide, x - half, y + half)
			+ getHeight(heightmap_data, cells_wide, x + half, y + half)
		) / 4.0f;

	setHeight(heightmap_data, cells_wide, x, y, avg + randomRange(roughness));
}


void
square(
	float  *heightmap_data, 
	size_t  cells_wide,
	int     x,
	int     y,
	int     size,
	float   roughness
)
{
	int half = size / 2;
	float avg = (
			getHeight(  heightmap_data, cells_wide, x,        y - half)
			+ getHeight(heightmap_data, cells_wide, x + half, y)
			+ getHeight(heightmap_data, cells_wide, x,        y + half)
			+ getHeight(heightmap_data, cells_wide, x - half, y)
		) / 4.0f;

	setHeight(heightmap_data, cells_wide, x, y, avg + randomRange(roughness));
}


void
diamondSquareSeeded(
	float  *heightmap_data, 
	int     cells_wide,
	float   Initial_Roughness, 
	float   Roughness_Decay, 
	size_t  Seed
)
{
	float (*heightmap)[cells_wide] = (float(*)[cells_wide])heightmap_data;
	
	srand(Seed);

	setHeight(heightmap_data, cells_wide, 0,          0,          randomRange(Initial_Roughness));
	setHeight(heightmap_data, cells_wide, cells_wide, 0,          randomRange(Initial_Roughness));
	setHeight(heightmap_data, cells_wide, 0,          cells_wide, randomRange(Initial_Roughness));
	setHeight(heightmap_data, cells_wide, cells_wide, cells_wide, randomRange(Initial_Roughness));

	float roughness = Initial_Roughness;

	/* Diamond-Square algorithm */
	for (int size = cells_wide; 1 < size; size /= 2) {
		int half = size / 2;

		for (int y = half; y < cells_wide; y += size) {
			for (int x = half; x < cells_wide; x += size) {
				diamond(heightmap_data, cells_wide, x, y, size, roughness);
			}
		}

		for (int y = 0; y < cells_wide; y += half) {
			for (int x = (y + half) % size; x < cells_wide; x += size) {
				square(heightmap_data, cells_wide, x, y, size, roughness);
			}
		}

		roughness *= Roughness_Decay;
	}

	/* Normalize heightmap to 0.0-1.0 range */
	float
		min_height = heightmap[0][0],
		max_height = heightmap[0][0];

	for (int y = 0; y < cells_wide; y++) {
		for (int x = 0; x < cells_wide; x++) {
			if (heightmap[y][x] < min_height) min_height  = heightmap[y][x];
			if (max_height < heightmap[y][x]) max_height = heightmap[y][x];
		}
	}

	float range = max_height - min_height;
	if (range < 0.0f) {
		return; 
	}

	for (int y = 0; y < cells_wide; y++) {
		for (int x = 0; x < cells_wide; x++) {
			heightmap[y][x]= (heightmap[y][x]- min_height) / range;
		}
	}
}


void
diamondSquare(
	float  *heightmap, 
	size_t  cells_wide,
	float   Initial_Roughness, 
	float   Roughness_Decay
) 
{
	diamondSquareSeeded(heightmap, cells_wide, Initial_Roughness, Roughness_Decay, (unsigned int)time(NULL));
}


Vector3 
calculateTriangleNormal(Vector3 v1, Vector3 v2, Vector3 v3) 
{
    Vector3 
		P1     = Vector3Subtract(    v2, v1),
		P2     = Vector3Subtract(    v3, v1),
		normal = Vector3CrossProduct(P1, P2);
    return Vector3Normalize(normal);
}

Vector3
calculateVertexNormal(
	Heightmap *map, 
	int    x, 
	int    z, 
	float  scale, 
	float  height_scale
)
{
	int     cells_wide             = map->cells_wide;
	float (*heightmap)[cells_wide] = (float(*)[cells_wide])map->heightmap;
	
	int 
		x_left  = ((x - 1) + cells_wide) % cells_wide,
		x_right = (x + 1) % cells_wide,
		z_up    = ((z - 1) + cells_wide) % cells_wide,
		z_down  = (z + 1) % cells_wide;

	float 
		left  = heightmap[z_up][x],
		right = heightmap[z_down][x],
		up    = heightmap[z][x_left], 
		down  = heightmap[z][x_right];

	Vector3 tangent_x = {
        2.0f * scale,                    // Step in X direction
        (right - left) * height_scale,   // Height difference
        0.0f
    };
    
    Vector3 tangent_z = {
        0.0f,
        (down - up) * height_scale,      // Height difference  
        2.0f * scale                     // Step in Z direction
    };
    
    // Cross product gives the normal
    Vector3 normal = Vector3CrossProduct(tangent_z, tangent_x);
    return Vector3Normalize(normal);
}


float
getLightingFactor(Vector3 normal, Vector3 sun_angle) 
{
	return  -Vector3DotProduct(normal, Vector3Normalize(sun_angle));
}

static inline 
struct TerrainSample {int x0, z0, x1, z1; float x_frac, z_frac;}
getTerrainSample(float world_size, size_t cells_wide, Vector3 position)
{
	
	float
		normalized_x = (position.x / world_size) + 0.5f,
		normalized_z = (position.z / world_size) + 0.5f,
		float_x = normalized_x * cells_wide,
		float_z = normalized_z * cells_wide;

	int
		x0 = (int)floorf(float_x) % cells_wide,
		z0 = (int)floorf(float_z) % cells_wide,
		x1 = (x0 + 1) % cells_wide,
		z1 = (z0 + 1) % cells_wide;
    
    return (struct TerrainSample) { 
			.x0 = x0,
			.z0 = z0,
			.x1 = x1,
			.z1 = z1,
			
			.x_frac = float_x - x0,
			.z_frac = float_z - z0,
		};
}

float
getTerrainHeight(Heightmap *map, Vector3 position)
{
	HeightmapData  *data                  = (HeightmapData*)&map->data;
	size_t          grid_size             = map->cells_wide;
	float         (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;

	struct TerrainSample sample = getTerrainSample(map->world_size, grid_size, position);
	
	float
		lower = Lerp(heightmap[sample.z0][sample.x0], heightmap[sample.z0][sample.x1], sample.x_frac),
		upper = Lerp(heightmap[sample.z1][sample.x0], heightmap[sample.z1][sample.x1], sample.x_frac);
	
	// Interpolate between bottom and top edges along z
	return Lerp(lower, upper, sample.z_frac) * data->height_scale;
}


float *
genHeightmapXOR()
{
    const int size = 256;
    float *map = DynamicArray(float, size * size);
	float (*heightmap)[size] = (float(*)[size])map;

    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            heightmap[y][x] = (float)(x^y)/256;
    return map;
}


float *
genHeightmapDiamondSquare(
	size_t cells_wide, 
	float roughness, 
	float decay, 
	size_t seed
)
{
	float *heightmap = DynamicArray(float, cells_wide * cells_wide);
	
	diamondSquareSeeded(heightmap, cells_wide, roughness,decay, seed);

	return heightmap;
}


Vector3 *
generateNormalMap(Heightmap *map)
{
	HeightmapData  *data                  = &map->data;
	int             grid_size             = map->cells_wide;

	Vector3
		*normalmap            = DynamicArray(Vector3, grid_size * grid_size),
		(*normals)[grid_size] = (Vector3(*)[grid_size])normalmap;
	
	for (int z = 0; z < grid_size; z++) {
		for (int x = 0; x < grid_size; x++) {
			normals[z][x] = calculateVertexNormal(
					map,
					x,
					z,
					1.0f,
					data->height_scale
				);
		}
	}

	return normalmap;
}


Color *
generateColorMap(Heightmap *map)
{
	HeightmapData  *data                  = &map->data;
	int             grid_size             = map->cells_wide;
	Vector3       
		            sun_angle             = data->sun_angle,
		          (*normalmap)[grid_size] = (Vector3(*)[grid_size])map->normalmap;
	
	Color
	 	*colormap           = DynamicArray(Color, grid_size * grid_size),
		(*colors)[grid_size] = (Color(*)[grid_size])colormap,

		  sun_color     = data->sun_color,
		  ambient_color = data->ambient_color,
		  hi_color      = data->hi_color,
		  lo_color      = data->lo_color;

	for (int z = 0; z < grid_size; z++) {
		for (int x = 0; x < grid_size; x++) {
			float 
				lighting_factor = getLightingFactor(
						normalmap[z][x],
						sun_angle
					),
				slope_factor = Vector3DotProduct(
						normalmap[z][x], 
						V3_UP
					);

			slope_factor = slope_factor > 0.5f? 1.0f : 0.0f;

			Color
				terrain_color = ColorLerp(
						lo_color,
						hi_color,
						slope_factor
					),
				light_color = ColorLerp(
						ambient_color,
						sun_color,
						lighting_factor
					);
			
			colors[z][x] = (Color) {
				(unsigned char)((terrain_color.r / 255.0f) * (light_color.r / 255.0f) * 255.0f),
				(unsigned char)((terrain_color.g / 255.0f) * (light_color.g / 255.0f) * 255.0f),
				(unsigned char)((terrain_color.b / 255.0f) * (light_color.b / 255.0f) * 255.0f),
				255
			};
		}
	}

	return colormap;
}

Color *
generateShadowMap(Heightmap *map)
{
	HeightmapData  *data                   = &map->data;
	int             cells_wide             = map->cells_wide;
	Vector3       
		          (*normalmap)[cells_wide] = (Vector3(*)[cells_wide])map->normalmap,
		            sun_angle              = data->sun_angle;

	Color
		  sun_color     = data->sun_color,
		  ambient_color = data->ambient_color,
		 *shadowmap     = DynamicArray(Color, cells_wide * cells_wide),
		(*shadows)[cells_wide] = (Color(*)[cells_wide])shadowmap;

	for (int z = 0; z < cells_wide; z++) {
		for (int x = 0; x < cells_wide; x++) {
			shadows[z][x] = ColorLerp(
					ambient_color,
					sun_color,
					getLightingFactor(normalmap[z][x], sun_angle)
				);
		}
	}

	return shadowmap;
}


ChunkData *
generateChunks(Heightmap *map)
{
	HeightmapData  *data                  = &map->data;
	int             grid_size             = map->cells_wide;
	float         (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;

	int        num_chunks = data->chunks_wide * data->chunks_wide;
	ChunkData *chunks     = DynamicArray(ChunkData, num_chunks);

	for (size_t chunk_z = 0; chunk_z < data->chunks_wide; chunk_z++) {
		for (size_t chunk_x = 0; chunk_x < data->chunks_wide; chunk_x++) {
			size_t idx = chunk_z * data->chunks_wide + chunk_x;

			float
				chunk_offset_x = (chunk_x - data->chunks_wide * 0.5f) * data->chunk_cells * data->cell_size,
				chunk_offset_z = (chunk_z - data->chunks_wide * 0.5f) * data->chunk_cells * data->cell_size,
				chunk_size     = data->chunk_cells * data->cell_size;

			int
				start_x = chunk_x * data->chunk_cells,
				start_z = chunk_z * data->chunk_cells,
				hm_x,
				hm_z;

			float 
				min_height = INFINITY,
				max_height = -INFINITY;

			for (size_t z = 0; z <= data->chunk_cells; z++) {
				for (size_t x = 0; x <= data->chunk_cells; x++) {
					
					hm_x = start_x + x % map->cells_wide;
					hm_z = start_z + z % map->cells_wide;
						
					float h  = heightmap[hm_z][hm_x] * data->height_scale + data->offset;
					
					if (h < min_height) min_height = h;
					if (h > max_height) max_height = h;
				}
			}
			
			chunks[idx] = (ChunkData){
					.heightmap = map,
					.chunk_x   = chunk_x,
					.chunk_z   = chunk_z,
					.position  = (Vector3){
							chunk_offset_x + chunk_size * 0.5f,
							(min_height + max_height)   * 0.5f,
							chunk_offset_z + chunk_size * 0.5f
						},
					.bounds    = (Vector3){
							chunk_size,
							max_height - min_height,
							chunk_size
						},
					.corners   = {
							&heightmap[start_z][start_x],
							&heightmap[start_z][hm_x],
							&heightmap[hm_z][start_x],
							&heightmap[hm_z][hm_x],
						},
				};
		}
	}

	return chunks;
}


/**********************
	SCENE CALLBACKS
**********************/

void
heightmapSceneSetup(Scene *scene, void *map_data)
{
	(void)scene;
	Heightmap     *map  = (Heightmap*)map_data;
	HeightmapData *data = (HeightmapData*)&map->data;
	
	map->normalmap = generateNormalMap(map);
	map->colormap  = generateColorMap(map);
	map->shadowmap = generateShadowMap(map);
	map->chunks    = generateChunks(map);
	
	if (data->texture.id != 0) {
        GenTextureMipmaps(&data->texture);
		SetTextureFilter(  data->texture, TEXTURE_FILTER_TRILINEAR);
    }

    for (int lod = 0; lod < MAX_LOD_LEVELS; lod++) {
    	int 
			step  = 1 <<lod,
			cells = data->chunk_cells / step;

		map->lod_info[lod].step           = step;
		map->lod_info[lod].cells_per_edge = cells;
		map->lod_info[lod].vertex_count   = (cells + 1) * (cells + 1);
		map->lod_info[lod].triangle_count = cells * cells * 2;
    	
    }
}


void 
heightmapSceneRender(Scene *scene, Head *head)
{
	Renderer      *renderer   = Engine_getRenderer(Scene_getEngine(scene));
	Heightmap     *map        = Scene_getData(scene);
	HeightmapData *data       = &map->data;
	Camera        *camera     = Head_getCamera(head);
	Vector3        camera_pos = camera->position;
	size_t         num_chunks = data->chunks_wide * data->chunks_wide;

	float 
		world_size   = map->world_size,
		chunk_size   = data->chunk_cells * data->cell_size,
		max_distance = Head_getRendererSettings(head)->max_render_distance;
		
	Vector3 snapped_cam = {
		roundf(camera_pos.x / chunk_size) * chunk_size,
		camera_pos.y,
		roundf(camera_pos.z / chunk_size) * chunk_size
	};
	snapped_cam.y = getTerrainHeight(map, snapped_cam) + data->offset;
		
	DBG_EXPR( 
			float half_width = map->world_size/2.0f;
			
			BoundingBox bbox = {
					{-half_width, data->offset, -half_width},
					{half_width, data->height_scale, half_width}
				};
			DrawBoundingBox(bbox, MAGENTA);

			DrawSphereWires(snapped_cam, 1.0f, 3, 8, ORANGE);

			/* Solid are positive axes, wireframe are negative axes. */
			DrawCube(     (Vector3){ half_width, data->height_scale,  0.0f},       5.0f, 5.0f, 5.0f, RED);
			DrawCubeWires((Vector3){-half_width, data->height_scale,  0.0f},       5.0f, 5.0f, 5.0f, RED);
			DrawCube(     (Vector3){ 0.0f,       data->height_scale,  0.0f},       5.0f, 5.0f, 5.0f, GREEN);
			DrawCubeWires((Vector3){ 0.0f,       data->offset,        0.0f},       5.0f, 5.0f, 5.0f, GREEN);
			DrawCube(     (Vector3){ 0.0f,       data->height_scale,  half_width}, 5.0f, 5.0f, 5.0f, BLUE);
			DrawCubeWires((Vector3){ 0.0f,       data->height_scale, -half_width}, 5.0f, 5.0f, 5.0f, BLUE);
		);
//*/
	for (size_t i = 0; i < num_chunks; i++) {
		ChunkData *chunk   = &map->chunks[i];

		float 
			chunk_pos_x = chunk->position.x,
			chunk_half  = (data->chunk_cells * data->cell_size) * 0.5f;

		int 
			start_x = chunk->idx.x * data->chunk_cells,
			start_z = chunk->idx.z * data->chunk_cells,
			hm_grid = data->chunks_wide * data->chunk_cells;

		float (*heightmap)[hm_grid] = (float(*)[hm_grid])map->heightmap;

		// Wrap the corner accesses
		int
			end_x = (start_x + data->chunk_cells) % hm_grid,
			end_z = (start_z + data->chunk_cells) % hm_grid;

		Vector3 corners[4] = {
			{chunk_pos_x - chunk_half, heightmap[start_z][start_x] * data->height_scale + data->offset, chunk->position.z - chunk_half},
			{chunk_pos_x + chunk_half, heightmap[start_z][end_x]   * data->height_scale + data->offset, chunk->position.z - chunk_half},
			{chunk_pos_x - chunk_half, heightmap[end_z][start_x]   * data->height_scale + data->offset, chunk->position.z + chunk_half},
			{chunk_pos_x + chunk_half, heightmap[end_z][end_x]     * data->height_scale + data->offset, chunk->position.z + chunk_half}
		};
		
		Frustum *frustum      = Head_getFrustum(head);
//*/
		// Check all possible wrap positions (including original at 0,0)
		for (int offset_x = -1; offset_x <= 1; offset_x++) {
			for (int offset_z = -1; offset_z <= 1; offset_z++) {
				Vector3 wrap_offset = {offset_x * world_size, 0, offset_z * world_size};
				Vector3 test_pos = Vector3Add(chunk->position, wrap_offset);
				
				// Calculate distance for this wrap position
				float test_dist_sq = INFINITY;
				for (int c = 0; c < 4; c++) {
					Vector3 test_corner = Vector3Add(corners[c], wrap_offset);
					float 
						dx = test_corner.x - snapped_cam.x,
						dz = test_corner.z - snapped_cam.z,
						d  = dx*dx + dz*dz;
					if (d < test_dist_sq) test_dist_sq = d;
				}
				
				// Frustum cull and render if visible
				if (isAABBInFrustum(
						test_pos,
						Vector3Scale(chunk->bounds, 0.5f),
						frustum,
						test_dist_sq,
						max_distance
					)
				) {
					if (offset_x != 0 || offset_z != 0) {
						rlPushMatrix();
						rlTranslatef(wrap_offset.x, wrap_offset.y, wrap_offset.z);

						Vector3 adjusted_cam = Vector3Subtract(snapped_cam, wrap_offset);
						RenderHeightmapChunk(chunk, adjusted_cam, getChunkLOD(test_dist_sq, map));
						rlPopMatrix();
					}
					else {
						RenderHeightmapChunk(chunk, snapped_cam, getChunkLOD(test_dist_sq, map));
					}
				}
			}
		}
		
	}
	EntityList *ent_list = Scene_getEntityList(scene);
	
	for (size_t i = 0; i < ent_list->count; i++) {
		Entity *entity = ent_list->entities[i];
		
		// Always submit at actual position
		Renderer_submitEntity(renderer, entity);
		
		// Check all 8 possible wrap positions
		for (int ox = -1; ox <= 1; ox++) {
			for (int oz = -1; oz <= 1; oz++) {
				if (ox == 0 && oz == 0) continue;
				
				Vector3 test_pos = {
					entity->position.x + (ox * world_size),
					entity->position.y,
					entity->position.z + (oz * world_size)
				};
				
				float 
					dx      = test_pos.x - camera_pos.x,
					dz      = test_pos.z - camera_pos.z,
					dist_sq = dx*dx + dz*dz;
				
				if (dist_sq < max_distance * max_distance) {
					Vector3 orig = entity->position;
					entity->position = test_pos;
					
					Renderer_submitEntity(renderer, entity);
					entity->position = orig;
				}
			}
		}
	}
}


CollisionResult
heightmapSceneCollision(Scene *scene, Entity *entity, Vector3 to)
{
    Heightmap     *map  = Scene_getData(scene);
    HeightmapData *data = &map->data;
    Vector3 from = entity->position;
    
    float half_world = map->world_size * 0.5f;
    
    // Normalize positions to world bounds for terrain sampling only
    Vector3 from_normalized = {
        fmodf(from.x + half_world * 3.0f, map->world_size) - half_world,
        from.y,
        fmodf(from.z + half_world * 3.0f, map->world_size) - half_world
    };
    
    Vector3 to_normalized = {
        fmodf(to.x + half_world * 3.0f, map->world_size) - half_world,
        to.y,
        fmodf(to.z + half_world * 3.0f, map->world_size) - half_world
    };
    
	if (to.y > from.y) {
        return NO_COLLISION;
    }
    
    float entity_half_height = 0.0f;
    if (entity->collision_shape == COLLISION_CYLINDER || 
        entity->collision_shape == COLLISION_SPHERE) {
        entity_half_height = entity->height * 0.5f;
    }
    
    float 
		terrain_at_from = getTerrainHeight(map, from_normalized) + data->offset + entity_half_height,
		terrain_at_to   = getTerrainHeight(map, to_normalized)   + data->offset + entity_half_height;
    
    if (from.y > terrain_at_from && to.y > terrain_at_to) {
        // Check wrapping BEFORE returning no collision
        if (to.x > half_world || to.x < -half_world || to.z > half_world || to.z < -half_world) {
            Vector3 wrapped = to;
            while (wrapped.x > half_world) wrapped.x -= map->world_size;
            while (wrapped.x < -half_world) wrapped.x += map->world_size;
            while (wrapped.z > half_world) wrapped.z -= map->world_size;
            while (wrapped.z < -half_world) wrapped.z += map->world_size;
            Entity_teleport(entity, wrapped);
        }
        return NO_COLLISION;
    }
    
    Vector3 hit_floor_point;
    float distance;
    
    if (from.y > terrain_at_from && to.y <= terrain_at_to) {
        float t = invLerp(from.y, to.y, terrain_at_to);
        hit_floor_point = Vector3Lerp(from, to, t);
        hit_floor_point.y = terrain_at_to;
        distance = Vector3Distance(from, hit_floor_point);
	} else {
		hit_floor_point = (Vector3){to.x, terrain_at_to, to.z};
		Vector3 intended_move = Vector3Subtract(to, from);
		distance = Vector3Length(intended_move);
	}
    
    float 
		normalized_x = (to_normalized.x / map->world_size) + 0.5f,
		normalized_z = (to_normalized.z / map->world_size) + 0.5f;
    size_t 
		grid_x = (size_t)CLAMP((normalized_x * map->cells_wide), 0, map->cells_wide - 1),
		grid_z = (size_t)CLAMP((normalized_z * map->cells_wide), 0, map->cells_wide - 1);
    Vector3 normal = calculateVertexNormal(map, grid_x, grid_z, 1.0f, data->height_scale);
    
	float 
		dot_up = Vector3DotProduct(normal, V3_UP),
		floor_threshold = cosf(entity->floor_max_angle * DEG2RAD);

	if (dot_up > floor_threshold) {
		normal = V3_UP;
	}
    
    // Wrap entity after collision is resolved
    if (to.x > half_world || to.x < -half_world || to.z > half_world || to.z < -half_world) {
        Vector3 wrapped = to;
        while (wrapped.x > half_world) wrapped.x -= map->world_size;
        while (wrapped.x < -half_world) wrapped.x += map->world_size;
        while (wrapped.z > half_world) wrapped.z -= map->world_size;
        while (wrapped.z < -half_world) wrapped.z += map->world_size;
        Entity_teleport(entity, wrapped);
    }
    
    return (CollisionResult){
        .hit         = true,
        .distance    = distance,
        .position    = hit_floor_point,
        .normal      = normal,
        .material_id = 0,
        .user_data   = NULL,
        .entity      = NULL,
    };
}


CollisionResult 
heightmapSceneRaycast(Scene *scene, Vector3 from, Vector3 to)
{
    Heightmap     *map  = Scene_getData(scene);
    HeightmapData *data = &map->data;
    
    float half_world = map->world_size * 0.5f;
    
    // Calculate shortest path considering wrapping
    Vector3 delta = Vector3Subtract(to, from);
    
    // Wrap delta components to shortest path
    if (delta.x > half_world) delta.x -= map->world_size;
    else if (delta.x < -half_world) delta.x += map->world_size;
    
    if (delta.z > half_world) delta.z -= map->world_size;
    else if (delta.z < -half_world) delta.z += map->world_size;
    
    // Recalculate 'to' using shortest path
    to = Vector3Add(from, delta);
    
    // Now wrap both positions into world bounds for grid traversal
    from.x = fmodf(from.x + half_world + map->world_size, map->world_size) - half_world;
    from.z = fmodf(from.z + half_world + map->world_size, map->world_size) - half_world;
    to.x   = fmodf(to.x   + half_world + map->world_size, map->world_size) - half_world;
    to.z   = fmodf(to.z   + half_world + map->world_size, map->world_size) - half_world;
    
    float ray_length = Vector3Distance(from, to);
    
    int     grid_size             = map->cells_wide;
    float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
    
    CollisionResult result = NO_COLLISION;
    
    K_Ray ray = (K_Ray){
        .position  = from,
        .direction = Vector3Normalize(Vector3Subtract(to, from)),
        .length    = ray_length,
    };
    
    // Convert world positions to heightmap grid coordinates
    float 
        start_x = (from.x / map->world_size + 0.5f) * map->cells_wide,
        start_z = (from.z / map->world_size + 0.5f) * map->cells_wide,
        end_x   = (to.x / map->world_size + 0.5f)   * map->cells_wide,
        end_z   = (to.z / map->world_size + 0.5f)   * map->cells_wide;
    
    int 
        x0 = (int)floorf(start_x),
        z0 = (int)floorf(start_z),
        x1 = (int)floorf(end_x),
        z1 = (int)floorf(end_z);
    
    // Bresenham line algorithm to traverse grid cells
    int 
        dx = abs(x1 - x0),
        dz = abs(z1 - z0),
        sx = (x0 < x1) ? 1 : -1,
        sz = (z0 < z1) ? 1 : -1,
        err = dx - dz,
        x = x0,
        z = z0;
    
    float closest_distance = ray.length;
    
    // Traverse cells along the line
    while (true) {
        // Check if within bounds
        if (x >= 0 && x < (int)map->cells_wide && z >= 0 && z < (int)map->cells_wide) {
            // Get the four corners of this heightmap cell
            float cell_size = map->world_size / map->cells_wide;
            float world_x = (x / (float)map->cells_wide - 0.5f) * map->world_size;
            float world_z = (z / (float)map->cells_wide - 0.5f) * map->world_size;
            
            Vector3 p1 = {
                world_x,
                heightmap[z][x]         * data->height_scale + data->offset,
                world_z
            };
            Vector3 p2 = {
                world_x + cell_size,
                heightmap[z + 1][x]     * data->height_scale + data->offset,
                world_z
            };
            Vector3 p3 = {
                world_x + cell_size,
                heightmap[z + 1][x + 1] * data->height_scale + data->offset,
                world_z + cell_size
            };
            Vector3 p4 = {
                world_x,
                heightmap[z][x + 1]     * data->height_scale + data->offset,
                world_z + cell_size
            };
            
            // Test quad using raylib's GetRayCollisionQuad
            RayCollision collision = GetRayCollisionQuad(ray.ray, p1, p2, p3, p4);
            
            if (collision.hit && collision.distance < closest_distance) {
                closest_distance = collision.distance;
                result.hit = true;
                result.distance = collision.distance;
                result.position = collision.point;
                result.normal = collision.normal;
                result.material_id = 0;
                result.user_data = NULL;
                result.entity = NULL;
            }
        }
        
        // Break if we reached the end
        if (x == x1 && z == z1) break;
        
        // Bresenham step
        int e2 = 2 * err;
        if (e2 > -dz) {
            err -= dz;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            z += sz;
        }
    }
    
    return result;
}


void 
heightmapSceneFree(Scene *scene)
{
	Heightmap     *map  = (Heightmap*)Scene_getData(scene);
	DynamicArray_free(map->colormap);
	DynamicArray_free(map->normalmap);
	DynamicArray_free(map->chunks);
	DynamicArray_free(map->heightmap);
}


/*********************
	PUBLIC METHODS    
*********************/

Scene * 
HeightmapScene_new(HeightmapData *heightmap_data, Engine *engine)
{
	Heightmap heightmap;
	
	heightmap.cells_wide = heightmap_data->chunks_wide * heightmap_data->chunk_cells;
	heightmap.world_size = heightmap.cells_wide        * heightmap_data->cell_size;
	heightmap.data       = *heightmap_data;

	float 
		lod_scalar     = ( DEFAULT_MAX_RENDER_DISTANCE 
				/ (
					MAX_LOD_LEVELS 
					* heightmap_data->chunk_cells 
					* heightmap_data->cell_size
				)
			),
		lod_increment  = lod_scalar * sqrtf(
				pow(
						(heightmap_data->chunk_cells / 2) 
							* heightmap_data->cell_size,
						2
					) 
				* 2
			);
	for (int i = 0; i < MAX_LOD_LEVELS; i++) 
		heightmap.lod_distances[i] = lod_increment * (i+1);
	
	heightmap.heightmap  = genHeightmapDiamondSquare(
			heightmap.cells_wide,
			1.0f, 
			0.5f,
			0
		);
	
	return Scene_new(
			&heightmap_Scene_Callbacks, 
			NULL, 
			&heightmap, 
			sizeof(Heightmap), 
			engine
		);
}


HeightmapData *
HeightmapScene_getData(Scene *scene)
{
	Heightmap *map = (Heightmap*)scene;
	return &map->data;
}


Color
HeightmapScene_sampleShadow(Scene *scene, Vector3 pos)
{
	Heightmap      *map                   = Scene_getData(scene);
	size_t          cells_wide            = map->cells_wide;
	float           world_size            = map->world_size;
	Color         (*shadowmap)[cells_wide] = (Color(*)[cells_wide])map->shadowmap;
	
	struct TerrainSample sample = getTerrainSample(world_size, cells_wide, pos);

	Color
		c_nw  = shadowmap[sample.z0][sample.x0],
		c_ne  = shadowmap[sample.z0][sample.x1],
		c_sw  = shadowmap[sample.z1][sample.x0],
		c_se  = shadowmap[sample.z1][sample.x1],
		
		lower = ColorLerp(c_nw, c_ne, sample.x_frac),
		upper = ColorLerp(c_sw, c_se, sample.x_frac);
	
	return ColorLerp(lower, upper, sample.z_frac);
}


float
HeightmapScene_getWorldSize(Scene *scene)
{
	Heightmap *map = Scene_getData(scene);
	return map->world_size;
}


float
HeightmapScene_getHeight(Scene *scene, Vector3 pos)
{
	Heightmap *map = Scene_getData(scene);
	return getTerrainHeight(map, pos);
}
