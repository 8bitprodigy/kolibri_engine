#include "game.h"


/*
	Callback Forward Declarations
*/


/*
	Template Declarations
*/
Renderable         
	r_1 = {
			.data   = &RED,
			.Render = testRenderableBoxCallback
		},  
	r_2 = {
			.data   = &GREEN,
			.Render = testRenderableBoxCallback
		},  
	r_3 = {
			.data   = &BLUE,
			.Render = testRenderableBoxCallback
		};

EntityVTable entity_Callbacks = {
		.Setup       = NULL,
		.Enter       = NULL,
		.Update      = NULL,
		.Render      = NULL,
		.OnCollision = NULL,
		.OnCollided  = NULL,
		.Exit        = NULL,
		.Free        = NULL
	};

Entity entityTemplate = {
		.renderables       = {&r_1,  &r_2,  &r_3},
		.lod_distances     = {16.0f, 48.0f, 128.0f},
		.lod_count         = 3,
		.renderable_offset = {0.0f, 0.5f, 0.0f},
		.visibility_radius = 1.5f,
		.bounds            = V3_ONE_INIT,
		.bounds_offset     = {0.0f, 0.5f, 0.0f},
		.user_data         = NULL,
		.vtable            = &entity_Callbacks,
		.position          = V3_ZERO_INIT,
		.orientation       = V4_ZERO_INIT,
		.scale             = V3_ONE_INIT,
		.collision         = {.layers = 1, .masks = 1},
		.active            = true,
		.visible           = true,
		.collision_shape   = COLLISION_BOX, 
		.solid             = true
	};


/*
	Callback Definitions
*/
