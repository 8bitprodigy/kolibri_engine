#include <raylib.h>
#include "_engine_.h"
#include "_entity_.h"
#include "head.h"

typedef struct
Engine
{
	Head       *heads[MAX_NUM_HEADS];
	EntityNode *entities;
	Scene      *scene;
	
	uint64 frame_num;
	uint   head_count;
	uint   entity_count;
	float  delta;

	union {
		uint8 bit_flags;
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

	EngineCallback    Setup;
	EngineCallback_1f Update;
	EngineCallback    PreRender;
	EngineCallback    Render;
	EngineCallback    PostRender;
	EngineCallback    Exit;
}
Engine;


/******************
	CONSTRUCTOR
******************/
Engine *
Engine_new(
	EngineCallback    Setup_Callback, 
	EngineCallback_1f Update_Callback, 
	EngineCallback    Prerender_Callback, 
	EngineCallback    Render_Callback, 
	EngineCallback    Postrender_Callback,
	EngineCallback    Exit_Callback
)
{
	Engine *engine = malloc(sizeof(Engine));

	if (!engine) {
		ERR_OUT("Failed to allocate memory for Engine.");
		return NULL;
	}

	engine->heads        = NULL;
	engine->entities     = NULL;
	engine->scene        = NULL;
	
	engine->frame_num    = 0;
	engine->head_count   = 0;
	engine->entity_count = 0;
	engine->delta        = 0.0f;

	engine->paused       = false;
	engine->request_exit = false;

	engine->Setup        = Setup_Callback
	engine->Update       = Update_Callback;
	engine->PreRender    = Prerender_Callback;
	engine->Render       = Render_Callback;
	engine->PostRender   = Postrender_Callback;
	engine->Exit         = Exit_Callback;
}


void
Engine_run(Engine *self)
{
	while(!self->request_exit) {
		self->request_exit = WindowShouldClose();

		Engine_update(self);
		Engine_render(self);

		self->frame_num++;
	}
	if (self->Exit) self->Exit(self);
}


void
Engine_update(Engine *self)
{
	int i = 0;
	EntityNode 
		*entity_nodes = self->entities,
		*node;
	
	float delta = GetFrameTime();
	self->delta = delta;

	for (int i = 0; i < self->head_count; i++) {
		Head_Update(self->heads[i])
	}
	if (engine->Update) engine->Update(self, delta);

	if (self->paused || self->request_exit) return;

	node = entity_nodes;
	do {
		Entity *entity = PRIVATE_TO_ENTITY(node)
		if (entity->Update) entity->Update(entity);

		node = node->next;
		i++;
	} while (node != entity_nodes);

	self->frame_num++;
}


void
Engine_render(Engine *self)
{
	/* Loop through Heads */
	for (int i = 0; i < self->head_count; i++) {
		Head *current_head = self->heads[i];
		
		BeginTextureMode(&Head_getViewport(current_head));
			Head_PreRender(current_head)
			BeginMode3D(&Head_getCamera(current_head));
				Head_Render(current_head);
			EndMode3D();
			Head_PostRender(current_head);
		EndTextureMode();	
	}
	/* End loop through Heads */
	if (engine->PreRender)  engine->PreRender(self);
	BeginDrawing()
		if (engine->Render)     engine->Render(self);
	EndDrawing()
	if (engine->PostRender) engine->PostRender(self);
}


void
Engine_pause(Engine *self, bool paused)
{
	self->paused = paused;
}


void
Engine_requestExit(Engine *self)
{
	self->request_exit = true;
}


bool
Engine_isPaused(Engine *self)
{
	return self->paused;
}


/******************
	PRIVATE API
******************/

void
Engine__addHead(Engine *self, Head *head)
{
	if (MAX_NUM_HEADS <= self->head_count) {
		Head_free(head);
		return;
	}
	/* Add head to array and then increment head_count */
	self->heads[self->head_count++] = head;
}
