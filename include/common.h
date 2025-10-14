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
    #define DBG_OUT(  Text, ... ) do{printf( "[DEBUG] " Text "\n", ##__VA_ARGS__ ); fflush(stdout);} while(false)
    #define ERR_OUT(  Error_Text ) perror( "[ERROR] " Error_Text "\n" )    
#else
    #define DBG_EXPR( expression ) 
    #define DBG_OUT(  Text, ... )
    #define ERR_OUT(  Error_Text )
#endif

#ifndef CLAMP
	#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))
#endif /* CLAMP */

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
#ifdef ENGINE_SINGLE_HEAD_ONLY
	/*
		If defined, only the first head can and will be used for rendering, and
		The scene will be rendered to the window context directly. 
	*/
	#define ENGINE_SINGLE_HEAD_ONLY 1
#endif
/* Head-related settings */
#ifdef HEAD_USE_RENDER_TEXTURE
	/* 
		If defined, Viewports will be rendered to render Textures instead of
		Scissor mode.
	*/
	#define HEAD_USE_RENDER_TEXTURE 1
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
	/* Should be a prime number */
	#define SPATIAL_HASH_SIZE 4099
#endif
#ifndef CELL_SIZE
	#define CELL_SIZE 20.0f
#endif
#ifndef INITIAL_ENTITY_CAPACITY
	#define INITIAL_ENTITY_CAPACITY 256
#endif
#ifndef ENTRY_POOL_SIZE
	#define ENTRY_POOL_SIZE 8192
#endif
#ifndef VIS_QUERY_SIZE
	#define VIS_QUERY_SIZE 1024
#endif
#ifndef COL_QUERY_SIZE
	#define COL_QUERY_SIZE 128
#endif

#define V2_ZERO      ((Vector2){0.0f, 0.0f})
#define V3_ZERO      ((Vector3){0.0f, 0.0f, 0.0f})
#define V4_ZERO      ((Vector4){0.0f, 0.0f, 0.0f, 0.0f})
#define V3_ONE       ((Vector3){1.0f, 1.0f, 1.0f})
#define V3_UP        ((Vector3){0.0f, 1.0f, 0.0f})
#define XF_ZERO      ((Xform){V3_ZERO,V3_ZERO,V3_ZERO,V3_ZERO})
#define NO_COLLISION ((CollisionResult){false,0.0f,V3_ZERO,V3_ZERO,0,NULL,NULL})


/**********************
	MACRO FUNCTIONS
**********************/
/*** Common Input Operations ***/

#define GET_KEY_OR_BUTTON_PRESSED( Controller, Button, Key ) (int)(IsGamepadButtonPressed(Controller, Button) || IsKeyPressed(Key))

#define GET_KEY_OR_BUTTON_DOWN( Controller, Button, Key ) (int)(IsGamepadButtonDown(Controller, Button) || IsKeyDown(Key))

#define GET_KEY_OR_BUTTON_AXIS( Controller, Btn_Pos, Key_Pos, Btn_Neg, Key_Neg ) ( \
        GET_KEY_OR_BUTTON_DOWN(   (Controller), (Btn_Pos), (Key_Pos) ) \
        - GET_KEY_OR_BUTTON_DOWN( (Controller), (Btn_Neg), (Key_Neg) ) \
    )

#define GET_KEY_OR_BUTTON_AXIS_PRESSED( Controller, Btn_Pos, Key_Pos, Btn_Neg, Key_Neg ) ( \
        GET_KEY_OR_BUTTON_PRESSED(   (Controller), (Btn_Pos), (Key_Pos) ) \
        - GET_KEY_OR_BUTTON_PRESSED( (Controller), (Btn_Neg), (Key_Neg) ) \
    )

#define GET_KEY_OR_BUTTON_VECTOR( Controller, Btn_Pos_X, Key_Pos_X, Btn_Neg_X, Key_Neg_X, Btn_Pos_Y, Key_Pos_Y, Btn_Neg_Y, Key_Neg_Y ) \
    (Vector2){ \
        GET_KEY_OR_BUTTON_AXIS( (Controller), (Btn_Pos_X), (Key_Pos_X), (Btn_Neg_X), (Key_Neg_X) ), \
        GET_KEY_OR_BUTTON_AXIS( (Controller), (Btn_Pos_Y), (Key_Pos_Y), (Btn_Neg_Y), (Key_Neg_Y) )  \
    }

#define GET_KEY_AXIS( Key_Pos, Key_Neg )  ((int)IsKeyDown((Key_Pos)) - (int)IsKeyDown((Key_Neg)))

#define GET_KEY_VECTOR( Key_Pos_X, Key_Neg_X, Key_Pos_Y, Key_Neg_Y ) \
    (Vector2){ \
        GET_KEY_AXIS( (Key_Pos_X), (Key_Neg_X) ), \
        GET_KEY_AXIS( (Key_Pos_Y), (Key_Neg_Y) )  \
    }

#define GET_BUTTON_AXIS( Controller, Btn_Pos, Btn_Neg ) ( \
        (int)IsGamepadButtonDown(  (Controller), (Btn_Pos)) \
        - (int)IsGamepadButtonDown((Controller), (Btn_Neg)) \
    )

#define GET_BUTTON_VECTOR( Controller, Btn_Pos_X, Btn_Neg_X, Btn_Pos_Y, Btn_Neg_Y ) \
    (Vector2){ \
        GET_BUTTON_AXIS( (Controller, (Btn_Pos_X), (Btn_Neg_X) ), \
        GET_BUTTON_AXIS( (Controller, (Btn_Pos_Y), (Btn_Neg_Y) ), \
    }


/* 
	COMMON TYPES
*/
typedef struct Engine Engine;
typedef struct Entity Entity;
typedef struct Head   Head;
typedef struct Scene  Scene;


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

typedef enum
{
	COLLISION_NONE     = 0,
	COLLISION_BOX      = 1,
	COLLISION_CYLINDER = 2,
	COLLISION_SPHERE   = 3,
}
CollisionShape;

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


/* 
	Structs 
*/
typedef struct 
CollisionResult 
{
	union {
		RayCollision ray_collision;
		struct {
			bool    hit;
			float   distance;     /* Distance to collision */
			Vector3 position;     /* Where collision occurred */
			Vector3 normal;       /* Surface normal at hit point */
		};
	};
    int     material_id;  /* For different surface types */
    void   *user_data;    /* Scene-specific data (material, etc.) */
    Entity *entity;       /* If hit an entity (NULL for Scene) */
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

/*
	K_Ray
		Wrapper around a raylib Ray, adding a length field.
*/
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
	Region
		Used by Head to define the screen region it draws to.
*/
typedef struct
Region
{
	int
		x,
		y,
		width,
		height;
}
Region;

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

static float
invLerp(float a, float b, float value)
{
	if (a==b) return 0.0f;
	return (value - a) / (b - a);
}

static void
moveCamera(Camera *cam, Vector3 new_position)
{
	Vector3 diff  = Vector3Subtract(cam->target, cam->position);
	cam->position = new_position;
	cam->target   = Vector3Add(new_position, diff);
}


#endif /* COMMON_H */
