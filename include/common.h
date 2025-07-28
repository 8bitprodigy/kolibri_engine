#ifndef COMMON_H
#define COMMON_H

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
    #define DBG_EXPR( expression ) expression
    #define DBG_OUT( Text, ... ) do{printf( "[DEBUG] " Text "\n", ##__VA_ARGS__ ); fflush(stdout);} while(false)
    #define DBG_LINE( vec2_1, vec2_2, height, color ) DrawLine3D(VECTOR2_TO_3( (vec2_1), (height) ), VECTOR2_TO_3( (vec2_2), (height) ), (color))
	#define ERR_OUT( Error_Text ) perror( "[ERROR] " Error_Text "\n" )    
#else
    #define DBG_EXPR( expression ) 
    #define DBG_OUT( Text, ... )
    #define DBG_LINE( vec2_1, vec2_2, height, color )
    #define ERR_OUT( Error_Text )
#endif

#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

/****************
	CONSTANTS
****************/
/* Engine-related constants */
#ifndef MAX_NUM_HEADS
	#define MAX_NUM_HEADS 4
#endif
#ifndef MAX_NUM_ENTITIES
	#define MAX_NUM_ENTITIES 1024
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

#define V2_ZERO      ((Vector2){0.0f, 0.0f})
#define V3_ZERO      ((Vector3){0.0f, 0.0f, 0.0f})
#define V3_ONE       ((Vector3){1.0f, 1.0f, 1.0f})
#define V3_UP        ((Vector3){0.0f, 1.0f, 0.0f})
#define XF_ZERO      ((Xform){V3_ZERO,V3_ZERO,V3_ZERO,V3_ZERO})
#define NO_COLLISION ((CollisionResult){V3_ZERO,V3_ZERO,0.0f,0,NULL,NULL,false})

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
#ifdef __DREAMCAST__
	#include <arch/types.h>
#else
typedef uint64_t     uint64;
typedef uint32_t     uint32;
typedef uint16_t     uint16;
typedef uint8_t      uint8;
typedef int64_t      int64;
typedef int32_t      int32;
typedef int16_t      int16;
typedef int8_t       int8;
#endif
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
    void   *user_data;    /* Scene-specific data (material, etc.) */
    Entity *entity;       /* If hit an entity (NULL for terrain) */
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


/*
	Renderable
*/
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
    RenderableType      type;
    void               *data;
    void (*Render)(struct Renderable *renderable, Entity *entity);
} 
Renderable;
/*
	/Renderable
*/


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
Xform
{
	union {
		Vector3 xf[4];
		struct {
			Vector3 
				position, 
				rotation,
				scale,
				skew;
		};
	};
}
Xform;


#endif /* COMMON_H */
