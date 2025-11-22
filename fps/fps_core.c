#include "fps_core.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct FPS_Core_T {
    FPS_EntityState *entities;
    uint32_t count;
    uint32_t capacity;
    FPS_EntityID next_id;

    FPS_PhysicsInterface physics;
    int has_physics;

    FPS_GameplayInterface gameplay;
    int has_gameplay;
};

static FPS_EntityState* fps_find_entity(FPS_CoreHandle core, FPS_EntityID id) {
    for (uint32_t i = 0; i < core->count; i++) {
        if (core->entities[i].id == id) {
            return &core->entities[i];
        }
    }
    return NULL;
}

static void fps_ensure_capacity(FPS_CoreHandle core, uint32_t desired) {
    if (desired <= core->capacity) {
        return;
    }
    uint32_t new_cap = core->capacity ? core->capacity * 2u : 16u;
    if (new_cap < desired) {
        new_cap = desired;
    }
    FPS_EntityState *new_entities = (FPS_EntityState*)realloc(core->entities, new_cap * sizeof(FPS_EntityState));
    if (!new_entities) {
        fprintf(stderr, "FPS_Core: allocation failure\n");
        exit(1);
    }
    core->entities = new_entities;
    core->capacity = new_cap;
}

FPS_CoreHandle FPS_Core_Init(void) {
    FPS_CoreHandle core = (FPS_CoreHandle)calloc(1, sizeof(*core));
    if (!core) {
        return NULL;
    }
    core->next_id = 1;
    return core;
}

void FPS_Core_Destroy(FPS_CoreHandle core) {
    if (!core) return;
    free(core->entities);
    free(core);
}

uint32_t FPS_Core_GetEntityCount(FPS_CoreHandle core) {
    return core ? core->count : 0;
}

FPS_EntityState* FPS_Core_GetEntities(FPS_CoreHandle core) {
    return core ? core->entities : NULL;
}

FPS_EntityID FPS_Core_SpawnEntity(FPS_CoreHandle core, FPS_EntityType type, FPS_Vec3 pos) {
    if (!core) return 0;
    fps_ensure_capacity(core, core->count + 1);
    FPS_EntityState *e = &core->entities[core->count++];
    memset(e, 0, sizeof(*e));
    e->id = core->next_id++;
    e->type = type;
    e->position = pos;
    e->rotation = (FPS_Quat){0, 0, 0, 1};
    return e->id;
}

void FPS_Core_DestroyEntity(FPS_CoreHandle core, FPS_EntityID id) {
    if (!core) return;
    for (uint32_t i = 0; i < core->count; i++) {
        if (core->entities[i].id == id) {
            core->entities[i] = core->entities[core->count - 1];
            core->count--;
            return;
        }
    }
}

void FPS_Core_UpdateEntityParam(FPS_CoreHandle core, FPS_EntityID id, int param_index, float value) {
    FPS_EntityState *e = fps_find_entity(core, id);
    if (!e) return;
    if (param_index >= 0 && param_index < 16) {
        e->params[param_index] = value;
    }
}

void FPS_Core_RegisterPhysics(FPS_CoreHandle core, const FPS_PhysicsInterface* physics_interface) {
    if (!core || !physics_interface) return;
    core->physics = *physics_interface;
    core->has_physics = 1;
}

void FPS_Core_RegisterGameplay(FPS_CoreHandle core, const FPS_GameplayInterface* gameplay_interface) {
    if (!core || !gameplay_interface) return;
    core->gameplay = *gameplay_interface;
    core->has_gameplay = 1;
}

static FPS_Vec3 fps_vec3_add(FPS_Vec3 a, FPS_Vec3 b) {
    FPS_Vec3 r = {a.x + b.x, a.y + b.y, a.z + b.z};
    return r;
}

static FPS_Vec3 fps_vec3_scale(FPS_Vec3 a, float s) {
    FPS_Vec3 r = {a.x * s, a.y * s, a.z * s};
    return r;
}

void FPS_Core_Update(FPS_CoreHandle core, FPS_InputFrame input, float dt) {
    if (!core || core->count == 0) {
        return;
    }

    // For the mock, treat the first entity as the player we drive with input.
    FPS_EntityState *player = &core->entities[0];
    player->velocity.x = input.move_axis_x;
    player->velocity.z = input.move_axis_y;

    FPS_Vec3 desired_move = fps_vec3_scale(player->velocity, dt);
    FPS_Vec3 target_pos = fps_vec3_add(player->position, desired_move);

    if (core->has_physics && core->physics.ResolveMovement) {
        target_pos = core->physics.ResolveMovement(player->position,
                                                   desired_move,
                                                   dt,
                                                   core->entities,
                                                   (int)core->count);
    }

    player->position = target_pos;

    if (core->has_gameplay) {
        if (input.action_primary && core->gameplay.OnFireWeapon) {
            core->gameplay.OnFireWeapon(core, player->id);
        }
        if (input.action_interact && core->gameplay.OnInteract) {
            core->gameplay.OnInteract(core, player->id);
        }
    }
}
