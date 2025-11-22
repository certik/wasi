#ifndef FPS_ASSETS_H
#define FPS_ASSETS_H

#include <stdbool.h>
#include <stdint.h>

// Minimal scene metadata to show the asset pipeline contract.
typedef struct {
    const char *name;
    uint32_t version;
} FPS_SceneMetadata;

// Serialized scene handle (mocked; a real impl would mirror scene_format.h).
typedef struct FPS_Scene {
    FPS_SceneMetadata meta;
    const char *source_path;
} FPS_Scene;

// Load/unload a scene. In this mock, we synthesize metadata and keep the path.
bool FPS_Scene_LoadFromFile(const char *path, FPS_Scene *out_scene);
void FPS_Scene_Unload(FPS_Scene *scene);

#endif // FPS_ASSETS_H
