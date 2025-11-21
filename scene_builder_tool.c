/*
 * Scene Builder Tool - Standalone CLI
 *
 * Generates and serializes 3D scenes to binary .scn files.
 *
 * Usage:
 *   ./scene_builder_tool [output.scn]
 *
 * Currently uses hardcoded map from game.c, but can be extended
 * to read configuration from file.
 */

#define PLATFORM_SKIP_ENTRY
#include <platform/platform.h>
#include <base/arena.h>
#include <base/base_io.h>
#include <stdlib.h>
#include <stdio.h>

#include "scene_builder.h"
#include "scene_format.h"

// Default map (from game.c)
#define MAP_WIDTH 10
#define MAP_HEIGHT 10

static int default_map[MAP_HEIGHT * MAP_WIDTH] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 9, 0, 0, 0, 0, 3,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 9, 0, 0, 2,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

// Default asset paths
#define SPHERE_OBJ_PATH "assets/equirectangular_sphere.obj"
#define BOOK_OBJ_PATH "assets/book.obj"
#define CHAIR_OBJ_PATH "assets/chair.obj"
#define CEILING_LIGHT_GLTF_PATH "assets/ceiling_light.glb"

#define FLOOR_TEXTURE_PATH "assets/WoodFloor007_1K-JPG_Color.jpg"
#define WALL_TEXTURE_PATH "assets/Concrete046_1K-JPG_Color.jpg"
#define CEILING_TEXTURE_PATH "assets/OfficeCeiling001_1K-JPG_Color.jpg"
#define SPHERE_TEXTURE_PATH "assets/Land_ocean_ice_2048.jpg"
#define BOOK_TEXTURE_PATH "assets/checker_board_4k.png"
#define CHAIR_TEXTURE_PATH "assets/chair_02_diff_1k.jpg"
#define WINDOW_TEXTURE_PATH BOOK_TEXTURE_PATH
#define CEILING_LIGHT_TEXTURE_PATH BOOK_TEXTURE_PATH

int main(int argc, char *argv[]) {
    // Initialize platform
    platform_init(argc, argv);

    printf("Scene Builder Tool v1.0\n");
    printf("=======================\n\n");

    // Parse arguments
    const char *output_path = "scene.scn";
    if (argc > 1) {
        output_path = argv[1];
    }

    printf("Output file: %s\n", output_path);
    printf("Map size: %dx%d\n", MAP_WIDTH, MAP_HEIGHT);
    printf("Generating scene...\n\n");

    // Create arena for scene builder
    Arena *arena = arena_new(8 * 1024 * 1024);  // 8MB arena
    if (!arena) {
        fprintf(stderr, "ERROR: Failed to create arena\n");
        return 1;
    }

    // Create scene builder
    SceneBuilder *builder = scene_builder_create(arena);
    if (!builder) {
        fprintf(stderr, "ERROR: Failed to create scene builder\n");
        arena_free(arena);
        return 1;
    }

    // Configure scene
    SceneConfig config = {0};
    config.map_data = default_map;
    config.map_width = MAP_WIDTH;
    config.map_height = MAP_HEIGHT;
    config.spawn_x = 5.5f;
    config.spawn_z = 5.5f;

    // Asset paths
    config.sphere_obj_path = SPHERE_OBJ_PATH;
    config.book_obj_path = BOOK_OBJ_PATH;
    config.chair_obj_path = CHAIR_OBJ_PATH;
    config.ceiling_light_gltf_path = CEILING_LIGHT_GLTF_PATH;

    // Texture paths
    config.floor_texture_path = FLOOR_TEXTURE_PATH;
    config.wall_texture_path = WALL_TEXTURE_PATH;
    config.ceiling_texture_path = CEILING_TEXTURE_PATH;
    config.sphere_texture_path = SPHERE_TEXTURE_PATH;
    config.book_texture_path = BOOK_TEXTURE_PATH;
    config.chair_texture_path = CHAIR_TEXTURE_PATH;
    config.window_texture_path = WINDOW_TEXTURE_PATH;
    config.ceiling_light_texture_path = CEILING_LIGHT_TEXTURE_PATH;

    // Generate scene
    printf("Loading assets and generating geometry...\n");
    if (!scene_builder_generate(builder, &config)) {
        fprintf(stderr, "ERROR: Failed to generate scene\n");
        scene_builder_free(builder);
        return 1;
    }

    printf("Scene generation complete!\n\n");

    // Save to file
    printf("Serializing and saving to %s...\n", output_path);
    if (!scene_builder_save(builder, output_path)) {
        fprintf(stderr, "ERROR: Failed to save scene to %s\n", output_path);
        scene_builder_free(builder);
        return 1;
    }

    printf("\nSuccess! Scene saved to %s\n", output_path);

    // Cleanup
    scene_builder_free(builder);

    return 0;
}
