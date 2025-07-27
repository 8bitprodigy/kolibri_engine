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

This engine is so simple, it weighs in at only \~1600 lines of C.

---

## Public API:

These are the APIs used to make your game(s).

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

### **engine.h**:

- *Typedefs*:
  
  - *Callbacks:*
    
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

### **head.h**:

### **scene.h**:

## Protected API:

These APIs are used internally within the engine. They are NOT to be used to make games. They are documented here in order to document the engine itself, and enable developers to better understand the engine for education or modification.

### **\_collison\_.h**:

### **\_engine\_.h**:

### **\_entity\_.h**:

### **\_head\_.h**:

### **\_renderer\_.h**:

### **\_scene\_.h**:



---

## Systems:

### Engine

This is the main system you will interface with in your game. All the other systems are subordinate to this in some manner. It uses an opaque struct to hold its state and references to all the other, subordinate systems.

### Entity

This is what will appear in the world - enemies, props, projectiles, etc.. You will create templates of this struct to define the properties and behaviors of your Entities.

### Head

This couples a camera, input, pointer to an entity, and rendering context together. You will use this to see the world and interface with it.

### Scene

### Collision

### Renderer

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
