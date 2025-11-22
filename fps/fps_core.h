#ifndef FPS_CORE_H
#define FPS_CORE_H

#include "fps_types.h"
#include "fps_plugin_physics.h"
#include "fps_plugin_logic.h"

// Opaque handle to the core instance
typedef struct FPS_Core_T* FPS_CoreHandle;

// -- Lifecycle --
FPS_CoreHandle FPS_Core_Init(void);
void FPS_Core_Destroy(FPS_CoreHandle core);

// -- The "Tick" --
// Advances the simulation by one step (dt).
void FPS_Core_Update(FPS_CoreHandle core, FPS_InputFrame input, float dt);

// -- State Access --
uint32_t FPS_Core_GetEntityCount(FPS_CoreHandle core);
FPS_EntityState* FPS_Core_GetEntities(FPS_CoreHandle core); // Returns array of all entities

// -- Mutation --
FPS_EntityID FPS_Core_SpawnEntity(FPS_CoreHandle core, FPS_EntityType type, FPS_Vec3 pos);
void FPS_Core_DestroyEntity(FPS_CoreHandle core, FPS_EntityID id);
void FPS_Core_UpdateEntityParam(FPS_CoreHandle core, FPS_EntityID id, int param_index, float value);

// -- Plugin Registration --
void FPS_Core_RegisterPhysics(FPS_CoreHandle core, const FPS_PhysicsInterface* physics_interface);
void FPS_Core_RegisterGameplay(FPS_CoreHandle core, const FPS_GameplayInterface* gameplay_interface);

#endif // FPS_CORE_H
