#include "fps_assets.h"

#include <stdio.h>
#include <string.h>

bool FPS_Scene_LoadFromFile(const char *path, FPS_Scene *out_scene) {
    if (!path || !out_scene) {
        return false;
    }
    // Mock: just fill metadata based on the path
    out_scene->meta.name = strrchr(path, '/');
    out_scene->meta.name = out_scene->meta.name ? out_scene->meta.name + 1 : path;
    out_scene->meta.version = 1;
    out_scene->source_path = path;
    return true;
}

void FPS_Scene_Unload(FPS_Scene *scene) {
    // Nothing to do in the mock implementation.
    (void)scene;
}
