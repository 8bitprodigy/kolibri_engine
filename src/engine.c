#include <raylib.h>
#include "_engine_.h"


/*
	MAIN STRUCT
*/
typedef struct
Engine
{
	EngineVTable   *vtable;
	Head           *heads;
	EntityNode     *entities;
	Scene          *scene;
	CollisionScene *collision_scene;
	Renderer       *renderer;
	
	uint64     frame_num;
	uint       head_count;
	uint       entity_count;
	uint       target_fps;
	
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
	engine->collision_scene  = CollisionScene__new(engine);
	engine->renderer         = Renderer__new(engine);
	
	engine->frame_num        = 0;
	engine->head_count       = 0;
	engine->entity_count     = 0;
	engine->delta            = 0.0f;

	engine->paused           = false;
	engine->request_exit     = false;
	engine->dirty_EntityList = true;

	engine->entity_list.entities = NULL;
	engine->entity_list.capacity = 0;
	engine->entity_list.count    = 0;

	engine->vtable           = vtable;

	if (vtable && vtable->Setup) vtable->Setup(engine);

	return engine;
}


void
Engine_free(Engine *self)
{
	Head__freeAll(self->heads);
	Scene__freeAll(self->scene);
	EntityNode__freeAll(self->entities);
	CollisionScene__free(self->collision_scene);
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


EntityList *
Engine_getEntityList(Engine *self)
{
	if (!self->dirty_EntityList) {
		return &self->entity_list;
	}

	if (!self->entities || self->entity_count == 0) {
		self->entity_list.count    = 0;
		self->dirty_EntityList     = false;
		return &self->entity_list;
	}

	if (self->entity_list.capacity < self->entity_count) {
		if (self->entity_list.entities) {
			free(self->entity_list.entities);
		}
		
		Entity **entities = malloc(sizeof(Entity*) * self->entity_count);
		if (!entities) {
			ERR_OUT("Failed to allocate EntityList array.");
			self->entity_list.count    = 0;
			self->entity_list.capacity = 0;
			return &self->entity_list;
		}

		self->entity_list.entities = entities;
		self->entity_list.capacity = self->entity_count;
	}

	EntityNode *current = self->entities;
	int index = 0;

	do {
		self->entity_list.entities[index++] = PRIVATE_TO_ENTITY(current);
		current                             = current->next;
	} while (current != self->entities);
	
	self->entity_list.count = self->entity_count;
	self->dirty_EntityList  = false;
	return &self->entity_list;
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
Engine_run(Engine *self, uint Target_FPS)
{
	const EngineVTable *vtable = self->vtable;
	
	if (0 < Target_FPS) SetTargetFPS(Target_FPS);
	
	if (vtable && vtable->Run) vtable->Run(self);
	
	if (self->head_count) {
		while(!self->request_exit) {
			self->request_exit = WindowShouldClose();

			Engine_update(self);
			Engine_render(self);

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
	const EngineVTable *vtable = self->vtable;
	
	float delta = GetFrameTime();
	self->delta = delta;

	Head__updateAll(self->heads, delta);

	if (self->paused || self->request_exit) return;

	EntityNode__updateAll(self->entities, delta);

	CollisionScene__markRebuild(self->collision_scene);
	CollisionScene__update(     self->collision_scene);

	Scene_update(self->scene, delta);
	
	if (vtable && vtable->Update) vtable->Update(self, delta);
}


void
Engine_render(Engine *self)
{
	const EngineVTable *vtable = self->vtable;
	/* Loop through Heads */
	/* Render to Head stage */
	for (uint i = 0; i < self->head_count; i++) {
		Head *current_head = &self->heads[i];
		
		BeginTextureMode(*Head_getViewport(current_head));
			Head_preRender(current_head); /* Skyboxes, perhaps */
			BeginMode3D(*Head_getCamera(current_head));
				if (self->scene) Scene_render(self->scene, current_head);
				Head_render(current_head); /* Called on render */
			EndMode3D();
			Head_postRender(current_head); /* UI overlays, etc. */
		EndTextureMode();	
	}
	/* End loop through Heads */
	/* Composition stage */
	BeginDrawing();
		/* This is how the final frame is composited together */
		if (vtable && vtable->Render) vtable->Render(self);
	EndDrawing();
}


void
Engine_resize(Engine *self, uint width, uint height)
{
	const EngineVTable *vtable = self->vtable;

	if(vtable && vtable->Resize) vtable->Resize(self, width, height);
}


void
Engine_pause(Engine *self, bool Paused)
{
	if (self->paused != Paused) {
		EngineVTable *vtable = self->vtable;
		if (Paused) {
			if (vtable && vtable->Pause) vtable->Pause(self);
		} else {
			if (vtable && vtable->Unpause) vtable->Unpause(self);
		}
	}
	self->paused = Paused;
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


EntityNode *
Engine__getEntities(Engine *self)
{
	return self->entities;
}

void
Engine__insertEntity(Engine *self, EntityNode *node)
{
	if (MAX_NUM_ENTITIES <= self->entity_count) return;
	if (!self->entities) {
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

void
Engine__setCollisionScene(Engine *self, CollisionScene *scene)
{
	self->collision_scene = scene;
}

CollisionScene *
Engine__getCollisionScene(Engine *self)
{
	return self->collision_scene;
}

Renderer *
Engine__getRenderer(Engine *self)
{
	return self->renderer;
}
