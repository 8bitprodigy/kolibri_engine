#ifndef HEAD_H
#define HEAD_H


#include "common.h"


typedef struct Head Head;
typedef struct Engine Engine;


typedef void (*HeadCallback)(   Head *head, Engine *engine);
typedef void (*HeadCallback_1f)(Head *head, Engine *engine, float delta);


Head *Head_new(  
    Engine          *engine,
	int              Controller_ID,
    HeadCallback     Setup,
    HeadCallback_1f  update,
    HeadCallback     prerender,
    HeadCallback     render,
    HeadCallback     postrender,
    HeadCallback     Exit
);
void Head_free(        Head *head);

Camera        *Head_getCamera(  Head *head);
RenderTexture *Head_getViewport(Head *head);
void           Head_setViewport(Head *head, int   width,     int                  height);
void          *Head_getUserData(Head *head);
void           Head_setUserData(Head *head, void *user_data, FreeUserDataCallback callback);

void Head_setCallbacks(
    Head            *head,
    HeadCallback     Setup,
    HeadCallback_1f  update,
    HeadCallback     prerender,
    HeadCallback     render,
    HeadCallback     postrender,
    HeadCallback     Exit
);
void Head_setCallbacksConditional(
    Head            *head,
    HeadCallback     Setup,
    HeadCallback_1f  update,
    HeadCallback     prerender,
    HeadCallback     render,
    HeadCallback     postrender,
    HeadCallback     Exit
);
#define Head_setSetupCallback(  head, callback) Head_setCallbacksConditional( (head), (callback), NULL, NULL, NULL, NULL, NULL)
#define Head_UpdateCallback(    head, callback) Head_setCallbacksConditional( (head), NULL, (callback), NULL, NULL, NULL, NULL)
#define Head_PreRenderCallback( head, callback) Head_setCallbacksConditional( (head), NULL, NULL, (callback), NULL, NULL, NULL)
#define Head_RenderCallback(    head, callback) Head_setCallbacksConditional( (head), NULL, NULL, NULL, (callback), NULL, NULL)
#define Head_PostRenderCallback(head, callback) Head_setCallbacksConditional( (head), NULL, NULL, NULL, NULL, (callback), NULL)
#define Head_ExitCallback(      head, callback) Head_setCallbacksConditional( (head), NULL, NULL, NULL, NULL, NULL, (callback))

void Head_Setup(     Head *head, Engine *engine);
void Head_Update(    Head *head, Engine *engine, float delta);
void Head_PreRender( Head *head, Engine *engine);
void Head_Render(    Head *head, Engine *engine);
void Head_PostRender(Head *head, Engine *engine);
void Head_Exit(      Head *head, Engine *engine);


#endif /* HEAD_H */
