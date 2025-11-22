#include "core.h"

#include <base/base_math.h>
#include <base/buddy.h>
#include <base/mem.h>

#define FPS_CORE_MAX_ENTITIES 64

struct FPS_Core_T {
    FPS_EntityState entities[FPS_CORE_MAX_ENTITIES];
    uint32_t count;
    FPS_EntityID next_id;
    FPS_PhysicsInterface physics;
    bool has_physics;
    FPS_GameplayInterface gameplay;
    bool has_gameplay;
    FPS_CoreConfig config;
};

static FPS_EntityState* fps_find_entity(FPS_CoreHandle core, FPS_EntityID id) {
    for (uint32_t i = 0; i < core->count; i++) {
        if (core->entities[i].id == id) {
            return &core->entities[i];
        }
    }
    return NULL;
}

static FPS_CoreConfig fps_default_config(void) {
    FPS_CoreConfig cfg;
    cfg.move_speed = 2.0f;      // units per second
    cfg.turn_speed = 2.0f;      // radians per second
    cfg.collision_radius = 0.2f;
    return cfg;
}

FPS_CoreHandle FPS_Core_Init(const FPS_CoreConfig *config) {
    size_t actual_size = 0;
    FPS_CoreHandle core = (FPS_CoreHandle)buddy_alloc(sizeof(*core), &actual_size);
    if (!core || actual_size < sizeof(*core)) {
        return NULL;
    }
    base_memset(core, 0, sizeof(*core));
    core->next_id = 1;
    if (config) {
        core->config = *config;
    } else {
        core->config = fps_default_config();
    }
    return core;
}

void FPS_Core_Destroy(FPS_CoreHandle core) {
    if (!core) return;
    buddy_free(core);
}

uint32_t FPS_Core_GetEntityCount(FPS_CoreHandle core) {
    return core ? core->count : 0;
}

FPS_EntityState* FPS_Core_GetEntities(FPS_CoreHandle core) {
    return core ? core->entities : NULL;
}

FPS_EntityID FPS_Core_SpawnEntity(FPS_CoreHandle core, FPS_EntityType type, FPS_Vec3 pos) {
    if (!core) return 0;
    if (core->count >= FPS_CORE_MAX_ENTITIES) {
        return 0;
    }
    FPS_EntityState *e = &core->entities[core->count++];
    base_memset(e, 0, sizeof(*e));
    e->id = core->next_id++;
    e->type = type;
    e->position = pos;
    e->rotation = (FPS_Quat){0.0f, 0.0f, 0.0f, 1.0f};
    return e->id;
}

void FPS_Core_UpdateEntityParam(FPS_CoreHandle core, FPS_EntityID id, int param_index, float value) {
    FPS_EntityState *e = fps_find_entity(core, id);
    if (!e) return;
    if (param_index >= 0 && param_index < 16) {
        e->params[param_index] = value;
    }
}

void FPS_Core_RegisterPhysics(FPS_CoreHandle core, const FPS_PhysicsInterface *physics_interface) {
    if (!core || !physics_interface) return;
    core->physics = *physics_interface;
    core->has_physics = true;
}

void FPS_Core_RegisterGameplay(FPS_CoreHandle core, const FPS_GameplayInterface *gameplay_interface) {
    if (!core || !gameplay_interface) return;
    core->gameplay = *gameplay_interface;
    core->has_gameplay = true;
}

static FPS_Vec3 fps_vec3_add(FPS_Vec3 a, FPS_Vec3 b) {
    FPS_Vec3 r = {a.x + b.x, a.y + b.y, a.z + b.z};
    return r;
}

static FPS_Vec3 fps_vec3_scale(FPS_Vec3 a, float s) {
    FPS_Vec3 r = {a.x * s, a.y * s, a.z * s};
    return r;
}

static void fps_apply_rotation(FPS_EntityState *entity, float yaw, float pitch) {
    float half_yaw = yaw * 0.5f;
    float half_pitch = pitch * 0.5f;
    float cy = fast_cos(half_yaw);
    float sy = fast_sin(half_yaw);
    float cp = fast_cos(half_pitch);
    float sp = fast_sin(half_pitch);
    entity->rotation.x = sp;
    entity->rotation.y = sy * cp;
    entity->rotation.z = -sy * sp;
    entity->rotation.w = cy * cp;
}

void FPS_Core_Update(FPS_CoreHandle core, FPS_InputFrame input, float dt) {
    if (!core || core->count == 0) {
        return;
    }

    FPS_EntityState *player = &core->entities[0];
    float yaw = player->params[FPS_PARAM_YAW] + input.look_axis_x * core->config.turn_speed * dt;
    float pitch = player->params[FPS_PARAM_PITCH] + input.look_axis_y * core->config.turn_speed * dt;
    player->params[FPS_PARAM_YAW] = yaw;
    player->params[FPS_PARAM_PITCH] = pitch;

    float forward_x = fast_sin(yaw);
    float forward_z = fast_cos(yaw);
    float right_x = forward_z;
    float right_z = -forward_x;

    FPS_Vec3 velocity = {0.0f, 0.0f, 0.0f};
    velocity.x = (right_x * input.move_axis_x + forward_x * input.move_axis_y) * core->config.move_speed;
    velocity.z = (right_z * input.move_axis_x + forward_z * input.move_axis_y) * core->config.move_speed;
    player->velocity = velocity;

    FPS_Vec3 target_pos = fps_vec3_add(player->position, fps_vec3_scale(velocity, dt));
    if (core->has_physics && core->physics.ResolveMovement) {
        target_pos = core->physics.ResolveMovement(player->position,
                                                   velocity,
                                                   dt,
                                                   core->entities,
                                                   (int)core->count);
    }
    player->position = target_pos;
    fps_apply_rotation(player, yaw, pitch);

    if (core->has_gameplay) {
        if (input.action_primary && core->gameplay.OnFireWeapon) {
            core->gameplay.OnFireWeapon(core, player->id);
        }
        if (input.action_interact && core->gameplay.OnInteract) {
            core->gameplay.OnInteract(core, player->id);
        }
    }
}
