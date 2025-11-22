#include "map_physics.h"

#include <base/base_math.h>

static FPS_MapPhysics *g_active = NULL;

static bool is_walkable(const FPS_Map *map, float x, float z, float radius) {
    int min_x = (int)(x - radius);
    int max_x = (int)(x + radius);
    int min_z = (int)(z - radius);
    int max_z = (int)(z + radius);

    for (int tz = min_z; tz <= max_z; tz++) {
        if (tz < 0 || tz >= map->height) {
            return false;
        }
        for (int tx = min_x; tx <= max_x; tx++) {
            if (tx < 0 || tx >= map->width) {
                return false;
            }
            int cell = map->cells[tz * map->width + tx];
            if (fps_map_cell_is_solid(cell)) {
                return false;
            }
        }
    }
    return true;
}

static FPS_Vec3 resolve_movement(FPS_Vec3 current_pos,
                                 FPS_Vec3 velocity,
                                 float dt,
                                 FPS_EntityState *all_entities,
                                 int count) {
    (void)all_entities;
    (void)count;
    if (!g_active || !g_active->map) {
        return current_pos;
    }
    float step_x = velocity.x * dt;
    float step_z = velocity.z * dt;

    float next_x = current_pos.x;
    float next_z = current_pos.z;
    if (is_walkable(g_active->map, current_pos.x + step_x, current_pos.z, g_active->collision_radius)) {
        next_x += step_x;
    }
    if (is_walkable(g_active->map, next_x, current_pos.z + step_z, g_active->collision_radius)) {
        next_z += step_z;
    }
    FPS_Vec3 next = {next_x, current_pos.y, next_z};
    return next;
}

static FPS_EntityID raycast(FPS_Vec3 origin,
                            FPS_Vec3 direction,
                            float max_dist,
                            FPS_EntityState *all_entities,
                            int count) {
    (void)origin;
    (void)direction;
    (void)max_dist;
    (void)all_entities;
    (void)count;
    return 0;
}

void fps_map_physics_init(FPS_MapPhysics *physics, const FPS_Map *map, float collision_radius) {
    if (!physics) return;
    physics->map = map;
    physics->collision_radius = collision_radius;
    g_active = physics;
}

FPS_PhysicsInterface fps_map_physics_interface(void) {
    FPS_PhysicsInterface iface;
    iface.ResolveMovement = resolve_movement;
    iface.Raycast = raycast;
    return iface;
}
