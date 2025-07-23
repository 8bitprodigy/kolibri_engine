#ifndef HEAD_H
#define HEAD_H


#include "common.h"


/* Callback Types */
typedef void (*HeadCallback)(   Head *head, Engine *engine);
typedef void (*HeadCallback_1f)(Head *head, Engine *engine, float delta);

typedef struct
HeadVTable
{
    HeadCallback     Setup;
    HeadCallback_1f  update;
    HeadCallback     prerender;
    HeadCallback     render;
    HeadCallback     postrender;
    HeadCallback     Exit;
    HeadCallback     Free;
}
HeadVTable;


/* Constructor */
Head *Head_new(  
	int              Controller_ID,
    HeadVTable      *vtable;
    Engine          *engine,
);
/* Destructor */
void Head_free(        Head *head);


/* Setters/Getters */
Camera        *Head_getCamera(  Head *head);
Engine        *Head_getEngine(  Head *head);
RenderTexture *Head_getViewport(Head *head);
void           Head_setViewport(Head *head, int   width,     int                  height);
void          *Head_getUserData(Head *head);
void           Head_setUserData(Head *head, void *user_data, FreeUserDataCallback callback);
void           Head_setVTable(  Head *head, HeadVTable *vtable);
HeadVTable    *Head_getVTable(  Head *head);

/* Methods */
void Head_Setup(     Head *head, Engine *engine);
void Head_Update(    Head *head, Engine *engine, float delta);
void Head_PreRender( Head *head, Engine *engine);
void Head_Render(    Head *head, Engine *engine);
void Head_PostRender(Head *head, Engine *engine);
void Head_Exit(      Head *head, Engine *engine);


#endif /* HEAD_H */
