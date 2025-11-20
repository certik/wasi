/*
 * Engine Implementation
 *
 * Deserializes and renders scenes from serialized blob format.
 */

#include "engine.h"
#include "scene_format.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_image/SDL_image.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "base/base_io.h"

// Scene struct (opaque to users)
struct Scene {
    void *blob;              // Pointer to mmap'd or malloc'd blob
    uint64_t blob_size;      // Total size of blob
    bool use_mmap;           // If true, munmap on free; else free()

    SceneHeader *header;     // Pointer to header
    SceneVertex *vertices;   // Pointer to vertices (after fixup)
    uint16_t *indices;       // Pointer to indices
    SceneLight *lights;      // Pointer to lights
    SceneTexture *textures;  // Pointer to textures (after fixup)
    char *strings;           // Pointer to string arena
};

// Engine rendering context
struct Engine {
    SDL_GPUDevice *device;

    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUTransferBuffer *vertex_transfer_buffer;
    SDL_GPUTransferBuffer *index_transfer_buffer;

    SDL_GPUTexture *textures[8];     // One per surface_type_id
    SDL_GPUSampler *samplers[8];     // One per texture

    uint32_t vertex_count;
    uint32_t index_count;
};

// ============================================================================
// Internal Helper Functions
// ============================================================================

static bool validate_header(SceneHeader *header, uint64_t blob_size) {
    // Check magic and version
    if (header->magic != SCENE_MAGIC) {
        SDL_Log("Invalid scene magic: 0x%08x (expected 0x%08x)", header->magic, SCENE_MAGIC);
        return false;
    }
    if (header->version != SCENE_VERSION) {
        SDL_Log("Invalid scene version: %u (expected %u)", header->version, SCENE_VERSION);
        return false;
    }
    if (header->total_size != blob_size) {
        SDL_Log("Scene total_size mismatch: %llu != %llu", header->total_size, blob_size);
        return false;
    }

    // Validate vertex data
    uint64_t vertex_offset = (uint64_t)(uintptr_t)header->vertices;
    if (header->vertex_count > 0) {
        if (vertex_offset + header->vertex_size > blob_size) {
            SDL_Log("Vertex data out of bounds");
            return false;
        }
        if (header->vertex_size != header->vertex_count * sizeof(SceneVertex)) {
            SDL_Log("Vertex size mismatch");
            return false;
        }
    }

    // Validate index data
    uint64_t index_offset = (uint64_t)(uintptr_t)header->indices;
    if (header->index_count > 0) {
        if (index_offset + header->index_size > blob_size) {
            SDL_Log("Index data out of bounds");
            return false;
        }
        if (header->index_size != header->index_count * sizeof(uint16_t)) {
            SDL_Log("Index size mismatch");
            return false;
        }
    }

    // Validate light data
    uint64_t light_offset = (uint64_t)(uintptr_t)header->lights;
    if (header->light_count > 0) {
        if (light_offset + header->light_size > blob_size) {
            SDL_Log("Light data out of bounds");
            return false;
        }
        if (header->light_size != header->light_count * sizeof(SceneLight)) {
            SDL_Log("Light size mismatch");
            return false;
        }
    }

    // Validate texture data
    uint64_t texture_offset = (uint64_t)(uintptr_t)header->textures;
    if (header->texture_count > 0) {
        if (texture_offset + header->texture_size > blob_size) {
            SDL_Log("Texture data out of bounds");
            return false;
        }
        if (header->texture_size != header->texture_count * sizeof(SceneTexture)) {
            SDL_Log("Texture size mismatch");
            return false;
        }
    }

    // Validate string arena
    uint64_t string_offset = (uint64_t)(uintptr_t)header->strings;
    if (header->string_size > 0) {
        if (string_offset + header->string_size > blob_size) {
            SDL_Log("String data out of bounds");
            return false;
        }
    }

    return true;
}

static void fixup_scene_pointers(Scene *scene) {
    SceneHeader *header = scene->header;
    char *base = (char *)scene->blob;

    // Fix up array pointers
    if (header->vertex_count > 0) {
        header->vertices = (SceneVertex *)(base + (uintptr_t)header->vertices);
        scene->vertices = header->vertices;
    } else {
        header->vertices = NULL;
        scene->vertices = NULL;
    }

    if (header->index_count > 0) {
        header->indices = (uint16_t *)(base + (uintptr_t)header->indices);
        scene->indices = header->indices;
    } else {
        header->indices = NULL;
        scene->indices = NULL;
    }

    if (header->light_count > 0) {
        header->lights = (SceneLight *)(base + (uintptr_t)header->lights);
        scene->lights = header->lights;
    } else {
        header->lights = NULL;
        scene->lights = NULL;
    }

    if (header->texture_count > 0) {
        header->textures = (SceneTexture *)(base + (uintptr_t)header->textures);
        scene->textures = header->textures;
    } else {
        header->textures = NULL;
        scene->textures = NULL;
    }

    if (header->string_size > 0) {
        header->strings = base + (uintptr_t)header->strings;
        scene->strings = header->strings;
    } else {
        header->strings = NULL;
        scene->strings = NULL;
    }

    // Fix up texture path_offset -> pointer
    for (uint32_t i = 0; i < header->texture_count; i++) {
        SceneTexture *tex = &scene->textures[i];
        // Validate string offset is within string arena
        if (tex->path_offset >= header->string_size) {
            SDL_Log("Warning: texture %u path_offset %llu out of bounds (string_size=%llu)",
                    i, tex->path_offset, header->string_size);
            tex->path_offset = 0; // Point to empty string or start of arena
        }
        // Convert offset to pointer by storing pointer in path_offset field
        // This is a bit of a hack, but works since we're converting offset->ptr
        tex->path_offset = (uint64_t)(uintptr_t)(scene->strings + tex->path_offset);
    }
}

// ============================================================================
// Scene API
// ============================================================================

Scene* scene_load_from_memory(void *blob, uint64_t blob_size, bool use_mmap) {
    if (!blob) {
        SDL_Log("scene_load_from_memory: blob is NULL");
        return NULL;
    }

    if (blob_size < sizeof(SceneHeader)) {
        SDL_Log("scene_load_from_memory: blob too small (%llu bytes)", blob_size);
        return NULL;
    }

    SceneHeader *header = (SceneHeader *)blob;
    if (!validate_header(header, blob_size)) {
        SDL_Log("scene_load_from_memory: header validation failed");
        return NULL;
    }

    Scene *scene = (Scene *)malloc(sizeof(Scene));
    if (!scene) {
        SDL_Log("scene_load_from_memory: failed to allocate Scene");
        return NULL;
    }

    scene->blob = blob;
    scene->blob_size = blob_size;
    scene->use_mmap = use_mmap;
    scene->header = header;

    fixup_scene_pointers(scene);

    SDL_Log("Loaded scene: %u vertices, %u indices, %u lights, %u textures",
            header->vertex_count, header->index_count, header->light_count, header->texture_count);

    return scene;
}

Scene* scene_load_from_file(const char *path) {
    if (!path) {
        SDL_Log("scene_load_from_file: path is NULL");
        return NULL;
    }

    // Open file
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        SDL_Log("scene_load_from_file: failed to open %s", path);
        return NULL;
    }

    // Get file size
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        SDL_Log("scene_load_from_file: failed to get size of %s", path);
        close(fd);
        return NULL;
    }

    if (size == 0) {
        SDL_Log("scene_load_from_file: file %s is empty", path);
        close(fd);
        return NULL;
    }

    // mmap the file
    void *blob = mmap(NULL, (size_t)size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd); // Can close fd after mmap

    if (blob == MAP_FAILED) {
        SDL_Log("scene_load_from_file: failed to mmap %s", path);
        return NULL;
    }

    SDL_Log("Loaded scene file %s (%lld bytes)", path, (long long)size);

    // Load from memory with use_mmap=true
    Scene *scene = scene_load_from_memory(blob, (uint64_t)size, true);
    if (!scene) {
        munmap(blob, (size_t)size);
        return NULL;
    }

    return scene;
}

const SceneVertex* scene_get_vertices(const Scene *scene, uint32_t *out_count) {
    if (!scene) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) {
        *out_count = scene->header->vertex_count;
    }
    return scene->vertices;
}

const uint16_t* scene_get_indices(const Scene *scene, uint32_t *out_count) {
    if (!scene) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) {
        *out_count = scene->header->index_count;
    }
    return scene->indices;
}

const SceneLight* scene_get_lights(const Scene *scene, uint32_t *out_count) {
    if (!scene) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) {
        *out_count = scene->header->light_count;
    }
    return scene->lights;
}

uint32_t scene_get_texture_count(const Scene *scene) {
    if (!scene) return 0;
    return scene->header->texture_count;
}

const char* scene_get_texture_path(const Scene *scene, uint32_t surface_type_id) {
    if (!scene) return NULL;

    // Find texture with matching surface_type_id
    for (uint32_t i = 0; i < scene->header->texture_count; i++) {
        if (scene->textures[i].surface_type_id == surface_type_id) {
            // path_offset was converted to pointer in fixup_scene_pointers
            return (const char *)(uintptr_t)scene->textures[i].path_offset;
        }
    }

    return NULL; // Not found
}

void scene_free(Scene *scene) {
    if (!scene) return;

    if (scene->use_mmap) {
        munmap(scene->blob, (size_t)scene->blob_size);
    } else {
        free(scene->blob);
    }

    free(scene);
}

// ============================================================================
// Engine API
// ============================================================================

// Map scene surface type IDs to shader binding slots.
// Shader slots: 0=floor, 1=wall, 2=ceiling, 3=sphere, 4=book, 5=chair.
static int map_surface_type_to_slot(uint32_t surface_type_id) {
    switch (surface_type_id) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 4: return 3;
        case 5: return 4;
        case 6: return 5;
        default: return -1; // Unsupported or no dedicated texture slot
    }
}

Engine* engine_create(SDL_GPUDevice *device) {
    if (!device) {
        SDL_Log("engine_create: device is NULL");
        return NULL;
    }

    Engine *engine = (Engine *)malloc(sizeof(Engine));
    if (!engine) {
        SDL_Log("engine_create: failed to allocate Engine");
        return NULL;
    }

    engine->device = device;
    engine->vertex_buffer = NULL;
    engine->index_buffer = NULL;
    engine->vertex_transfer_buffer = NULL;
    engine->index_transfer_buffer = NULL;

    for (int i = 0; i < 8; i++) {
        engine->textures[i] = NULL;
        engine->samplers[i] = NULL;
    }

    engine->vertex_count = 0;
    engine->index_count = 0;

    return engine;
}

bool engine_upload_scene(Engine *engine, const Scene *scene) {
    if (!engine || !scene) {
        SDL_Log("engine_upload_scene: NULL parameter");
        return false;
    }

    uint32_t vertex_count, index_count;
    const SceneVertex *vertices = scene_get_vertices(scene, &vertex_count);
    const uint16_t *indices = scene_get_indices(scene, &index_count);

    if (!vertices || vertex_count == 0) {
        SDL_Log("engine_upload_scene: no vertices");
        return false;
    }
    if (!indices || index_count == 0) {
        SDL_Log("engine_upload_scene: no indices");
        return false;
    }

    SDL_Log("Uploading scene: %u vertices, %u indices", vertex_count, index_count);

    // Create vertex buffer
    SDL_GPUBufferCreateInfo vertex_buffer_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(SceneVertex) * vertex_count,
    };
    engine->vertex_buffer = SDL_CreateGPUBuffer(engine->device, &vertex_buffer_info);
    if (!engine->vertex_buffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return false;
    }

    // Create index buffer
    SDL_GPUBufferCreateInfo index_buffer_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = sizeof(uint16_t) * index_count,
    };
    engine->index_buffer = SDL_CreateGPUBuffer(engine->device, &index_buffer_info);
    if (!engine->index_buffer) {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(engine->device, engine->vertex_buffer);
        engine->vertex_buffer = NULL;
        return false;
    }

    // Create transfer buffers
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = vertex_buffer_info.size,
    };
    engine->vertex_transfer_buffer = SDL_CreateGPUTransferBuffer(engine->device, &transfer_info);

    transfer_info.size = index_buffer_info.size;
    engine->index_transfer_buffer = SDL_CreateGPUTransferBuffer(engine->device, &transfer_info);

    if (!engine->vertex_transfer_buffer || !engine->index_transfer_buffer) {
        SDL_Log("Failed to create transfer buffers: %s", SDL_GetError());
        if (engine->vertex_transfer_buffer) {
            SDL_ReleaseGPUTransferBuffer(engine->device, engine->vertex_transfer_buffer);
            engine->vertex_transfer_buffer = NULL;
        }
        if (engine->index_transfer_buffer) {
            SDL_ReleaseGPUTransferBuffer(engine->device, engine->index_transfer_buffer);
            engine->index_transfer_buffer = NULL;
        }
        SDL_ReleaseGPUBuffer(engine->device, engine->vertex_buffer);
        SDL_ReleaseGPUBuffer(engine->device, engine->index_buffer);
        engine->vertex_buffer = NULL;
        engine->index_buffer = NULL;
        return false;
    }

    // Map and copy vertex data
    void *mapped_vertices = SDL_MapGPUTransferBuffer(engine->device, engine->vertex_transfer_buffer, false);
    if (!mapped_vertices) {
        SDL_Log("Failed to map vertex transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(engine->device, engine->vertex_transfer_buffer);
        SDL_ReleaseGPUTransferBuffer(engine->device, engine->index_transfer_buffer);
        SDL_ReleaseGPUBuffer(engine->device, engine->vertex_buffer);
        SDL_ReleaseGPUBuffer(engine->device, engine->index_buffer);
        engine->vertex_transfer_buffer = NULL;
        engine->index_transfer_buffer = NULL;
        engine->vertex_buffer = NULL;
        engine->index_buffer = NULL;
        return false;
    }
    SDL_memcpy(mapped_vertices, vertices, vertex_buffer_info.size);
    SDL_UnmapGPUTransferBuffer(engine->device, engine->vertex_transfer_buffer);

    // Map and copy index data
    void *mapped_indices = SDL_MapGPUTransferBuffer(engine->device, engine->index_transfer_buffer, false);
    if (!mapped_indices) {
        SDL_Log("Failed to map index transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(engine->device, engine->vertex_transfer_buffer);
        SDL_ReleaseGPUTransferBuffer(engine->device, engine->index_transfer_buffer);
        SDL_ReleaseGPUBuffer(engine->device, engine->vertex_buffer);
        SDL_ReleaseGPUBuffer(engine->device, engine->index_buffer);
        engine->vertex_transfer_buffer = NULL;
        engine->index_transfer_buffer = NULL;
        engine->vertex_buffer = NULL;
        engine->index_buffer = NULL;
        return false;
    }
    SDL_memcpy(mapped_indices, indices, index_buffer_info.size);
    SDL_UnmapGPUTransferBuffer(engine->device, engine->index_transfer_buffer);

    // Upload via copy pass
    SDL_GPUCommandBuffer *upload_cmdbuf = SDL_AcquireGPUCommandBuffer(engine->device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmdbuf);

    SDL_GPUTransferBufferLocation src = {
        .transfer_buffer = engine->vertex_transfer_buffer,
        .offset = 0,
    };
    SDL_GPUBufferRegion dst = {
        .buffer = engine->vertex_buffer,
        .offset = 0,
        .size = vertex_buffer_info.size,
    };
    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);

    src.transfer_buffer = engine->index_transfer_buffer;
    dst.buffer = engine->index_buffer;
    dst.size = index_buffer_info.size;
    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmdbuf);

    // Store counts
    engine->vertex_count = vertex_count;
    engine->index_count = index_count;

    SDL_Log("Scene uploaded successfully");
    return true;
}

bool engine_load_textures(Engine *engine, const Scene *scene) {
    if (!engine || !scene) {
        SDL_Log("engine_load_textures: NULL parameter");
        return false;
    }

    uint32_t texture_count = scene_get_texture_count(scene);
    SDL_Log("Loading %u textures", texture_count);

    if (texture_count == 0) {
        SDL_Log("No textures to load");
        return true;
    }

    if (texture_count > 8) {
        SDL_Log("Warning: scene has %u textures, but engine only supports 8", texture_count);
        texture_count = 8;
    }

    // Load each texture
    for (uint32_t i = 0; i < scene->header->texture_count; i++) {
        uint32_t surface_type_id = scene->textures[i].surface_type_id;
        int binding_slot = map_surface_type_to_slot(surface_type_id);
        if (binding_slot < 0 || binding_slot >= 8) {
            SDL_Log("Warning: texture %u has unsupported surface_type_id %u, skipping", i, surface_type_id);
            continue;
        }

        const char *path = (const char *)(uintptr_t)scene->textures[i].path_offset;
        if (!path || path[0] == '\0') {
            SDL_Log("Warning: texture %u has empty path, skipping", i);
            continue;
        }

        SDL_Log("Loading texture %u (surface_type=%u -> slot %d) from %s", i, surface_type_id, binding_slot, path);

        // Load texture using SDL_image
        SDL_Surface *surface = IMG_Load(path);
        if (!surface) {
            SDL_Log("Failed to load texture %s: %s", path, SDL_GetError());
            continue; // Skip this texture but continue with others
        }

        SDL_Log("Loaded texture: %dx%d, format=0x%08x", surface->w, surface->h, surface->format);

        // Convert to RGBA32 if needed
        if (surface->format != SDL_PIXELFORMAT_RGBA32 && surface->format != SDL_PIXELFORMAT_ABGR32) {
            SDL_Log("Converting texture from format 0x%08x to RGBA32", surface->format);
            SDL_Surface *converted_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            if (!converted_surface) {
                SDL_Log("Failed to convert texture: %s", SDL_GetError());
                continue;
            }
            surface = converted_surface;
        }

        int tex_width = surface->w;
        int tex_height = surface->h;
        uint32_t tex_data_size = (uint32_t)(tex_width * tex_height * 4);

        // Create GPU texture
        SDL_GPUTextureCreateInfo tex_info = {
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .width = (uint32_t)tex_width,
            .height = (uint32_t)tex_height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
        };
        SDL_GPUTexture *texture = SDL_CreateGPUTexture(engine->device, &tex_info);
        if (!texture) {
            SDL_Log("Failed to create GPU texture: %s", SDL_GetError());
            SDL_DestroySurface(surface);
            continue;
        }

        // Create transfer buffer
        SDL_GPUTransferBufferCreateInfo transfer_info = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = tex_data_size,
        };
        SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer(engine->device, &transfer_info);
        if (!transfer_buffer) {
            SDL_Log("Failed to create texture transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTexture(engine->device, texture);
            SDL_DestroySurface(surface);
            continue;
        }

        // Map and copy texture data
        unsigned char *mapped = (unsigned char *)SDL_MapGPUTransferBuffer(engine->device, transfer_buffer, false);
        if (!mapped) {
            SDL_Log("Failed to map texture transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(engine->device, transfer_buffer);
            SDL_ReleaseGPUTexture(engine->device, texture);
            SDL_DestroySurface(surface);
            continue;
        }
        SDL_memcpy(mapped, surface->pixels, tex_data_size);
        SDL_UnmapGPUTransferBuffer(engine->device, transfer_buffer);

        SDL_DestroySurface(surface);

        // Upload texture
        SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(engine->device);
        SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmdbuf);

        SDL_GPUTextureTransferInfo transfer_src = {
            .transfer_buffer = transfer_buffer,
            .offset = 0,
            .pixels_per_row = (uint32_t)tex_width,
            .rows_per_layer = (uint32_t)tex_height,
        };

        SDL_GPUTextureRegion region = {
            .texture = texture,
            .mip_level = 0,
            .layer = 0,
            .x = 0,
            .y = 0,
            .z = 0,
            .w = (uint32_t)tex_width,
            .h = (uint32_t)tex_height,
            .d = 1,
        };

        SDL_UploadToGPUTexture(copy_pass, &transfer_src, &region, false);
        SDL_EndGPUCopyPass(copy_pass);
        SDL_SubmitGPUCommandBuffer(cmdbuf);
        SDL_ReleaseGPUTransferBuffer(engine->device, transfer_buffer);

        // Create sampler
        SDL_GPUSamplerCreateInfo sampler_info = {
            .min_filter = SDL_GPU_FILTER_LINEAR,
            .mag_filter = SDL_GPU_FILTER_LINEAR,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        };
        SDL_GPUSampler *sampler = SDL_CreateGPUSampler(engine->device, &sampler_info);
        if (!sampler) {
            SDL_Log("Failed to create sampler: %s", SDL_GetError());
            SDL_ReleaseGPUTexture(engine->device, texture);
            continue;
        }

        // Store in engine
        engine->textures[binding_slot] = texture;
        engine->samplers[binding_slot] = sampler;

        SDL_Log("Texture loaded successfully into slot %d", binding_slot);
    }

    SDL_Log("Texture loading complete");
    return true;
}

bool engine_render(Engine *engine, SDL_GPUCommandBuffer *cmdbuf, SDL_GPURenderPass *render_pass,
                  const void *uniforms, uint32_t uniform_size) {
    if (!engine || !cmdbuf || !render_pass) {
        SDL_Log("engine_render: NULL parameter");
        return false;
    }

    if (!engine->vertex_buffer || !engine->index_buffer) {
        SDL_Log("engine_render: no scene uploaded");
        return false;
    }

    // Bind vertex buffer
    SDL_GPUBufferBinding vertex_binding = {
        .buffer = engine->vertex_buffer,
        .offset = 0,
    };
    SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);

    // Bind index buffer
    SDL_GPUBufferBinding index_binding = {
        .buffer = engine->index_buffer,
        .offset = 0,
    };
    SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    // Bind textures and samplers (slots 0-7)
    // Bind textures 0-5 plus shared sampler at slot 6 (matches WGSL layout).
    // Require primary bindings to exist
    for (int i = 0; i < 6; i++) {
        if (!engine->textures[i] || !engine->samplers[i]) {
            SDL_Log("engine_render: missing texture or sampler at slot %d", i);
            return false;
        }
    }

    SDL_GPUTextureSamplerBinding bindings[7] = {
        {engine->textures[0], engine->samplers[0]},
        {engine->textures[1], engine->samplers[1]},
        {engine->textures[2], engine->samplers[2]},
        {engine->textures[3], engine->samplers[3]},
        {engine->textures[4], engine->samplers[4]},
        {engine->textures[5], engine->samplers[5]},
        {engine->textures[0], engine->samplers[0]}, // sampler-only binding uses slot 0 sampler
    };
    SDL_BindGPUFragmentSamplers(render_pass, 0, bindings, 7);

    // Push uniforms if provided
    if (uniforms && uniform_size > 0) {
        SDL_PushGPUVertexUniformData(cmdbuf, 0, uniforms, uniform_size);
        SDL_PushGPUFragmentUniformData(cmdbuf, 0, uniforms, uniform_size);
    }

    // Draw indexed
    SDL_DrawGPUIndexedPrimitives(render_pass, engine->index_count, 1, 0, 0, 0);

    return true;
}

void engine_free(Engine *engine) {
    if (!engine) return;

    // Release GPU buffers
    if (engine->vertex_buffer) {
        SDL_ReleaseGPUBuffer(engine->device, engine->vertex_buffer);
    }
    if (engine->index_buffer) {
        SDL_ReleaseGPUBuffer(engine->device, engine->index_buffer);
    }
    if (engine->vertex_transfer_buffer) {
        SDL_ReleaseGPUTransferBuffer(engine->device, engine->vertex_transfer_buffer);
    }
    if (engine->index_transfer_buffer) {
        SDL_ReleaseGPUTransferBuffer(engine->device, engine->index_transfer_buffer);
    }

    // Release textures and samplers
    for (int i = 0; i < 8; i++) {
        if (engine->textures[i]) {
            SDL_ReleaseGPUTexture(engine->device, engine->textures[i]);
        }
        if (engine->samplers[i]) {
            SDL_ReleaseGPUSampler(engine->device, engine->samplers[i]);
        }
    }

    free(engine);
}
