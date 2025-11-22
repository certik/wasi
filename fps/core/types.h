#ifndef FPS_CORE_TYPES_H
#define FPS_CORE_TYPES_H

#include <base/base_types.h>

typedef uint32_t FPS_EntityID;
typedef uint32_t FPS_EntityType;

typedef struct {
    float x, y, z;
} FPS_Vec3;

typedef struct {
    float x, y, z, w;
} FPS_Quat;

typedef struct {
    FPS_EntityID id;
    FPS_EntityType type;
    FPS_Vec3 position;
    FPS_Quat rotation;
    FPS_Vec3 velocity;
    uint32_t state_flags;
    float params[16];
} FPS_EntityState;

typedef struct {
    float move_axis_x;
    float move_axis_y;
    float look_axis_x;
    float look_axis_y;
    bool action_primary;
    bool action_interact;
    bool action_jump;
} FPS_InputFrame;

#endif // FPS_CORE_TYPES_H
