#include <raylib.h>
#include "_engine_.h"
#include "_renderer_.h"


#ifdef __PSP__
	#include <psprtc.h>

static u64 psp_time_start       = 0;
static int psp_time_initialized = 0;

double 
GetTime_PSP(void) 
{
    u64 tick;
    sceRtcGetCurrentTick(&tick);
    
    if (!psp_time_initialized) {
        psp_time_start = tick;
        psp_time_initialized = 1;
        return 0.0;  // Explicitly return 0 on first call
    }
    
    return (double)(tick - psp_time_start) / 1000000.0;
}

	#define GetTime GetTime_PSP
#endif


#define foreach_Head( head_ptr ) \
	for (unsigned i = 0; i < self->head_count && ((head_ptr) = &self->heads[i], 1); i++)
	
#ifdef HEAD_USE_RENDER_TEXTURE
	#define BeginRenderMode( head ) do { \
			Region         region = Head_getRegion((head)); \
			RenderTexture *rtex   = Head_getViewport((head)); \
			BeginTextureMode(*rtex); \
				rlViewport(0, 0, rtex->texture.width, rtex->texture.height); \
		} while(0)
		
	#define EndRenderMode() EndTextureMode()
#elif ENGINE_SINGLE_HEAD_ONLY
	#define foreach_Head( head_ptr ) \
		for ((head_ptr) = &self->heads[0]; (head_ptr); (head_ptr) = NULL)
	#define BeginRenderMode( head )
	#define EndRenderMode()
#else /* SCISSOR MODE */
	#define SCISSOR_MODE
	#define BeginRenderMode( head ) do{\
			Region region = Head_getRegion((head)); \
			BeginScissorMode(region.x, region.y, region.width, region.height); \
				rlViewport(region.x, region.y, region.width, region.height); \
		}while(0)
	#define EndRenderMode() EndScissorMode()
#endif /* HEAD_USE_RENDER_TEXTURE */


/*
	MAIN STRUCT
*/
typedef struct
Engine
{
	EngineVTable   *vtable;
	Head           *heads;
	Scene          *scene;
	Renderer       *renderer;
	
	uint64          
					frame_num,
					tick_num;
	Vector2i        screen_size;
	
	double          
					last_tick_time,
					last_frame_time,
					current_time,
					start_time,
					stop_time,
					last_pause_time,
					time_spent_paused;
	float           
					delta, 
					tick_length,
					tick_elapsed;

	uint       
		            head_count,
		            scene_count,
		            target_fps;
	
	int             tick_rate;
	
	union {
		uint8 flags;
		struct {
			bool paused      :1;
			bool request_exit:1;
			bool flag_2      :1; /* 2-7 not yet defined */
			bool flag_3      :1; 
			bool flag_4      :1;
			bool flag_5      :1;
			bool flag_6      :1;
			bool flag_7      :1;
		};
	};
}
Engine;


/******************
	CONSTRUCTOR
******************/
Engine *
Engine_new(EngineVTable *vtable, int tick_rate)
{
	Engine *engine = malloc(sizeof(Engine));

	if (!engine) {
		ERR_OUT("Failed to allocate memory for Engine.");
		return NULL;
	}
	double current_time = GetTime();

	*engine                   = (Engine){0};
	DBG_OUT(
			"After zero-init, tick_num = %llu", 
			(unsigned long long)engine->tick_num
		);

	engine->heads             = NULL;
	engine->scene             = NULL;
	DBG_OUT(
			"Before Renderer__new, tick_num = %llu", 
			(unsigned long long)engine->tick_num
		);
	engine->renderer          = Renderer__new(engine);
	DBG_OUT(
			"After Renderer__new, tick_num = %llu", 
			(unsigned long long)engine->tick_num
		);

	
	engine->frame_num         = 0;
	engine->tick_num          = 0;
	DBG_OUT(
			"After manual tick_num = 0, tick_num = %llu", 
			(unsigned long long)engine->tick_num
		);

	engine->head_count        = 0;
	engine->delta             = 0.0f;
	engine->tick_length       = 1.0f / tick_rate;
	engine->tick_elapsed      = 1.0f;
	engine->tick_rate         = tick_rate;
	
	engine->last_tick_time    = current_time;
	engine->current_time      = engine->last_tick_time;
	engine->start_time        = 0.0f;
	engine->stop_time         = 0.0f;
	engine->last_pause_time   = 0.0f;
	engine->time_spent_paused = 0.0f;

	engine->paused            = false;
	engine->request_exit      = false;
	
	engine->vtable            = vtable;

	if (vtable && vtable->Setup) vtable->Setup(engine);
	
	DBG_OUT(
			"End of Engine_new, tick_num = %llu", 
			(unsigned long long)engine->tick_num
		);

	return engine;
}


void
Engine_free(Engine *self)
{
	Head__freeAll(self->heads);
	Scene__freeAll(self->scene);
	Renderer__free(self->renderer);
	free(self);
}


/*
	SETTERS/GETTERS
*/
float
Engine_getDeltaTime(Engine *self)
{
	return self->delta;
}


uint64
Engine_getFrameNumber(Engine *self)
{
	return self->frame_num;
}

float
Engine_getTickElapsed(Engine *self)
{
    return self->tick_elapsed;
}

void
Engine_setTickRate(Engine *self, int tick_rate)
{
	self->tick_rate   = tick_rate;
	if (tick_rate <= 0) return;
	self->tick_length = 1.0f / tick_rate;
}

int
Engine_getTickRate(Engine *self)
{
	return self->tick_rate;
}

float
Engine_getTickLength(Engine *self)
{
	return self->tick_length;
}

double
Engine_getTime(Engine *self)
{
	return self->current_time;
}

double
Engine_getPauseTime(Engine *self)
{
	return self->time_spent_paused;
}

Head *
Engine_getHeads(Engine *self)
{
	return self->heads;
}

Renderer *
Engine_getRenderer(Engine *self)
{
	return self->renderer;
}

Scene *
Engine_getScene(Engine *self)
{
	return self->scene;
}


void 
Engine_setVTable(Engine *self, EngineVTable *vtable)
{
	self->vtable = vtable;
}


EngineVTable *
Engine_getVTable(Engine *engine)
{
	return engine->vtable;
}


void
Engine_run(Engine *self)
{
	DBG_OUT(
			"Start of Engine_run, tick_num = %llu", 
			(unsigned long long)self->tick_num
		);
	
	const EngineVTable *vtable = self->vtable;
	self->request_exit    = false;
	self->start_time      = GetTime();
	DBG_OUT("Inside Engine_run, start_time set to: %f", self->start_time);
	self->current_time    = 0.0f;
	self->last_tick_time  = self->tick_length;
	self->last_frame_time = self->tick_length;
	
	DBG_OUT(
			"After time init, tick_num = %llu", 
			(unsigned long long)self->tick_num
		);
	
	SetExitKey(KEY_NULL);
	
	if (vtable && vtable->Run) vtable->Run(self);
	
	DBG_OUT(
			"After vtable->Run, tick_num = %llu", 
			(unsigned long long)self->tick_num
		);
	
	if (self->head_count) {
		while(!self->request_exit) {
			DBG_OUT(
					"=== FRAME #%llu START ===", 
					(unsigned long long)self->frame_num
				);
			
#ifndef __PSP__
			self->request_exit = WindowShouldClose();
#endif /* !__PSP__ */

			/* Handle screen resolution changes */
			Vector2i new_screen_size = (Vector2i){
					.x = GetScreenWidth(), 
					.y = GetScreenHeight()
				};
			if (
				new_screen_size.w    != self->screen_size.w
				&& new_screen_size.h != self->screen_size.h
				&& vtable 
				&& vtable->Resize
			)
				Engine_resize(self, new_screen_size.w, new_screen_size.h);
			
			self->screen_size = new_screen_size;
			DBG_OUT("Before update");
			DBG_OUT(
					"Pre-render state: tick_num=%llu, last_tick_time=%f, frame_num=%llu", 
					(unsigned long long)self->tick_num,
					self->last_tick_time,
					(unsigned long long)self->frame_num
				);
			Engine_update(self);
			/* For extrapolation: how far into the next tick are we? */
			self->tick_elapsed = (
					self->current_time - self->last_tick_time
				) / self->tick_length;
			DBG_OUT("Render: starting");
			BeginDrawing();
				DBG_OUT("Render: began drawing");
				Engine_render(self);
			DBG_OUT("Render: scene done");
				rlDrawRenderBatchActive(); 
			DBG_OUT("About to call EndDrawing()");
			DBG_OUT(
					"Current frame_num: %llu, tick_num: %llu", 
					(unsigned long long)self->frame_num, 
					(unsigned long long)self->tick_num
				);
				EndDrawing();
			DBG_OUT("EndDrawing() completed successfully");
			DBG_OUT("Render: ended drawing");

			DBG_OUT(
					"About to increment frame_num from %llu", 
					(unsigned long long)self->frame_num
				);
			self->frame_num++;
			DBG_OUT("frame_num incremented to %llu", (unsigned long long)self->frame_num);
			DBG_OUT("=== FRAME END ===");
			DBG_OUT("About to check loop condition, request_exit = %d", self->request_exit);
		}
		DBG_OUT("Exited main loop");
	}
	else { /* For uses such as game servers */
		while(!self->request_exit) {
			self->request_exit = WindowShouldClose();

			Engine_update(self);
			/* For extrapolation: how far into the next tick are we? */
			self->tick_elapsed = (
					self->current_time - self->last_tick_time
				) / self->tick_length;
			self->frame_num++;
		}
	}
	if (vtable && vtable->Exit) vtable->Exit(self);
}


void
Engine_update(Engine *self)
{
	//DBG_OUT("Engine updating...");
	if (self->paused || self->request_exit) return;
	const EngineVTable *vtable = self->vtable;
	
	if (vtable && vtable->Update) vtable->Update(self, self->tick_length);

	double raw_time = GetTime();
	self->current_time = raw_time - self->start_time - self->time_spent_paused;
	float frame_delta = self->current_time - self->last_frame_time;
	self->last_frame_time = self->current_time;
	self->delta           = frame_delta;
	
	DBG_OUT(
			"Raw GetTime: %f, Engine time: %f, Last tick: %f, Delta: %f, Tick check: %f >= %f", 
			raw_time, 
			self->current_time, 
			self->last_tick_time, 
			frame_delta,
			self->current_time - self->last_tick_time,  // The actual value being compared
			self->tick_length                            // What it needs to exceed
		);
	
    DBG_OUT("Before head updates");
	Head__updateAll(self->heads, frame_delta);
	
	DBG_OUT("After head updates, tick_num = %llu", (unsigned long long)self->tick_num);
	
	if (self->tick_rate <= 0) return;

	DBG_OUT(
			"Before tick loop, tick_num = %llu, condition check: %f >= %f", 
			(unsigned long long)self->tick_num,
			self->current_time - self->last_tick_time,
			self->tick_length
		);
	
	/* Run ticks for elapsed time */
	while (self->tick_length <= self->current_time - self->last_tick_time) {
		
		DBG_OUT("Engine Tick #%llu", (unsigned long long)self->tick_num);
		
		Scene_update(self->scene, self->tick_length);
		DBG_OUT(
				"Engine Tick #%llu - After Scene_update", 
				(unsigned long long)self->tick_num
			);
    
		if (vtable && vtable->Tick) vtable->Tick(self, self->tick_length);
		DBG_OUT("Engine Tick #%llu - After vtable->Tick", (unsigned long long)self->tick_num);
    
		
		self->last_tick_time += self->tick_length;
		self->tick_num++;
		DBG_OUT(
				"Engine Tick #%llu - COMPLETED", 
				(unsigned long long)self->tick_num
			);
	}
	DBG_OUT("Exited tick loop");
}


void
Engine_render(Engine *self)
{
	DBG_OUT("Render: Engine_render() entered.");

	DBG_OUT("Render: About to render all entities");
	EntityNode__renderAll(self->scene->entities, self->delta);
	const EngineVTable *vtable = self->vtable;
	ClearBackground(BLACK);
	/* Loop through Heads */
	/* Render to Head stage */
	Head *current_head;
	DBG_OUT("Render: Rendering Heads");
	int j = 1;
	foreach_Head(current_head) {
		//current_head = &self->heads[i];
		DBG_OUT("Render: Rendering Head #%i", j);
		BeginRenderMode(current_head);
			DBG_OUT("Render: About to preRender");
			Head_preRender(current_head); /* Skyboxes, perhaps */
			DBG_OUT("Render: About to BeginMode3D");
			BeginMode3D(*Head_getCamera(current_head));
			DBG_OUT("Render: About to render scene");
				Renderer__render(self->renderer, current_head);
			EndMode3D();
			DBG_OUT("Render: Scene rendered, about to postRender");
			Head_postRender(current_head); /* UI overlays, etc. */
			DBG_OUT("Render: About to call EndRenderMode()");
		EndRenderMode();	
	}
	DBG_OUT("Render: Rendered to all Heads");
	/* End loop through Heads */
	/* Final stage. If you need to render stuff over the frame, do it here */
	DBG_OUT("Render: About to call Engine.Render()");
	if (vtable && vtable->Render) vtable->Render(self);
}


void
Engine_resize(Engine *self, uint width, uint height)
{
	const EngineVTable *vtable = self->vtable;

	if (vtable && vtable->Resize) vtable->Resize(self, width, height);

	Head *head = self->heads;
	do {
		HeadVTable *hvtable = head->vtable;
		if (hvtable && hvtable->Resize) hvtable->Resize(head, width, height);
		head = head->next;
	} while(head != self->heads);
}


void
Engine_pause(Engine *self, bool Paused)
{
	if (self->paused != Paused) {
		self->paused = Paused;
		EngineVTable *vtable = self->vtable;
		if (Paused) {
			self->last_pause_time = GetTime();
			if (vtable && vtable->Pause) vtable->Pause(self);
		} else {
			self->time_spent_paused += GetTime() - self->last_pause_time;
			if (vtable && vtable->Unpause) vtable->Unpause(self);
		}
	}
}


bool
Engine_isPaused(Engine *self)
{
	return self->paused;
}


void
Engine_requestExit(Engine *self)
{
	self->request_exit = true;
}

/**********************
	PRIVATE METHODS
**********************/

void
Engine__insertHead(Engine *self, Head *head)
{
	if (MAX_NUM_HEADS <= self->head_count) return;
	if (!self->heads) {
		self->heads = head;
		self->head_count++;
		return;
	}
	Head 
		*to   = self->heads,
		*last = to->prev;

	last->next = head;
	to->prev   = head;
	
	head->next = to;
	head->prev = last;
	
	self->head_count++;
}

void
Engine__removeHead(Engine *self, Head *head)
{
	if (!self->head_count) return;
	Head *head_1 = head->prev;
	Head *head_2 = head->next;

	head_1->next = head_2;
	head_2->prev = head_1;

	if (self->heads == head) self->heads = head_2;
	
	self->head_count--;
}

Scene *
Engine__getScene(Engine *self)
{
	return self->scene;
}

void
Engine__insertScene(Engine *self, Scene *scene)
{
	if (!self->scene) {
		self->scene = scene;
		return;
	}
	Scene 
		*to   = self->scene,
		*last = to->prev;

	last->next = scene;
	to->prev   = scene;
	
	scene->next = to;
	scene->prev = last;
}

void 
Engine__removeScene(Engine *self, Scene *scene)
{
	if (!self->scene) return;
	Scene *scene_1 = scene->prev;
	Scene *scene_2 = scene->next;

	scene_1->next = scene_2;
	scene_2->prev = scene_1;

	if (self->scene == scene) self->scene = scene_2;
}

Renderer *
Engine__getRenderer(Engine *self)
{
	return self->renderer;
}
