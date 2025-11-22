#ifndef FPS_PLUGIN_PHYSICS_H
#define FPS_PLUGIN_PHYSICS_H

#include "fps_types.h"

typedef struct {
    // The Core calls this every tick for moving entities.
    // Input: Current Pos, Desired Velocity.
    // Output: New Pos (sliding along walls, stopping at floor).
    FPS_Vec3 (*ResolveMovement)(FPS_Vec3 current_pos, FPS_Vec3 velocity, float dt,
                                FPS_EntityState *all_entities, int count);

    // Used for shooting/picking up objects.
    // Returns the ID of the entity hit by the ray.
    FPS_EntityID (*Raycast)(FPS_Vec3 origin, FPS_Vec3 direction, float max_dist,
                            FPS_EntityState *all_entities, int count);
} FPS_PhysicsInterface;

#endif // FPS_PLUGIN_PHYSICS_H
