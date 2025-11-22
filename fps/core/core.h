#ifndef FPS_CORE_H
#define FPS_CORE_H

#include "types.h"
#include "plugin_physics.h"
#include "plugin_logic.h"

typedef struct FPS_Core_T* FPS_CoreHandle;

typedef struct {
    float move_speed;
    float turn_speed;
    float collision_radius;
} FPS_CoreConfig;

#define FPS_PARAM_YAW 0
#define FPS_PARAM_PITCH 1

FPS_CoreHandle FPS_Core_Init(const FPS_CoreConfig *config);
void FPS_Core_Destroy(FPS_CoreHandle core);

void FPS_Core_Update(FPS_CoreHandle core, FPS_InputFrame input, float dt);

uint32_t FPS_Core_GetEntityCount(FPS_CoreHandle core);
FPS_EntityState* FPS_Core_GetEntities(FPS_CoreHandle core);

FPS_EntityID FPS_Core_SpawnEntity(FPS_CoreHandle core, FPS_EntityType type, FPS_Vec3 pos);
void FPS_Core_UpdateEntityParam(FPS_CoreHandle core, FPS_EntityID id, int param_index, float value);

void FPS_Core_RegisterPhysics(FPS_CoreHandle core, const FPS_PhysicsInterface *physics_interface);
void FPS_Core_RegisterGameplay(FPS_CoreHandle core, const FPS_GameplayInterface *gameplay_interface);

#endif // FPS_CORE_H
