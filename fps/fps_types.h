#ifndef FPS_TYPES_H
#define FPS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t FPS_EntityID;
typedef uint32_t FPS_EntityType; // e.g., TYPE_PLAYER, TYPE_BUILDING, TYPE_GUN

typedef struct {
    float x, y, z;
} FPS_Vec3;

typedef struct {
    float x, y, z, w;
} FPS_Quat; // Quaternion for rotation

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
    float move_axis_x;  // -1.0 to 1.0 (Strafe)
    float move_axis_y;  // -1.0 to 1.0 (Forward/Back)
    float look_axis_x;  // Mouse Delta X
    float look_axis_y;  // Mouse Delta Y
    bool action_primary; // e.g., Fire
    bool action_interact; // e.g., Pickup/Open
    bool action_jump;
} FPS_InputFrame;

#endif // FPS_TYPES_H
