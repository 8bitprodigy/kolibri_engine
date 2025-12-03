#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "dynamicarray.h"
#include <raylib.h>
#include <raymath.h>
#include "heightmap.h"


#define INDEX(array, width, x, y) (array[y * width + x])


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
XORHeightmap(HeightmapData *map)
{
	static const int scale = 256/TERRAIN_NUM_SQUARES;
	for (int y = 0; y < TERRAIN_NUM_SQUARES+1; y++) {
		for (int x = 0; x < TERRAIN_NUM_SQUARES+1; x++) {
			map->heightmap[(y*map->size)+x] = (float)(x ^ y)/TERRAIN_NUM_SQUARES;
		}
	}
}


float 
randomRange(float range) 
{
    return ((float)rand() / RAND_MAX) * 2.0f * range - range;
}

// Set height at position with bounds checking
void 
setHeight(HeightmapData *map, int x, int y, float height) 
{
    if (x >= 0 && x <= TERRAIN_NUM_SQUARES && y >= 0 && y <= TERRAIN_NUM_SQUARES) {
        map->heightmap[(y*map->size)+x]= height;
    }
}

// Get height at position with bounds checking
float 
getHeight(HeightmapData *map, int x, int y) 
{
    if (x >= 0 && x <= TERRAIN_NUM_SQUARES && y >= 0 && y <= TERRAIN_NUM_SQUARES) {
        return map->heightmap[(y*map->size)+x];
    }
    return 0.0f;
}


void
diamond(
	HeightmapData *map, 
	int    x, 
	int    y, 
	int    size,
	float  roughness
)
{
	int half = size / 2;
	float avg = (
			getHeight(  map, x - half, y - half)
			+ getHeight(map, x + half, y - half)
			+ getHeight(map, x - half, y + half)
			+ getHeight(map, x + half, y + half)
		) / 4.0f;

	setHeight(map, x, y, avg + randomRange(roughness));
}


void
square(
	HeightmapData *map, 
	int    x,
	int    y,
	int    size,
	float  roughness
)
{
	int half = size / 2;
	float avg = (
			getHeight(  map, x,        y - half)
			+ getHeight(map, x + half, y)
			+ getHeight(map, x,        y + half)
			+ getHeight(map, x - half, y)
		) / 4.0f;

	setHeight(map, x, y, avg + randomRange(roughness));
}


void
diamondSquareSeeded(
	HeightmapData *map, 
	float      Initial_Roughness, 
	float      Roughness_Decay, 
	size_t     Seed
)
{
	srand(Seed);

	setHeight(map, 0,                   0,                   randomRange(Initial_Roughness));
	setHeight(map, TERRAIN_NUM_SQUARES, 0,                   randomRange(Initial_Roughness));
	setHeight(map, 0,                   TERRAIN_NUM_SQUARES, randomRange(Initial_Roughness));
	setHeight(map, TERRAIN_NUM_SQUARES, TERRAIN_NUM_SQUARES, randomRange(Initial_Roughness));

	float roughness = Initial_Roughness;

	/* Diamond-Square algorithm */
	for (int size = TERRAIN_NUM_SQUARES; 1 < size; size /= 2) {
		int half = size / 2;

		for (int y = half; y < TERRAIN_NUM_SQUARES; y += size) {
			for (int x = half; x < TERRAIN_NUM_SQUARES; x += size) {
				diamond(map, x, y, size, roughness);
			}
		}

		for (int y = 0; y <= TERRAIN_NUM_SQUARES; y += half) {
			for (int x = (y + half) % size; x <= TERRAIN_NUM_SQUARES; x += size) {
				square(map, x, y, size, roughness);
			}
		}

		roughness *= Roughness_Decay;
	}

	/* Normalize heightmap to 0.0-1.0 range */
	float
		min_height = map->heightmap[0],
		max_height = map->heightmap[0];

	for (int y = 0; y <= TERRAIN_NUM_SQUARES; y++) {
		for (int x = 0; x <= TERRAIN_NUM_SQUARES; x++) {
			if (map->heightmap[(y*map->size)+x] < min_height) min_height  = map->heightmap[(y*map->size)+x];
			if (max_height < map->heightmap[(y*map->size)+x]) max_height = map->heightmap[(y*map->size)+x];
		}
	}

	float range = max_height - min_height;
	if (range < 0.0f) {
		return; 
	}

	for (int y = 0; y <= TERRAIN_NUM_SQUARES; y++) {
		for (int x = 0; x <= TERRAIN_NUM_SQUARES; x++) {
			map->heightmap[(y*map->size)+x]= (map->heightmap[(y*map->size)+x]- min_height) / range;
		}
	}
}


void
diamondSquare(
	HeightmapData *map, 
	float  Initial_Roughness, 
	float  Roughness_Decay
) 
{
	diamondSquareSeeded(map, Initial_Roughness, Roughness_Decay, (unsigned int)time(NULL));
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
	HeightmapData *map, 
	int    x, 
	int    z, 
	float  scale, 
	float  height_scale
)
{
	float 
		left   = (x > 0) ? map->heightmap[(z*map->size)+x-1]                   : map->heightmap[(z*map->size)+x],
		right  = (x < TERRAIN_NUM_SQUARES) ? map->heightmap[(z*map->size)+x+1] : map->heightmap[(z*map->size)+x],
		up     = (z > 0) ? map->heightmap[((z-1)*map->size)+x]                   : map->heightmap[(z*map->size)+x],
		down   = (z < TERRAIN_NUM_SQUARES) ? map->heightmap[((z+1)*map->size)+x] : map->heightmap[(z*map->size)+x];

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
    Vector3 normal = Vector3CrossProduct(tangent_x, tangent_z);
    return Vector3Normalize(normal);
}

float
getLightingFactor(Vector3 normal, float ambient) 
{
	float dot = Vector3DotProduct(normal, Vector3Normalize(SUN_ANGLE));
	return (dot < 0.0f) ? ambient : ambient + (dot * (1.0f - ambient));
}

float 
getSimpleLighting(HeightmapData *map, int x, int z) {
    float current_height  = map->heightmap[(z*map->size)+x];
    float neighbor_height = 0.0f;
    int   neighbor_count  = 0;
    
    // Average neighboring heights
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) continue;
            
            int nx = x + dx;
            int nz = z + dz;
            
            if (nx >= 0 && nx <= TERRAIN_NUM_SQUARES && 
                nz >= 0 && nz <= TERRAIN_NUM_SQUARES) {
                neighbor_height += map->heightmap[(nz*map->size)+nx];
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
genTerrain(
	HeightmapData *map,
	float size, 
	float offset, 
	int   resolution, 
	Color color1, 
	Color color2
)
{
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
				map->heightmap[(z*map->size)+x] * TERRAIN_HEIGHT_SCALE + offset,
				((float)z / resolution - 0.5f) * size
			};

			tex_coords[i] = (Vector2){
				(float)x / resolution,
				(float)z / resolution
			};

			normals[i] = calculateVertexNormal(map, x, z, 1.0f, TERRAIN_HEIGHT_SCALE);
			float lighting_factor = getLightingFactor(normals[i], 0.5f);
			colors[i]  = (Color){
				(unsigned char)(Lerp(color2.r, color1.r, map->heightmap[(z*map->size)+x]  * lighting_factor)),
				(unsigned char)(Lerp(color2.g, color1.g, map->heightmap[(z*map->size)+x]) * lighting_factor),
				(unsigned char)(Lerp(color2.b, color1.b, map->heightmap[(z*map->size)+x]) * lighting_factor),
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
getTerrainHeight(HeightmapData *map, Vector3 position)
{
	float
		normalized_x = (position.x / WORLD_SIZE) + 0.5f,
		normalized_z = (position.z / WORLD_SIZE) + 0.5f,
		float_x = normalized_x * TERRAIN_NUM_SQUARES,
		float_z = normalized_z * TERRAIN_NUM_SQUARES;
    
    int 
		x = (int)floorf(float_x),
		z = (int)floorf(float_z);

	if (x < 0 || z < 0 
		|| TERRAIN_NUM_SQUARES < x 
		|| TERRAIN_NUM_SQUARES < z
	) {
		return 0.0f;
	}
	
	float
		x_frac = float_x - x,
		z_frac = float_z - z,
		lower = Lerp(map->heightmap[(z*map->size)+x],     map->heightmap[(z*map->size)+x+1],   x_frac),
		upper = Lerp(map->heightmap[((z+1)*map->size)+x], map->heightmap[((z+1)*map->size)+x+1], x_frac);
	
	return Lerp(lower, upper, z_frac) * TERRAIN_HEIGHT_SCALE;
}
/*
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
*/
float *
genHeightmapXOR()
{
	float *heightmap = DynamicArray(float, 256*256);

	for (int y = 0; y < 256; y++)
		for (int x = 0; x < 256; x++)
			heightmap[y * 256 + x] = (float)(x^y)/256.0f;
	
	return heightmap;
}

float *
genHeightmapDiamondSquare(size_t cells_wide, size_t seed)
{
	
}

Scene * 
Heightmap_new(HeightmapData *heightmap_data)
{
	
}


void
heightmapSceneSetup(Scene *scene, void *map_data)
{
	
}

void 
heightmapSceneRender(Scene *scene, Head *head)
{
	
}

CollisionResult
heightmapSceneCollision(Scene *scene, Entity *entity, Vector3 to)
{
	
}

CollisionResult 
heightmapSceneRaycast(Scene *scene, Vector3 from, Vector3 to)
{
	
}

void 
heightmapSceneFree(Scene *scene, void *map_data)
{
	
}

