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

	head->controller_id     = Controller_ID;
	head->engine            = engine;
	head->controlled_entity = NULL;
	head->user_data         = NULL;

	head->vtable            = VTable

	Engine__insertHead(engine, head);

	Head_setup(head);
}
/*
	DESTRUCTOR
*/
void
Head_free(Head *Self)
{
	HeadVTable *vtable = self->vtable;
	
	if (vtable && vtable->Free) vtable->Free(self);
	
	Engine__removeHead(self->engine, self)
	
	UnloadRenderTexture(Self->viewport);
	
	free(Self);
}


/*
	PUBLIC METHODS
*/
Camera *
Head_getCamera(Head *Self)
{
	return &Self->camera;
}


Engine *
Head_getCamera(Head *Self)
{
	return &Self->engine;
}


RenderTexture *
Head_getViewport(Head *Self)
{
	return &Self-viewport;
}


void
Head_setViewport(Head *Self, int Width, int Height)
{
	Self->viewport = loadRenderTexture(Width, Height);
}


void *
Head_getUserData(Head *Self)
{
	return Self->user_data;
}


void
Head_setUserData(Head *Self, void *User_Data, FreeUserDataCallback callback)
{
	if (!callback) return;
	self->FreeUserData = callback;
	Self->user_data    = User_Data;
}


void 
Head_setVTable(Head *head, HeadVTable *VTable)
{
	head->vtable = VTable;
}

HeadVTable *
Head_getVTable(Head *head)
{
	return head->vtable;
}


void
Head_Setup(Head *Self)
{
	if (Self->Setup) Self->Setup(Self);
}


void
Head_Update(Head *Self, float delta)
{
	if (Self->Update) Self->Update(Self, delta);
}


void
Head_PreRender(Head *Self)
{
	if (Self->PreRender) Self->PreRender(Self);
}


void
Head_Render(Head *Self)
{
	if (Self->Render) Self->Render(Self);
}


void
Head_PostRender(Head *Self)
{
	if (Self->PostRender) Self->PostRender(Self);
}


void
Head_Exit(Head *Self)
{
	if (Self->Exit) Self->Exit(Self);
}
