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

### 4\. Tooling: The "Recorder"

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
