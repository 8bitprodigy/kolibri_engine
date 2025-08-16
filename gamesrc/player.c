#include "game.h"


/*
	Callback Forward Declarations
*/


/*
	Template Declarations
*/
TestRenderableData rd_player = (TestRenderableData){.color = MAGENTA};
	
Renderable         
	r_player = (Renderable){
			.data   = &rd_player,
			.Render = testRenderableWiresCallback
		};

EntityVTable player_Callbacks = (EntityVTable){
	.Setup       = NULL,
	.Enter       = NULL,
	.Update      = NULL,
	.Render      = NULL,
	.OnCollision = NULL,
	.OnCollided  = NULL,
	.Exit        = NULL,
	.Free        = NULL
};

Entity playerTemplate = (Entity){
	.renderables       = {&r_player},
	.lod_distances     = {48.0f},
	.lod_count         = 1,
	.visibility_radius = 2.5f,
	.bounds            = {1.0f, 2.0f, 1.0f},
	.bounds_offset     = {0.0f, 1.0f, 0.0f},
	.renderable_offset = {0.0f, 1.0f, 0.0f},
	.user_data         = NULL,
	.vtable            = &player_Callbacks,
	.position          = V3_ZERO,
	.rotation          = V4_ZERO,
	.scale             = V3_ONE,
	.collision         = {.layers = 1, .masks = 1},
	.active            = true,
	.visible           = true,
	.collision_shape   = COLLISION_CYLINDER, 
	.solid             = true
};


/*
	Callback Definitions
*/
/* Player Callbacks */
