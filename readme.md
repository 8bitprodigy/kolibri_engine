# Kolibri Engine

---

**Rationale:** Create a simple, minimalist 3D game engine in C using raylib for education, hacking, modification, and legitimate game development. With so many game engines focusing on how many features they have, this engine stands apart, stripped down to its bare essentials. 

It doesn't do networking.

It doesn't have a UI.

It doesn't manage assets.

It doesn't manage memory.

It doesn't handle audio or input.

It doesn't have a particle system.

it doesn't have billboards, a native 3D model format, or a native scene format.

It doesn't even have a configuration file format. 

Instead, all these features are left up to the game developer to support.
It's so minimal, one could use it as a framework to build a more specific, more fully-featured engine atop it, making it *not just* a game engine, but a *game engine engine*. All it does have is:

- Main loop with management(pause, unpause, exit) and callbacks

- "Heads" - couples a render surface with a camera and event callbacks

- Basic entity system with callbacks to define behavior

- Generic scene interface with callbacks to support different scene types through dependency injection

- Simple collision system to handle entity-entity collisions(AABB using spatial hashing)

- Simple renderer using sphere-frustum intersection to handle frustum culling

- A dynamic array container API (used internally, but exposed publicly for convenience)

This engine is so simple, it weighs in at only \~3000 lines of C.

---

## Systems:

### Collision

This handles continuous collision between all `Entity`s in the scene.

### DynamicArray

A dynamically resizeable array used internally by the engine and provided to users.

### Engine

This is the main system you will interface with in your game. All the other systems are subordinate to this in some manner. It uses an opaque struct to hold its state and references to all the other, subordinate systems.

### Entity

This is what will appear in the world - enemies, props, projectiles, etc.. You will create templates of this struct to define the properties and behaviors of your Entities.

### Head

This couples a camera, input, pointer to an entity, and rendering context together. You will use this to see the world and interface with it.

### Renderer

This manages rendering the `Scene` to the various screen `Region`s handled by all current `Head`s.

### Scene

This provides an interface which handles the world geometry and collision through callbacks. This is how maps/level formats will be implemented.

### SpatialHash

This provides a broad-phase method to store objects in the `Scene` in order to provide spatial queries for systems like `Collision`, provided to users.


---

## Public API:

These are the APIs used to make your game(s).

### **kolibri.h**

This just imports all the other headers.

## **collision.h**:
  - `CollisionResult Collision_checkAABB( Entity *a, Entity *b)`: Checks if two entities with AABB colliders are colliding and returns a `CollisionResult`.
  - `CollisionResult Collision_checkRayAABB( K_Ray ray, Entity *entity)`: Checks if `K_Ray`, `ray`, intersects with the AABB collider of `entity`.
  - `CollisionResult Collision_checkDiscreet( Entity *a, Entity *b)`: Runs a discreet collision detection check and returns a `CollisionResult`.
  - `CollisionResult Collision_checkContinuous( Entity *a, Entity *b, Vector3 movement)`: Runs a continuous collision detection check and returns a `CollisionResult` with the info.

### **common.h**:

Defines macros, constants, enums, and types used commonly throughout the engine and game codebase.

-    Most macro-defined constants are overridable

- `V3_ZERO`: a Vector3 of all zeroes.

- `V3_UP`: a Vector3 of a +Y-up normal.

- Enumeration of:
  
  - For looping through `Xform.xf[]`:
    
    - `POSITION`: 0
    
    - `ROTATION`: 1
    
    - `SCALE`: 2
    
    - `SKEW`: 3

- Forward declaration of the following structs:
  
  - `Engine`(*Opaque*): This is the main part you will interface with in your game
  
  - `Entity`(*Transparent*): This is what will appear in the world - enemies, props, projectiles, etc.. You will create templates of this struct to define the properties and behaviors of your Entities.
  
  - `Head`(*Opaque*): This couples a camera, input, pointer to an entity, and rendering context together. You will use this to see the world and interface with it.

- The following values: `uint64`, `uint16`, `uint32`, `uint16`, `uint8`, `int64`, `int32`, `int16`, `int8`, `uint`, and `word`. `word` is the same width as the platform's word-size.

- The following structs:
  
  - `CollisionResult`: holds information regarding the result of a collision between two entities. It has the following fields:
    
    - `Vector3 position`: where collision occurred
    
    - `Vector3 normal`: surface normal at collision point
    
    - `float distance`: Distance to collision
    
    - `int material_id`: indicates surface type
    
    - `void *user_data`: scene-specific data
    
    - `bool hit`: self-explanatory
  
  - `EntityList`: holds a pointer to an array of entities for various uses. It has the following fields:
    
    - `Entity **entities`: pointer to the array of entities
    
    - `uint count`: number of entities in the array
    
    - `uint capacity`: actual capacity of the array
  
  - `Renderable`:  holds user-defined information for something which can be rendered to visualize an entity. Used in the `Entity` struct. It has the following fields:
    
    - `Vector3 offset`: an offset to be applied to the renderable if adjusting its placement in relation to the entity is needed
    
    - `void *data`: a pointer to the data to be rendered
    
    - ` void (*RenderableCallback)(Renderable *renderable, Vector3 position, Vector3 rotation, Vector3 scale)`: a function pointer to the callback used to render the `Renderable`
  
  - `Xform` holds `position`, `rotation`, `scale`, and `skew`, internally union'd with `xf[4]` to allow for swizzling.
  
### **dynamicarray.h**
- *Macro Defines*:
  - `DYNAMIC_ARRAY_GROWTH_FACTOR`(default 2.0f): The amount to multiply the size of the array by when growing it(overridable).

- *Macro Functions*:
  - `DynamicArray(type, capacity)`: Constructs a new `DynamicArray` in a simple manner. Takes the type of the data the array will store, followed by the starting capacity for the array.
  - `DynamicArray_add(array, datum)`: Wrapper for `DynamicArray_append()` for adding a single element to the Array.

- *Constructor / Destructor*:
  - `void *DynamicArray_new(size_t datum_size, size_t capacity)`: Constructs a new `DynamicArray` with element size of `datum_size`, containing `capacity` elements.
  - `void DynamicArray_free(void *array)`: Frees the given `DynamicArray`, `array`, from memory.

- *Methods*:
  - `void DynamicArray_grow(void **array)`: Increases the capacity of `array` by `DYNAMIC_ARRAY_GROWTH_FACTOR` times its capacity.
  - `void DynamicArray_shrink(void *array)`: Decreases the capacity of `array` by its capacity divided by `DYNAMIC_ARRAY_GROWTH_FACTOR`.
  - `void DynamicArray_append(void **array, void *data, size_t length)`: Appends `length` number of elements from the given array `data` to the `DynamicArray`, `array`.
  - `size_t DynamicArray_capacity(void *array)`: Gets the capacity of `array`.
  - `void DynamicArray_clear(void *array)`: Clears all entries from `array`(really, it just sets its length to 0).
  - `size_t DynamicArray_datumSize(void *array)`: Gets the size of the type of the element stored in `array`.
  - `void DynamicArray_concat(void **array1, void *array2)`: Concatenates `array2` to `array1`.
  - `void DynamicArray_delete(void *array, size_t index, size_t length)`: Removes the elements starting at `index` for `length` number of cells from `array`.
  - `void DynamicArray_insert(void **array, size_t index, void *data, size_t length)`:Inserts `length` number of elements from `data` into `array` at `index`.
  - `size_t DynamicArray_length(void *array)`: Gets the number of elements currently stored in `array`.
  - `void DynamicArray_replace(void **array, size_t index, void *data, size_t length)`: Replaces `length` number of elements in `array` with the same number of elements from `data`, starting at `index`.

### **engine.h**:

- *Typedefs*:
  
  - *Callback types:*
    
    - `void (*EngineCallback)(Engine *engine)`: General callback.
    
    - `void (*EngineUpdateCallback)(Engine *engine, float delta)`: Called after updating every other system. Passes delta time with it.
    
    - `void (*EngineResizeCallback)(Engine *engine, uint width, uint height)`: Called after resizing the window. Use it to adjust the viewport size of `Head`s if needed.
  
  - *Struct:*
    
    - `EngineVTable`: holds all the callbacks called by Engine. It holds the following fields:
      
      - `EngineCallback Setup`: Called at the end of constructing the `Engine` struct.
      
      - `EngineCallback Run`: Called upon starting the engine with the `Engine_run()` method. Runs before the main loop starts. Use it to initialize certain things.
      
      - `EngineUpdateCallback Update`: Called every frame after updating all the other systems and entities. 
      
      - `EngineCallback Render`: Called after rendering to all the active `Head`s. Used for final composition of the rendered frame before presenting it to the screen.
      
      - `EngineResizeCallback Resize`: Called upon resizing the window. Use it to adjust the viewport size of all the `head`s if needed.
      
      - `EngineCallback Exit`: Called upon stopping the engine. Use it to deinitialize certain things if needed.
      
      - `EngineCallback Free`: Called before freeing up `Engine` from memory. Use it to free up anything you need to get rid of so it's not stuck in memory, inaccessible.
  
  - *Engine Methods:*
    
    - *Constructor/Destructor*
      
      - `Engine *Engine_new( EngineVTable *vtable)`: Constructs a new `Engine` struct, and returns a pointer. Takes a prepared `EngineVTable`, already populated with callbacks.
      
      - `void Engine_free(Engine *engine)`: Frees up an `Engine` from memory. This will call the `Free` callback if it has been provided.

    - *Setters*

      - `void Engine_setVTable(Engine *engine, EngineVTable *vtable)`: Sets the engine's VTable to a different EngineVTable.
      
    - *Getters*

      - `float Engine_getDeltaTime(Engine *engine)`: Gets the delta time for the current frame.

      - `uint64 EngineGetFrameNumber(Engine *engine)`: Gets the number of frames which have been rendered since running the engine.

      - `EntityList *Engine_getEntityList(Engine *engine)`: Gets a list of all the Entities subordinate to the Engine.

      - `Head *Engine_getHeads(Engine *engine)`: Returns a pointer to the root Head.

      - `Scene *Engine_getScene(Engine *engine)`: Returns a pointer to the current Scene.

    - *Methods*

      - `void Engine_run(Engine *engine)`: Runs the Engine, starting the simulation.

      - `void Engine_pause(Engine *engine, bool paused)`: Sets the paused state of the engine. Useful for pausing the simulation, and to pass control between the Engine and a menu.

      - `void Engine_requestExit(Engine *engine)`: Requests the engine to exit on the next update.

### **entity.h**:

- *Typedefs*:
```c
typedef void (*EntityCallback)(         Entity *entity);
typedef void (*EntityCollisionCallback)(Entity *entity, CollisionResult collision);
typedef void (*EntityUpdateCallback)(   Entity *entity, float           delta);

typedef struct
EntityVTable
{
    EntityCallback          Setup;       /* Called on initialization of a new Entity */
    EntityCallback          Enter;       /* Called upon Entity entering the scene */
    EntityUpdateCallback    Update;      /* Called once every tick prior to rendering */
    EntityUpdateCallback    Render;      /* Called once every frame prior to rendering the Entity */
    EntityCollisionCallback OnCollision; /* Called when the Entity collides with something while moving */
    EntityCollisionCallback OnCollided;  /* Called when another Entity collides with this Entity */
    EntityCallback          Exit;        /* Called upon Entity exiting the scene */
    EntityCallback          Free;        /* Called upon freeing Entity from memory */
}
EntityVTable;


typedef struct 
Entity 
{
    void           *user_data;
    EntityVTable   *vtable;
    Renderable     *renderables[  MAX_LOD_LEVELS];
    float           lod_distances[MAX_LOD_LEVELS];
    uint8           lod_count;
    float           visibility_radius;
    float           floor_max_angle;
    union {
        Vector3 bounds;
        struct {
            float
                radius,
                height,
                _reserved_;
        };
    };
    Vector3         
                    bounds_offset,
                    renderable_offset,
                    velocity;
    int            max_slides;
    union {
        Transform transform;
        struct {
            Vector3    position;
            Quaternion orientation;
            Vector3    scale;
        };
    };
    
    struct {
        union {
            uint8 layers;
            struct {
                bool
                    layer_0:1,
                    layer_1:1,
                    layer_2:1,
                    layer_3:1,
                    layer_4:1,
                    layer_5:1,
                    layer_6:1,
                    layer_7:1;
            };
        };
        union {
            uint8 masks;
            struct {
                bool
                    mask_0:1,
                    mask_1:1,
                    mask_2:1,
                    mask_3:1,
                    mask_4:1,
                    mask_5:1,
                    mask_6:1,
                    mask_7:1;
            };
        };
    }
    collision;
    
    union {
        uint8 flags;
        struct {
            bool
                active         :1,
                visible        :1,
                solid          :1; /* Solid or area collision */
            CollisionShape 
                collision_shape:2; /* 0 = None | 1 = AABB | 2 = Cylinder | 3 = Sphere */
            bool
                _flag_5        :1,
                _flag_6        :1,
                _flag_7        :1;
        };
    };

    char *local_data[];
} 
Entity;
```

- *Constructor / Destructor *
  - `Entity *Entity_new(const Entity *template_entity, Scene *scene, size_t local_data_size)`: Constructs a new entity from the given `template_entity`, adding it to the `scene`, with optional `local_data_size` memory allocated at the end.
  - `void Entity_free(Entity *entity)`: Destructs the given `entity` removing it from the scene.

- *Setters / Getters*:
  - `double Entity_getAge(Entity *entity)`: Get the age of the entity in seconds since the time of its creation.
  - `BoundingBox Entity_getBoundingBox(Entity *entity)`: Get the raylib `BoundingBox` of the entity.
  - `Renderable *Entity_getLODRenderable(Entity *entity, Vector3 camera_position)`: Get the `Renderable` for `entity` at the given `camera_position`.
  - `Engine *Entity_getEngine(Entity *entity)`: Get the `engine` which `entity` is subordinate to.
  - `Entity *Entity_getNext(Entity *entity)`: Get the next `Entity` in the linked list in relation to `entity`.
  - `Entity *Entity_getPrev(Entity *entity)`: Get the previous `Entity` in the linked list in relation to `entity`.
  - `Scene *Entity_getScene(Entity *entity)`: Get the `Scene` the `entity` is currently in.
  - `uint64 Entity_getUniqueID(Entity *entity)`: Get the unique ID of `entity`.
  - `bool Entity_isOnFloor(Entity *entity)`: Returns true if `entity` is touching a floor.
  - `bool Entity_isOnWall(Entity *entity)`: Returns true if `entity` is touching a wall.
  - `bool Entity_isOnCeiling(Entity *entity)`: Returns true if `entity` is touching a ceiling.

- *Methods*:
  - `CollisionResult Entity_move(Entity *entity, Vector3 movement)`: Move the `entity` by `movement` until the first collision.
  - `CollisionResult Entity_moveAndSlide(Entity *entity, Vector3 movement)`: Move the `entity` by `movement`, allowing it to slide(requires you set the `Entity.max_slides` value).

### **head.h**:
- *Typedefs*:
```c
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
```

- *Constructor / Destructor*:
  - `Head *Head_new(int Controller_ID, Region region, HeadVTable *vtable, Engine *engine)`: Constructs a new `Head` with a screen region of `region`, the callbacks in `vtable`, adds it to `engine` and returns the pointer.
  - `void Head_free(Head *head)`: Destructs the `head`, calling its `HeadVTable.Free()` callback, and then freeing itself from memory.

- *Setters / Getters*:
  - `Head *Head_getNext(Head *head)`: Gets the pointer to the next `Head` in the linked list from `head`.
  - `Head *Head_getPrev(Head *head)`: Gets the pointer to the previous `Head` in the linked list from `head`.
  - `Camera3D *Head_getCamera(Head *head)`: Gets the pointer to `head`'s `Camera3D`.
  - `Engine *Head_getEngine(Head *head)`: Gets the pointer to the `Engine` the given `head` is in.
  - `Frustum *Head_getFrustum(Head *head)`: Gets the pointer to the `head`'s `Camera3D`'s `Frustum`.
  - `RenderTexture *Head_getViewport(Head *head)`: **Only available if `HEAD_USE_RENDER_TEXTURE` is defined**. Gets the pointer to the `RenderTexture` viewport.
  - `Region Head_getRegion(Head *head)`: Gets the pointer to `head`'s `Region` of the screen it renders to.
  - `void Head_setRegion(Head *head, Region region)`: Sets the `head`'s screen region.
  - `void *Head_getUserData( Head *head)`: Gets the pointer to the data pointed to by `head`.
  - `void Head_setUserData(Head *head, void *user_data)`: Sets the data `head` points to.
  - `void Head_setVTable(Head *head, HeadVTable *vtable)`: Sets the `HeadVTable` `head` uses for callbacks.
  - `HeadVTable *Head_getVTable(Head *head)`: Gets the pointer to the `HeadVTable` currently used by `head`.
  - `RendererSettings *Head_getRendererSettings(Head *head)`: Gets the pointer to the `RenderSettings` used by `head`.

- *Methods*:
  - `void Head_setup(Head *head)`: Calls the `HeadVTable.Setup()` function `head` currently points to.
  - `void Head_update(Head *head, float delta)`: Calls the `HeadVTable.Update()` function `head` currently points to.
  - `void Head_preRender(Head *head)`: Calls the `HeadVTable.PreRender()` function `head` currently points to.
  - `void Head_postRender(Head *head)`: Calls the `HeadVTable.PostRender()` function `head` currently points to.
  - `void Head_exit(Head *head)`: Calls the `HeadVTable.Exit()` function `head` currently points to.
  

### **renderer.h**:
- *Typedefs*:
  - `Renderer`: Opaque struct of the renderer.

- *Methods*:
  - `void Renderer_submitEntity(Renderer *renderer, Entity *entity)`: Called each frame in order to submit an entity to be rendered.
  - `void Renderer_submitGeometry(Renderer *renderer, Renderable *renderable, Vector3 pos, Vector3 bounds)`: Called each frame in order to submit a chunk of geometry(such as level geometry) to be rendered.

### **scene.h**:
- *Typedefs*:
```c
typedef struct Scene Scene;


typedef void            (*SceneCallback)(         Scene *scene);
typedef void            (*SceneDataCallback)(     Scene *scene, void    *map_data);
typedef void            (*SceneUpdateCallback)(   Scene *scene, float    delta);
typedef void            (*SceneEntityCallback)(   Scene *scene, Entity  *entity);
typedef CollisionResult (*SceneCollisionCallback)(Scene *scene, Entity  *entity, Vector3 to);
typedef CollisionResult (*SceneRaycastCallback)(  Scene *scene, Vector3  from,   Vector3 to);
typedef void            (*SceneRenderCallback)(   Scene *scene, Head    *head);


typedef struct
SceneVTable
{
    SceneDataCallback            Setup;          /* Called on initialization */
    SceneCallback                Enter;          /* Called on entering the engine */
    SceneUpdateCallback          Update;         /* Called once every frame after updating all the entities and before rendering */
    SceneEntityCallback          EntityEnter;    /* Called every time an Entity enters the scene */
    SceneEntityCallback          EntityExit;     /* Called every time an Entity exits the scene */
    SceneCollisionCallback       CheckCollision; /* Called when checking if an entity would collide if moved */
    SceneCollisionCallback       MoveEntity;     /* Called Every time an Entity moves in order to check if it has collided with the scene */
    SceneRaycastCallback         Raycast;        /* Called Every time a raycast is performed in order to check if has collided with the scene */
    SceneRenderCallback          Render;         /* Called once every frame in order to render the scene */
    SceneCallback                Exit;           /* Called upon Scene exiting the engine */
    SceneDataCallback            Free;           /* Called upon freeing the Scene from memory */
}
SceneVTable;
```

- *Constructor / Destructor*:
  - `Scene *Scene_new(SceneVTable *scene_type, void *data, Engine *engine)`: Constructs a new `Scene` with the callbacks defined in `scene_type`, `data`, and adds it to `engine`.
  - `void Scene_free(Scene *scene)`: Destructs `scene` by calling its `SceneVTable.Free()` callback, and then freeing itself from memory.

- *Setters / Getters*;
  - `Engine *Scene_getEngine(Scene *scene)`: Gets the pointer to the `Engine` `scene` is currently in.
  - `uint Scene_getEntityCount( Scene *scene)`: Gets the number of `Entity`s in `scene`.
  - `EntityList *Scene_getEntityList(  Scene *scene)`: Gets the `Entity`s in the scene as an array.
  - `void *Scene_getMapData(Scene *scene)`: Gets the pointer to `scene`'s data.

- *Methods*;
  - `void Scene_enter(Scene *scene)`: Calls the `SceneVTable.Enter()` function `scene` currently points to.
  - `void Scene_update(Scene *scene, float    delta)`: Calls the `SceneVTable.Update()` function `scene` currently points to.
  - `void Scene_entityEnter(Scene *scene, Entity  *entity)`: Calls the `SceneVTable.EntityEnter()` function `scene` currently points to.
  - `void Scene_entityExit(Scene *scene, Entity  *entity)`: Calls the `SceneVTable.EntityExit()` function `scene` currently points to.
  - `CollisionResult Scene_checkCollision( Scene *scene, Entity  *entity, Vector3 to)`: Calls the `SceneVTable.CheckCollision()` function `scene` currently points to. Returns the `CollisionResult`.
  - `CollisionResult Scene_checkContinuous(Scene *scene, Entity  *entity, Vector3 movement)`: Calls the `SceneVTable.CheckContinuous()` function `scene` currently points to. Returns the `CollisionResult`.
  - `CollisionResult Scene_raycast(Scene *scene, Vector3  from,   Vector3 to)`: Calls the `SceneVTable.Raycast()` function `scene` currently points to.
  - `void Scene_render(Scene *scene, Head    *head)`: Calls the `SceneVTable.Render()` function `scene` currently points to.
  - `void Scene_exit(Scene *scene)`: Calls the `SceneVTable.Exit()` function `scene` currently points to.


### **spatialhash.h**:
- *Typedefs*:
  - `SpatialHash`: Opaque struct for the spatial hash.

- *Constructor / Destructor*:
  - `SpatialHash *SpatialHash_new(void)`: Construct a new `SpatialHash`.
  - `void SpatialHash_free(SpatialHash *hash)`: Destruct a `SpatialHash`, freeing it from memory.

- *Methods*:
  - `void SpatialHash_clear(SpatialHash *hash)`: Clears all the items from the `SpatialHash`, `hash`.
  - `void  SpatialHash_insert(SpatialHash *hash, void *data, Vector3 center, Vector3 bounds)`: Inserts `data` into `hash` at `center` with `bounds` dimensions for later retrieval.
  - `void *SpatialHash_queryRegion(SpatialHash *hash, BoundingBox region, void **query_results, int *count)`: Queries all the entries in `hash` within `region`, and populates `query_results` with `count` of them.

## Protected API:

These APIs are used internally within the engine. They are NOT to be used to make games. They are documented here in order to enable developers to better understand the engine for education and modification.

### **\_collison\_.h**:

### **\_engine\_.h**:

### **\_entity\_.h**:

### **\_head\_.h**:

### **\_renderer\_.h**:

### **\_scene\_.h**:
  
### **\_spatialhash\_.h**:



---

## License: Public Domain or 0BSD

```
Zero-Clause BSD

=============== 

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
```
