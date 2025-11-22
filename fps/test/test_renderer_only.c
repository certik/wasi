#include "../render/text/renderer.h"
#include "../core/map.h"
#include "../core/types.h"

#include <base/base_io.h>
#include <base/mem.h>

static void write_cstr(const char *text) {
    ciovec_t iov = {text, base_strlen(text)};
    write_all(WASI_STDOUT_FD, &iov, 1);
}

static FPS_Map make_small_map(void) {
    FPS_Map map;
    base_memset(&map, 0, sizeof(map));
    map.width = 5;
    map.height = 3;
    int cells[] = {
        1,1,1,1,1,
        1,0,0,0,1,
        1,1,1,1,1,
    };
    for (int i = 0; i < (int)(sizeof(cells)/sizeof(cells[0])); i++) {
        map.cells[i] = cells[i];
    }
    return map;
}

int app_main(void) {
    FPS_Map map = make_small_map();
    FPS_TextRenderer renderer;
    if (!fps_text_renderer_init(&renderer, &map)) {
        PRINT_ERR("renderer init failed");
        return 1;
    }

    FPS_EntityState entities[2];
    base_memset(entities, 0, sizeof(entities));
    entities[0].id = 1;
    entities[0].type = 1;
    entities[0].position = (FPS_Vec3){2.0f, 0.0f, 1.0f};
    entities[1].id = 2;
    entities[1].type = 2;
    entities[1].position = (FPS_Vec3){3.0f, 0.0f, 1.0f};

    write_cstr("== Renderer pass 1 ==\n");
    fps_text_renderer_draw(&renderer, entities, 2);

    // Move entities and render again.
    entities[0].position.x = 1.0f;
    entities[1].position.x = 2.0f;
    write_cstr("== Renderer pass 2 ==\n");
    fps_text_renderer_draw(&renderer, entities, 2);
    return 0;
}
