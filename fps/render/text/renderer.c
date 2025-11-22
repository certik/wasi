#include "renderer.h"

#include <base/base_io.h>
#include <base/mem.h>
#include <base/numconv.h>

static void write_cstr(const char *text) {
    ciovec_t iov = {text, base_strlen(text)};
    write_all(WASI_STDOUT_FD, &iov, 1);
}

static char cell_to_char(int cell) {
    switch (cell) {
        case 1: return '#';
        case 2: return 'B';
        case 3: return 'C';
        case 9: return '*';
        default: return '.';
    }
}

bool fps_text_renderer_init(FPS_TextRenderer *renderer, const FPS_Map *map) {
    if (!renderer || !map) {
        return false;
    }
    renderer->map = map;
    return true;
}

void fps_text_renderer_draw(FPS_TextRenderer *renderer,
                            const FPS_EntityState *entities,
                            int count) {
    if (!renderer || !renderer->map) return;

    write_cstr("\x1b[2J\x1b[H");

    int player_x = -1;
    int player_z = -1;
    if (count > 0) {
        const FPS_EntityState *player = &entities[0];
        player_x = (int)player->position.x;
        player_z = (int)player->position.z;
    }

    char line[128];
    for (int z = 0; z < renderer->map->height; z++) {
        int idx = 0;
        line[idx++] = '|';
        for (int x = 0; x < renderer->map->width; x++) {
            char ch = cell_to_char(renderer->map->cells[z * renderer->map->width + x]);
            if (x == player_x && z == player_z) {
                ch = '@';
            }
            line[idx++] = ch;
        }
        line[idx++] = '|';
        line[idx++] = '\n';
        line[idx] = '\0';
        write_cstr(line);
    }

    write_cstr("Controls: WASD move, Q quit\n");
}
