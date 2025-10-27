#include <raylib.h>
#include "_engine_.h"


#define foreach_Head( head_ptr ) \
	for (int i = 0; i < self->head_count && ((head_ptr) = &self->heads[i], 1); i++)
	
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
					pause_time;
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

	engine->heads            = NULL;
	engine->scene            = NULL;
	engine->renderer         = Renderer__new(engine);
	
	engine->frame_num        = 0;
	engine->tick_num         = 0;
	engine->head_count       = 0;
	engine->delta            = 0.0f;
	engine->tick_length      = 1.0f / tick_rate;
	engine->tick_elapsed     = 1.0f;
	engine->tick_rate        = tick_rate;

	engine->last_tick_time   = GetTime();
	engine->current_time     = engine->last_tick_time;

	engine->paused           = false;
	engine->request_exit     = false;
	
	engine->vtable           = vtable;

	if (vtable && vtable->Setup) vtable->Setup(engine);

	DBG_OUT("Tick rate: %i ticks/second.", engine->tick_rate);
	
	return engine;
}


void
Engine_free(Engine *self)
{
	Head__freeAll(self->heads);
	Scene__freeAll(self->scene);
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

Head *
Engine_getHeads(Engine *self)
{
	return self->heads;
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
	self->request_exit         = false;
	const EngineVTable *vtable = self->vtable;
	
	SetExitKey(KEY_NULL);
	
	if (vtable && vtable->Run) vtable->Run(self);
	
	if (self->head_count) {
		while(!self->request_exit) {
			self->request_exit = WindowShouldClose();

			/* Handle screen resolution changes */
			Vector2i new_screen_size = (Vector2i){
					GetScreenWidth(), 
					GetScreenHeight()
				};
			if (
				new_screen_size.w    != self->screen_size.w
				&& new_screen_size.h != self->screen_size.h
				&& vtable 
				&& vtable->Resize
			)
				Engine_resize(self, new_screen_size.w, new_screen_size.h);
			self->screen_size = new_screen_size;
			
			Engine_update(self);
			BeginDrawing();
				Engine_render(self);
			EndDrawing();

			self->frame_num++;
		}
	}
	else { /* For uses such as game servers */
		while(!self->request_exit) {
			self->request_exit = WindowShouldClose();

			Engine_update(self);

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
	
	self->current_time = GetTime();
	float frame_time = self->current_time - self->last_frame_time;
	self->last_frame_time = self->current_time;
	
	Head__updateAll(self->heads, frame_time);
	
	if (self->tick_rate <= 0) return;

	
	/* Run ticks for elapsed time */
	while (self->current_time - self->last_tick_time >= self->tick_length) {
		
		//DBG_OUT("Engine Tick #%i", self->tick_num);
		Scene_update(self->scene, self->tick_length);
		if (vtable && vtable->Update) vtable->Update(self, self->tick_length);
		
		self->last_tick_time += self->tick_length;
		self->tick_num++;
	}
	
	/* For extrapolation: how far into the next tick are we? */
	self->tick_elapsed = (self->current_time - self->last_tick_time) / self->tick_length;
}


void
Engine_render(Engine *self)
{
	const EngineVTable *vtable = self->vtable;
	ClearBackground(BLACK);
	/* Loop through Heads */
	/* Render to Head stage */
	Head *current_head;
	foreach_Head(current_head) {
		//current_head = &self->heads[i];
		BeginRenderMode(current_head);
			Head_preRender(current_head); /* Skyboxes, perhaps */
			BeginMode3D(*Head_getCamera(current_head));
				if (self->scene) Scene_render(self->scene, current_head);
			EndMode3D();
			Head_postRender(current_head); /* UI overlays, etc. */
		EndRenderMode();	
	}
	/* End loop through Heads */
	/* Final stage. If you need to render stuff over the frame, do it here */
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
			self->pause_time = GetTime();
			if (vtable && vtable->Pause) vtable->Pause(self);
		} else {
			self->last_tick_time += GetTime() - self->pause_time;
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
