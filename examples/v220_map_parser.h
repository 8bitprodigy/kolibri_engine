#ifndef V220_MAP_PARSER_H
#define V220_MAP_PARSER_H


#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2

#define MAX_BRUSHES 8192
#define MAX_PLANES 32768
#define MAX_ENTITIES 4096
#define MAX_BRUSH_PLANES 128
#define MAX_ENTITY_KEYS 64
#define MAX_LINE_LENGTH 1024
#define MAX_TOKEN_LENGTH 256

typedef struct 
{
    int 
        x_to,      // Which engine axis does map's X go to (0, 1, or 2)
        y_to,      // Which engine axis does map's Y go to (0, 1, or 2)
        z_to,      // Which engine axis does map's Z go to (0, 1, or 2)
        flip_x,    // 1 to negate X, 0 to keep
        flip_y,    // 1 to negate Y, 0 to keep
        flip_z;    // 1 to negate Z, 0 to keep
    float scale;
} 
AxisRemapping;

// Preset configurations
static const AxisRemapping AXIS_REMAP_NONE   = {0, 1, 2, 0, 0, 0, 1};  // Quake/idTech (Z-up)
static const AxisRemapping AXIS_REMAP_RAYLIB = {0, 2, 1, 0, 1, 0, 1.0f/64.0f};  // Raylib (Y-up)

typedef struct 
{
    Vector3 normal;
    float distance;
    char texture[64];
    Vector3 u_axis, v_axis;
    float u_offset, v_offset;
    float rotation;
    float u_scale, v_scale;
} 
MapPlane;

typedef struct 
{
    MapPlane planes[MAX_BRUSH_PLANES];
    int plane_count;
} 
MapBrush;

typedef struct 
{
    char key[64];
    char value[256];
} 
EntityKeyValue;

typedef struct 
{
    EntityKeyValue properties[MAX_ENTITY_KEYS];
    int property_count;
    MapBrush brushes[MAX_BRUSH_PLANES]; // Brush entities can have brushes
    int brush_count;
} 
MapEntity;

typedef struct 
{
    MapEntity entities[MAX_ENTITIES];
    int entity_count;
    MapBrush world_brushes[MAX_BRUSHES];
    int world_brush_count;
} 
MapData;


MapData*    ParseValve220Map( const char *filename, AxisRemapping remap);
void        FreeMapData(      MapData    *map);
const char* GetEntityProperty(MapEntity  *entity, const char *key);


#endif /* V220_MAP_PARSER_H */
