#ifndef FPS_PLUGIN_LOGIC_H
#define FPS_PLUGIN_LOGIC_H

#include "fps_types.h"

// Forward declaration to avoid circular include with fps_core.h
typedef struct FPS_Core_T* FPS_CoreHandle;

typedef struct {
    // Called when 'action_primary' is true
    void (*OnFireWeapon)(FPS_CoreHandle core, FPS_EntityID shooter_id);

    // Called when 'action_interact' is true.
    void (*OnInteract)(FPS_CoreHandle core, FPS_EntityID player_id);
} FPS_GameplayInterface;

#endif // FPS_PLUGIN_LOGIC_H
