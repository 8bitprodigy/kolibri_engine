#include "game.h"


/*
	Callback Forward Declarations
*/


/*
	Template Declarations
*/
TestRenderableData 
	rd_1 = (TestRenderableData){.color = RED}, 
	rd_2 = (TestRenderableData){.color = GREEN}, 
	rd_3 = (TestRenderableData){.color = BLUE};
	
Renderable         
	r_1 = (Renderable){
			.data   = &rd_1,
			.Render = testRenderableBoxCallback
		},  
	r_2 = (Renderable){
			.data   = &rd_2,
			.Render = testRenderableBoxCallback
		},  
	r_3 = (Renderable){
			.data   = &rd_3,
			.Render = testRenderableBoxCallback
		};

EntityVTable entity_Callbacks = (EntityVTable){
	.Setup       = NULL,
	.Enter       = NULL,
	.Update      = NULL,
	.Render      = NULL,
	.OnCollision = NULL,
	.OnCollided  = NULL,
	.Exit        = NULL,
	.Free        = NULL
};

Entity entityTemplate = (Entity){
	.renderables       = {&r_1,  &r_2,  &r_3},
	.lod_distances     = {8.0f, 16.0f, 48.0f},
	.lod_count         = 3,
	.renderable_offset = {0.0f, 0.5f, 0.0f},
	.visibility_radius = 1.5f,
	.bounds            = V3_ONE,
	.bounds_offset     = {0.0f, 0.5f, 0.0f},
	.user_data         = NULL,
	.vtable            = &entity_Callbacks,
	.position          = V3_ZERO,
	.orientation       = V4_ZERO,
	.scale             = V3_ONE,
	.collision         = {.layers = 1, .masks = 1},
	.active            = true,
	.visible           = true,
	.collision_shape   = COLLISION_BOX, 
	.solid             = true
};


/*
	Callback Definitions
*/
