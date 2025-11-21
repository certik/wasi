/*
 * Engine Implementation
 *
 * Deserializes and renders scenes from serialized blob format.
 */

#include "engine.h"
#include "scene_format.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "base/base_io.h"

#define RC_CASCADE_COUNT 3
#define RC_RADIANCE_RES 32
#define RC_RAY_MAX_STEPS 32
#define RC_RAY_MIN_STEP 0.05f
#define RC_RAY_HIT_EPS 0.02f
#define RC_BOUNCE_FACTOR 0.6f
#define RC_AMBIENT_BASE 0.03f

typedef struct {
    SDL_GPUTexture *texture;
    float *cpu_data;
    float spacing;
    float origin[3];
    uint32_t dim;
} RadianceCascade;

// Scene struct (opaque to users)
struct Scene {
    void *blob;              // Pointer to mmap'd or malloc'd blob
    uint64_t blob_size;      // Total size of blob
    bool use_mmap;           // If true, release via platform_file_unmap
    uint64_t mmap_handle;    // Opaque handle for platform_file_unmap (0 if not mapped)

    SceneHeader *header;     // Pointer to header
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

    // Baked SDF
    SceneSDFInfo sdf_info;
    const float *sdf_data;

    // Radiance cascade data
    RadianceCascade cascades[RC_CASCADE_COUNT];
    float cascade_extent;
    bool cascades_ready;
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

    // Validate SDF data (optional)
    uint64_t sdf_offset = (uint64_t)(uintptr_t)header->sdf;
    if (header->sdf_size > 0) {
        if (sdf_offset + header->sdf_size > blob_size) {
            SDL_Log("SDF data out of bounds");
            return false;
        }
        uint64_t expected_sdf = (uint64_t)header->sdf_info.dim[0] *
                                (uint64_t)header->sdf_info.dim[1] *
                                (uint64_t)header->sdf_info.dim[2] *
                                sizeof(float);
        if (expected_sdf != header->sdf_size) {
            SDL_Log("SDF size mismatch: expected %llu, got %llu",
                    (unsigned long long)expected_sdf,
                    (unsigned long long)header->sdf_size);
            return false;
        }
    }

    return true;
}

static void release_scene_blob(void *blob, bool use_mmap, uint64_t mmap_handle) {
    if (!blob) return;
    if (use_mmap) {
        platform_file_unmap(mmap_handle);
    } else {
        free(blob);
    }
}

static void fixup_scene_pointers(Scene *scene) {
    SceneHeader *header = scene->header;
    char *base = (char *)scene->blob;

    // Fix up array pointers
    if (header->vertex_count > 0) {
        header->vertices = (SceneVertex *)(base + (uintptr_t)header->vertices);
    } else {
        header->vertices = NULL;
    }

    if (header->index_count > 0) {
        header->indices = (uint16_t *)(base + (uintptr_t)header->indices);
    } else {
        header->indices = NULL;
    }

    if (header->light_count > 0) {
        header->lights = (SceneLight *)(base + (uintptr_t)header->lights);
    } else {
        header->lights = NULL;
    }

    if (header->texture_count > 0) {
        header->textures = (SceneTexture *)(base + (uintptr_t)header->textures);
    } else {
        header->textures = NULL;
    }

    if (header->string_size > 0) {
        header->strings = base + (uintptr_t)header->strings;
    } else {
        header->strings = NULL;
    }

    if (header->sdf_size > 0) {
        header->sdf = (float *)(base + (uintptr_t)header->sdf);
    } else {
        header->sdf = NULL;
    }

    // Fix up texture path_offset -> pointer
    for (uint32_t i = 0; i < header->texture_count; i++) {
        SceneTexture *tex = &header->textures[i];
        // Validate string offset is within string arena
        if (tex->path_offset >= header->string_size) {
            SDL_Log("Warning: texture %u path_offset %llu out of bounds (string_size=%llu)",
                    i, tex->path_offset, header->string_size);
            tex->path_offset = 0; // Point to empty string or start of arena
        }
        // Convert offset to pointer by storing pointer in path_offset field
        // This is a bit of a hack, but works since we're converting offset->ptr
            tex->path_offset = (uint64_t)(uintptr_t)(header->strings + tex->path_offset);
    }
}

// ========================================================================
// SDF sampling helpers
// ========================================================================

static inline size_t sdf_index(const SceneSDFInfo *info, uint32_t x, uint32_t y, uint32_t z) {
    return ((size_t)z * info->dim[1] + (size_t)y) * info->dim[0] + (size_t)x;
}

static float sample_sdf_trilinear(const SceneSDFInfo *info, const float *sdf, const float p[3]) {
    if (!info || !sdf || info->voxel_size <= 0.0f) {
        return 0.0f;
    }

    float gx = (p[0] - info->origin[0]) / info->voxel_size - 0.5f;
    float gy = (p[1] - info->origin[1]) / info->voxel_size - 0.5f;
    float gz = (p[2] - info->origin[2]) / info->voxel_size - 0.5f;

    if (gx < 0.0f || gy < 0.0f || gz < 0.0f ||
        gx > (float)(info->dim[0] - 1) || gy > (float)(info->dim[1] - 1) || gz > (float)(info->dim[2] - 1)) {
        return info->max_distance;
    }

    uint32_t x0 = (uint32_t)floorf(gx);
    uint32_t y0 = (uint32_t)floorf(gy);
    uint32_t z0 = (uint32_t)floorf(gz);
    uint32_t x1 = (x0 + 1 < info->dim[0]) ? x0 + 1 : x0;
    uint32_t y1 = (y0 + 1 < info->dim[1]) ? y0 + 1 : y0;
    uint32_t z1 = (z0 + 1 < info->dim[2]) ? z0 + 1 : z0;

    float tx = gx - (float)x0;
    float ty = gy - (float)y0;
    float tz = gz - (float)z0;

    float c000 = sdf[sdf_index(info, x0, y0, z0)];
    float c100 = sdf[sdf_index(info, x1, y0, z0)];
    float c010 = sdf[sdf_index(info, x0, y1, z0)];
    float c110 = sdf[sdf_index(info, x1, y1, z0)];
    float c001 = sdf[sdf_index(info, x0, y0, z1)];
    float c101 = sdf[sdf_index(info, x1, y0, z1)];
    float c011 = sdf[sdf_index(info, x0, y1, z1)];
    float c111 = sdf[sdf_index(info, x1, y1, z1)];

    float c00 = c000 * (1.0f - tx) + c100 * tx;
    float c10 = c010 * (1.0f - tx) + c110 * tx;
    float c01 = c001 * (1.0f - tx) + c101 * tx;
    float c11 = c011 * (1.0f - tx) + c111 * tx;

    float c0 = c00 * (1.0f - ty) + c10 * ty;
    float c1 = c01 * (1.0f - ty) + c11 * ty;

    return c0 * (1.0f - tz) + c1 * tz;
}

static bool sdf_ray_march_hit(const SceneSDFInfo *info, const float *sdf, const float start[3],
                              const float dir[3], float max_distance) {
    if (!info || !sdf) {
        return false;
    }

    float traveled = 0.0f;
    for (int i = 0; i < RC_RAY_MAX_STEPS && traveled < max_distance; i++) {
        float p[3] = {
            start[0] + dir[0] * traveled,
            start[1] + dir[1] * traveled,
            start[2] + dir[2] * traveled,
        };
        float d = sample_sdf_trilinear(info, sdf, p);
        if (d < RC_RAY_HIT_EPS) {
            return true;
        }
        float step = d;
        if (step < RC_RAY_MIN_STEP) {
            step = RC_RAY_MIN_STEP;
        }
        traveled += step;
    }
    return false;
}

static bool sample_radiance_from_cascade(const RadianceCascade *cascade, const float pos[3], float out_rgb[3]) {
    if (!cascade || !cascade->cpu_data || cascade->spacing <= 0.0f) {
        return false;
    }
    float gx = (pos[0] - cascade->origin[0]) / cascade->spacing;
    float gy = (pos[1] - cascade->origin[1]) / cascade->spacing;
    float gz = (pos[2] - cascade->origin[2]) / cascade->spacing;
    if (gx < 0.0f || gy < 0.0f || gz < 0.0f ||
        gx > (float)(cascade->dim - 1) || gy > (float)(cascade->dim - 1) || gz > (float)(cascade->dim - 1)) {
        return false;
    }

    uint32_t x0 = (uint32_t)floorf(gx);
    uint32_t y0 = (uint32_t)floorf(gy);
    uint32_t z0 = (uint32_t)floorf(gz);
    uint32_t x1 = (x0 + 1 < cascade->dim) ? x0 + 1 : x0;
    uint32_t y1 = (y0 + 1 < cascade->dim) ? y0 + 1 : y0;
    uint32_t z1 = (z0 + 1 < cascade->dim) ? z0 + 1 : z0;

    float tx = gx - (float)x0;
    float ty = gy - (float)y0;
    float tz = gz - (float)z0;

    size_t idx000 = ((size_t)z0 * (size_t)cascade->dim + (size_t)y0) * (size_t)cascade->dim * 4 + (size_t)x0 * 4;
    size_t idx100 = ((size_t)z0 * (size_t)cascade->dim + (size_t)y0) * (size_t)cascade->dim * 4 + (size_t)x1 * 4;
    size_t idx010 = ((size_t)z0 * (size_t)cascade->dim + (size_t)y1) * (size_t)cascade->dim * 4 + (size_t)x0 * 4;
    size_t idx110 = ((size_t)z0 * (size_t)cascade->dim + (size_t)y1) * (size_t)cascade->dim * 4 + (size_t)x1 * 4;
    size_t idx001 = ((size_t)z1 * (size_t)cascade->dim + (size_t)y0) * (size_t)cascade->dim * 4 + (size_t)x0 * 4;
    size_t idx101 = ((size_t)z1 * (size_t)cascade->dim + (size_t)y0) * (size_t)cascade->dim * 4 + (size_t)x1 * 4;
    size_t idx011 = ((size_t)z1 * (size_t)cascade->dim + (size_t)y1) * (size_t)cascade->dim * 4 + (size_t)x0 * 4;
    size_t idx111 = ((size_t)z1 * (size_t)cascade->dim + (size_t)y1) * (size_t)cascade->dim * 4 + (size_t)x1 * 4;

    float c000_r = cascade->cpu_data[idx000 + 0];
    float c000_g = cascade->cpu_data[idx000 + 1];
    float c000_b = cascade->cpu_data[idx000 + 2];
    float c100_r = cascade->cpu_data[idx100 + 0];
    float c100_g = cascade->cpu_data[idx100 + 1];
    float c100_b = cascade->cpu_data[idx100 + 2];
    float c010_r = cascade->cpu_data[idx010 + 0];
    float c010_g = cascade->cpu_data[idx010 + 1];
    float c010_b = cascade->cpu_data[idx010 + 2];
    float c110_r = cascade->cpu_data[idx110 + 0];
    float c110_g = cascade->cpu_data[idx110 + 1];
    float c110_b = cascade->cpu_data[idx110 + 2];
    float c001_r = cascade->cpu_data[idx001 + 0];
    float c001_g = cascade->cpu_data[idx001 + 1];
    float c001_b = cascade->cpu_data[idx001 + 2];
    float c101_r = cascade->cpu_data[idx101 + 0];
    float c101_g = cascade->cpu_data[idx101 + 1];
    float c101_b = cascade->cpu_data[idx101 + 2];
    float c011_r = cascade->cpu_data[idx011 + 0];
    float c011_g = cascade->cpu_data[idx011 + 1];
    float c011_b = cascade->cpu_data[idx011 + 2];
    float c111_r = cascade->cpu_data[idx111 + 0];
    float c111_g = cascade->cpu_data[idx111 + 1];
    float c111_b = cascade->cpu_data[idx111 + 2];

    float c00_r = c000_r * (1.0f - tx) + c100_r * tx;
    float c00_g = c000_g * (1.0f - tx) + c100_g * tx;
    float c00_b = c000_b * (1.0f - tx) + c100_b * tx;
    float c10_r = c010_r * (1.0f - tx) + c110_r * tx;
    float c10_g = c010_g * (1.0f - tx) + c110_g * tx;
    float c10_b = c010_b * (1.0f - tx) + c110_b * tx;
    float c01_r = c001_r * (1.0f - tx) + c101_r * tx;
    float c01_g = c001_g * (1.0f - tx) + c101_g * tx;
    float c01_b = c001_b * (1.0f - tx) + c101_b * tx;
    float c11_r = c011_r * (1.0f - tx) + c111_r * tx;
    float c11_g = c011_g * (1.0f - tx) + c111_g * tx;
    float c11_b = c011_b * (1.0f - tx) + c111_b * tx;

    float c0_r = c00_r * (1.0f - ty) + c10_r * ty;
    float c0_g = c00_g * (1.0f - ty) + c10_g * ty;
    float c0_b = c00_b * (1.0f - ty) + c10_b * ty;
    float c1_r = c01_r * (1.0f - ty) + c11_r * ty;
    float c1_g = c01_g * (1.0f - ty) + c11_g * ty;
    float c1_b = c01_b * (1.0f - ty) + c11_b * ty;

    out_rgb[0] = c0_r * (1.0f - tz) + c1_r * tz;
    out_rgb[1] = c0_g * (1.0f - tz) + c1_g * tz;
    out_rgb[2] = c0_b * (1.0f - tz) + c1_b * tz;
    return true;
}

static void compute_radiance_for_cascade(const SceneHeader *header, const SceneSDFInfo *sdf_info,
                                         const float *sdf_data, RadianceCascade *cascade,
                                         const RadianceCascade *coarse) {
    if (!header || !cascade || !sdf_info || !sdf_data) {
        return;
    }
    uint32_t dim = cascade->dim;
    float ambient = RC_AMBIENT_BASE;
    for (uint32_t z = 0; z < dim; z++) {
        for (uint32_t y = 0; y < dim; y++) {
            for (uint32_t x = 0; x < dim; x++) {
                float world_pos[3] = {
                    cascade->origin[0] + cascade->spacing * (float)x,
                    cascade->origin[1] + cascade->spacing * (float)y,
                    cascade->origin[2] + cascade->spacing * (float)z,
                };
                float radiance[3] = {ambient, ambient, ambient};
                float occluded = 0.0f;

                for (uint32_t i = 0; i < header->light_count; i++) {
                    const SceneLight *light = &header->lights[i];
                    float to_light[3] = {
                        light->position[0] - world_pos[0],
                        light->position[1] - world_pos[1],
                        light->position[2] - world_pos[2],
                    };
                    float dist2 = to_light[0] * to_light[0] + to_light[1] * to_light[1] + to_light[2] * to_light[2];
                    if (dist2 < 1e-6f) {
                        continue;
                    }
                    float dist = SDL_sqrtf(dist2);
                    float inv = 1.0f / dist;
                    float dir[3] = {to_light[0] * inv, to_light[1] * inv, to_light[2] * inv};

                    bool blocked = sdf_ray_march_hit(sdf_info, sdf_data, world_pos, dir, dist);
                    if (!blocked) {
                        float attenuation = 1.0f / (1.0f + 0.09f * dist + 0.032f * dist * dist);
                        radiance[0] += light->color[0] * attenuation;
                        radiance[1] += light->color[1] * attenuation;
                        radiance[2] += light->color[2] * attenuation;
                    } else {
                        occluded += 1.0f;
                    }
                }

                if (coarse) {
                    float bounce[3];
                    if (sample_radiance_from_cascade(coarse, world_pos, bounce)) {
                        radiance[0] += bounce[0] * RC_BOUNCE_FACTOR;
                        radiance[1] += bounce[1] * RC_BOUNCE_FACTOR;
                        radiance[2] += bounce[2] * RC_BOUNCE_FACTOR;
                    }
                }

                float visibility = 1.0f;
                if (header->light_count > 0) {
                    visibility = 1.0f - (occluded / (float)header->light_count);
                }

                size_t idx = ((size_t)z * (size_t)dim + (size_t)y) * (size_t)dim * 4 + (size_t)x * 4;
                cascade->cpu_data[idx + 0] = radiance[0];
                cascade->cpu_data[idx + 1] = radiance[1];
                cascade->cpu_data[idx + 2] = radiance[2];
                cascade->cpu_data[idx + 3] = visibility;
            }
        }
    }
}

static bool upload_radiance_texture(Engine *engine, RadianceCascade *cascade) {
    if (!engine || !cascade || !cascade->cpu_data) {
        return false;
    }

    if (cascade->texture) {
        SDL_ReleaseGPUTexture(engine->device, cascade->texture);
        cascade->texture = NULL;
    }

    uint32_t dim = cascade->dim;
    size_t byte_size = (size_t)dim * (size_t)dim * (size_t)dim * 4 * sizeof(float);

    SDL_GPUTextureCreateInfo tex_info = {
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
        .width = dim,
        .height = dim * dim, // flatten z into rows
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };

    cascade->texture = SDL_CreateGPUTexture(engine->device, &tex_info);
    if (!cascade->texture) {
        SDL_Log("Failed to create radiance cascade texture: %s", SDL_GetError());
        return false;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = byte_size,
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(engine->device, &transfer_info);
    if (!transfer) {
        SDL_Log("Failed to allocate transfer buffer for radiance cascade: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(engine->device, cascade->texture);
        cascade->texture = NULL;
        return false;
    }

    void *mapped = SDL_MapGPUTransferBuffer(engine->device, transfer, false);
    if (!mapped) {
        SDL_Log("Failed to map transfer buffer for radiance cascade: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(engine->device, transfer);
        SDL_ReleaseGPUTexture(engine->device, cascade->texture);
        cascade->texture = NULL;
        return false;
    }
    SDL_memcpy(mapped, cascade->cpu_data, byte_size);
    SDL_UnmapGPUTransferBuffer(engine->device, transfer);

    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(engine->device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmdbuf);
    SDL_GPUTextureTransferInfo transfer_src = {
        .transfer_buffer = transfer,
        .offset = 0,
        .pixels_per_row = dim,
        .rows_per_layer = dim * dim,
    };
    SDL_GPUTextureRegion region = {
        .texture = cascade->texture,
        .mip_level = 0,
        .layer = 0,
        .x = 0,
        .y = 0,
        .z = 0,
        .w = dim,
        .h = dim * dim,
        .d = 1,
    };
    SDL_UploadToGPUTexture(copy_pass, &transfer_src, &region, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmdbuf);
    SDL_ReleaseGPUTransferBuffer(engine->device, transfer);
    return true;
}

static bool build_radiance_cascades(Engine *engine, const SceneHeader *header) {
    if (!engine || !header || !engine->sdf_data ||
        engine->sdf_info.dim[0] == 0 || engine->sdf_info.dim[1] == 0 || engine->sdf_info.dim[2] == 0) {
        SDL_Log("No SDF available, skipping radiance cascades");
        return false;
    }

    float center[3] = {
        (header->bounds.min[0] + header->bounds.max[0]) * 0.5f,
        (header->bounds.min[1] + header->bounds.max[1]) * 0.5f,
        (header->bounds.min[2] + header->bounds.max[2]) * 0.5f,
    };

    float extent_x = header->bounds.max[0] - header->bounds.min[0];
    float extent_y = header->bounds.max[1] - header->bounds.min[1];
    float extent_z = header->bounds.max[2] - header->bounds.min[2];
    float max_extent = extent_x;
    if (extent_y > max_extent) max_extent = extent_y;
    if (extent_z > max_extent) max_extent = extent_z;
    engine->cascade_extent = max_extent;

    float base_spacing = max_extent / (float)(RC_RADIANCE_RES - 1);

    for (int i = 0; i < RC_CASCADE_COUNT; i++) {
        RadianceCascade *c = &engine->cascades[i];
        c->dim = RC_RADIANCE_RES;
        c->spacing = base_spacing * powf(2.0f, (float)i);
        float span = c->spacing * (float)(c->dim - 1);
        c->origin[0] = center[0] - span * 0.5f;
        c->origin[1] = center[1] - span * 0.5f;
        c->origin[2] = center[2] - span * 0.5f;

        size_t voxel_count = (size_t)c->dim * (size_t)c->dim * (size_t)c->dim * 4;
        if (!c->cpu_data) {
            c->cpu_data = (float *)SDL_malloc(voxel_count * sizeof(float));
        }
        if (!c->cpu_data) {
            SDL_Log("Failed to allocate CPU radiance buffer for cascade %d", i);
            return false;
        }
        SDL_memset(c->cpu_data, 0, voxel_count * sizeof(float));

        const RadianceCascade *coarse = (i > 0) ? &engine->cascades[i - 1] : NULL;
        compute_radiance_for_cascade(header, &engine->sdf_info, engine->sdf_data, c, coarse);

        if (!upload_radiance_texture(engine, c)) {
            return false;
        }
    }

    engine->cascades_ready = true;
    SDL_Log("Radiance cascades built");
    return true;
}

// ============================================================================
// Scene API
// ============================================================================

Scene* scene_load_from_memory(void *blob, uint64_t blob_size, bool use_mmap, uint64_t mmap_handle) {
    if (!blob) {
        SDL_Log("scene_load_from_memory: blob is NULL");
        return NULL;
    }

    if (blob_size < sizeof(SceneHeader)) {
        SDL_Log("scene_load_from_memory: blob too small (%llu bytes)", blob_size);
        release_scene_blob(blob, use_mmap, mmap_handle);
        return NULL;
    }

    SceneHeader *header = (SceneHeader *)blob;
    if (!validate_header(header, blob_size)) {
        SDL_Log("scene_load_from_memory: header validation failed");
        release_scene_blob(blob, use_mmap, mmap_handle);
        return NULL;
    }

    Scene *scene = (Scene *)malloc(sizeof(Scene));
    if (!scene) {
        SDL_Log("scene_load_from_memory: failed to allocate Scene");
        release_scene_blob(blob, use_mmap, mmap_handle);
        return NULL;
    }

    scene->blob = blob;
    scene->blob_size = blob_size;
    scene->use_mmap = use_mmap;
    scene->mmap_handle = mmap_handle;
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

    uint64_t mmap_handle = 0;
    void *data = NULL;
    size_t size = 0;
    if (platform_read_file_mmap(path, &mmap_handle, &data, &size)) {
        if (size == 0 || data == NULL) {
            SDL_Log("scene_load_from_file: file %s is empty", path);
            platform_file_unmap(mmap_handle);
            return NULL;
        }
        SDL_Log("Loaded scene file %s via mmap (%zu bytes)", path, size);
        return scene_load_from_memory(data, (uint64_t)size, true, mmap_handle);
    }

    SDL_Log("scene_load_from_file: mmap unavailable, falling back to buffered read");

    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    if (!file) {
        SDL_Log("scene_load_from_file: failed to open %s", path);
        return NULL;
    }

    Sint64 file_size = SDL_GetIOSize(file);
    if (file_size <= 0) {
        SDL_Log("scene_load_from_file: invalid size for %s", path);
        SDL_CloseIO(file);
        return NULL;
    }

    void *blob = malloc((size_t)file_size);
    if (!blob) {
        SDL_Log("scene_load_from_file: out of memory reading %s", path);
        SDL_CloseIO(file);
        return NULL;
    }

    size_t read_bytes = SDL_ReadIO(file, blob, (size_t)file_size);
    SDL_CloseIO(file);
    if (read_bytes != (size_t)file_size) {
        SDL_Log("scene_load_from_file: short read on %s", path);
        free(blob);
        return NULL;
    }

    SDL_Log("Loaded scene file %s via buffered read (%lld bytes)", path, (long long)file_size);
    return scene_load_from_memory(blob, (uint64_t)file_size, false, 0);
}

const SceneHeader* scene_get_header(const Scene *scene) {
    return scene ? scene->header : NULL;
}

void scene_free(Scene *scene) {
    if (!scene) return;

    if (scene->use_mmap) {
        platform_file_unmap(scene->mmap_handle);
    } else {
        free(scene->blob);
    }

    free(scene);
}

// ============================================================================
// Engine API
// ============================================================================

// Map scene surface type IDs to shader binding slots.
// Shader slots: 0=floor, 1=wall, 2=ceiling, 3=window, 4=sphere, 5=book, 6=chair.
static int map_surface_type_to_slot(uint32_t surface_type_id) {
    switch (surface_type_id) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        case 4: return 4;
        case 5: return 5;
        case 6: return 6;
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
    engine->sdf_data = NULL;
    SDL_memset(&engine->sdf_info, 0, sizeof(engine->sdf_info));
    engine->cascade_extent = 0.0f;
    engine->cascades_ready = false;
    for (int i = 0; i < RC_CASCADE_COUNT; i++) {
        engine->cascades[i].texture = NULL;
        engine->cascades[i].cpu_data = NULL;
        engine->cascades[i].spacing = 0.0f;
        engine->cascades[i].origin[0] = 0.0f;
        engine->cascades[i].origin[1] = 0.0f;
        engine->cascades[i].origin[2] = 0.0f;
        engine->cascades[i].dim = 0;
    }

    return engine;
}

bool engine_upload_scene(Engine *engine, const Scene *scene) {
    if (!engine || !scene) {
        SDL_Log("engine_upload_scene: NULL parameter");
        return false;
    }

    const SceneHeader *header = scene->header;
    uint32_t vertex_count = header->vertex_count;
    uint32_t index_count = header->index_count;
    const SceneVertex *vertices = header->vertices;
    const uint16_t *indices = header->indices;
    engine->sdf_info = header->sdf_info;
    engine->sdf_data = header->sdf;
    engine->cascades_ready = false;

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

    if (engine->sdf_data && header->sdf_size > 0) {
        if (!build_radiance_cascades(engine, header)) {
            SDL_Log("Radiance cascades build failed");
        }
    } else {
        SDL_Log("Scene has no SDF; skipping radiance cascades");
    }
    return true;
}

bool engine_load_textures(Engine *engine, const Scene *scene) {
    if (!engine || !scene) {
        SDL_Log("engine_load_textures: NULL parameter");
        return false;
    }

    const SceneHeader *header = scene->header;
    uint32_t texture_count = header->texture_count;
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
    for (uint32_t i = 0; i < texture_count; i++) {
        uint32_t surface_type_id = header->textures[i].surface_type_id;
        int binding_slot = map_surface_type_to_slot(surface_type_id);
        if (binding_slot < 0 || binding_slot >= 8) {
            SDL_Log("Warning: texture %u has unsupported surface_type_id %u, skipping", i, surface_type_id);
            continue;
        }

        const char *path = (const char *)(uintptr_t)header->textures[i].path_offset;
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
        size_t tex_row_size = (size_t)tex_width * 4; // RGBA8 after conversion
        size_t tex_data_size = tex_row_size * (size_t)tex_height;

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
        unsigned char *dst = mapped;
        const unsigned char *src = (const unsigned char *)surface->pixels;
        size_t src_pitch = (size_t)surface->pitch;
        for (int row = 0; row < tex_height; row++) {
            SDL_memcpy(dst + tex_row_size * (size_t)row,
                       src + src_pitch * (size_t)row,
                       tex_row_size);
        }
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

    // Bind textures and samplers (slots 0-10)
    // Bind textures 0-6 plus shared sampler at slot 7, followed by radiance cascades.
    // Require primary bindings to exist
    for (int i = 0; i < 7; i++) {
        if (!engine->textures[i] || !engine->samplers[i]) {
            SDL_Log("engine_render: missing texture or sampler at slot %d", i);
            return false;
        }
    }

    SDL_GPUTexture *cascade_tex[RC_CASCADE_COUNT];
    for (int i = 0; i < RC_CASCADE_COUNT; i++) {
        cascade_tex[i] = engine->cascades[i].texture ? engine->cascades[i].texture : engine->textures[0];
    }

    SDL_GPUTextureSamplerBinding bindings[11] = {
        {engine->textures[0], engine->samplers[0]},
        {engine->textures[1], engine->samplers[1]},
        {engine->textures[2], engine->samplers[2]},
        {engine->textures[3], engine->samplers[3]},
        {engine->textures[4], engine->samplers[4]},
        {engine->textures[5], engine->samplers[5]},
        {engine->textures[6], engine->samplers[6]},
        {engine->textures[0], engine->samplers[0]}, // sampler-only binding uses slot 0 sampler
        {cascade_tex[0], engine->samplers[0]},
        {cascade_tex[1], engine->samplers[0]},
        {cascade_tex[2], engine->samplers[0]},
    };
    SDL_BindGPUFragmentSamplers(render_pass, 0, bindings, 11);

    // Push uniforms if provided
    if (uniforms && uniform_size > 0) {
        SDL_PushGPUVertexUniformData(cmdbuf, 0, uniforms, uniform_size);
        SDL_PushGPUFragmentUniformData(cmdbuf, 0, uniforms, uniform_size);
    }

    // Draw indexed
    SDL_DrawGPUIndexedPrimitives(render_pass, engine->index_count, 1, 0, 0, 0);

    return true;
}

bool engine_has_gi(const Engine *engine) {
    return engine && engine->cascades_ready;
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

    for (int i = 0; i < RC_CASCADE_COUNT; i++) {
        if (engine->cascades[i].texture) {
            SDL_ReleaseGPUTexture(engine->device, engine->cascades[i].texture);
        }
        if (engine->cascades[i].cpu_data) {
            SDL_free(engine->cascades[i].cpu_data);
        }
    }

    free(engine);
}
