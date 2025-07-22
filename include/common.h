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

#ifdef PLATFORM_PSP
	#define MAX_NUM_HEADS 1
#else
	#define MAX_NUM_HEADS 4
#endif

#ifdef PLATFORM_PSP
    #define SCREEN_WIDTH  480
    #define SCREEN_HEIGHT 272
#elifdef __DREAMCAST__
    #define SCREEN_WIDTH  640
    #define SCREEN_HEIGHT 480
#elifdef __linux__
    #define SCREEN_WIDTH  854
    #define SCREEN_HEIGHT 480
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
}


typedef struct Engine Engine;
typedef struct Entity Entity;
typedef struct Head   Head;

/* 
	COMMON TYPES
*/
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
	Vector3 
		position, 
		rotation,
		scale,
		skew;
}
Xform;

/* Value Types */
typedef uint64_t     uint64;
typedef uint16_t     uint16;
typedef uint8_t      uint8;
typedef int64_t      int64;
typedef int16_t      int16;
typedef int8_t       int8;
typedef unsigned int uint;

#endif /* COMMON_H */
