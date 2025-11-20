/*
 * Engine API
 *
 * Deserializes and renders scenes. Accepts serialized scene blobs
 * and provides GPU rendering interface.
 */

#ifndef ENGINE_H
#define ENGINE_H

#include "scene_format.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

// Opaque scene handle (deserialized scene data)
typedef struct Scene Scene;

// Load scene from memory blob (takes ownership, will munmap/free on scene_free)
// blob_size is the total size of the blob in bytes
// use_mmap: if true, blob was mmap'd and should be munmap'd; if false, use free()
Scene* scene_load_from_memory(void *blob, uint64_t blob_size, bool use_mmap);

// Load scene from file (uses mmap for zero-copy)
Scene* scene_load_from_file(const char *path);

// Get scene data for rendering
const SceneVertex* scene_get_vertices(const Scene *scene, uint32_t *out_count);
const uint16_t* scene_get_indices(const Scene *scene, uint32_t *out_count);
const SceneLight* scene_get_lights(const Scene *scene, uint32_t *out_count);

// Get texture info (returns number of textures, fills arrays if non-NULL)
uint32_t scene_get_texture_count(const Scene *scene);
const char* scene_get_texture_path(const Scene *scene, uint32_t surface_type_id);

// Free scene (munmap or free blob, free Scene struct)
void scene_free(Scene *scene);

// Engine rendering context
typedef struct Engine Engine;

// Create engine with SDL GPU device
Engine* engine_create(SDL_GPUDevice *device);

// Upload scene data to GPU buffers
bool engine_upload_scene(Engine *engine, const Scene *scene);

// Load textures from scene texture paths
bool engine_load_textures(Engine *engine, const Scene *scene);

// Render the scene with given uniforms
// uniforms should be a pointer to SceneUniforms (from game.c)
bool engine_render(Engine *engine, SDL_GPUCommandBuffer *cmdbuf, SDL_GPURenderPass *render_pass,
                  const void *uniforms, uint32_t uniform_size);

// Free engine resources
void engine_free(Engine *engine);

#endif // ENGINE_H
