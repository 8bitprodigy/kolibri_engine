#ifndef HEAD_H
#define HEAD_H


#include "common.h"


#ifndef DEFAULT_MAX_RENDER_DISTANCE
	#define DEFAULT_MAX_RENDER_DISTANCE 256.0f
#endif
#ifndef DEFAULT_MAX_ENTITIES_PER_FRAME
	#define DEFAULT_MAX_ENTITIES_PER_FRAME 1024
#endif
#ifndef DEFAULT_RENDER_FLAGS
	#define DEFAULT_RENDER_FLAGS 7
#endif
#define DEFAULT_RENDERERSETTINGS ((RendererSettings){ \
		DEFAULT_MAX_RENDER_DISTANCE, \
		DEFAULT_MAX_ENTITIES_PER_FRAME, \
		DEFAULT_RENDER_FLAGS \
	})


/* Callback Types */
typedef void        (*HeadCallback)(            Head *head);
typedef void        (*HeadUpdateCallback)(      Head *head, float delta);
typedef void        (*HeadResizeCallback)(      Head *head, uint  width,  uint height);

typedef struct
HeadVTable
{
    HeadCallback       Setup;      /* Called immediately after initialization */
    HeadUpdateCallback Update;     /* Called every frame before rendering */
    HeadCallback       PreRender;  /* Called during render, prior to rendering the scene.*/
    HeadCallback       PostRender; /* Called after rendering the scene */
    HeadResizeCallback Resize;     /* Called upon window resize */
    HeadCallback       Exit;       /* Called upon removal of Head from engine */
    HeadCallback       Free;       /* Called upon freeing the Head from memory */
}
HeadVTable;

typedef struct
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
    Region           region,
    HeadVTable      *vtable,
    Engine          *engine,
    size_t           local_data_size
);
/* Destructor */
void Head_free(Head *head);


/* Setters/Getters */
Head             *Head_getNext(            Head *head);
Head             *Head_getPrev(            Head *head);
Camera3D         *Head_getCamera(          Head *head);
Engine           *Head_getEngine(          Head *head);
Frustum          *Head_getFrustum(         Head *head);
#ifdef HEAD_USE_RENDER_TEXTURE
RenderTexture    *Head_getViewport(        Head *head);
#endif /* HEAD_USE_RENDER_TEXTURE */
Region            Head_getRegion(          Head *head);
void              Head_setRegion(          Head *head, Region region);
void             *Head_getLocalData(       Head *head);
void             *Head_getUserData(        Head *head);
void              Head_setUserData(        Head *head, void *user_data);
void              Head_setVTable(          Head *head, HeadVTable *vtable);
HeadVTable       *Head_getVTable(          Head *head);
RendererSettings *Head_getRendererSettings(Head *head);

/* Methods */
void Head_setup(     Head *head);
void Head_update(    Head *head, float delta);
void Head_preRender( Head *head);
void Head_postRender(Head *head);
void Head_exit(      Head *head);


#endif /* HEAD_H */
