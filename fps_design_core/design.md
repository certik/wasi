This is a design based on the "Dependable C" philosophy. The goal is to create APIs that are **data-oriented**, **implementation-agnostic**, and **stable**.

You would typically implement this in **C (C99/C11)** or a very strict subset of **C++**.

### 1\. Shared Primitives (`fps_types.h`)

Before defining the core, we define the "Language" that all plugins speak. This ensures the Physics engine and the Renderer agree on what a "Vector" is without knowing about each other.

```c
#ifndef FPS_TYPES_H
#define FPS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// -- The Primitives --
typedef uint32_t FPS_EntityID;
typedef uint32_t FPS_EntityType; // e.g., TYPE_PLAYER, TYPE_BUILDING, TYPE_GUN

typedef struct {
    float x, y, z;
} FPS_Vec3;

typedef struct {
    float x, y, z, w;
} FPS_Quat; // Quaternion for rotation

// -- The State of a single Entity --
// This is the "Atom" of your universe.
typedef struct {
    FPS_EntityID id;
    FPS_EntityType type;
    FPS_Vec3 position;
    FPS_Quat rotation;
    FPS_Vec3 velocity;
    
    // Bitmask for quick queries (Is Grounded, Is Dead, Is Interactable)
    uint32_t state_flags; 
    
    // Generic parameter array for gameplay data (Health, Ammo, Door Open %)
    // Steenberg prefers generic arrays over specific named structs to allow new features later.
    float params[16]; 
} FPS_EntityState;

// -- Input Primitive --
// We don't use "Key_W" or "Button_A". We use abstract Actions.
typedef struct {
    float move_axis_x;  // -1.0 to 1.0 (Strafe)
    float move_axis_y;  // -1.0 to 1.0 (Forward/Back)
    float look_axis_x;  // Mouse Delta X
    float look_axis_y;  // Mouse Delta Y
    bool action_primary; // e.g., Fire
    bool action_interact; // e.g., Pickup/Open
    bool action_jump;
} FPS_InputFrame;

#endif
```

-----

### 2\. The Core API (`fps_core.h`)

The Core is the "Black Box." It holds the list of Entities and advances time. It does *not* know how to calculate collisions or draw pixels; it delegates that.

```c
#ifndef FPS_CORE_H
#define FPS_CORE_H

#include "fps_types.h"

// Opaque handle to the core instance
typedef struct FPS_Core_T* FPS_CoreHandle;

// -- Lifecycle --
FPS_CoreHandle FPS_Core_Init();
void FPS_Core_Destroy(FPS_CoreHandle core);

// -- The "Tick" --
// Advances the simulation by one step (dt).
// 1. Applies Input to Player Entity
// 2. Calls Physics Plugin to resolve movement
// 3. Calls Logic Plugin to handle interactions (pickup, damage)
void FPS_Core_Update(FPS_CoreHandle core, FPS_InputFrame input, float dt);

// -- State Access (The "Truth") --
// Tooling and Renderers use this to see the world.
uint32_t FPS_Core_GetEntityCount(FPS_CoreHandle core);
FPS_EntityState* FPS_Core_GetEntities(FPS_CoreHandle core); // Returns array of all entities

// -- Mutation --
// Plugins call these to change the world.
FPS_EntityID FPS_Core_SpawnEntity(FPS_CoreHandle core, FPS_EntityType type, FPS_Vec3 pos);
void FPS_Core_DestroyEntity(FPS_CoreHandle core, FPS_EntityID id);
void FPS_Core_UpdateEntityParam(FPS_CoreHandle core, FPS_EntityID id, int param_index, float value);

// -- Plugin Registration --
// This is how we plug in behavior.
void FPS_Core_RegisterPhysics(FPS_CoreHandle core, void* physics_interface);
void FPS_Core_RegisterGameplay(FPS_CoreHandle core, void* gameplay_interface);

#endif
```

-----

### 3\. The Plugin Interfaces

These are the "Protocols" or "Contracts" that external modules must satisfy.

#### A. The Physics Plugin (`fps_plugin_physics.h`)

The Core asks this plugin: "This entity wants to move here. Is that allowed?"

```c
typedef struct {
    // The Core calls this every tick for moving entities.
    // Input: Current Pos, Desired Velocity. 
    // Output: New Pos (sliding along walls, stopping at floor).
    FPS_Vec3 (*ResolveMovement)(FPS_Vec3 current_pos, FPS_Vec3 velocity, float dt, FPS_EntityState* all_entities, int count);

    // Used for shooting/picking up objects
    // Returns the ID of the entity hit by the ray.
    FPS_EntityID (*Raycast)(FPS_Vec3 origin, FPS_Vec3 direction, float max_dist, FPS_EntityState* all_entities, int count);
} FPS_PhysicsInterface;
```

#### B. The Gameplay Logic Plugin (`fps_plugin_logic.h`)

This defines what makes your game an FPS.

```c
typedef struct {
    // Called when 'action_primary' is true
    void (*OnFireWeapon)(FPS_CoreHandle core, FPS_EntityID shooter_id);

    // Called when 'action_interact' is true.
    // The plugin uses Core_GetEntities to find the closest item, 
    // checks the Raycast, and then calls Core_UpdateEntityParam to "pick it up".
    void (*OnInteract)(FPS_CoreHandle core, FPS_EntityID player_id);
} FPS_GameplayInterface;
```

#### C. The Renderer (The "Viewer")

The Renderer is usually the *driver* of the application loop, or it runs on its own thread. It is a **Subscriber**.

```c
// The Renderer doesn't dictate the game. It just visualizes the Core's state.
void FPS_Renderer_DrawFrame(FPS_EntityState* entities, int count, FPS_Vec3 camera_pos) {
    for(int i=0; i<count; i++) {
        FPS_EntityState e = entities[i];
        
        // The Renderer maps the Abstract Type to a Concrete Asset
        // The Core doesn't know about "gun_mesh.obj", only "TYPE_WEAPON"
        Model* model = AssetManager_GetModelForType(e.type);
        
        Render_DrawModel(model, e.position, e.rotation);
    }
}
```

-----

### 4\. Platform / Foundation APIs (from current code)

The nostdlib build relies on a small, platform-neutral surface provided in `platform/platform.h`. Add these to the design so hosts know what the runtime guarantees:

- Memory / heap: `wasi_heap_base()`, `wasi_heap_size()`, `wasi_heap_grow(size_t)`
- Process exit: `wasi_proc_exit(int status)`
- File descriptors and rights: types `wasi_fd_t`, `ciovec_t`, `iovec_t`; rights/oflags constants (`WASI_RIGHTS_READ/WRITE/RDWR`, `WASI_O_CREAT`, `WASI_O_TRUNC`, `WASI_SEEK_SET/CUR/END`, std fds)
- FD ops: `wasi_fd_write/read/seek/tell/close`, `wasi_path_open`
- Args: `wasi_args_sizes_get`, `wasi_args_get`
- Math intrinsics: `fast_sqrt`, `fast_sqrtf`
- Lifecycle: `platform_init(int argc, char** argv)` with `PLATFORM_SKIP_ENTRY` semantics (either platform-provided `_start` calls `platform_init`+`app_main`, or caller provides entrypoint and must call `platform_init`)
- File mapping for zero-copy scene loads: `platform_read_file_mmap`, `platform_file_unmap`

### 5\. Memory / Allocators

Higher layers cannot assume malloc/free; the repo exposes these allocator APIs:

- Buddy allocator (`base/buddy.h`): `buddy_init`, `buddy_alloc(size_t, size_t*)`, `buddy_free`, `buddy_print_stats`
- Arena allocator (`base/arena.h`): `Arena* arena_new(size_t)`, `void* arena_alloc(Arena*, size_t)`, `arena_pos_t arena_get_pos`, `void arena_reset`, `void arena_free`, `arena_chunk_count`, `arena_current_chunk_index`, macro `arena_alloc_array`
- Scratch rotation (`base/scratch.h`): `scratch_begin`, `scratch_begin_avoid_conflict(Arena *conflict)`, `scratch_begin_from_arena`, `scratch_end`
- Minimal libc replacements (`base/mem.h`, `base/base_string.h`) for strlen/memcpy/etc.
- FD-based I/O helpers: `base/base_io.h` (`write_all`, `writeln`, `writeln_int`, `writeln_loc`, `PRINT_ERR`) and `base/io.h` (`read_file`, `read_file_ok`, `println` macros) built on the wasi_fd API above.

### 6\. Asset / Rendering Pipeline Contracts

The engine and tools move scene data through a serialized format; these should live alongside the core/gameplay design:

- Scene format (`scene_format.h`): `SceneHeader` + arrays (`SceneVertex`, `SceneLight`, `SceneTexture`) and string arena; constants `SCENE_MAGIC`, `SCENE_VERSION`
- Scene builder (`scene_builder.h`): `SceneConfig` (map grid, spawn, asset/texture paths), `scene_builder_create`, `scene_builder_generate`, `scene_builder_serialize`, `scene_builder_save`, `scene_builder_free` (arena-backed, no malloc)
- Scene runtime / engine (`engine.h`): scene lifetime (`scene_load_from_memory/file`, `scene_get_header`, `scene_free` with mmap handle), engine (`engine_create`, `engine_upload_scene`, `engine_load_textures`, `engine_render`, `engine_free`), driven by SDL GPU device; uniforms supplied by game layer
- Host/SDL glue: SDL entrypoint calls `platform_init` (see `game.c`), and platform input must map host events into the abstract `FPS_InputFrame` the core consumes.

-----

#### How the Asset / Rendering Pipeline plugs into the plugin architecture

- **Renderer-as-plugin:** Treat the renderer as another plugin with a narrow contract. It subscribes to the core’s `FPS_EntityState` array and also consumes a `Scene` (static world mesh + textures) loaded through the asset pipeline. The renderer does *not* mutate core state.

- **Data flow on startup:** 
  1) Platform initializes memory/IO.  
  2) Asset loader maps or reads a scene blob (`platform_read_file_mmap` → `scene_load_from_memory`, or buffered read → `scene_load_from_file`).  
  3) Renderer plugin is created (`engine_create(SDL_GPUDevice*)`), uploads static geometry (`engine_upload_scene`), loads textures (`engine_load_textures`).  
  4) Core and gameplay/physics plugins are registered. Renderer receives the `Scene*` plus per-frame dynamic data (camera + uniforms) from the gameplay layer.

- **Per-frame loop:** 
  - Input → `FPS_Core_Update`.  
  - Renderer pulls truth via `FPS_Core_GetEntities` and submits draw calls with the already-uploaded scene buffers: `engine_render(engine, cmdbuf, render_pass, uniforms_ptr, uniform_size)`. Uniforms come from game code (e.g., camera matrices, lights).

- **Asset loading rules:** 
  - Prefer zero-copy: `platform_read_file_mmap` to get `(handle, data, size)` and pass `use_mmap=true` to `scene_load_from_memory`; release with `platform_file_unmap` inside `scene_free`.  
  - Fallback: buffered read (`read_file`) into arena storage; call `scene_load_from_memory` with `use_mmap=false`, `mmap_handle=0`.  
  - Builder side uses an arena (no malloc) to generate and serialize scene blobs; saving uses `scene_builder_save` (fd write via platform API).

- **Renderer plugin interface (proposal, aligning with existing engine):**
  ```c
  typedef struct FPS_Renderer_T* FPS_Renderer;

  // Construct/destroy with platform/SDL GPU device
  FPS_Renderer FPS_Renderer_Create(SDL_GPUDevice *device);
  void FPS_Renderer_Destroy(FPS_Renderer r);

  // Static scene upload (once at load)
  bool FPS_Renderer_LoadScene(FPS_Renderer r, const Scene *scene);

  // Optional: load/refresh textures separately
  bool FPS_Renderer_LoadTextures(FPS_Renderer r, const Scene *scene);

  // Per-frame draw: entities from core + game-provided uniforms
  bool FPS_Renderer_Draw(FPS_Renderer r,
                         const FPS_EntityState *entities, int count,
                         const void *uniforms, uint32_t uniform_size,
                         SDL_GPUCommandBuffer *cmdbuf,
                         SDL_GPURenderPass *render_pass);
  ```
  This wraps the existing `engine_*` API and makes the renderer swappable without exposing SDL details to the core.

-----

### 7\. Tooling: The "Recorder"

Because we architected the Core to take a generic `FPS_InputFrame` and output `FPS_EntityState`, we can build a "Black Box Recorder" trivially.

```c
// tool_recorder.c

void RecordSession(char* filename) {
    FPS_CoreHandle core = FPS_Core_Init();
    FILE* file = fopen(filename, "wb");
    
    while(GameIsRunning) {
        // 1. Get Input from Platform Layer
        FPS_InputFrame input = Platform_GetInput();
        
        // 2. Write Input to Disk (The "Recording")
        fwrite(&input, sizeof(FPS_InputFrame), 1, file);
        
        // 3. Tick Core
        FPS_Core_Update(core, input, 0.016f);
        
        // 4. Render
        FPS_Renderer_DrawFrame(FPS_Core_GetEntities(core), ...);
    }
}

void ReplaySession(char* filename) {
    FPS_CoreHandle core = FPS_Core_Init();
    FILE* file = fopen(filename, "rb");
    FPS_InputFrame recorded_input;
    
    // Replay exact simulation without user touching the keyboard
    while(fread(&recorded_input, sizeof(FPS_InputFrame), 1, file)) {
        FPS_Core_Update(core, recorded_input, 0.016f);
        FPS_Renderer_DrawFrame(FPS_Core_GetEntities(core), ...);
    }
}
```

### Summary of Responsibilities

1.  **Platform Layer:** Captures mouse movements, normalizes them into `FPS_InputFrame`.
2.  **Core:** Takes `FPS_InputFrame`. Updates Player Entity position.
3.  **Physics Plugin:** Called by Core. Checks if Player Entity is colliding with Building Entity. Corrects position.
4.  **Logic Plugin:** Checks if Player picked up an item. If yes, sets `ItemEntity.parent = PlayerID`.
5.  **Renderer:** Reads the array of Entity States. Draws a gun model at the player's hand position (because the state says the gun is parented to the player).

This architecture allows you to:

  * Swap the **Renderer** from OpenGL to Vulkan without breaking the game logic.
  * Swap the **Physics** from a simple box collider to a complex physics engine without rewriting the renderer.
  * Write a **Bot** that generates `FPS_InputFrame` data programmatically to test the game at 1000x speed (headless).
