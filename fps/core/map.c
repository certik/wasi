#include "map.h"

#include <base/mem.h>

// Map layout copied from the original game. Values:
// 1 = wall, 2/3 = solid props, 9 = ceiling light marker, 5-8 = spawn markers.
#define FPS_MAP_WIDTH 10
#define FPS_MAP_HEIGHT 10

static const int g_default_map[FPS_MAP_HEIGHT][FPS_MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1},
    {1,7,9,0,0,0,9,0,0,1},
    {1,0,1,2,1,0,2,0,0,1},
    {1,0,1,0,9,0,0,1,0,1},
    {1,0,1,0,1,0,0,9,0,1},
    {1,0,1,0,3,0,1,0,0,1},
    {1,0,3,0,1,1,0,0,1,1},
    {1,0,1,0,1,0,0,0,0,1},
    {1,0,0,9,1,0,0,1,0,1},
    {1,1,1,1,1,1,1,1,1,1}
};

static int find_spawn(FPS_Map *map, FPS_MapSpawn *out_spawn) {
    if (!out_spawn) {
        return 0;
    }
    for (int z = 0; z < map->height; z++) {
        for (int x = 0; x < map->width; x++) {
            int cell = map->cells[z * map->width + x];
            if (cell >= 5 && cell <= 8) {
                out_spawn->x = (float)x + 0.5f;
                out_spawn->z = (float)z + 0.5f;
                if (cell == 5) {
                    out_spawn->yaw = -3.14159265f * 0.5f;
                } else if (cell == 6) {
                    out_spawn->yaw = 0.0f;
                } else if (cell == 7) {
                    out_spawn->yaw = 3.14159265f * 0.5f;
                } else {
                    out_spawn->yaw = 3.14159265f;
                }
                map->cells[z * map->width + x] = 0;
                return 1;
            }
        }
    }
    return 0;
}

bool fps_map_load_default(FPS_Map *map, FPS_MapSpawn *out_spawn) {
    if (!map) {
        return false;
    }
    map->width = FPS_MAP_WIDTH;
    map->height = FPS_MAP_HEIGHT;
    base_memset(map->cells, 0, sizeof(map->cells));
    if (FPS_MAP_WIDTH * FPS_MAP_HEIGHT > (int)(sizeof(map->cells) / sizeof(map->cells[0]))) {
        return false;
    }
    for (int z = 0; z < FPS_MAP_HEIGHT; z++) {
        for (int x = 0; x < FPS_MAP_WIDTH; x++) {
            map->cells[z * FPS_MAP_WIDTH + x] = g_default_map[z][x];
        }
    }
    if (out_spawn) {
        if (!find_spawn(map, out_spawn)) {
            out_spawn->x = 1.5f;
            out_spawn->z = 1.5f;
            out_spawn->yaw = 0.0f;
        }
    }
    return true;
}

bool fps_map_within(const FPS_Map *map, int x, int z) {
    if (!map) return false;
    if (x < 0 || z < 0) return false;
    if (x >= map->width || z >= map->height) return false;
    return true;
}

int fps_map_cell(const FPS_Map *map, int x, int z) {
    if (!fps_map_within(map, x, z)) {
        return 1;
    }
    return map->cells[z * map->width + x];
}

bool fps_map_cell_is_solid(int cell_value) {
    return cell_value == 1 || cell_value == 2 || cell_value == 3;
}
