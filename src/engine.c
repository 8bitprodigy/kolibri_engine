#include <raylib.h>
#include "_collision_.h"
#include "_engine_.h"
#include "_entity_.h"
#include "_head_.h"
#include "_scene_.h"


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
		uint8 flags;
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
Engine_new(EngineVTable *vtable)
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

	engine->vtable           = vtable;

	if (engine->Setup) engine->Setup(engine);
}


void
Engine_free(Engine *engine)
{
	/* Free everything else first! */
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
Engine_setVTable(Engine *engine, EngineVTable *vtable)
{
	engine->vtable = vtable;
}


EngineVTable *
Engine_getVTable(Engine *engine)
{
	return engine->vtable;
}


void
Engine_run(Engine *self)
{
	const EngineVTable *vtable = self-vtable;
	
	if (vtable && vtable->Run) vtable->Run(self);
	if (self->head_count) {
		while(!self->request_exit) {
			self->request_exit = WindowShouldClose();

			Engine_update(self);
			Engine_render(self);

			self->frame_num++;
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
	int i = 0;
	EntityNode 
		*entity_nodes = self->entities,
		*node;

	const EntityVTable *vtable = self->vtable;
	
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
	if (vtable && vtable->Update) vtable->Update(self, delta);
	self->frame_num++;
}


void
Engine_render(Engine *self)
{
	const EntityVTable *vtable = self->vtable;
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
		/* This is how the final frame is composited together */
		if (vtable && vtable->Render) vtable->Render(self);
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

void
Engine__insertHead(Engine *self, Head *head)
{
	if (self->HEAD_COUNT_MAX <= self->head_count) return;
	if (self->heads) {
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


EntityNode *
Engine__getEntities(Engine *self)
{
	return self->entities;
}

void
Engine__insertEntity(Engine *self, EntityNode *node)
{
	if (self->ENTITY_COUNT_MAX <= self->entity_count) return;
	if (self->entities) {
		self->entities = node;
		self->entity_count++;
		return;
	}
	EntityNode 
		*to   = self->entities,
		*last = to->prev;

	last->next = node;
	to->prev   = node;
	
	node->next = to;
	node->prev = last;
	
	self->entity_count++;
}

void
Engine__removeEntity(Engine *self, EntityNode *node)
{
	if (!self->entity_count) return;
	EntityNode *node_1 = node->prev;
	EntityNode *node_2 = node->next;

	node_1->next = node_2;
	node_2->prev = node_1;

	if (self->entities == node) self->entities = node_2;
	
	self->entity_count--;
}

Scene *
Engine__getScene(Engine *self)
{
	return self->scene;
}

void
Engine__insertScene(Engine *self, Scene *scene)
{
	if (self->scene) {
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

void
Engine__setCollisionScene(Engine *self, CollisionScene *scene)
{
	self->collision_scene   = scene;
}
