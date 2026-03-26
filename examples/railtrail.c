#include <stddef.h>
#include <stdlib.h>

#include "dynamicarray.h"
#include "entity.h"
#include "railtrail.h"
#include "ribbon.h"
#include "scene.h"
/* MUST COME LAST */
#include <raymath.h>


/* ------------------------------------------------------------------ */
/*  Private types                                                       */
/* ------------------------------------------------------------------ */

typedef struct
RailTrailInfo
{
	float       scroll_speed;
	RibbonInfo *ribbon_info;
	Renderable  renderable;
}
RailTrailInfo;

typedef struct
{
	RibbonData ribbon_data;
	float      lifetime;
}
RailTrailData;


/* ------------------------------------------------------------------ */
/*  Forward declarations                                                */
/* ------------------------------------------------------------------ */

static void railTrailUpdate(Entity *self, float delta);


/* ------------------------------------------------------------------ */
/*  Static template objects                                             */
/* ------------------------------------------------------------------ */

static Renderable
rail_Renderable = {
		.data        = NULL,
		.Render      = RenderRibbon,
		.transparent = true,
	};

static EntityVTable
RailTrail_Callbacks = {
		.Setup       = NULL,
		.Enter       = NULL,
		.Update      = railTrailUpdate,
		.Render      = NULL,        /* RenderRibbon is immediate, no Render cb needed */
		.OnCollision = NULL,
		.OnCollided  = NULL,
		.Exit        = NULL,
		.Free        = NULL,
	};

static Entity
RailTrail_Template = {
		.renderables       = {&rail_Renderable},
		.lod_distances     = {2048.0f},
		.lod_count         = 1,
		.visibility_radius = 0.5f,
		.bounds            = {0.05f, 0.05f, 0.05f},
		.bounds_offset     = {0.0f,  0.0f,  0.0f},
		.renderable_offset = {0.0f,  0.0f,  0.0f},
		.vtable            = &RailTrail_Callbacks,
		.position          = V3_ZERO_INIT,
		.orientation       = V4_ZERO_INIT,
		.scale             = V3_ONE_INIT,
		.velocity          = V3_ZERO_INIT,
		.collision         = {.layers = 0, .masks = 0},
		.active            = true,
		.visible           = true,
		.collision_shape   = COLLISION_SPHERE,
		.solid             = false,
	};


/* ------------------------------------------------------------------ */
/*  Entity callbacks                                                    */
/* ------------------------------------------------------------------ */

static void
railTrailUpdate(Entity *self, float delta)
{
	RailTrailData *tdata = (RailTrailData*)&self->local_data;
	RibbonData    *rdata = &tdata->ribbon_data;
	RailTrailInfo *info  = (RailTrailInfo*)self->user_data;

	double age = Entity_getAge(self);

	/* ---- Lifetime expiry ----------------------------------------- */
	if (age >= tdata->lifetime) {
		self->active  = false;
		self->visible = false;
		Entity_free(self);
		return;
	}

	/* ---- Scroll --------------------------------------------------- */
	rdata->scroll_offset -= info->scroll_speed * delta;

	/* ---- Fade ----------------------------------------------------- */
	/*
	 *  t goes from 1.0 at spawn to 0.0 at end of lifetime.
	 *  We write the faded color back into ribbon_info->color so
	 *  RenderRibbon picks it up without needing a color in RibbonData.
	 *
	 *  NOTE: if you ever fire fast enough to have two rail trails alive
	 *  simultaneously this will race — promote color into RibbonData
	 *  and read it there in RenderRibbon instead.
	 */
	float t                  = 1.0f - (float)(age / tdata->lifetime);
	Color base               = info->ribbon_info->color;
	base.a                   = (unsigned char)(base.a * t);
//	info->ribbon_info->color = base;
}


/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

RailTrailInfo *
RailTrailInfo_new(
	float      width,
	float      lifetime,
	float      scroll_speed,
	Color      color,
	Texture2D  atlas,
	size_t     num_frames
)
{
	RailTrailInfo *info;
	if (!(info = malloc(sizeof(RailTrailInfo)))) {
		ERR_OUT("Failed to allocate memory for RailTrailInfo.");
		return NULL;
	}

	RibbonInfo *ribbon_info = RibbonInfo_new(
			width,
			lifetime,
			color,
			atlas,
			num_frames
		);
	if (!ribbon_info) {
		free(info);
		return NULL;
	}

	info->scroll_speed = scroll_speed;
	info->ribbon_info  = ribbon_info;
	info->renderable   = (Renderable){
			.data        = ribbon_info,
			.Render      = RenderRibbon,
			.transparent = true,
		};

	return info;
}


void
RailTrail_new(
	RailTrailInfo *info,
	Vector3        muzzle,
	Vector3        endpoint,
	Engine        *engine,
	Scene         *scene
)
{
	Entity *trail = Entity_new(
			&RailTrail_Template,
			engine,
			sizeof(RailTrailData)
		);
	if (!trail) {
		ERR_OUT("Failed to construct RailTrail.");
		return;
	}

	Vector3 diff   = Vector3Subtract(endpoint, muzzle);
	float   length = Vector3Length(diff);

	/* ---- Per-instance state --------------------------------------- */

	RailTrailData *tdata = (RailTrailData*)&trail->local_data;
	tdata->lifetime      = info->ribbon_info->lifetime;
	tdata->ribbon_data   = (RibbonData){
			.start         = muzzle,
			.end           = endpoint,
			.width_delta   = 0.0f,
			.scroll_offset = 0.0f,
			.colors        = NULL,
			.offsets       = NULL,
			.current_frame = 0,
			.playing       = true,
		};
    
	/* ---- Wire the renderable -------------------------------------- */

	trail->renderables[0] = &info->renderable;
	trail->user_data      = info;

	/* ---- Position at midpoint for frustum culling ----------------- */
	trail->position          = Vector3Add(muzzle, Vector3Scale(diff, 0.5f));
	trail->visibility_radius = length * 0.5f + 1.0f;
	trail->visible           = true;
	trail->active            = true;
	trail->solid             = false;

	Entity_addToScene(trail, scene);
}
