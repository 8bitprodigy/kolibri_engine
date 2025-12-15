#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "dynamicarray.h"
#include "engine.h"
#include "entity.h"
#include "heightmap.h"
#include "renderer.h"


#define INDEX(array, width, x, y) (array[y * width + x])

typedef struct
{
	Renderable renderable;
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
		int posa[2];
	};
	Vector3
		position,
		bounds;
}
ChunkData;

typedef struct
{
	HeightmapData data;

	float      world_size;
	size_t     cells_wide;
	float     *heightmap;
	Color     *colormap;
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
void            heightmapSceneFree(     Scene *scene, void   *map_data);


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


const Vector3 SUN_ANGLE = (Vector3){0.3f, -0.8f, 0.3f};


void
RenderHeightMapChunk(Renderable *renderable, void *_data_, Camera3D *camera)
{
	Heightmap     *map  = (Heightmap*)renderable->data;
	HeightmapData *data = (HeightmapData*)&map->data;

	ChunkData *chunk = (ChunkData*)renderable;

	float 
		offset       = data->offset,
		height_scale = data->height_scale,
		cell_size    = data->cell_size;
		
	int
		chunk_cells = data->chunk_cells,
		chunks_wide = data->chunks_wide,
		
		chunk_x = chunk->idx.x,
		chunk_z = chunk->idx.z,

		grid_size      = data->chunk_cells + 1,
		total_cells    = data->chunks_wide * data->chunk_cells,
		heightmap_grid = total_cells + 1;

	float (*heightmap)[heightmap_grid]   = (float(*)[heightmap_grid])map->heightmap;
	Color (*colormap)[heightmap_grid]    = (Color(*)[heightmap_grid])map->colormap;
	Vector3 (*normalmap)[heightmap_grid] = (Vector3(*)[heightmap_grid])map->normalmap;

	int
		start_x = chunk_x * chunk_cells,
		start_z = chunk_z * chunk_cells;

	float
		chunk_offset_x = (chunk_x - chunks_wide / 2.0f) * chunk_cells * cell_size,
		chunk_offset_z = (chunk_z - chunks_wide / 2.0f) * chunk_cells * cell_size;

	DBG_EXPR(
			DBG_OUT(
					"Rendering chunk %d,%d at pos (%.1f, %.1f, %.1f)", 
					chunk->chunk_x, 
					chunk->chunk_z, 
					chunk->position.x, 
					chunk->position.y, 
					chunk->position.z
				);

			DrawCubeWires(
					chunk->position, 
					chunk->bounds.x, 
					chunk->bounds.y, 
					chunk->bounds.z, 
					ORANGE
				);
		);
	
	rlPushMatrix();
	
	rlBegin(RL_TRIANGLES);

	if (data->texture.id != 0) {
		rlSetTexture(data->texture.id);
	}
	
	for (int z = 0; z < chunk_cells; z++) {
		for (int x = 0; x < chunk_cells; x++) {
			int
				hm_x = start_x + x,
				hm_z = start_z + z;

			Vector3
				v_tl = {
						x * cell_size + chunk_offset_x,
						heightmap[hm_z][hm_x] * height_scale + offset,
						z * cell_size + chunk_offset_z
					},
				v_tr = {
						(x + 1) * cell_size + chunk_offset_x,
						heightmap[hm_z][hm_x + 1] * height_scale + offset,
						z * cell_size + chunk_offset_z
					},
				v_bl = {
						x * cell_size + chunk_offset_x,
						heightmap[hm_z + 1][hm_x] * height_scale + offset,
						(z + 1) * cell_size + chunk_offset_z
					},
				v_br = {
						(x + 1) * cell_size + chunk_offset_x,
						heightmap[hm_z + 1][hm_x + 1] * height_scale + offset,
						(z + 1) * cell_size + chunk_offset_z,
					};

			Color 
				c_tl = colormap[hm_z][hm_x],
				c_tr = colormap[hm_z][hm_x + 1],
				c_bl = colormap[hm_z + 1][hm_x],
				c_br = colormap[hm_z + 1][hm_x + 1];

			Vector3
				n_tl = normalmap[hm_z][hm_x],
				n_tr = normalmap[hm_z][hm_x + 1],
				n_bl = normalmap[hm_z + 1][hm_x],
				n_br = normalmap[hm_z + 1][hm_x + 1];

			Vector2
				uv_tl = {0.0f, 0.0f},
				uv_tr = {1.0f, 0.0f},
				uv_bl = {0.0f, 1.0f},
				uv_br = {1.0f, 1.0f};

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


float 
randomRange(float range) 
{
    return ((float)rand() / RAND_MAX) * 2.0f * range - range;
}

// Set height at position with bounds checking
void 
setHeight(float *heightmap_data, size_t cells_wide, int x, int y, float height) 
{
    size_t  grid_size = cells_wide + 1;
    float (*heightmap)[grid_size] = (float(*)[grid_size])heightmap_data;
	
    if (x >= 0 && x <= cells_wide && y >= 0 && y <= cells_wide) {
        heightmap[y][x] = height;
    }
}

// Get height at position with bounds checking
float 
getHeight(float *heightmap_data, size_t cells_wide, int x, int y) 
{
    size_t  grid_size = cells_wide + 1;
    float (*heightmap)[grid_size] = (float(*)[grid_size])heightmap_data;
	
    if (x >= 0 && x <= cells_wide && y >= 0 && y <= cells_wide) {
        return heightmap[y][x];
    }
    return 0.0f;
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
	int     grid_size               = cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])heightmap_data;
	
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

		for (int y = 0; y <= cells_wide; y += half) {
			for (int x = (y + half) % size; x <= cells_wide; x += size) {
				square(heightmap_data, cells_wide, x, y, size, roughness);
			}
		}

		roughness *= Roughness_Decay;
	}

	/* Normalize heightmap to 0.0-1.0 range */
	float
		min_height = heightmap[0][0],
		max_height = heightmap[0][0];

	for (int y = 0; y <= cells_wide; y++) {
		for (int x = 0; x <= cells_wide; x++) {
			if (heightmap[y][x] < min_height) min_height  = heightmap[y][x];
			if (max_height < heightmap[y][x]) max_height = heightmap[y][x];
		}
	}

	float range = max_height - min_height;
	if (range < 0.0f) {
		return; 
	}

	for (int y = 0; y <= cells_wide; y++) {
		for (int x = 0; x <= cells_wide; x++) {
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
	int     grid_size               = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	
	float 
		left   = (x > 0) ? heightmap[z-1][x]               : heightmap[z][x],
		right  = (x < map->cells_wide) ? heightmap[z+1][x] : heightmap[z][x],
		up     = (z > 0) ? heightmap[z][x-1]               : heightmap[z][x],
		down   = (z < map->cells_wide) ? heightmap[z][x+1] : heightmap[z][x];

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
getLightingFactor(Vector3 normal, Vector3 sun_angle, float ambient) 
{
	float dot = -Vector3DotProduct(normal, Vector3Normalize(sun_angle));
	return  ambient + (dot * (1.0f - ambient));
}

float 
getSimpleLighting(Heightmap *map, int x, int z) 
{
	int     grid_size               = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	
    float current_height  = heightmap[z][x];
    float neighbor_height = 0.0f;
    int   neighbor_count  = 0;
    
    // Average neighboring heights
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) continue;
            
            int nx = x + dx;
            int nz = z + dz;
            
            if (nx >= 0 && nx <= map->cells_wide && 
                nz >= 0 && nz <= map->cells_wide) {
                neighbor_height += heightmap[nz][nx];
                neighbor_count++;
            }
        }
    }
    
    if (neighbor_count > 0) {
        neighbor_height /= neighbor_count;
        
        // Calculate relative height difference
        float height_diff = current_height - neighbor_height;
        
        // Convert to lighting factor (higher = brighter)
        return 0.5f + (height_diff * 2.0f); // Adjust multiplier as needed
    }
    
    return 0.8f; // Default lighting
}

Mesh
genTerrain(Heightmap *map)
{
	int     grid_size             = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	HeightmapData *data           = (HeightmapData*)&map->data; 
	
	float 
		size          = map->world_size,//DynamicArray_length(map->heightmap),
		offset        = data->offset;
	size_t resolution = map->cells_wide;
	Color 
		color1 = data->hi_color,
		color2 = data->lo_color;
	Mesh mesh = {0};
	int 
		edge_vert_count = resolution + 1,
		vertex_count = edge_vert_count * edge_vert_count;

	Vector3 *vertices   = (Vector3 *)malloc(vertex_count * sizeof(Vector3));
	Vector2 *tex_coords = (Vector2 *)malloc(vertex_count * sizeof(Vector2));
	Vector3 *normals    = (Vector3 *)malloc(vertex_count * sizeof(Vector3));
	Color   *colors     = (Color   *)malloc(vertex_count * sizeof(Color));

	for (int z = 0; z < edge_vert_count; z++) {
		for (int x = 0; x < edge_vert_count; x++) {
			int i = z * edge_vert_count + x;

			vertices[i] = (Vector3){
				((float)x / resolution - 0.5f) * size,
				heightmap[z][x] * data->height_scale + offset,
				((float)z / resolution - 0.5f) * size
			};

			tex_coords[i] = (Vector2){
				(float)x,// / resolution,
				(float)z,// / resolution
			};

			normals[i] = calculateVertexNormal(map, x, z, 1.0f, data->height_scale);
			float lighting_factor = getLightingFactor(
				normals[i], 
				data->sun_angle, 
				data->ambient_value
			);

			colors[i] = (Color){
				(unsigned char)(Lerp(color2.r, color1.r, heightmap[z][x]) * lighting_factor),
				(unsigned char)(Lerp(color2.g, color1.g, heightmap[z][x]) * lighting_factor),
				(unsigned char)(Lerp(color2.b, color1.b, heightmap[z][x]) * lighting_factor),
				255
			};
		}
	}

	int triangle_count = resolution * resolution * 2;
	unsigned short *indices = (unsigned short *)malloc(triangle_count * 3 * sizeof(unsigned short));
	int index = 0;

	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			int
				topLeft     = z * edge_vert_count + x,
				topRight    = topLeft + 1,
				bottomLeft  = (z + 1) * edge_vert_count + x,
				bottomRight = bottomLeft + 1;

			/* First Triangle */
			indices[index++] = topLeft;
			indices[index++] = bottomLeft;
			indices[index++] = topRight;

			/* Second Triangle */
			indices[index++] = topRight;
			indices[index++] = bottomLeft;
			indices[index++] = bottomRight;
		}
	}

	mesh.vertexCount   = vertex_count;
	mesh.triangleCount = triangle_count;
	mesh.vertices      = (float *)vertices;
	mesh.texcoords     = (float *)tex_coords;
	mesh.normals       = (float *)normals;
	mesh.colors        = (unsigned char *)colors;
	mesh.indices       = indices;

	UploadMesh(&mesh, false);
	
	return mesh;
}

float
getTerrainHeight(Heightmap *map, Vector3 position)
{
	int     grid_size               = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	
	float
		normalized_x = (position.x / map->world_size) + 0.5f,
		normalized_z = (position.z / map->world_size) + 0.5f,
		float_x = normalized_x * map->cells_wide,
		float_z = normalized_z * map->cells_wide;
    
    int 
		x = (int)floorf(float_x),
		z = (int)floorf(float_z);

	if (x < 0 || z < 0 
		|| map->cells_wide <= x 
		|| map->cells_wide <= z
	) {
		return 0.0f;
	}
	HeightmapData *data = (HeightmapData*)&map->data;
	
	float
		x_frac = float_x - x,
		z_frac = float_z - z,
		// Interpolate bottom edge (z row) along x
		lower = Lerp(heightmap[z][x], heightmap[z][x+1], x_frac),
		// Interpolate top edge (z+1 row) along x
		upper = Lerp(heightmap[z+1][x], heightmap[z+1][x+1], x_frac);
	
	// Interpolate between bottom and top edges along z
	return Lerp(lower, upper, z_frac) * data->height_scale;
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
    size_t size = cells_wide + 1;
	float *heightmap = DynamicArray(float, size * size);
	
	diamondSquareSeeded(heightmap, cells_wide, roughness,decay, seed);

	return heightmap;
}

Vector3 *
generateNormalMap(Heightmap *map)
{
	HeightmapData  *data                  = &map->data;
	int             grid_size             = map->cells_wide + 1;
	float         (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;

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
	int             grid_size             = map->cells_wide + 1;
	float         (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	Vector3       (*normalmap)[grid_size] = (Vector3(*)[grid_size])map->normalmap;
	
	Color
	 	*colormap           = DynamicArray(Color, grid_size * grid_size),
		(*colors)[grid_size] = (Color(*)[grid_size])colormap;

	for (int z = 0; z < grid_size; z++) {
		for (int x = 0; x < grid_size; x++) {
			float lighting_factor = getLightingFactor(
					normalmap[z][x],
					data->sun_angle,
					data->ambient_value
				);

			Color
				hi_color = data->hi_color,
				lo_color = data->lo_color;
			
			colors[z][x] = (Color){
				(unsigned char)(Lerp(lo_color.r, hi_color.r, heightmap[z][x]) * lighting_factor),
				(unsigned char)(Lerp(lo_color.g, hi_color.g, heightmap[z][x]) * lighting_factor),
				(unsigned char)(Lerp(lo_color.b, hi_color.b, heightmap[z][x]) * lighting_factor),
				255
			};
		}
	}

	return colormap;
}

Scene * 
HeightmapScene_new(HeightmapData *heightmap_data, Engine *engine)
{
	Heightmap heightmap;
	
	heightmap.cells_wide = heightmap_data->chunks_wide * heightmap_data->chunk_cells;
	int grid_size        = heightmap.cells_wide + 1;
	heightmap.world_size = heightmap.cells_wide       * heightmap_data->cell_size;
	heightmap.data       = *heightmap_data;
	heightmap.heightmap  = genHeightmapDiamondSquare(
			heightmap.cells_wide,
			0.6, 
			0.3,
			69
		);
	
	return Scene_new(
			&heightmap_Scene_Callbacks, 
			NULL, 
			&heightmap, 
			sizeof(Heightmap), 
			engine
		);
}

ChunkData *
generateChunks(Heightmap *map)
{
	HeightmapData  *data                  = &map->data;
	int             grid_size             = map->cells_wide + 1;
	float         (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;

	int        num_chunks = data->chunks_wide * data->chunks_wide;
	ChunkData *chunks     = DynamicArray(ChunkData, num_chunks);

	for (int chunk_z = 0; chunk_z < data->chunks_wide; chunk_z++) {
		for (int chunk_x = 0; chunk_x < data->chunks_wide; chunk_x++) {
			int idx = chunk_z * data->chunks_wide + chunk_x;

			float
				chunk_offset_x = (chunk_x - data->chunks_wide * 0.5f) * data->chunk_cells * data->cell_size,
				chunk_offset_z = (chunk_z - data->chunks_wide * 0.5f) * data->chunk_cells * data->cell_size,
				chunk_size     = data->chunk_cells * data->cell_size;

			int
				start_x = chunk_x * data->chunk_cells,
				start_z = chunk_z * data->chunk_cells;

			float 
				min_height = INFINITY,
				max_height = -INFINITY;

			for (int z = 0; z <= data->chunk_cells; z++) {
				for (int x = 0; x <= data->chunk_cells; x++) {
					int 
						hm_x = start_x + x,
						hm_z = start_z + z;
					float h  = heightmap[hm_z][hm_x] * data->height_scale + data->offset;
					if (h < min_height) min_height = h;
					if (h > max_height) max_height = h;
				}
			}

			chunks[idx] = (ChunkData){
					.renderable = {
							.data        = map,
							.Render      = RenderHeightMapChunk,
							.transparent = false,
						},
					.chunk_x  = chunk_x,
					.chunk_z  = chunk_z,
					.position = (Vector3){
							chunk_offset_x + chunk_size * 0.5f,
							(min_height + max_height)   * 0.5f,
							chunk_offset_z + chunk_size * 0.5f
						},
					.bounds = (Vector3){
							chunk_size,
							max_height - min_height,
							chunk_size
						},
				};
		}
	}

	return chunks;
}


void
heightmapSceneSetup(Scene *scene, void *map_data)
{
	Heightmap     *map  = (Heightmap*)map_data;
	HeightmapData *data = (HeightmapData*)&map->data;

	size_t chunks_wide = data->chunks_wide;
	size_t num_chunks  = chunks_wide * chunks_wide;

	map->normalmap = generateNormalMap(map);
	map->colormap  = generateColorMap(map);
	map->chunks    = generateChunks(map);
	
	if (data->texture.id != 0) {
        GenTextureMipmaps(&data->texture);
		SetTextureFilter(  data->texture, TEXTURE_FILTER_TRILINEAR);
    }
}

void 
heightmapSceneRender(Scene *scene, Head *head)
{
	Renderer      *renderer = Engine_getRenderer(Scene_getEngine(scene));
	Heightmap     *map      = Scene_getMapData(scene);
	HeightmapData *data     = &map->data;
	
	DBG_EXPR( 
			float half_width = map->world_size/2.0f;
			
			BoundingBox bbox = {
					{-half_width, data->offset, -half_width},
					{half_width, data->height_scale, half_width}
				};
			DrawBoundingBox(bbox, MAGENTA);

			/* Solid are positive axes, wireframe are negative axes. */
			DrawCube(     (Vector3){ half_width, data->height_scale, 0.0f},        5.0f, 5.0f, 5.0f, RED);
			DrawCubeWires((Vector3){-half_width, data->height_scale, 0.0f},        5.0f, 5.0f, 5.0f, RED);
			DrawCube(     (Vector3){ 0.0f,       data->height_scale, 0.0f},        5.0f, 5.0f, 5.0f, GREEN);
			DrawCubeWires((Vector3){ 0.0f,       data->offset,       0.0f},        5.0f, 5.0f, 5.0f, GREEN);
			DrawCube(     (Vector3){0.0f,        data->height_scale,  half_width}, 5.0f, 5.0f, 5.0f, BLUE);
			DrawCubeWires((Vector3){0.0f,        data->height_scale, -half_width}, 5.0f, 5.0f, 5.0f, BLUE);
		);
//*/
	size_t num_chunks = data->chunks_wide * data->chunks_wide;
	for (int i = 0; i < num_chunks; i++) 
		Renderer_submitGeometry(
				renderer, 
				&map->chunks[i].renderable, 
				map->chunks[i].position,
				map->chunks[i].bounds
			);
	EntityList *ent_list = Scene_getEntityList(scene);
	for (int i = 0; i < ent_list->count; i++) 
		Renderer_submitEntity(renderer, ent_list->entities[i]);
}

CollisionResult
heightmapSceneCollision(Scene *scene, Entity *entity, Vector3 to)
{
    Heightmap     *map  = Scene_getMapData(scene);
    HeightmapData *data = &map->data;
    Vector3 from = entity->position;
    
	if (to.y > from.y) {
        return NO_COLLISION;
    }
    // Calculate entity half height for centering
    float entity_half_height = 0.0f;
    if (entity->collision_shape == COLLISION_CYLINDER || 
        entity->collision_shape == COLLISION_SPHERE) {
        entity_half_height = entity->height * 0.5f;
    }
    
    // Get terrain surface heights (where entity center should be)
    float 
		terrain_at_from = getTerrainHeight(map, from) + data->offset + entity_half_height,
		terrain_at_to   = getTerrainHeight(map, to)   + data->offset + entity_half_height;
    
    // If both positions above terrain, no collision (like infinite plane)
    if (from.y > terrain_at_from && to.y > terrain_at_to) {
        return NO_COLLISION;
    }
    
    // Calculate hit point and distance
    Vector3 hit_floor_point;
    float distance;
    
    if (from.y > terrain_at_from && to.y <= terrain_at_to) {
        // Crossing from above to terrain - interpolate to find hit point
        float t = invLerp(from.y, to.y, terrain_at_to);
        hit_floor_point = Vector3Lerp(from, to, t);
        hit_floor_point.y = terrain_at_to;  // Snap to exact terrain height
        distance = Vector3Distance(from, hit_floor_point);
	} else {
		// Already at/below terrain
		hit_floor_point = (Vector3){to.x, terrain_at_to, to.z};
		
		// Return distance as the FULL intended movement, not just vertical adjustment
		// This allows horizontal movement while correcting Y
		Vector3 intended_move = Vector3Subtract(to, from);
		distance = Vector3Length(intended_move);  // Full movement distance
	}
    
    // Calculate normal
    float 
		normalized_x = (to.x / map->world_size) + 0.5f,
		normalized_z = (to.z / map->world_size) + 0.5f;
    int 
		grid_x = CLAMP((int)(normalized_x * map->cells_wide), 0, map->cells_wide),
		grid_z = CLAMP((int)(normalized_z * map->cells_wide), 0, map->cells_wide);
    Vector3 normal = calculateVertexNormal(map, grid_x, grid_z, 1.0f, data->height_scale);
    // Check if this is a walkable floor
	float 
		dot_up = Vector3DotProduct(normal, V3_UP),
		floor_threshold = cosf(entity->floor_max_angle * DEG2RAD);

	// If it's a walkable floor, return vertical normal so slide doesn't deflect horizontally
	if (dot_up > floor_threshold) {
		normal = V3_UP;
	}
    return (CollisionResult){
        true,
        distance,
        hit_floor_point,
        normal,
        0,
        NULL,
        NULL,
    };
}

CollisionResult 
heightmapSceneRaycast(Scene *scene, Vector3 from, Vector3 to)
{
    Heightmap     *map  = Scene_getMapData(scene);
    HeightmapData *data = &map->data;
    
	int     grid_size             = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	
    CollisionResult result = NO_COLLISION;
    
    K_Ray ray = (K_Ray){
        .position  = from,
        .direction = Vector3Normalize(Vector3Subtract(to, from)),
        .length    = Vector3Distance(from, to),
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
        if (x >= 0 && x < map->cells_wide && z >= 0 && z < map->cells_wide) {
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
heightmapSceneFree(Scene *scene, void *map_data)
{
	Heightmap     *map  = (Heightmap*)map_data;
	HeightmapData *data = &map->data;
	DynamicArray_free(map->heightmap);
	DynamicArray_free(map->colormap);
	DynamicArray_free(map->normalmap);
	DynamicArray_free(map->chunks);
}

