#ifndef COMMON_H
#define COMMON_H

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __linux__
	#define ERR_OUT( Error_Text ) perror( "[ERROR] " Error_Text "\n" )    
#else
    #define ERR_OUT( Error_Text )
#endif

/****************
	CONSTANTS
****************/
#ifdef PLATFORM_PSP
    #define SCREEN_WIDTH  480
    #define SCREEN_HEIGHT 272
#elifdef __DREAMCAST__
    #define SCREEN_WIDTH  640
    #define SCREEN_HEIGHT 480
#elifdef __linux__
	#ifndef SCREEN_WIDTH
		#define SCREEN_WIDTH  854
	#endif
    #ifndef SCREEN_HEIGHT
		#define SCREEN_HEIGHT 480
	#endif
#endif

/* Engine-related constants */
#ifndef MAX_NUM_HEADS
	#define MAX_NUM_HEADS 4
#endif
/* Renderable-related constants */
#ifndef MAX_LOD_LEVELS
	#define MAX_LOD_LEVELS 4
#endif
#ifndef MAX_RENDERABLES_PER_ENTITY  
	#define MAX_RENDERABLES_PER_ENTITY 4
#endif
/* Collision system-related constants */
#ifndef SPATIAL_HASH_SIZE
	#define SPATIAL_HASH_SIZE 1024
#endif
#ifndef CELL_SIZE
	#define CELL_SIZE 4.0f
#endif
#ifndef INITIAL_ENTITY_CAPACITY
	#define INITIAL_ENTITY_CAPACITY 256
#endif
#ifndef ENTRY_POOL_SIZE
	#define ENTRY_POOL_SIZE 2048
#endif
#ifndef QUERY_SIZE
	#define QUERY_SIZE 512
#endif

#define V3_ZERO ((Vector3){0.0f, 0.0f, 0.0f})
#define V3_UP   ((Vector3){0.0f, 1.0f, 0.0f})

/*
	COMMON ENUMERATIONS
*/
enum {
    POSITION,
    ROTATION,
    SCALE,
    SKEW,
};


typedef struct Engine Engine;
typedef struct Entity Entity;
typedef struct Head   Head;

/* 
	COMMON TYPES
*/

/* Value Types */
typedef uint64_t     uint64;
typedef uint16_t     uint16;
typedef uint8_t      uint8;
typedef int64_t      int64;
typedef int16_t      int16;
typedef int8_t       int8;
typedef unsigned int uint;
typedef size_t       word;

/* Structs */
typedef struct 
CollisionResult 
{
    Vector3 position;     /* Where collision occurred */
    Vector3 normal;       /* Surface normal at hit point */
    float   distance;     /* Distance to collision */
    int     material_id;  /* For different surface types */
    void*   user_data;    /* Scene-specific data (material, etc.) */
    Entity* entity;       /* If hit an entity (NULL for terrain) */
    bool    hit;
}
CollisionResult;

typedef struct
EntityList
{
	Entity **entities;
	uint     count;
	uint     capacity;
}
EntityList;


typedef enum 
{
    RENDERABLE_NONE,
    RENDERABLE_MODEL,
    RENDERABLE_BILLBOARD,
    RENDERABLE_PARTICLES,
    RENDERABLE_CUSTOM    
} 
RenderableType;

typedef struct 
Renderable
{
    RenderableType type;
    Vector3        offset;
    union {
        struct {
            Model *model;
            Color  tint;
        } 
        model;
        
        struct {
            Texture2D texture;
            Color     tint;
            bool      face_camera;
        } 
        billboard;
        
        struct {
            Texture2D  texture;
            Vector3   *positions;
            Color     *colors;
            int        count;
            int        max_count;
        } 
        particles;
        
        struct {
            void  *data;
            void (*draw)(void* data, Vector3 position, Vector3 rotation, Vector3 scale);
        } 
        custom;
    };
} 
Renderable;


typedef struct
Vector2i
{
	union {
		struct {
			int x;
			int y;
		};
		struct {
			int w;
			int h;
		};
		int v[2];
	};
}
Vector2i;

typedef struct 
{
    Entity **entities;
    int 
		count,
		capacity;
} 
VisibleSet;

typedef struct
Xform
{
	Vector3 
		position, 
		rotation,
		scale,
		skew;
}
Xform;


#endif /* COMMON_H */
