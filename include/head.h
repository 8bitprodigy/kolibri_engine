#ifndef HEAD_H
#define HEAD_H


#include "common.h"


#define DEFAULT_RENDERERSETTINGS ((RendererSettings){ 128.0f, 1024, 7 })


/* Callback Types */
typedef void (*HeadCallback)(            Head *head);
typedef void (*HeadUpdateCallback)(      Head *head, float delta);
typedef void (*HeadResizeCallback)(      Head *head, uint  width,  uint height);
typedef void (*HeadFreeUserDataCallback)(void *user_data);

typedef struct
HeadVTable
{
    HeadCallback       Setup;
    HeadUpdateCallback Update;
    HeadCallback       PreRender;
    HeadCallback       Render;
    HeadCallback       PostRender;
    HeadResizeCallback Resize;
    HeadCallback       Exit;
    HeadCallback       Free;
}
HeadVTable;

typedef struct
RendererSettings
{
	float max_render_distance;
	int   max_entities_per_frame;
	union {
		uint8 flags;
		struct {
			bool frustum_culling          :1;
			bool sort_transparent_entities:1;
			bool level_of_detail          :1;
			bool flag_3                   :1; /* 3-4 not yet defined */
			bool flag_4                   :1;
			bool draw_entity_origin       :1;
			bool draw_bounding_boxes      :1;
			bool show_lod_levels          :1;
		};
	};
}
RendererSettings;


/* Constructor */
Head *Head_new(  
	int              Controller_ID,
    HeadVTable      *vtable,
    Engine          *engine
);
/* Destructor */
void Head_free(Head *head);


/* Setters/Getters */
Head             *Head_getNext(            Head *head);
Head             *Head_getPrev(            Head *head);
Camera3D         *Head_getCamera(          Head *head);
Engine           *Head_getEngine(          Head *head);
RenderTexture    *Head_getViewport(        Head *head);
void              Head_setViewport(        Head *head, int   width,     int                      height);
Rectangle         Head_getRegion(          Head *head);
void              Head_setRegion(          Head *head, int   x,         int                      y,        int width, int height);
void             *Head_getUserData(        Head *head);
void              Head_setUserData(        Head *head, void *user_data, HeadFreeUserDataCallback callback);
void              Head_freeUserData(       Head *head);
void              Head_setVTable(          Head *head, HeadVTable *vtable);
HeadVTable       *Head_getVTable(          Head *head);
RendererSettings *Head_getRendererSettings(Head *head);

/* Methods */
void Head_setup(     Head *head);
void Head_update(    Head *head, float delta);
void Head_preRender( Head *head);
void Head_render(    Head *head);
void Head_postRender(Head *head);
void Head_exit(      Head *head);


#endif /* HEAD_H */
