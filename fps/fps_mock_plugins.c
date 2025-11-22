#include "fps_mock_plugins.h"
#include "fps_core.h"

#include <stdio.h>
#include <math.h>

static FPS_Vec3 mock_resolve(FPS_Vec3 current_pos, FPS_Vec3 velocity, float dt,
                             FPS_EntityState *all_entities, int count) {
    (void)all_entities;
    (void)count;
    // Simple physics: clamp movement length to simulate a collision against a unit sphere.
    float max_step = 1.0f * dt;
    float len_sq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    if (len_sq > max_step * max_step && len_sq > 0.0f) {
        float inv = max_step / (float)sqrt(len_sq);
        velocity.x *= inv;
        velocity.y *= inv;
        velocity.z *= inv;
    }
    FPS_Vec3 result = {current_pos.x + velocity.x, current_pos.y + velocity.y, current_pos.z + velocity.z};
    return result;
}

static FPS_EntityID mock_raycast(FPS_Vec3 origin, FPS_Vec3 direction, float max_dist,
                                 FPS_EntityState *all_entities, int count) {
    (void)origin;
    (void)direction;
    (void)max_dist;
    // Mock: just return the first entity if any.
    if (count > 0) {
        return all_entities[0].id;
    }
    return 0;
}

static void mock_fire(FPS_CoreHandle core, FPS_EntityID shooter_id) {
    (void)core;
    printf("[Gameplay] Entity %u fired\n", shooter_id);
}

static void mock_interact(FPS_CoreHandle core, FPS_EntityID player_id) {
    (void)core;
    printf("[Gameplay] Entity %u interacted\n", player_id);
}

FPS_PhysicsInterface FPS_MockPhysicsInterface(void) {
    FPS_PhysicsInterface iface = {
        .ResolveMovement = mock_resolve,
        .Raycast = mock_raycast,
    };
    return iface;
}

FPS_GameplayInterface FPS_MockGameplayInterface(void) {
    FPS_GameplayInterface iface = {
        .OnFireWeapon = mock_fire,
        .OnInteract = mock_interact,
    };
    return iface;
}
