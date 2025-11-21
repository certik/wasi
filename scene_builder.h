/*
 * Scene Builder API
 *
 * Constructs 3D scenes and serializes them to binary format.
 * Can be used as a library (in-game scene building) or standalone tool.
 */

#ifndef SCENE_BUILDER_H
#define SCENE_BUILDER_H

#include "scene_format.h"
#include <base/arena.h>
#include <stdint.h>
#include <stdbool.h>

// Configuration for scene generation
typedef struct {
    int *map_data;           // Grid map (0=empty, 1=wall, 2=window_ns, 3=window_ew, 9=light)
    int map_width;
    int map_height;
    float spawn_x;           // Initial camera position
    float spawn_z;

    // Asset paths (can be NULL to skip)
    const char *sphere_obj_path;
    const char *book_obj_path;
    const char *chair_obj_path;
    const char *ceiling_light_gltf_path;

    // Texture paths (for string arena)
    const char *floor_texture_path;
    const char *wall_texture_path;
    const char *ceiling_texture_path;
    const char *sphere_texture_path;
    const char *book_texture_path;
    const char *chair_texture_path;
    const char *window_texture_path;
    const char *ceiling_light_texture_path;
} SceneConfig;

// Opaque scene builder context
typedef struct SceneBuilder SceneBuilder;

// Create a new scene builder with an arena allocator
SceneBuilder* scene_builder_create(Arena *arena);

// Generate scene geometry from configuration
bool scene_builder_generate(SceneBuilder *builder, const SceneConfig *config);

// Serialize the scene to a binary blob (two-phase: measure, then write)
// Returns allocated blob size, blob pointer filled in *out_blob
uint64_t scene_builder_serialize(SceneBuilder *builder, uint8_t **out_blob);

// Save serialized scene to file
bool scene_builder_save(SceneBuilder *builder, const char *path);

// Free scene builder (if arena was internally allocated)
void scene_builder_free(SceneBuilder *builder);

#endif // SCENE_BUILDER_H
