#include "_engine_.h"
#include "_head_.h"


#define PLANE_NORMAL( val_1, val_2, val_3, val_4 ) Vector3Normalize( \
		Vector3CrossProduct( \
			(val_1), \
			Vector3Add( \
				(val_2), \
				Vector3Scale( \
					(val_3), \
					(val_4) \
				) \
			) \
		) \
	)


typedef struct Engine Engine;


/*
	Private Methods
*/
static void
updateHeadFrustum(Head *head)
{
    Frustum  *frustum = &head->frustum;
    Camera3D *camera  = &head->camera;

    /* Update cached values */
    frustum->position = camera->position;
    frustum->forward  = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
    frustum->right    = Vector3Normalize(Vector3CrossProduct(frustum->forward, camera->up));
    frustum->up       = Vector3Normalize(Vector3CrossProduct(frustum->right, frustum->forward));

    /* Calculate FOV values */
    frustum->vfov_rad     = DEG2RAD * camera->fovy;
    frustum->aspect_ratio = (float)head->region.width / (float)head->region.height;
    frustum->hfov_rad     = 2.0f * atanf(tanf(frustum->vfov_rad * 0.5f) * frustum->aspect_ratio);

    /* Calculate frustum limits */
    frustum->horiz_limit = frustum->hfov_rad * 0.5f;
    frustum->vert_limit  = frustum->vfov_rad * 0.5f;

    /* Pre-calculate trig values */
    float max_distance = head->settings.max_render_distance;
    float half_v_side  = tanf(frustum->vert_limit);
    float half_h_side  = tanf(frustum->horiz_limit);

    /* Calculate far plane corners more efficiently */
    Vector3 far_center = Vector3Add(frustum->position, Vector3Scale(frustum->forward, max_distance));
    Vector3 far_up     = Vector3Scale(frustum->up, half_v_side * max_distance);
    Vector3 far_right  = Vector3Scale(frustum->right, half_h_side * max_distance);

    /* Use a more direct approach for plane calculation */
    Vector3 corners[4] = {
        Vector3Subtract(Vector3Add(far_center, far_up), far_right),    /* top-left */
        Vector3Add(Vector3Add(far_center, far_up), far_right),         /* top-right */
        Vector3Subtract(Vector3Subtract(far_center, far_up), far_right), /* bottom-left */
        Vector3Add(Vector3Subtract(far_center, far_up), far_right)     /* bottom-right */
    };

    /* Calculate planes using pre-computed corners */
    /* Left plane */
    Vector3 
		left_v1 = Vector3Subtract(corners[0], frustum->position),
		left_v2 = Vector3Subtract(corners[2], frustum->position),
		left_normal = Vector3Normalize(Vector3CrossProduct(left_v2, left_v1));
    frustum->planes[FRUSTUM_LEFT] = (Plane){left_normal, -Vector3DotProduct(left_normal, frustum->position)};

    /* Right plane */
    Vector3
		right_v1 = Vector3Subtract(corners[3], frustum->position),
		right_v2 = Vector3Subtract(corners[1], frustum->position),
		right_normal = Vector3Normalize(Vector3CrossProduct(right_v2, right_v1));
    frustum->planes[FRUSTUM_RIGHT] = (Plane){right_normal, -Vector3DotProduct(right_normal, frustum->position)};

    /* Top plane */
    Vector3 
		top_v1 = Vector3Subtract(corners[1], frustum->position),
		top_v2 = Vector3Subtract(corners[0], frustum->position),
		top_normal = Vector3Normalize(Vector3CrossProduct(top_v2, top_v1));
    frustum->planes[FRUSTUM_TOP] = (Plane){top_normal, -Vector3DotProduct(top_normal, frustum->position)};

    /* Bottom plane */
    Vector3 
		bottom_v1 = Vector3Subtract(corners[2], frustum->position),
		bottom_v2 = Vector3Subtract(corners[3], frustum->position),
		bottom_normal = Vector3Normalize(Vector3CrossProduct(bottom_v2, bottom_v1));
    frustum->planes[FRUSTUM_BOTTOM] = (Plane){bottom_normal, -Vector3DotProduct(bottom_normal, frustum->position)};

    /* Near and far planes */
    frustum->planes[FRUSTUM_NEAR] = (Plane){frustum->forward, -Vector3DotProduct(frustum->forward, frustum->position) - 0.1f};
    Vector3 far_normal = Vector3Scale(frustum->forward, -1.0f);
    frustum->planes[FRUSTUM_FAR] = (Plane){far_normal, -Vector3DotProduct(far_normal, far_center)};

    frustum->dirty = false;
}

/*
	CONSTRUCTOR
*/
Head *
Head_new(
    Region      region,
    HeadVTable *VTable,
    Engine     *engine,
    size_t      local_data_size
)
{
	Head *head = malloc(sizeof(Head) + local_data_size);

	if (!head) {
		ERR_OUT("Failed to allocate memory for Head.");
		return NULL;
	}
	
	head->next              = head;
	head->prev              = head;

	head->camera            = (Camera3D){0};
	head->camera.up         = V3_UP;
	head->camera.fovy       = 45.0f;
	head->camera.projection = CAMERA_PERSPECTIVE;
	head->engine            = engine;
	head->user_data         = NULL;
	head->region            = region;

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
#ifdef HEAD_USE_RENDER_TEXTURE
	UnloadRenderTexture(Self->viewport);
#endif /* HEAD_USE_RENDER_TEXTURE */
	
	free(Self);
}

void
Head__freeAll(Head *self)
{
    if (self == NULL) return;
    
    Head *current = self;
    Head *first = self;
    
    do {
        Head *next = current->next;
        Head_free(current);
        current = next;
    } while (current != NULL && current != first);
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

Frustum *
Head_getFrustum(Head *head)
{
    return &head->frustum;
}


#ifdef HEAD_USE_RENDER_TEXTURE
RenderTexture *
Head_getViewport(Head *Self)
{
	return &Self->viewport;
}
#endif /* HEAD_USE_RENDER_TEXTURE */


Region
Head_getRegion(Head *Self)
{
	return Self->region;
}

void
Head_setRegion(Head *Self, Region new_region)
{
	Region *region = &Self->region;
	*region = new_region;
#ifdef HEAD_USE_RENDER_TEXTURE
	Self->viewport = LoadRenderTexture(new_region.width, new_region.height);
#endif /* HEAD_USE_RENDER_TEXTURE */
}


void *
Head_getLocalData(Head *Self)
{
	return (void*)&Self->local_data;
}


void *
Head_getUserData(Head *Self)
{
	return Self->user_data;
}


void
Head_setUserData(Head *Self, void *User_Data)
{
	Self->user_data = User_Data;
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
	Camera3D *camera       = &Self->camera;
	
	HeadVTable *vtable = Self->vtable;
	if (vtable && vtable->Update) vtable->Update(Self, delta);

	bool camera_changed = 
		   !Vector3Equals(Self->prev_position, camera->position)
		|| !Vector3Equals(Self->prev_target, camera->target)
		|| Self->prev_fovy   != camera->fovy
		|| Self->prev_width  != Self->region.width
		|| Self->prev_height != Self->region.height;

	if (!(camera_changed || Self->frustum.dirty)) return;

	updateHeadFrustum(Self);

	/* Update previous values */
	Self->prev_position = camera->position;
	Self->prev_target   = camera->target;
	Self->prev_fovy     = camera->fovy;
	Self->prev_width    = Self->region.width;
	Self->prev_height   = Self->region.height;
}


void
Head_preRender(Head *Self)
{
	HeadVTable *vtable = Self->vtable;
	if (vtable && vtable->PreRender) vtable->PreRender(Self);
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
