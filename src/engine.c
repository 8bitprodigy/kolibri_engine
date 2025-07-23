#include <raylib.h>
#include "_engine_.h"
#include "_entity_.h"
#include "head.h"


/*
	MAIN STRUCT
*/
typedef struct
Engine
{
	Head           *heads;
	EntityNode     *entities;
	Scene          *scene;
	CollisionScene *collision_scene;
	
	uint64     frame_num;
	uint       head_count;
	const uint HEAD_COUNT_MAX;
	uint       entity_count;
	const uint ENTITY_COUNT_MAX;
	float      delta;

	EntityList entity_list;
	
	union {
		uint8 bit_flags;
		struct {
			bool paused          :1;
			bool request_exit    :1;
			bool dirty_EntityList:1; /* Set to true whenever an entity is added/removed; false after generating a new array of entity pointers */
			bool flag_3          :1; /* 3-7 not yet defined */
			bool flag_4          :1;
			bool flag_5          :1;
			bool flag_6          :1;
			bool flag_7          :1;
		};
	};

	EngineCallback    Setup;
	EngineCallback    Run;
	EngineCallback_1f Update;
	EngineCallback    Render;
	EngineCallback    Exit;
	EngineCallback    Free;
}
Engine;


/******************
	CONSTRUCTOR
******************/
Engine *
Engine_new(
	EngineCallback    Setup_Callback, 
	EngineCallback    Run_Callback, 
	EngineCallback_1f Update_Callback, 
	EngineCallback    Render_Callback, 
	EngineCallback    Exit_Callback,
	EngineCallback    Free_Callback
)
{
	Engine *engine = malloc(sizeof(Engine));

	if (!engine) {
		ERR_OUT("Failed to allocate memory for Engine.");
		return NULL;
	}

	engine->heads            = NULL;
	engine->entities         = NULL;
	engine->scene            = NULL;
	
	engine->frame_num        = 0;
	engine->head_count       = 0;
	engine->entity_count     = 0;
	engine->delta            = 0.0f;

	engine->paused           = false;
	engine->request_exit     = false;
	engine->dirty_entityList = true;

	engine->Setup            = Setup_Callback
	engine->Run              = Run_Callback
	engine->Update           = Update_Callback;
	engine->Render           = Render_Callback;
	engine->Exit             = Exit_Callback;
	engine->Free             = Free_Callback;

	if (engine->Setup) engine->Setup(engine);
}


void
Engine_free(Engine *engine)
{
	/* Free everything else, first! */
	free(engine);
}


/*
	SETTERS/GETTERS
*/
EntityList *
Engine_getEntityList(Engine *engine)
{
	if (engine->dirty_EntityList) {
		/* Generate new array of entities */
		engine->dirty_EntityList = false;
	}

	return engine->entity_list;
}


void 
Engine_setCallbacks(
    Engine            *engine,
    EngineCallback     Setup,
    EngineCallback     Run,
    EngineCallback_1f  Update,
    EngineCallback     Render,
    EngineCallback     Exit,
    EngineCallback     Free
)
{
	engine->Setup  = Setup;
	engine->Run    = Run;
	engine->Update = Update;
	engine->Render = Render;
	engine->Exit   = Exit;
	engine->Free   = Free;
}


void 
Engine_setCallbacksConditional(
    Engine            *engine,
    EngineCallback     Setup,
    EngineCallback     Run,
    EngineCallback_1f  Update,
    EngineCallback     Render,
    EngineCallback     Exit,
    EngineCallback     Free
)
{
	if (Setup)  engine->Setup  = Setup;
	if (Run)    engine->Run    = Run;
	if (Update) engine->Update = Update;
	if (Render) engine->Render = Render;
	if (Exit)   engine->Exit   = Exit;
	if (Free)   engine->Free   = Free;
}


void
Engine_run(Engine *self)
{
	if (self->Run) self->Run(self);
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

	if (self->paused || self->request_exit) return;

	node = entity_nodes;
	do {
		Entity *entity = PRIVATE_TO_ENTITY(node)
		if (entity->Update) entity->Update(entity, delta);

		node = node->next;
		i++;
	} while (node != entity_nodes);
	if (engine->Update) engine->Update(self, delta);
	self->frame_num++;
}


void
Engine_render(Engine *self)
{
	/* Loop through Heads */
	/* Render to Head stage */
	for (int i = 0; i < self->head_count; i++) {
		Head *current_head = self->heads[i];
		
		BeginTextureMode(&Head_getViewport(current_head));
			Head_PreRender(current_head) /* Skyboxes, perhaps */
			BeginMode3D(&Head_getCamera(current_head));
				if (self->scene) Scene_render(self->scene, current_head);
				Head_Render(current_head); /* Called on render */
			EndMode3D();
			Head_PostRender(current_head); /* UI overlays, etc. */
		EndTextureMode();	
	}
	/* End loop through Heads */
	/* Composition stage */
	BeginDrawing()
		if (self->Render) self->Render(self);
	EndDrawing()
}


void
Engine_pause(Engine *self, bool paused)
{
	self->paused = paused;
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
Head *
Engine__getHeads(Engine *self)
{
	return self->heads;
}

Entity *
Engine__getEntities(Engine *self)
{
	return self->entities;
}

Scene *
Engine__getScene(Engine *self)
{
	return self->scene;
}
