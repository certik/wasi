#ifndef FPS_PLUGIN_PHYSICS_H
#define FPS_PLUGIN_PHYSICS_H

#include "types.h"

typedef struct {
    FPS_Vec3 (*ResolveMovement)(FPS_Vec3 current_pos,
                                FPS_Vec3 velocity,
                                float dt,
                                FPS_EntityState *all_entities,
                                int count);

    FPS_EntityID (*Raycast)(FPS_Vec3 origin,
                            FPS_Vec3 direction,
                            float max_dist,
                            FPS_EntityState *all_entities,
                            int count);
} FPS_PhysicsInterface;

#endif // FPS_PLUGIN_PHYSICS_H
