#ifndef FPS_CORE_MAP_H
#define FPS_CORE_MAP_H

#include <base/base_types.h>

typedef struct {
    float x;
    float z;
    float yaw;
} FPS_MapSpawn;

typedef struct {
    int width;
    int height;
    int cells[1024];
} FPS_Map;

bool fps_map_load_default(FPS_Map *map, FPS_MapSpawn *out_spawn);
bool fps_map_within(const FPS_Map *map, int x, int z);
int fps_map_cell(const FPS_Map *map, int x, int z);
bool fps_map_cell_is_solid(int cell_value);

#endif // FPS_CORE_MAP_H
