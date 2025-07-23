#include "_head_.h"


typedef struct Engine Engine;

typedef struct 
Head
{
    Camera3D              camera;
    RenderTexture         viewport;
    int                   controller_id;
    Engine               *engine;
    Entity               *controlled_entity;
    void                 *user_data;
    FreeUserDataCallback *FreeUserData;

    struct Head
		*prev,
		*next;

	HeadVTable *vtable;
} 
Head;


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

	insertHead(head, Engine__getHead(engine));
}
/*
	DESTRUCTOR
*/
void
Head_free(Head *Self)
{
	if(
	UnloadRenderTexture(Self->viewport);
	free(Self);
}


/*
	PRIVATE METHODS
*/
void
insertHead(Head *head, Scene *to)
{
	if (!head || !to) {
		ERR_OUT("insertEntity() received a NULL pointer as argument");
		return;
	}
    
	if (!to->prev || !to->next) {
		ERR_OUT("insertEntity() received improperly initialized EntityNode `to`.");
		return;
	}
    
	Scene *last = to->prev;
    
	last->next = head;
	to->prev   = head;
	
	head->next = to;
	head->prev = last;
}


void
removeHead(Head *head)
{
	EntityNode *head_1 = head->prev;
	EntityNode *head_2 = head->next;

	head_1->next = head_2;
	head_2->prev = head_1;
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
Head_Setup(Head *Self, Engine *engine)
{
	if (Self->Setup) Self->Setup(Self, engine);
}


void
Head_Update(Head *Self, Engine *engine, float delta)
{
	if (Self->Update) Self->Update(Self, engine);
}


void
Head_PreRender(Head *Self, Engine *engine)
{
	if (Self->PreRender) Self->PreRender(Self, engine);
}


void
Head_Render(Head *Self, Engine *engine)
{
	if (Self->Render) Self->Render(Sself, engine);
}


void
Head_PostRender(Head *Self, Engine *engine)
{
	if (Self->PostRender) Self->PostRender(Self, engine);
}


void
Head_Exit(Head *Self, Engine *engine)
{
	if (Self->Exit) Self->Exit(Self, engine);
}
