#ifndef FPS_PLUGIN_LOGIC_H
#define FPS_PLUGIN_LOGIC_H

#include "types.h"

typedef struct FPS_Core_T* FPS_CoreHandle;

typedef struct {
    void (*OnFireWeapon)(FPS_CoreHandle core, FPS_EntityID shooter_id);
    void (*OnInteract)(FPS_CoreHandle core, FPS_EntityID player_id);
} FPS_GameplayInterface;

#endif // FPS_PLUGIN_LOGIC_H
