#include "_engine_.h"
#include "_head_.h"


typedef struct Engine Engine;


/*
	CONSTRUCTOR
*/
Head *
Head_new(
	int         Controller_ID,
    HeadVTable *VTable,
    Engine     *engine
)
{
	Head *head = malloc(sizeof(Head));

	if (!head) {
		ERR_OUT("Failed to allocate memory for Head.");
		return NULL;
	}
	
	head->next              = head;
	head->prev              = head;
	
	head->controller_id     = Controller_ID;
	head->engine            = engine;
	head->controlled_entity = NULL;
	head->user_data         = NULL;
	head->region            = (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};

	head->vtable            = VTable;

	head->settings          = DEFAULT_RENDERERSETTINGS;

	Engine__insertHead(engine, head);

	Head_setup(head);

	return head;
}
/*
	DESTRUCTOR
*/
void
Head_free(Head *Self)
{
	HeadVTable *vtable = Self->vtable;
	
	if (vtable && vtable->Free) vtable->Free(Self);
	
	Engine__removeHead(Self->engine, Self);
	
	UnloadRenderTexture(Self->viewport);
	
	free(Self);
}

void
Head__freeAll(Head *self)
{
	Head *next = self->next;
	if (next == self) {
		Head_free(self);
		return;
	}
	
	Head_free(self);
	Head__freeAll(next);
}


/*
	PUBLIC METHODS
*/
Head *
Head_getNext(Head *Self)
{
	return Self->next;
}


Head *
Head_getPrev(Head *Self)
{
	return Self->prev;
}


Camera3D *
Head_getCamera(Head *Self)
{
	return &Self->camera;
}


Engine *
Head_getEngine(Head *Self)
{
	return Self->engine;
}


RenderTexture *
Head_getViewport(Head *Self)
{
	return &Self->viewport;
}


void
Head_setViewport(Head *Self, int Width, int Height)
{
	Self->viewport = LoadRenderTexture(Width, Height);
	Rectangle *region = &Self->region;
	region->x      = 0;
	region->y      = 0;
	region->width  = Width;
	region->height = -Height;
}

Rectangle
Head_getRegion(Head *Self)
{
	return Self->region;
}

void
Head_setRegion(Head *Self, int X, int Y, int Width, int Height)
{
	Rectangle *region = &Self->region;
	region->x      = X;
	region->y      = Y;
	region->width  = Width;
	region->height = Height;
}


void *
Head_getUserData(Head *Self)
{
	return Self->user_data;
}


void
Head_setUserData(Head *Self, void *User_Data, HeadFreeUserDataCallback callback)
{
	if (!callback) return;
	Self->FreeUserData = callback;
	Self->user_data    = User_Data;
}


void
Head_freeUserData(Head *Self)
{
	if (!Self->FreeUserData) return;
	Self->FreeUserData(Self->user_data);
}


void 
Head_setVTable(Head *Self, HeadVTable *VTable)
{
	Self->vtable = VTable;
}

HeadVTable *
Head_getVTable(Head *Self)
{
	return Self->vtable;
}

RendererSettings *
Head_getRendererSettings(Head *Self)
{
	return &Self->settings;
}


void
Head_setup(Head *Self)
{
	HeadVTable *vtable = Self->vtable;
	if (vtable && vtable->Setup) vtable->Setup(Self);
}


void
Head_update(Head *Self, float delta)
{
	HeadVTable *vtable = Self->vtable;
	if (vtable && vtable->Update) vtable->Update(Self, delta);
}


void
Head_preRender(Head *Self)
{
	HeadVTable *vtable = Self->vtable;
	if (vtable && vtable->PreRender) vtable->PreRender(Self);
}


void
Head_render(Head *Self)
{
	HeadVTable *vtable = Self->vtable;
	if (vtable && vtable->Render) vtable->Render(Self);
}


void
Head_postRender(Head *Self)
{
	HeadVTable *vtable = Self->vtable;
	if (vtable && vtable->PostRender) vtable->PostRender(Self);
}


void
Head_exit(Head *Self)
{
	HeadVTable *vtable = Self->vtable;
	if (vtable && vtable->Exit) vtable->Exit(Self);
}


/*
	Protected methods
*/
void
Head__updateAll(Head *head, float delta)
{
	Head *starting_head = head;
	
	do {
		HeadVTable *vtable = head->vtable;
		Head_update(head, delta);

		head = head->next;
	} while (head != starting_head);
}
