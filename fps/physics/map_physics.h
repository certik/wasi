#ifndef FPS_MAP_PHYSICS_H
#define FPS_MAP_PHYSICS_H

#include "../core/core.h"
#include "../core/map.h"

typedef struct {
    const FPS_Map *map;
    float collision_radius;
} FPS_MapPhysics;

void fps_map_physics_init(FPS_MapPhysics *physics, const FPS_Map *map, float collision_radius);
FPS_PhysicsInterface fps_map_physics_interface(void);

#endif // FPS_MAP_PHYSICS_H
