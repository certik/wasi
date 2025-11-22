#include "../core/core.h"
#include "../core/map.h"
#include "../physics/map_physics.h"
#include "../render/text/renderer.h"

#include <platform/platform.h>
#include <base/base_io.h>
#include <base/mem.h>

enum {
    FPS_ENTITY_PLAYER = 1,
};

static void write_cstr(const char *text) {
    ciovec_t iov = {text, base_strlen(text)};
    write_all(WASI_STDOUT_FD, &iov, 1);
}

static int read_command_char(void) {
    char buf[32];
    iovec_t iov = {buf, sizeof(buf)};
    size_t nread = 0;
    int rc = wasi_fd_read(WASI_STDIN_FD, &iov, 1, &nread);
    if (rc != 0 || nread == 0) {
        return -1;
    }
    for (size_t i = 0; i < nread; i++) {
        char c = buf[i];
        if (c == '\n' || c == '\r') {
            continue;
        }
        return (int)c;
    }
    return -1;
}

int app_main(void) {
    FPS_Map map;
    FPS_MapSpawn spawn;
    if (!fps_map_load_default(&map, &spawn)) {
        PRINT_ERR("Failed to load default map");
        return 1;
    }

    FPS_CoreConfig core_cfg = {
        .move_speed = 2.0f,
        .turn_speed = 2.5f,
        .collision_radius = 0.2f,
    };

    FPS_CoreHandle core = FPS_Core_Init(&core_cfg);
    if (!core) {
        PRINT_ERR("Failed to initialize core");
        return 1;
    }

    FPS_MapPhysics physics_state;
    fps_map_physics_init(&physics_state, &map, core_cfg.collision_radius);
    FPS_PhysicsInterface physics_iface = fps_map_physics_interface();
    FPS_Core_RegisterPhysics(core, &physics_iface);

    FPS_TextRenderer renderer;
    fps_text_renderer_init(&renderer, &map);

    FPS_EntityID player_id = FPS_Core_SpawnEntity(core, FPS_ENTITY_PLAYER, (FPS_Vec3){spawn.x, 0.0f, spawn.z});
    FPS_Core_UpdateEntityParam(core, player_id, FPS_PARAM_YAW, spawn.yaw);

    bool running = true;
    while (running) {
        fps_text_renderer_draw(&renderer,
                               FPS_Core_GetEntities(core),
                               (int)FPS_Core_GetEntityCount(core));
        write_cstr("Command (WASD move, J/L turn, Q quit): ");

        FPS_InputFrame input;
        base_memset(&input, 0, sizeof(input));
        int c = read_command_char();
        if (c < 0) {
            continue;
        }
        switch (c) {
            case 'w':
            case 'W':
                input.move_axis_y = 1.0f;
                break;
            case 's':
            case 'S':
                input.move_axis_y = -1.0f;
                break;
            case 'a':
            case 'A':
                input.move_axis_x = -1.0f;
                break;
            case 'd':
            case 'D':
                input.move_axis_x = 1.0f;
                break;
            case 'j':
            case 'J':
                input.look_axis_x = -1.0f;
                break;
            case 'l':
            case 'L':
                input.look_axis_x = 1.0f;
                break;
            case 'q':
            case 'Q':
                running = false;
                continue;
            default:
                continue;
        }

        FPS_Core_Update(core, input, 0.1f);
    }

    FPS_Core_Destroy(core);
    write_cstr("Bye.\n");
    return 0;
}
