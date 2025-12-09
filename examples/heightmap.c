#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "dynamicarray.h"
#include "engine.h"
#include "entity.h"
#include <raylib.h>
#include <raymath.h>
#include "heightmap.h"
#include "renderer.h"


#define INDEX(array, width, x, y) (array[y * width + x])


typedef struct
{
	Mesh      mesh;
	Model     model;
	Heightmap base;
}
HeightmapData;


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


float 
randomRange(float range) 
{
    return ((float)rand() / RAND_MAX) * 2.0f * range - range;
}

// Set height at position with bounds checking
void 
setHeight(Heightmap *map, int x, int y, float height) 
{
	int     grid_size               = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	
    if (x >= 0 && x <= map->cells_wide && y >= 0 && y <= map->cells_wide) {
        heightmap[y][x] = height;
    }
}

// Get height at position with bounds checking
float 
getHeight(Heightmap *map, int x, int y) 
{
	int     grid_size               = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	
    if (x >= 0 && x <= map->cells_wide && y >= 0 && y <= map->cells_wide) {
        return heightmap[y][x];
    }
    return 0.0f;
}


void
diamond(
	Heightmap *map, 
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
	Heightmap *map, 
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
	Heightmap *map, 
	float      Initial_Roughness, 
	float      Roughness_Decay, 
	size_t     Seed
)
{
	int     grid_size               = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	
	srand(Seed);

	setHeight(map, 0,                   0,                   randomRange(Initial_Roughness));
	setHeight(map, map->cells_wide, 0,                   randomRange(Initial_Roughness));
	setHeight(map, 0,                   map->cells_wide, randomRange(Initial_Roughness));
	setHeight(map, map->cells_wide, map->cells_wide, randomRange(Initial_Roughness));

	float roughness = Initial_Roughness;

	/* Diamond-Square algorithm */
	for (int size = map->cells_wide; 1 < size; size /= 2) {
		int half = size / 2;

		for (int y = half; y < map->cells_wide; y += size) {
			for (int x = half; x < map->cells_wide; x += size) {
				diamond(map, x, y, size, roughness);
			}
		}

		for (int y = 0; y <= map->cells_wide; y += half) {
			for (int x = (y + half) % size; x <= map->cells_wide; x += size) {
				square(map, x, y, size, roughness);
			}
		}

		roughness *= Roughness_Decay;
	}

	/* Normalize heightmap to 0.0-1.0 range */
	float
		min_height = heightmap[0][0],
		max_height = heightmap[0][0];

	for (int y = 0; y <= map->cells_wide; y++) {
		for (int x = 0; x <= map->cells_wide; x++) {
			if (heightmap[y][x] < min_height) min_height  = heightmap[y][x];
			if (max_height < heightmap[y][x]) max_height = heightmap[y][x];
		}
	}

	float range = max_height - min_height;
	if (range < 0.0f) {
		return; 
	}

	for (int y = 0; y <= map->cells_wide; y++) {
		for (int x = 0; x <= map->cells_wide; x++) {
			heightmap[y][x]= (heightmap[y][x]- min_height) / range;
		}
	}
}


void
diamondSquare(
	Heightmap *map, 
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
getLightingFactor(Vector3 normal, float ambient) 
{
	float dot = Vector3DotProduct(normal, Vector3Normalize(SUN_ANGLE));
	return (dot < 0.0f) ? ambient : ambient + (dot * (1.0f - ambient));
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
genTerrain(
	Heightmap *map
)
{
	int     grid_size               = map->cells_wide + 1;
	float (*heightmap)[grid_size] = (float(*)[grid_size])map->heightmap;
	
	float 
		size       = map->world_size,//DynamicArray_length(map->heightmap),
		offset     = map->offset;
	int resolution = map->cells_wide;
	Color 
		color1 = map->hi_color,
		color2 = map->lo_color;
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
				heightmap[z][x] * map->height_scale + offset,
				((float)z / resolution - 0.5f) * size
			};

			tex_coords[i] = (Vector2){
				(float)x / resolution,
				(float)z / resolution
			};

			normals[i] = calculateVertexNormal(map, x, z, 1.0f, map->height_scale);
			float lighting_factor = getLightingFactor(normals[i], 0.5f);
			colors[i]  = (Color){
				(unsigned char)(Lerp(color2.r, color1.r, heightmap[z][x]  * lighting_factor)),
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
	
	float
		x_frac = float_x - x,
		z_frac = float_z - z,
		// Interpolate bottom edge (z row) along x
		lower = Lerp(heightmap[z][x], heightmap[z][x+1], x_frac),
		// Interpolate top edge (z+1 row) along x
		upper = Lerp(heightmap[z+1][x], heightmap[z+1][x+1], x_frac);
	
	// Interpolate between bottom and top edges along z
	return Lerp(lower, upper, z_frac) * map->height_scale;
}

float *
genHeightmapXOR()
{
    const int size = 256 + 1;  
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
	
}

Scene * 
HeightmapScene_new(Heightmap *heightmap, Engine *engine)
{
	HeightmapData heightmap_data;
	heightmap_data.base  = *heightmap;
	return Scene_new(
			&heightmap_Scene_Callbacks, 
			NULL, 
			&heightmap_data, 
			sizeof(HeightmapData), 
			engine
		);
}


void
heightmapSceneSetup(Scene *scene, void *map_data)
{
	HeightmapData *data = (HeightmapData*)map_data;
	Heightmap *heightmap = &data->base;
	
    int grid_size = heightmap->cells_wide + 1;
    float (*hm)[grid_size] = (float(*)[grid_size])heightmap->heightmap;
           
	data->mesh  = genTerrain(heightmap);
	data->model = LoadModelFromMesh(data->mesh);
}

void 
heightmapSceneRender(Scene *scene, Head *head)
{
	Renderer      *renderer = Engine_getRenderer(Scene_getEngine(scene));
	HeightmapData *data     = Scene_getMapData(scene);
	Heightmap     *map      = &data->base;
	
	DrawModel(data->model, V3_ZERO, 1.0f, WHITE);
	
	BoundingBox bbox = {
			{-map->world_size/2, 0.0f, -map->world_size/2},
			{map->world_size/2, map->height_scale, map->world_size/2}
		};
	DrawBoundingBox(bbox, RED);
	
	EntityList *ent_list = Scene_getEntityList(scene);
	for (int i = 0; i < ent_list->count; i++) {
		Renderer_submitEntity(renderer, ent_list->entities[i]);
	}
}

CollisionResult
heightmapSceneCollision(Scene *scene, Entity *entity, Vector3 to)
{
    HeightmapData *data = Scene_getMapData(scene);
    Heightmap     *map  = &data->base;
    Vector3 from = entity->position;
    
    // Calculate entity half height for centering
    float entity_half_height = 0.0f;
    if (entity->collision_shape == COLLISION_CYLINDER || 
        entity->collision_shape == COLLISION_SPHERE) {
        entity_half_height = entity->height * 0.5f;
    }
    
    // Get terrain surface heights (where entity center should be)
    float terrain_at_from = getTerrainHeight(map, from) + map->offset + entity_half_height;
    float terrain_at_to = getTerrainHeight(map, to) + map->offset + entity_half_height;
    
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
    float normalized_x = (to.x / map->world_size) + 0.5f;
    float normalized_z = (to.z / map->world_size) + 0.5f;
    int grid_x = CLAMP((int)(normalized_x * map->cells_wide), 0, map->cells_wide);
    int grid_z = CLAMP((int)(normalized_z * map->cells_wide), 0, map->cells_wide);
    Vector3 normal = calculateVertexNormal(map, grid_x, grid_z, 1.0f, map->height_scale);
    
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
    HeightmapData *data = Scene_getMapData(scene);
    Heightmap     *map  = &data->base;
    
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
                heightmap[z][x]         * map->height_scale + map->offset,
                world_z
            };
            Vector3 p2 = {
                world_x + cell_size,
                heightmap[z + 1][x]     * map->height_scale + map->offset,
                world_z
            };
            Vector3 p3 = {
                world_x + cell_size,
                heightmap[z + 1][x + 1] * map->height_scale + map->offset,
                world_z + cell_size
            };
            Vector3 p4 = {
                world_x,
                heightmap[z][x + 1]     * map->height_scale + map->offset,
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
	HeightmapData *data = (HeightmapData*)map_data;
	Heightmap     *map  = &data->base;
	DynamicArray_free(map->heightmap);
	UnloadModel(data->model);
	UnloadMesh( data->mesh);
}

