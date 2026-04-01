#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#include "dynamicarray.h"
#include "engine.h"
#include "entity.h"
#include "lightning_beam.h"
#include "ribbon.h"
#include "scene.h"
/* MUST COME LAST */
#include <raymath.h>
#include <rlgl.h>


/* ------------------------------------------------------------------ */
/*  Private types                                                       */
/* ------------------------------------------------------------------ */

typedef struct
LightningBeamInfo
{
	RibbonInfo *ribbon_info;
	Renderable  renderable;
}
LightningBeamInfo;

/*
 *  LightningBeamData is just RibbonData — no extra fields needed.
 *  We alias it for clarity at the call site.
 */
typedef struct
{
	RibbonData ribbon_data;
	double     last_fired;
}
LightningBeamData;


/* ------------------------------------------------------------------ */
/*  Forward declarations                                                */
/* ------------------------------------------------------------------ */

static void lightningBeamRender(Entity *self, float delta);
static void lightningBeamUpdate(Entity *self, float delta);


/* ------------------------------------------------------------------ */
/*  Static template objects                                             */
/* ------------------------------------------------------------------ */

static Renderable
lightning_Renderable = {
		.data        = NULL,
		.Render      = RenderRibbon,
		.transparent = true,
	};

static EntityVTable
LightningBeam_Callbacks = {
		.Setup       = NULL,
		.Enter       = NULL,
		.Update      = lightningBeamUpdate,
		.Render      = lightningBeamRender,
		.OnCollision = NULL,
		.OnCollided  = NULL,
		.Exit        = NULL,
		.Free        = NULL,
	};

static Entity
LightningBeam_Template = {
		.renderables       = {&lightning_Renderable},
		.lod_distances     = {2048.0f},
		.lod_count         = 1,
		.visibility_radius = 0.5f,
		.bounds            = {0.05f, 0.05f, 0.05f},
		.bounds_offset     = {0.0f,  0.0f,  0.0f},
		.renderable_offset = {0.0f,  0.0f,  0.0f},
		.vtable            = &LightningBeam_Callbacks,
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

/*------------------------------------------------------------------
	Update callback
------------------------------------------------------------------*/
static void
lightningBeamUpdate(Entity *self, float delta)
{
	(void)delta;
	LightningBeamData *data = (LightningBeamData*)self->local_data;

	if (
		Entity_getAge(self) - data->last_fired 
		>= Engine_getTickLength(Entity_getEngine(self))
	) {
		self->visible = false;
	}
}

/*------------------------------------------------------------------
	Render callback
------------------------------------------------------------------*/

static void
lightningBeamRender(Entity *self, float delta)
{
    (void)delta;
	RibbonData *data = &((LightningBeamData*)self->local_data)->ribbon_data;
    RibbonInfo *info = ((LightningBeamInfo*)self->user_data)->ribbon_info;

    float 
		age     = Entity_getAge(self),
		timeout = Engine_getTickLength(Entity_getEngine(self));

    data->current_frame  = (size_t)(rand() % info->num_frames);
    data->scroll_offset  = (float)rand() / (float)RAND_MAX;
    data->width_delta    = ((float)rand()/RAND_MAX);
    data->flip_u         = rand() % 2;
    data->flip_v         = rand() % 2;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

LightningBeamInfo *
LightningBeamInfo_new(
	float      width,
	Color      color,
	Texture2D  atlas,
	size_t     num_frames
)
{
	LightningBeamInfo *info;
	if (!(info = malloc(sizeof(LightningBeamInfo)))) {
		ERR_OUT("Failed to allocate memory for LightningBeamInfo.");
		return NULL;
	}

	/*
	 *  Lifetime of one tick. The entity is spawned each tick the
	 *  trigger is held and dies before the next tick fires, so there
	 *  is never more than one beam alive at a time.
	 *  1.0f/60.0f is a safe default — the entity will be gone well
	 *  before the next Fire() call regardless of actual tick rate.
	 */
	RibbonInfo *ribbon_info = RibbonInfo_new(
			width,
			1.0f/60.0f,
			color,
			atlas,
			num_frames
		);
	if (!ribbon_info) {
		free(info);
		return NULL;
	}

	info->ribbon_info = ribbon_info;
	info->renderable  = (Renderable){
			.data        = ribbon_info,
			.Render      = RenderRibbon,
			.transparent = true,
		};

	return info;
}


Entity *
LightningBeam_new(
	LightningBeamInfo *info,
	Vector3            muzzle,
	Vector3            endpoint,
	Engine            *engine,
	Scene             *scene
)
{
	Entity *beam = Entity_new(
			&LightningBeam_Template,
			engine,
			sizeof(LightningBeamData)
		);
	if (!beam) {
		ERR_OUT("Failed to construct LightningBeam.");
		return;
	}

	Vector3 diff   = Vector3Subtract(endpoint, muzzle);
	float   length = Vector3Length(diff);

	LightningBeamData *data = (LightningBeamData*)beam->local_data;
	data->last_fired  = Entity_getAge(beam);
	data->ribbon_data = (RibbonData){
			.start         = muzzle,
			.end           = endpoint,
			.width_delta   = 0.0f,
			.scroll_offset = 0.0f,
			.colors        = NULL,
			.offsets       = NULL,
			.current_frame = 0,
			.playing       = true,
            .flip_u        = false,
            .flip_v        = false,
		};

	beam->renderables[0]    = &info->renderable;
	beam->user_data         = info;
	beam->position          = Vector3Add(muzzle, Vector3Scale(diff, 0.5f));
	beam->visibility_radius = length * 0.5f + 1.0f;
	beam->visible           = true;
	beam->active            = true;
	beam->solid             = false;

	Entity_addToScene(beam, scene);
	return beam;
}


void
LightningBeam_fire(
	LightningBeamInfo  *info,
	Entity            **beam,
	Vector3             muzzle,
	Vector3             endpoint,
	Engine             *engine,
	Scene              *scene
)
{
	if (!*beam || !(*beam)->active) {
		*beam = LightningBeam_new(info, muzzle, endpoint, engine, scene);
	}
	LightningBeamData *data = (LightningBeamData*)(*beam)->local_data;
	data->ribbon_data.start = muzzle;
	data->ribbon_data.end   = endpoint;
	data->last_fired        = Entity_getAge(*beam);
	(*beam)->position       = Vector3Add(
			muzzle,
			Vector3Scale(Vector3Subtract(endpoint, muzzle), 0.5f)
		);
	(*beam)->visible        = true;
}
