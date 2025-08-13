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
	#define SPATIAL_HASH_SIZE 2053
#endif
#ifndef CELL_SIZE
	#define CELL_SIZE 20.0f
#endif
#ifndef INITIAL_ENTITY_CAPACITY
	#define INITIAL_ENTITY_CAPACITY 256
#endif
#ifndef ENTRY_POOL_SIZE
	#define ENTRY_POOL_SIZE 16384
#endif
#ifndef QUERY_SIZE
	#define QUERY_SIZE 1024
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
    Entity *entity;       /* If hit an entity (NULL for Scene) */
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

typedef struct 
Plane
{
	Vector3 normal;
	float   distance;
}
Plane;

typedef enum
{
	FRUSTUM_LEFT,
	FRUSTUM_RIGHT,
	FRUSTUM_TOP,
	FRUSTUM_BOTTOM,
	FRUSTUM_NEAR,
	FRUSTUM_FAR
}
FrustumPlaneIndices;

typedef struct 
Frustum
{
	Plane planes[6];

	Vector3
		forward,
		right,
		up,
		position;

	float
		hfov_rad,
		vfov_rad,
		horiz_limit,
		vert_limit,
		aspect_ratio;

	bool dirty;
}
Frustum;

typedef struct
K_Ray
{
	union {
		Ray ray;
		struct {
			Vector3
				position,
				direction;
		};
	};
	float length;
}
K_Ray;

/*
	Renderable
*/
typedef struct 
Renderable
{
    void  *data;
    void (*Render)(struct Renderable *renderable, void *data);
	union {
		uint8 flags;
		struct {
			bool transparent:1;
			bool flag_1     :1;
			bool flag_2     :1;
			bool flag_3     :1;
			bool flag_4     :1;
			bool flag_5     :1;
			bool flag_6     :1;
			bool flag_7     :1;
		};
	};
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


/*
	Utility Functions
*/
static int 
nextPrime(int n) {
    if (n <= 1) return 2;
    if (n <= 3) return n;
    if (n % 2 == 0) n++;
    
    while (true) {
        bool is_prime = true;
        for (int i = 3; i * i <= n; i += 2) {
            if (n % i == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime) return n;
        n += 2;
    }
}


#endif /* COMMON_H */
