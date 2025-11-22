#include "../core/core.h"
#include "../core/map.h"
#include "../physics/map_physics.h"
#include "../render/text/renderer.h"

#include <base/base_io.h>
#include <base/mem.h>

enum { FPS_ENTITY_PLAYER = 1 };

static void write_cstr(const char *text) {
    ciovec_t iov = {text, base_strlen(text)};
    write_all(WASI_STDOUT_FD, &iov, 1);
}

int app_main(void) {
    FPS_Map map;
    FPS_MapSpawn spawn;
    if (!fps_map_load_default(&map, &spawn)) {
        PRINT_ERR("map load failed");
        return 1;
    }

    FPS_CoreConfig cfg = {
        .move_speed = 1.0f,
        .turn_speed = 0.0f,
        .collision_radius = 0.2f,
    };
    FPS_CoreHandle core = FPS_Core_Init(&cfg);
    if (!core) {
        PRINT_ERR("core init failed");
        return 1;
    }

    FPS_MapPhysics physics_state;
    fps_map_physics_init(&physics_state, &map, cfg.collision_radius);
    FPS_PhysicsInterface physics_iface = fps_map_physics_interface();
    FPS_Core_RegisterPhysics(core, &physics_iface);

    FPS_TextRenderer renderer;
    fps_text_renderer_init(&renderer, &map);

    FPS_EntityID player = FPS_Core_SpawnEntity(core, FPS_ENTITY_PLAYER, (FPS_Vec3){spawn.x, 0.0f, spawn.z});
    FPS_Core_UpdateEntityParam(core, player, FPS_PARAM_YAW, 0.0f);

    write_cstr("== Start ==\n");
    fps_text_renderer_draw(&renderer, FPS_Core_GetEntities(core), (int)FPS_Core_GetEntityCount(core));

    FPS_InputFrame input;
    base_memset(&input, 0, sizeof(input));
    input.move_axis_y = -1.0f; // step north
    FPS_Core_Update(core, input, 1.0f);

    write_cstr("== After move ==\n");
    fps_text_renderer_draw(&renderer, FPS_Core_GetEntities(core), (int)FPS_Core_GetEntityCount(core));

    FPS_Core_Destroy(core);
    return 0;
}
