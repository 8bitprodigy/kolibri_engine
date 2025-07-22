#include "_head_.h"


typedef struct Engine Engine;

typedef struct 
{
    Camera3D              camera;
    RenderTexture         viewport;
    int                   controller_id;
    Engine               *engine;
    Entity               *controlled_entity;
    void                 *user_data;
    FreeUserDataCallback *FreeUserData;

	HeadCallback          Setup;
	HeadCallback_1f       Update;
	HeadCallback          PreRender;
	HeadCallback          Render;
	HeadCallback          PostRender;
	HeadCallback          Exit;
} 
Head;


Head *
Head_new(
	int          Controller_ID,
    Callback     Setup,
    Callback_1f  Update,
    Callback     PreRender,
    Callback     Render,
    Callback     PostRender,
    Callback     Exit,
    Engine      *engine
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

	Head_setCallbacks(
		head,
		Setup,
		Update,
		PreRender,
		Render,
		PostRender,
		Exit
	)

	Engine__addHead(engine, head);
}


void
Head_free(Head *Self)
{
	/*
		Clean up stuff first!
		Gotta make a way to clean up user_data -- probably a callback
	*/
	UnloadRenderTexture(Self->viewport);
	free(Self);
}


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
Head_setCallbacks(
    Head            *head,
    HeadCallback     Setup,
    HeadCallback_1f  update,
    HeadCallback     prerender,
    HeadCallback     render,
    HeadCallback     postrender,
    HeadCallback     Exit
)
{
	head->Setup      = Setup;
	head->Update     = Update;
	head->PreRender  = PreRender;
	head->Render     = Render;
	head->PostRender = PostRender;
	head->Exit       = Exit;
}


void 
Head_setCallbacksConditional(
    Head            *head,
    HeadCallback     Setup,
    HeadCallback_1f  update,
    HeadCallback     prerender,
    HeadCallback     render,
    HeadCallback     postrender,
    HeadCallback     Exit
)
{
	if (Setup)      head->Setup      = Setup;
	if (Update)     head->Update     = Update;
	if (PreRender)  head->PreRender  = PreRender;
	if (Render)     head->Render     = Render;
	if (PostRender) head->PostRender = PostRender;
	if (Exit)       head->Exit       = Exit;
}


void
Head_Setup(Head *Self, Engine *engine)
{
	if (!Self->Setup) return;
	Self->Setup(Self, engine);
}


void
Head_Update(Head *Self, Engine *engine, float delta)
{
	if (!Self->Update) return;
	Self->Update(Self, engine);
}


void
Head_PreRender(Head *Self, Engine *engine)
{
	if (!Self->PreRender() return;
	Self->PreRender(Self, engine);
}


void
Head_Render(Head *Self, Engine *engine)
{
	if (!Self->Render() return;
	Self->Render(Sself, engine);
}


void
Head_PostRender(Head *Self, Engine *engine)
{
	if (!Self->PostRender() return;
	Self->PostRender(Self, engine);
}


void
Head_Exit(Head *Self, Engine *engine)
{
	if (!Self->Exit() return;
	Self->Exit(Self, engine);
}
