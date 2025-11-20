/*
 * Scene Builder Implementation
 *
 * Constructs 3D scenes and serializes them to binary format.
 * Extracted from game.c for use as a library or standalone tool.
 */

// Don't use SDL_MAIN_USE_CALLBACKS when used as a library
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "scene_builder.h"
#include "scene_format.h"
#include <base/arena.h>
#include <base/mem.h>
#include <base/base_string.h>
#include <base/base_math.h>
#include <base/scratch.h>
#include <platform/platform.h>

#define CGLTF_IMPLEMENTATION
#include "tpl/cgltf.h"

// Map dimensions and generation parameters
#define MAP_WIDTH 10
#define MAP_HEIGHT 10
#define PI 3.14159265358979323846f
#define WALL_HEIGHT 2.0f
#define CHECKER_SIZE 4.0f
#define LIGHT_FLOOR_CELL 9
#define MAX_STATIC_LIGHTS 16
#define CEILING_LIGHT_HEIGHT (WALL_HEIGHT - 0.1f)
#define CEILING_LIGHT_INTENSITY 1.4f
#define CEILING_LIGHT_MODEL_SCALE 0.5f
#define CEILING_LIGHT_SURFACE_TYPE 7.0f

// Buffer limits
#define MAX_SCENE_VERTICES 40000
#define MAX_GENERATED_VERTICES 40000
#define MAX_GENERATED_INDICES 48000
#define OBJ_MAX_VERTICES 20000
#define OBJ_MAX_INDICES 20000
#define OBJ_MAX_TEMP_VERTICES 8000

// ============================================================================
// Internal data structures
// ============================================================================

// Temporary mesh format (separate arrays, as loaded from files)
typedef struct {
    float *positions;
    float *uvs;
    float *normals;
    float *surface_types;
    float *triangle_ids;
    uint16_t *indices;
    uint32_t position_count;
    uint32_t uv_count;
    uint32_t normal_count;
    uint32_t vertex_count;
    uint32_t index_count;
} MeshData;

// Context for procedural mesh generation
typedef struct {
    float *positions;
    float *uvs;
    float *normals;
    float *surface_types;
    float *triangle_ids;
    uint16_t *indices;

    uint32_t position_idx;
    uint32_t uv_idx;
    uint32_t normal_idx;
    uint32_t surface_idx;
    uint32_t triangle_idx;
    uint32_t index_idx;

    uint16_t index_offset;
    uint32_t triangle_counter;

    float inv_wall_height;
    float window_bottom;
    float window_top;
    float window_margin;
} MeshGenContext;

// SceneBuilder internal structure
struct SceneBuilder {
    Arena *arena;
    bool owns_arena;

    // Final vertex data (GPU-ready format)
    SceneVertex *vertices;
    uint16_t *indices;
    SceneLight *lights;

    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t light_count;

    // Texture path tracking
    const char **texture_paths;  // Array of texture path pointers
    uint32_t *surface_type_ids;  // Corresponding surface type IDs
    uint32_t texture_count;
};

// Light color palette (from game.c)
static const float g_light_color_palette[][3] = {
    {1.00f, 0.95f, 0.85f}, // Warm white
    {0.85f, 0.90f, 1.00f}, // Cool white
    {1.00f, 0.85f, 0.70f}, // Warm amber
    {0.70f, 0.85f, 1.00f}, // Cool blue
    {1.00f, 0.90f, 0.80f}, // Soft warm
    {0.90f, 0.95f, 1.00f}, // Cool tint
    {0.96f, 0.90f, 1.00f}, // Soft magenta hue
};

// OBJ loader storage (static to avoid stack overflow)
static float g_obj_positions[OBJ_MAX_VERTICES * 3];
static float g_obj_uvs[OBJ_MAX_VERTICES * 2];
static float g_obj_normals[OBJ_MAX_VERTICES * 3];
static float g_obj_surface_types[OBJ_MAX_VERTICES];
static uint16_t g_obj_indices[OBJ_MAX_INDICES];
static MeshData g_obj_mesh_data;
static MeshData g_ceiling_light_mesh_data;
static int g_ceiling_light_mesh_status = 0;

static float g_temp_obj_positions[OBJ_MAX_TEMP_VERTICES * 3];
static float g_temp_obj_uvs[OBJ_MAX_TEMP_VERTICES * 2];
static float g_temp_obj_normals[OBJ_MAX_TEMP_VERTICES * 3];

typedef struct {
    uint32_t position;
    uint32_t uv;
    uint32_t normal;
} ObjVertexRef;

// Mesh generation storage
static float g_positions_storage[MAX_GENERATED_VERTICES * 3];
static float g_uvs_storage[MAX_GENERATED_VERTICES * 2];
static float g_normals_storage[MAX_GENERATED_VERTICES * 3];
static float g_surface_storage[MAX_GENERATED_VERTICES];
static float g_triangle_storage[MAX_GENERATED_VERTICES];
static uint16_t g_index_storage[MAX_GENERATED_INDICES];
static MeshData g_mesh_data_storage;

// ============================================================================
// Helper functions
// ============================================================================

static inline int is_solid_cell(int value) {
    return (value == 1) || (value == 2) || (value == 3);
}

static inline int is_digit_char(char c) {
    return c >= '0' && c <= '9';
}

static const char *skip_spaces(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    return p;
}

static bool parse_float_token(const char **cursor, const char *end, float *out_value) {
    const char *p = skip_spaces(*cursor, end);
    if (p >= end) {
        return false;
    }

    int sign = 1;
    if (*p == '-' || *p == '+') {
        if (*p == '-') {
            sign = -1;
        }
        p++;
    }

    float value = 0.0f;
    int has_digits = 0;
    while (p < end && is_digit_char(*p)) {
        has_digits = 1;
        value = value * 10.0f + (float)(*p - '0');
        p++;
    }

    if (p < end && *p == '.') {
        p++;
        float frac = 0.1f;
        while (p < end && is_digit_char(*p)) {
            has_digits = 1;
            value += (float)(*p - '0') * frac;
            frac *= 0.1f;
            p++;
        }
    }

    if (!has_digits) {
        return false;
    }

    *out_value = value * (float)sign;
    *cursor = p;
    return true;
}

static bool parse_vec_components(const char **cursor, const char *end, float *dst, int count) {
    for (int i = 0; i < count; i++) {
        if (!parse_float_token(cursor, end, &dst[i])) {
            return false;
        }
    }
    return true;
}

static bool parse_uint_token(const char **cursor, const char *end, uint32_t *out_value) {
    const char *p = skip_spaces(*cursor, end);
    if (p >= end || !is_digit_char(*p)) {
        return false;
    }

    uint32_t value = 0;
    while (p < end && is_digit_char(*p)) {
        value = value * 10u + (uint32_t)(*p - '0');
        p++;
    }

    *out_value = value;
    *cursor = p;
    return true;
}

static bool parse_face_vertex(const char **cursor, const char *end, ObjVertexRef *out) {
    const char *p = skip_spaces(*cursor, end);
    uint32_t v_idx = 0;
    if (!parse_uint_token(&p, end, &v_idx)) {
        return false;
    }

    uint32_t vt_idx = 0;
    uint32_t vn_idx = 0;

    if (p < end && *p == '/') {
        p++;
        if (p < end && is_digit_char(*p)) {
            if (!parse_uint_token(&p, end, &vt_idx)) {
                return false;
            }
        }
        if (p < end && *p == '/') {
            p++;
            if (p < end && is_digit_char(*p)) {
                if (!parse_uint_token(&p, end, &vn_idx)) {
                    return false;
                }
            }
        }
    }

    out->position = v_idx > 0 ? v_idx - 1 : 0;
    out->uv = vt_idx > 0 ? vt_idx - 1 : 0;
    out->normal = vn_idx > 0 ? vn_idx - 1 : 0;
    *cursor = p;
    return true;
}

static float mesh_min_y(const MeshData *mesh) {
    if (!mesh || mesh->vertex_count == 0) {
        return 0.0f;
    }
    float min_y = mesh->positions[1];
    for (uint32_t i = 0; i < mesh->vertex_count; i++) {
        float y = mesh->positions[i * 3 + 1];
        if (y < min_y) {
            min_y = y;
        }
    }
    return min_y;
}

// ============================================================================
// Mesh generation helper functions
// ============================================================================

static inline void push_position(MeshGenContext *ctx, float x, float y, float z) {
    ctx->positions[ctx->position_idx++] = x;
    ctx->positions[ctx->position_idx++] = y;
    ctx->positions[ctx->position_idx++] = z;
}

static inline void push_uv(MeshGenContext *ctx, float u, float v) {
    ctx->uvs[ctx->uv_idx++] = u;
    ctx->uvs[ctx->uv_idx++] = v;
}

static inline void push_normal(MeshGenContext *ctx, float x, float y, float z) {
    ctx->normals[ctx->normal_idx++] = x;
    ctx->normals[ctx->normal_idx++] = y;
    ctx->normals[ctx->normal_idx++] = z;
}

static inline void push_surface_type(MeshGenContext *ctx, float type) {
    ctx->surface_types[ctx->surface_idx++] = type;
}

static inline void push_triangle_id(MeshGenContext *ctx, float id) {
    ctx->triangle_ids[ctx->triangle_idx++] = id;
}

static inline void push_index(MeshGenContext *ctx, uint16_t idx) {
    ctx->indices[ctx->index_idx++] = idx;
}

static void push_north_segment_range(MeshGenContext *ctx, float x0, float x1, float z,
                                     float y0, float y1, float base_u, float surface_type) {
    float height = y1 - y0;
    float u_span = x1 - x0;
    uint16_t base = ctx->index_offset;

    push_position(ctx, x0, y0, z);
    push_uv(ctx, base_u, 0.0f);
    push_normal(ctx, 0.0f, 0.0f, -1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x1, y0, z);
    push_uv(ctx, base_u + u_span, 0.0f);
    push_normal(ctx, 0.0f, 0.0f, -1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x0, y1, z);
    push_uv(ctx, base_u, height * ctx->inv_wall_height);
    push_normal(ctx, 0.0f, 0.0f, -1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x1, y1, z);
    push_uv(ctx, base_u + u_span, height * ctx->inv_wall_height);
    push_normal(ctx, 0.0f, 0.0f, -1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_index(ctx, base + 0);
    push_index(ctx, base + 1);
    push_index(ctx, base + 2);
    push_index(ctx, base + 1);
    push_index(ctx, base + 3);
    push_index(ctx, base + 2);

    ctx->index_offset += 4;
    ctx->triangle_counter += 2;
}

static void push_south_segment_range(MeshGenContext *ctx, float x0, float x1, float z,
                                     float y0, float y1, float base_u, float surface_type) {
    float height = y1 - y0;
    float u_span = x1 - x0;
    uint16_t base = ctx->index_offset;

    push_position(ctx, x0, y0, z);
    push_uv(ctx, base_u, 0.0f);
    push_normal(ctx, 0.0f, 0.0f, 1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x0, y1, z);
    push_uv(ctx, base_u, height * ctx->inv_wall_height);
    push_normal(ctx, 0.0f, 0.0f, 1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x1, y0, z);
    push_uv(ctx, base_u + u_span, 0.0f);
    push_normal(ctx, 0.0f, 0.0f, 1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x1, y1, z);
    push_uv(ctx, base_u + u_span, height * ctx->inv_wall_height);
    push_normal(ctx, 0.0f, 0.0f, 1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_index(ctx, base + 0);
    push_index(ctx, base + 1);
    push_index(ctx, base + 2);
    push_index(ctx, base + 1);
    push_index(ctx, base + 3);
    push_index(ctx, base + 2);

    ctx->index_offset += 4;
    ctx->triangle_counter += 2;
}

static void push_west_segment_range(MeshGenContext *ctx, float x, float z0, float z1,
                                    float y0, float y1, float base_v, float surface_type) {
    float height = y1 - y0;
    float v_span = z1 - z0;
    uint16_t base = ctx->index_offset;

    push_position(ctx, x, y0, z0);
    push_uv(ctx, 0.0f, base_v);
    push_normal(ctx, -1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x, y0, z1);
    push_uv(ctx, v_span, base_v);
    push_normal(ctx, -1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x, y1, z0);
    push_uv(ctx, 0.0f, base_v + height * ctx->inv_wall_height);
    push_normal(ctx, -1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x, y1, z1);
    push_uv(ctx, v_span, base_v + height * ctx->inv_wall_height);
    push_normal(ctx, -1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_index(ctx, base + 0);
    push_index(ctx, base + 2);
    push_index(ctx, base + 1);
    push_index(ctx, base + 1);
    push_index(ctx, base + 2);
    push_index(ctx, base + 3);

    ctx->index_offset += 4;
    ctx->triangle_counter += 2;
}

static void push_east_segment_range(MeshGenContext *ctx, float x, float z0, float z1,
                                    float y0, float y1, float base_v, float surface_type) {
    float height = y1 - y0;
    float v_span = z1 - z0;
    uint16_t base = ctx->index_offset;

    push_position(ctx, x, y0, z0);
    push_uv(ctx, 0.0f, base_v);
    push_normal(ctx, 1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x, y1, z0);
    push_uv(ctx, 0.0f, base_v + height * ctx->inv_wall_height);
    push_normal(ctx, 1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x, y0, z1);
    push_uv(ctx, v_span, base_v);
    push_normal(ctx, 1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x, y1, z1);
    push_uv(ctx, v_span, base_v + height * ctx->inv_wall_height);
    push_normal(ctx, 1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_index(ctx, base + 0);
    push_index(ctx, base + 2);
    push_index(ctx, base + 1);
    push_index(ctx, base + 1);
    push_index(ctx, base + 2);
    push_index(ctx, base + 3);

    ctx->index_offset += 4;
    ctx->triangle_counter += 2;
}

static void push_horizontal_fill(MeshGenContext *ctx, float x0, float x1, float z0, float z1,
                                  float y, float surface_type) {
    float u_span = x1 - x0;
    float v_span = z1 - z0;
    float ny = (surface_type == 0.0f) ? 1.0f : -1.0f;

    uint16_t base = ctx->index_offset;

    push_position(ctx, x0, y, z0);
    push_uv(ctx, 0.0f, 0.0f);
    push_normal(ctx, 0.0f, ny, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x1, y, z0);
    push_uv(ctx, u_span, 0.0f);
    push_normal(ctx, 0.0f, ny, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x0, y, z1);
    push_uv(ctx, 0.0f, v_span);
    push_normal(ctx, 0.0f, ny, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x1, y, z1);
    push_uv(ctx, u_span, v_span);
    push_normal(ctx, 0.0f, ny, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_index(ctx, base + 0);
    push_index(ctx, base + 1);
    push_index(ctx, base + 2);
    push_index(ctx, base + 1);
    push_index(ctx, base + 3);
    push_index(ctx, base + 2);

    ctx->index_offset += 4;
    ctx->triangle_counter += 2;
}

static void push_north_segment(MeshGenContext *ctx, float x, float z, float y0, float y1) {
    push_north_segment_range(ctx, x, x + 1.0f, z, y0, y1, 0, 1.0f);
}

static void push_south_segment(MeshGenContext *ctx, float x, float z, float y0, float y1) {
    push_south_segment_range(ctx, x, x + 1.0f, z + 1.0f, y0, y1, 0, 1.0f);
}

static void push_west_segment(MeshGenContext *ctx, float x, float z, float y0, float y1) {
    push_west_segment_range(ctx, x, z, z + 1.0f, y0, y1, 0, 1.0f);
}

static void push_east_segment(MeshGenContext *ctx, float x, float z, float y0, float y1) {
    push_east_segment_range(ctx, x + 1.0f, z, z + 1.0f, y0, y1, 0, 1.0f);
}

static void add_mesh_instance(MeshGenContext *ctx, const MeshData *mesh,
                              float scale, float tx, float ty, float tz,
                              float surface_type) {
    if (!mesh) {
        return;
    }

    uint16_t base_vertex = (uint16_t)ctx->surface_idx;
    for (uint32_t i = 0; i < mesh->vertex_count; i++) {
        const float *pos = &mesh->positions[i * 3];
        push_position(ctx,
            pos[0] * scale + tx,
            pos[1] * scale + ty,
            pos[2] * scale + tz);
        push_uv(ctx,
            mesh->uvs[i * 2 + 0],
            mesh->uvs[i * 2 + 1]);
        push_normal(ctx,
            mesh->normals[i * 3 + 0],
            mesh->normals[i * 3 + 1],
            mesh->normals[i * 3 + 2]);
        push_surface_type(ctx, surface_type);
        push_triangle_id(ctx, 0.0f);
    }

    for (uint32_t i = 0; i < mesh->index_count; i++) {
        push_index(ctx, base_vertex + mesh->indices[i]);
    }
}

// ============================================================================
// OBJ file loader
// ============================================================================

static MeshData* load_obj_file(const char *path) {
    Scratch scratch = scratch_begin();
    MeshData *result = NULL;
    static int obj_load_counter = 0;
    obj_load_counter++;
    SDL_Log("load_obj_file[%d]: %s", obj_load_counter, path);

    if (!path) {
        SDL_Log("OBJ path is NULL");
        goto cleanup;
    }

    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    if (!file) {
        SDL_Log("Failed to open OBJ file: %s", path);
        goto cleanup;
    }

    Sint64 file_size = SDL_GetIOSize(file);
    if (file_size <= 0) {
        SDL_Log("Failed to get OBJ file size: %s", path);
        SDL_CloseIO(file);
        goto cleanup;
    }

    char *file_data = (char *)arena_alloc(scratch.arena, (size_t)file_size + 1);
    if (!file_data) {
        SDL_Log("Failed to allocate memory for OBJ file");
        SDL_CloseIO(file);
        goto cleanup;
    }

    size_t bytes_read = SDL_ReadIO(file, file_data, (size_t)file_size);
    SDL_CloseIO(file);

    if (bytes_read != (size_t)file_size) {
        SDL_Log("Failed to read OBJ file completely");
        goto cleanup;
    }
    file_data[file_size] = '\0';

    uint32_t pos_count = 0;
    uint32_t uv_count = 0;
    uint32_t normal_count = 0;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;

    const char *line_start = file_data;
    const char *end = file_data + file_size;

    while (line_start < end) {
        const char *line_end = line_start;
        while (line_end < end && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }

        bool ok = true;
        if (line_end > line_start && line_start[0] != '#') {
            if (line_start[0] == 'v' && line_start[1] == ' ') {
                if (pos_count >= OBJ_MAX_TEMP_VERTICES) {
                    SDL_Log("OBJ loader error: too many vertex positions in %s (max %d)", path, OBJ_MAX_TEMP_VERTICES);
                    ok = false;
                } else {
                    const char *cursor = line_start + 2;
                    float vec[3];
                    ok = parse_vec_components(&cursor, line_end, vec, 3);
                    if (!ok) {
                        SDL_Log("OBJ loader error: malformed vertex position in %s", path);
                    } else {
                        g_temp_obj_positions[pos_count * 3 + 0] = vec[0];
                        g_temp_obj_positions[pos_count * 3 + 1] = vec[1];
                        g_temp_obj_positions[pos_count * 3 + 2] = vec[2];
                        pos_count++;
                    }
                }
            } else if (line_start[0] == 'v' && line_start[1] == 't' && line_start[2] == ' ') {
                if (uv_count >= OBJ_MAX_TEMP_VERTICES) {
                    SDL_Log("OBJ loader error: too many UVs in %s (max %d)", path, OBJ_MAX_TEMP_VERTICES);
                    ok = false;
                } else {
                    const char *cursor = line_start + 3;
                    float uv[2];
                    ok = parse_vec_components(&cursor, line_end, uv, 2);
                    if (!ok) {
                        SDL_Log("OBJ loader error: malformed UV in %s", path);
                    } else {
                        g_temp_obj_uvs[uv_count * 2 + 0] = uv[0];
                        g_temp_obj_uvs[uv_count * 2 + 1] = uv[1];
                        uv_count++;
                    }
                }
            } else if (line_start[0] == 'v' && line_start[1] == 'n' && line_start[2] == ' ') {
                if (normal_count >= OBJ_MAX_TEMP_VERTICES) {
                    SDL_Log("OBJ loader error: too many normals in %s (max %d)", path, OBJ_MAX_TEMP_VERTICES);
                    ok = false;
                } else {
                    const char *cursor = line_start + 3;
                    float normal[3];
                    ok = parse_vec_components(&cursor, line_end, normal, 3);
                    if (!ok) {
                        SDL_Log("OBJ loader error: malformed normal in %s", path);
                    } else {
                        g_temp_obj_normals[normal_count * 3 + 0] = normal[0];
                        g_temp_obj_normals[normal_count * 3 + 1] = normal[1];
                        g_temp_obj_normals[normal_count * 3 + 2] = normal[2];
                        normal_count++;
                    }
                }
            } else if (line_start[0] == 'f' && line_start[1] == ' ') {
                const char *cursor = line_start + 2;
                ObjVertexRef face[3];
                for (int i = 0; i < 3; i++) {
                    if (!parse_face_vertex(&cursor, line_end, &face[i])) {
                        SDL_Log("OBJ loader error: malformed face in %s", path);
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    cursor = skip_spaces(cursor, line_end);
                    if (cursor < line_end) {
                        SDL_Log("OBJ loader error: only triangle faces are supported in %s", path);
                        ok = false;
                    }
                }
                if (ok) {
                    for (int i = 0; i < 3; i++) {
                        if (vertex_count >= OBJ_MAX_VERTICES || index_count >= OBJ_MAX_INDICES) {
                            SDL_Log("OBJ loader error: too many vertices/indices in %s (max vertices %d)", path, OBJ_MAX_VERTICES);
                            ok = false;
                            break;
                        }
                        const ObjVertexRef *ref = &face[i];
                        g_obj_positions[vertex_count * 3 + 0] = g_temp_obj_positions[ref->position * 3 + 0];
                        g_obj_positions[vertex_count * 3 + 1] = g_temp_obj_positions[ref->position * 3 + 1];
                        g_obj_positions[vertex_count * 3 + 2] = g_temp_obj_positions[ref->position * 3 + 2];

                        g_obj_uvs[vertex_count * 2 + 0] = g_temp_obj_uvs[ref->uv * 2 + 0];
                        g_obj_uvs[vertex_count * 2 + 1] = g_temp_obj_uvs[ref->uv * 2 + 1];

                        g_obj_normals[vertex_count * 3 + 0] = g_temp_obj_normals[ref->normal * 3 + 0];
                        g_obj_normals[vertex_count * 3 + 1] = g_temp_obj_normals[ref->normal * 3 + 1];
                        g_obj_normals[vertex_count * 3 + 2] = g_temp_obj_normals[ref->normal * 3 + 2];

                        g_obj_surface_types[vertex_count] = 4.0f;
                        g_obj_indices[index_count++] = (uint16_t)vertex_count;
                        vertex_count++;
                    }
                }
                if (!ok) {
                    goto cleanup;
                }
            }

            if (!ok) {
                goto cleanup;
            }
        }

        line_start = line_end;
        while (line_start < end && (*line_start == '\n' || *line_start == '\r')) {
            line_start++;
        }
    }

    SDL_Log("Loaded OBJ: %u vertices, %u indices", vertex_count, index_count);

    g_obj_mesh_data.positions = g_obj_positions;
    g_obj_mesh_data.uvs = g_obj_uvs;
    g_obj_mesh_data.normals = g_obj_normals;
    g_obj_mesh_data.surface_types = g_obj_surface_types;
    g_obj_mesh_data.indices = g_obj_indices;
    g_obj_mesh_data.position_count = vertex_count;
    g_obj_mesh_data.uv_count = vertex_count;
    g_obj_mesh_data.normal_count = vertex_count;
    g_obj_mesh_data.vertex_count = vertex_count;
    g_obj_mesh_data.index_count = index_count;
    result = &g_obj_mesh_data;

cleanup:
    scratch_end(scratch);
    return result;
}

// ============================================================================
// glTF file loader (for ceiling light)
// ============================================================================

static const char *cgltf_result_to_string(cgltf_result result) {
    switch (result) {
        case cgltf_result_success: return "success";
        case cgltf_result_data_too_short: return "data too short";
        case cgltf_result_unknown_format: return "unknown format";
        case cgltf_result_invalid_json: return "invalid json";
        case cgltf_result_invalid_gltf: return "invalid gltf";
        case cgltf_result_invalid_options: return "invalid options";
        case cgltf_result_file_not_found: return "file not found";
        case cgltf_result_io_error: return "io error";
        case cgltf_result_out_of_memory: return "out of memory";
        case cgltf_result_legacy_gltf: return "legacy gltf";
        case cgltf_result_max_enum: return "max enum";
    }
    return "unknown error";
}

static const cgltf_accessor* find_attribute_accessor(const cgltf_primitive *primitive,
                                                     cgltf_attribute_type type,
                                                     int index) {
    if (!primitive) {
        return NULL;
    }
    for (cgltf_size i = 0; i < primitive->attributes_count; i++) {
        const cgltf_attribute *attr = &primitive->attributes[i];
        if (attr->type == type && attr->index == index) {
            return attr->data;
        }
    }
    return NULL;
}

static void compute_normals_from_triangles(float *positions, uint16_t *indices,
                                           cgltf_size vertex_count, cgltf_size index_count,
                                           float *normals) {
    if (!positions || !indices || !normals) {
        return;
    }

    base_memset(normals, 0, vertex_count * 3 * sizeof(float));
    for (cgltf_size i = 0; i + 2 < index_count; i += 3) {
        uint16_t i0 = indices[i + 0];
        uint16_t i1 = indices[i + 1];
        uint16_t i2 = indices[i + 2];
        if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
            continue;
        }

        float vx0 = positions[i0 * 3 + 0];
        float vy0 = positions[i0 * 3 + 1];
        float vz0 = positions[i0 * 3 + 2];

        float vx1 = positions[i1 * 3 + 0];
        float vy1 = positions[i1 * 3 + 1];
        float vz1 = positions[i1 * 3 + 2];

        float vx2 = positions[i2 * 3 + 0];
        float vy2 = positions[i2 * 3 + 1];
        float vz2 = positions[i2 * 3 + 2];

        float ax = vx1 - vx0;
        float ay = vy1 - vy0;
        float az = vz1 - vz0;

        float bx = vx2 - vx0;
        float by = vy2 - vy0;
        float bz = vz2 - vz0;

        float nx = ay * bz - az * by;
        float ny = az * bx - ax * bz;
        float nz = ax * by - ay * bx;

        normals[i0 * 3 + 0] += nx;
        normals[i0 * 3 + 1] += ny;
        normals[i0 * 3 + 2] += nz;

        normals[i1 * 3 + 0] += nx;
        normals[i1 * 3 + 1] += ny;
        normals[i1 * 3 + 2] += nz;

        normals[i2 * 3 + 0] += nx;
        normals[i2 * 3 + 1] += ny;
        normals[i2 * 3 + 2] += nz;
    }

    for (cgltf_size i = 0; i < vertex_count; i++) {
        float nx = normals[i * 3 + 0];
        float ny = normals[i * 3 + 1];
        float nz = normals[i * 3 + 2];
        float len = fast_sqrtf(nx * nx + ny * ny + nz * nz);
        if (len > 0.0001f) {
            float inv = 1.0f / len;
            normals[i * 3 + 0] = nx * inv;
            normals[i * 3 + 1] = ny * inv;
            normals[i * 3 + 2] = nz * inv;
        } else {
            normals[i * 3 + 0] = 0.0f;
            normals[i * 3 + 1] = -1.0f;
            normals[i * 3 + 2] = 0.0f;
        }
    }
}

static MeshData* load_ceiling_light_mesh(const char *path, Arena *arena) {
    if (g_ceiling_light_mesh_status == 1) {
        if (g_ceiling_light_mesh_data.vertex_count > 0) {
            return &g_ceiling_light_mesh_data;
        }
        return NULL;
    }
    if (g_ceiling_light_mesh_status == -1) {
        return NULL;
    }

    if (!path) {
        SDL_Log("Ceiling light path is NULL");
        g_ceiling_light_mesh_status = -1;
        return NULL;
    }

    cgltf_options options = {0};
    cgltf_data *data = NULL;
    float *positions = NULL;
    float *normals = NULL;
    float *uvs = NULL;
    uint16_t *indices = NULL;

    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        SDL_Log("Failed to parse %s: %s", path, cgltf_result_to_string(result));
        g_ceiling_light_mesh_status = -1;
        return NULL;
    }

    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        SDL_Log("Failed to load buffers for %s: %s", path, cgltf_result_to_string(result));
        g_ceiling_light_mesh_status = -1;
        cgltf_free(data);
        return NULL;
    }

    if (data->meshes_count == 0) {
        SDL_Log("Ceiling light glb contains no meshes");
        g_ceiling_light_mesh_status = -1;
        cgltf_free(data);
        return NULL;
    }

    cgltf_size total_vertex_count = 0;
    cgltf_size total_index_count = 0;
    cgltf_size node_mesh_count = 0;
    for (cgltf_size node_index = 0; node_index < data->nodes_count; node_index++) {
        const cgltf_node *node = &data->nodes[node_index];
        if (!node->mesh) {
            continue;
        }
        const cgltf_mesh *mesh = node->mesh;
        if (mesh->primitives_count == 0) {
            continue;
        }
        node_mesh_count++;
        for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; prim_index++) {
            const cgltf_primitive *primitive = &mesh->primitives[prim_index];
            if (primitive->type != cgltf_primitive_type_triangles) {
                SDL_Log("Ceiling light primitive node %u meshPrim %u must use triangle topology", (unsigned)node_index, (unsigned)prim_index);
                g_ceiling_light_mesh_status = -1;
                cgltf_free(data);
                return NULL;
            }
            const cgltf_accessor *position_accessor = find_attribute_accessor(primitive, cgltf_attribute_type_position, 0);
            if (!position_accessor || position_accessor->count == 0) {
                SDL_Log("Ceiling light primitive node %u meshPrim %u missing POSITION data", (unsigned)node_index, (unsigned)prim_index);
                g_ceiling_light_mesh_status = -1;
                cgltf_free(data);
                return NULL;
            }
            if (position_accessor->count >= UINT16_MAX) {
                SDL_Log("Ceiling light primitive node %u meshPrim %u vertex count unsupported: %u", (unsigned)node_index, (unsigned)prim_index, (unsigned)position_accessor->count);
                g_ceiling_light_mesh_status = -1;
                cgltf_free(data);
                return NULL;
            }
            total_vertex_count += position_accessor->count;
            if (primitive->indices) {
                total_index_count += primitive->indices->count;
            } else {
                total_index_count += position_accessor->count;
            }
        }
    }

    if (node_mesh_count == 0 || total_vertex_count == 0 || total_index_count == 0) {
        SDL_Log("Ceiling light glb has no mesh nodes");
        g_ceiling_light_mesh_status = -1;
        cgltf_free(data);
        return NULL;
    }

    positions = (float *)arena_alloc(arena, sizeof(float) * total_vertex_count * 3);
    normals = (float *)arena_alloc(arena, sizeof(float) * total_vertex_count * 3);
    uvs = (float *)arena_alloc(arena, sizeof(float) * total_vertex_count * 2);
    indices = (uint16_t *)arena_alloc(arena, sizeof(uint16_t) * total_index_count);
    if (!positions || !normals || !uvs || !indices) {
        SDL_Log("Out of memory loading ceiling light mesh");
        goto fail;
    }

    cgltf_size vertex_offset = 0;
    cgltf_size index_offset = 0;
    bool have_normals = true;
    bool have_uvs = true;

    for (cgltf_size node_index = 0; node_index < data->nodes_count; node_index++) {
        const cgltf_node *node = &data->nodes[node_index];
        if (!node->mesh || node->mesh->primitives_count == 0) {
            continue;
        }
        cgltf_float world_matrix[16];
        cgltf_node_transform_world(node, world_matrix);
        const cgltf_mesh *mesh = node->mesh;
        for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; prim_index++) {
            const cgltf_primitive *primitive = &mesh->primitives[prim_index];
            const cgltf_accessor *position_accessor = find_attribute_accessor(primitive, cgltf_attribute_type_position, 0);
            const cgltf_accessor *uv_accessor = find_attribute_accessor(primitive, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor *normal_accessor = find_attribute_accessor(primitive, cgltf_attribute_type_normal, 0);

            cgltf_size prim_vertex_count = position_accessor->count;
            for (cgltf_size i = 0; i < prim_vertex_count; i++) {
                cgltf_float temp[3] = {0.0f, 0.0f, 0.0f};
                if (!cgltf_accessor_read_float(position_accessor, i, temp, 3)) {
                    SDL_Log("Failed reading POSITION for node %u primitive %u vertex %u", (unsigned)node_index, (unsigned)prim_index, (unsigned)i);
                    goto fail;
                }
                float x = (float)temp[0];
                float y = (float)temp[1];
                float z = (float)temp[2];
                float tx = world_matrix[0] * x + world_matrix[4] * y + world_matrix[8] * z + world_matrix[12];
                float ty = world_matrix[1] * x + world_matrix[5] * y + world_matrix[9] * z + world_matrix[13];
                float tz = world_matrix[2] * x + world_matrix[6] * y + world_matrix[10] * z + world_matrix[14];
                positions[(vertex_offset + i) * 3 + 0] = tx;
                positions[(vertex_offset + i) * 3 + 1] = ty;
                positions[(vertex_offset + i) * 3 + 2] = tz;
            }

            if (uv_accessor && uv_accessor->count == prim_vertex_count) {
                for (cgltf_size i = 0; i < prim_vertex_count; i++) {
                    cgltf_float temp[2] = {0.0f, 0.0f};
                    if (!cgltf_accessor_read_float(uv_accessor, i, temp, 2)) {
                        SDL_Log("Failed reading UV for node %u primitive %u vertex %u", (unsigned)node_index, (unsigned)prim_index, (unsigned)i);
                        goto fail;
                    }
                    uvs[(vertex_offset + i) * 2 + 0] = (float)temp[0];
                    uvs[(vertex_offset + i) * 2 + 1] = (float)temp[1];
                }
            } else {
                have_uvs = false;
                for (cgltf_size i = 0; i < prim_vertex_count; i++) {
                    uvs[(vertex_offset + i) * 2 + 0] = 0.0f;
                    uvs[(vertex_offset + i) * 2 + 1] = 0.0f;
                }
            }

            if (normal_accessor && normal_accessor->count == prim_vertex_count) {
                for (cgltf_size i = 0; i < prim_vertex_count; i++) {
                    cgltf_float temp[3] = {0.0f, 1.0f, 0.0f};
                    if (!cgltf_accessor_read_float(normal_accessor, i, temp, 3)) {
                        SDL_Log("Failed reading normal for node %u primitive %u vertex %u", (unsigned)node_index, (unsigned)prim_index, (unsigned)i);
                        goto fail;
                    }
                    float nx = (float)temp[0];
                    float ny = (float)temp[1];
                    float nz = (float)temp[2];
                    float tnx = world_matrix[0] * nx + world_matrix[4] * ny + world_matrix[8] * nz;
                    float tny = world_matrix[1] * nx + world_matrix[5] * ny + world_matrix[9] * nz;
                    float tnz = world_matrix[2] * nx + world_matrix[6] * ny + world_matrix[10] * nz;
                    normals[(vertex_offset + i) * 3 + 0] = tnx;
                    normals[(vertex_offset + i) * 3 + 1] = tny;
                    normals[(vertex_offset + i) * 3 + 2] = tnz;
                }
            } else {
                have_normals = false;
                for (cgltf_size i = 0; i < prim_vertex_count; i++) {
                    normals[(vertex_offset + i) * 3 + 0] = 0.0f;
                    normals[(vertex_offset + i) * 3 + 1] = -1.0f;
                    normals[(vertex_offset + i) * 3 + 2] = 0.0f;
                }
            }

            const cgltf_accessor *index_accessor = primitive->indices;
            if (index_accessor) {
                for (cgltf_size i = 0; i < index_accessor->count; i++) {
                    cgltf_size idx = cgltf_accessor_read_index(index_accessor, i);
                    if (idx >= prim_vertex_count) {
                        SDL_Log("Ceiling light primitive node %u meshPrim %u index %u out of range", (unsigned)node_index, (unsigned)prim_index, (unsigned)idx);
                        goto fail;
                    }
                    if (vertex_offset + idx >= UINT16_MAX) {
                        SDL_Log("Ceiling light combined index exceeds supported range");
                        goto fail;
                    }
                    indices[index_offset + i] = (uint16_t)(vertex_offset + idx);
                }
                index_offset += index_accessor->count;
            } else {
                for (cgltf_size i = 0; i < prim_vertex_count; i++) {
                    if (vertex_offset + i >= UINT16_MAX) {
                        SDL_Log("Ceiling light implicit index exceeds supported range");
                        goto fail;
                    }
                    indices[index_offset + i] = (uint16_t)(vertex_offset + i);
                }
                index_offset += prim_vertex_count;
            }

            vertex_offset += prim_vertex_count;
        }
    }

    cgltf_size vertex_count = total_vertex_count;
    cgltf_size index_count = total_index_count;

    if (!have_normals) {
        compute_normals_from_triangles(positions, indices, vertex_count, index_count, normals);
    }

    g_ceiling_light_mesh_data.positions = positions;
    g_ceiling_light_mesh_data.uvs = uvs;
    g_ceiling_light_mesh_data.normals = normals;
    g_ceiling_light_mesh_data.surface_types = NULL;
    g_ceiling_light_mesh_data.triangle_ids = NULL;
    g_ceiling_light_mesh_data.indices = indices;
    g_ceiling_light_mesh_data.position_count = (uint32_t)vertex_count;
    g_ceiling_light_mesh_data.uv_count = (uint32_t)vertex_count;
    g_ceiling_light_mesh_data.normal_count = (uint32_t)vertex_count;
    g_ceiling_light_mesh_data.vertex_count = (uint32_t)vertex_count;
    g_ceiling_light_mesh_data.index_count = (uint32_t)index_count;

    positions = NULL;
    normals = NULL;
    uvs = NULL;
    indices = NULL;

    cgltf_free(data);
    g_ceiling_light_mesh_status = 1;
    SDL_Log("Loaded ceiling light mesh (%u vertices, %u indices)",
            g_ceiling_light_mesh_data.vertex_count, g_ceiling_light_mesh_data.index_count);
    return &g_ceiling_light_mesh_data;

fail:
    if (data) cgltf_free(data);
    g_ceiling_light_mesh_status = -1;
    return NULL;
}

// ============================================================================
// Scene generation (adapted from generate_mesh in game.c)
// ============================================================================

static MeshData* generate_procedural_mesh(const SceneConfig *config) {
    MeshGenContext ctx = {0};
    ctx.positions = g_positions_storage;
    ctx.uvs = g_uvs_storage;
    ctx.normals = g_normals_storage;
    ctx.surface_types = g_surface_storage;
    ctx.triangle_ids = g_triangle_storage;
    ctx.indices = g_index_storage;
    ctx.inv_wall_height = 1.0f / WALL_HEIGHT;
    ctx.window_bottom = WALL_HEIGHT * 0.3f;
    ctx.window_top = WALL_HEIGHT - ctx.window_bottom;
    ctx.window_margin = 0.15f;

    int width = config->map_width;
    int height = config->map_height;
    int *map = config->map_data;

    // Floor
    push_position(&ctx, 0.0f, 0.0f, 0.0f);
    push_uv(&ctx, 0.0f, 0.0f);
    push_normal(&ctx, 0.0f, 1.0f, 0.0f);
    push_surface_type(&ctx, 0.0f);
    push_triangle_id(&ctx, (float)ctx.triangle_counter);

    push_position(&ctx, (float)width, 0.0f, 0.0f);
    push_uv(&ctx, (float)width * CHECKER_SIZE / 4.0f, 0.0f);
    push_normal(&ctx, 0.0f, 1.0f, 0.0f);
    push_surface_type(&ctx, 0.0f);
    push_triangle_id(&ctx, (float)(ctx.triangle_counter + 1));

    push_position(&ctx, 0.0f, 0.0f, (float)height);
    push_uv(&ctx, 0.0f, (float)height * CHECKER_SIZE / 4.0f);
    push_normal(&ctx, 0.0f, 1.0f, 0.0f);
    push_surface_type(&ctx, 0.0f);
    push_triangle_id(&ctx, 0.0f);

    push_position(&ctx, (float)width, 0.0f, (float)height);
    push_uv(&ctx, (float)width * CHECKER_SIZE / 4.0f, (float)height * CHECKER_SIZE / 4.0f);
    push_normal(&ctx, 0.0f, 1.0f, 0.0f);
    push_surface_type(&ctx, 0.0f);
    push_triangle_id(&ctx, 0.0f);

    push_index(&ctx, 0);
    push_index(&ctx, 1);
    push_index(&ctx, 2);
    push_index(&ctx, 1);
    push_index(&ctx, 3);
    push_index(&ctx, 2);

    ctx.index_offset = 4;
    ctx.triangle_counter = 2;

    // Ceiling
    uint16_t base = ctx.index_offset;
    push_position(&ctx, 0.0f, WALL_HEIGHT, 0.0f);
    push_uv(&ctx, 0.0f, 0.0f);
    push_normal(&ctx, 0.0f, -1.0f, 0.0f);
    push_surface_type(&ctx, 2.0f);
    push_triangle_id(&ctx, (float)ctx.triangle_counter);

    push_position(&ctx, (float)width, WALL_HEIGHT, 0.0f);
    push_uv(&ctx, (float)width * CHECKER_SIZE, 0.0f);
    push_normal(&ctx, 0.0f, -1.0f, 0.0f);
    push_surface_type(&ctx, 2.0f);
    push_triangle_id(&ctx, (float)(ctx.triangle_counter + 1));

    push_position(&ctx, 0.0f, WALL_HEIGHT, (float)height);
    push_uv(&ctx, 0.0f, (float)height * CHECKER_SIZE);
    push_normal(&ctx, 0.0f, -1.0f, 0.0f);
    push_surface_type(&ctx, 2.0f);
    push_triangle_id(&ctx, 0.0f);

    push_position(&ctx, (float)width, WALL_HEIGHT, (float)height);
    push_uv(&ctx, (float)width * CHECKER_SIZE, (float)height * CHECKER_SIZE);
    push_normal(&ctx, 0.0f, -1.0f, 0.0f);
    push_surface_type(&ctx, 2.0f);
    push_triangle_id(&ctx, 0.0f);

    push_index(&ctx, base + 0);
    push_index(&ctx, base + 1);
    push_index(&ctx, base + 2);
    push_index(&ctx, base + 1);
    push_index(&ctx, base + 3);
    push_index(&ctx, base + 2);

    ctx.index_offset += 4;
    ctx.triangle_counter += 2;

    // Walls
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            int cell = map[z * width + x];
            if (!is_solid_cell(cell)) {
                continue;
            }

            int is_window_ns = (cell == 2);
            int is_window_ew = (cell == 3);

            float x_inner0 = (float)x + ctx.window_margin;
            float x_inner1 = (float)(x + 1) - ctx.window_margin;
            float z_inner0 = (float)z + ctx.window_margin;
            float z_inner1 = (float)(z + 1) - ctx.window_margin;

            if (z == 0 || !is_solid_cell(map[(z - 1) * width + x])) {
                if (is_window_ns) {
                    push_north_segment_range(&ctx, (float)x, (float)(x + 1), (float)z, 0.0f, ctx.window_bottom, 0, 1.0f);
                    push_north_segment_range(&ctx, (float)x, (float)(x + 1), (float)z, ctx.window_top, WALL_HEIGHT, 0, 1.0f);
                    push_north_segment_range(&ctx, (float)x, x_inner0, (float)z, ctx.window_bottom, ctx.window_top, 0, 3.0f);
                    push_north_segment_range(&ctx, x_inner1, (float)(x + 1), (float)z, ctx.window_bottom, ctx.window_top, 0, 3.0f);
                } else {
                    push_north_segment(&ctx, (float)x, (float)z, 0.0f, WALL_HEIGHT);
                }
            }

            if (z == height - 1 || !is_solid_cell(map[(z + 1) * width + x])) {
                if (is_window_ns) {
                    float south_z = (float)z + 1.0f;
                    push_south_segment_range(&ctx, (float)x, (float)(x + 1), south_z, 0.0f, ctx.window_bottom, 0, 1.0f);
                    push_south_segment_range(&ctx, (float)x, (float)(x + 1), south_z, ctx.window_top, WALL_HEIGHT, 0, 1.0f);
                    push_south_segment_range(&ctx, (float)x, x_inner0, south_z, ctx.window_bottom, ctx.window_top, 0, 3.0f);
                    push_south_segment_range(&ctx, x_inner1, (float)(x + 1), south_z, ctx.window_bottom, ctx.window_top, 0, 3.0f);
                } else {
                    push_south_segment(&ctx, (float)x, (float)z, 0.0f, WALL_HEIGHT);
                }
            }

            if (x == 0 || !is_solid_cell(map[z * width + (x - 1)])) {
                if (is_window_ew) {
                    push_west_segment_range(&ctx, (float)x, (float)z, (float)(z + 1), 0.0f, ctx.window_bottom, 0, 1.0f);
                    push_west_segment_range(&ctx, (float)x, (float)z, (float)(z + 1), ctx.window_top, WALL_HEIGHT, 0, 1.0f);
                    push_west_segment_range(&ctx, (float)x, (float)z, z_inner0, ctx.window_bottom, ctx.window_top, 0, 3.0f);
                    push_west_segment_range(&ctx, (float)x, z_inner1, (float)(z + 1), ctx.window_bottom, ctx.window_top, 0, 3.0f);
                } else {
                    push_west_segment(&ctx, (float)x, (float)z, 0.0f, WALL_HEIGHT);
                }
            }

            if (x == width - 1 || !is_solid_cell(map[z * width + (x + 1)])) {
                if (is_window_ew) {
                    push_east_segment_range(&ctx, (float)(x + 1), (float)z, (float)(z + 1), 0.0f, ctx.window_bottom, 0, 1.0f);
                    push_east_segment_range(&ctx, (float)(x + 1), (float)z, (float)(z + 1), ctx.window_top, WALL_HEIGHT, 0, 1.0f);
                    push_east_segment_range(&ctx, (float)(x + 1), (float)z, z_inner0, ctx.window_bottom, ctx.window_top, 0, 3.0f);
                    push_east_segment_range(&ctx, (float)(x + 1), z_inner1, (float)(z + 1), ctx.window_bottom, ctx.window_top, 0, 3.0f);
                } else {
                    push_east_segment(&ctx, (float)x, (float)z, 0.0f, WALL_HEIGHT);
                }
            }

            if (is_window_ns) {
                push_west_segment_range(&ctx, x_inner0, (float)z, (float)(z + 1), ctx.window_bottom, ctx.window_top, 1, 3.0f);
                push_east_segment_range(&ctx, x_inner1, (float)z, (float)(z + 1), ctx.window_bottom, ctx.window_top, 1, 3.0f);
                push_horizontal_fill(&ctx, x_inner0, x_inner1, (float)z, (float)(z + 1), ctx.window_bottom, 0.0f);
                push_horizontal_fill(&ctx, x_inner0, x_inner1, (float)z, (float)(z + 1), ctx.window_top, 2.0f);
            } else if (is_window_ew) {
                push_north_segment_range(&ctx, (float)x, (float)(x + 1), z_inner0, ctx.window_bottom, ctx.window_top, 1, 3.0f);
                push_south_segment_range(&ctx, (float)x, (float)(x + 1), z_inner1, ctx.window_bottom, ctx.window_top, 1, 3.0f);
                push_horizontal_fill(&ctx, (float)x, (float)(x + 1), z_inner0, z_inner1, ctx.window_bottom, 0.0f);
                push_horizontal_fill(&ctx, (float)x, (float)(x + 1), z_inner0, z_inner1, ctx.window_top, 2.0f);
            }
        }
    }

    // Load sphere mesh and add it to window cells
    SDL_Log("Before sphere: position_idx=%u, surface_idx=%u, index_idx=%u",
            ctx.position_idx, ctx.surface_idx, ctx.index_idx);

    MeshData *sphere_mesh = load_obj_file(config->sphere_obj_path);
    if (sphere_mesh) {
        SDL_Log("Adding spheres to window cells");
        SDL_Log("Sphere mesh: vertex_count=%u, index_count=%u",
                sphere_mesh->vertex_count, sphere_mesh->index_count);

        int sphere_count = 0;
        for (int z = 0; z < height; z++) {
            for (int x = 0; x < width; x++) {
                int cell = map[z * width + x];
                if (cell == 2 || cell == 3) {
                    float cx = (float)x + 0.5f;
                    float cy = WALL_HEIGHT * 0.5f;
                    float cz = (float)z + 0.5f;
                    float scale = 0.3f;

                    SDL_Log("Sphere %d at cell(%d,%d): base_vertex=%u, surface_idx before=%u",
                            sphere_count, x, z, (uint16_t)ctx.surface_idx, ctx.surface_idx);

                    add_mesh_instance(&ctx, sphere_mesh, scale, cx, cy, cz, 4.0f);

                    SDL_Log("Sphere %d: surface_idx after=%u, index_idx after=%u",
                            sphere_count, ctx.surface_idx, ctx.index_idx);
                    sphere_count++;
                }
            }
        }
        SDL_Log("Added %d spheres total", sphere_count);
    }

    SDL_Log("After spheres: position_idx=%u, surface_idx=%u, index_idx=%u",
            ctx.position_idx, ctx.surface_idx, ctx.index_idx);

    MeshData *book_mesh = load_obj_file(config->book_obj_path);
    if (book_mesh) {
        SDL_Log("Adding book at spawn position (%.2f, %.2f)", config->spawn_x, config->spawn_z);
        float min_y = mesh_min_y(book_mesh);
        const float book_scale = 2.0f;
        const float floor_y = 0.0f;
        float book_y = floor_y - min_y * book_scale + 0.01f;
        add_mesh_instance(&ctx, book_mesh, book_scale, config->spawn_x, book_y, config->spawn_z, 5.0f);
    }

    MeshData *chair_mesh = load_obj_file(config->chair_obj_path);
    if (chair_mesh) {
        SDL_Log("Adding chair near spawn position (%.2f, %.2f)", config->spawn_x, config->spawn_z);
        float min_y = mesh_min_y(chair_mesh);
        const float chair_scale = 0.75f;
        const float floor_y = 0.0f;
        float chair_y = floor_y - min_y * chair_scale + 0.01f;
        float chair_x = config->spawn_x + 0.9f;
        float chair_z = config->spawn_z - 0.2f;
        add_mesh_instance(&ctx, chair_mesh, chair_scale, chair_x, chair_y, chair_z, 6.0f);
    }

    g_mesh_data_storage.positions = g_positions_storage;
    g_mesh_data_storage.uvs = g_uvs_storage;
    g_mesh_data_storage.normals = g_normals_storage;
    g_mesh_data_storage.surface_types = g_surface_storage;
    g_mesh_data_storage.triangle_ids = g_triangle_storage;
    g_mesh_data_storage.indices = g_index_storage;
    g_mesh_data_storage.position_count = ctx.position_idx;
    g_mesh_data_storage.uv_count = ctx.uv_idx;
    g_mesh_data_storage.normal_count = ctx.normal_idx;
    g_mesh_data_storage.vertex_count = ctx.surface_idx;
    g_mesh_data_storage.index_count = ctx.index_idx;

    return &g_mesh_data_storage;
}

// ============================================================================
// Public API implementation
// ============================================================================

SceneBuilder* scene_builder_create(Arena *arena) {
    bool owns_arena = false;
    if (!arena) {
        arena = arena_new(4 * 1024 * 1024);  // 4MB default
        if (!arena) {
            SDL_Log("Failed to create arena for SceneBuilder");
            return NULL;
        }
        owns_arena = true;
    }

    SceneBuilder *builder = (SceneBuilder *)arena_alloc(arena, sizeof(SceneBuilder));
    if (!builder) {
        if (owns_arena) {
            arena_free(arena);
        }
        return NULL;
    }

    base_memset(builder, 0, sizeof(SceneBuilder));
    builder->arena = arena;
    builder->owns_arena = owns_arena;

    return builder;
}

bool scene_builder_generate(SceneBuilder *builder, const SceneConfig *config) {
    if (!builder || !config || !config->map_data) {
        SDL_Log("Invalid arguments to scene_builder_generate");
        return false;
    }

    SDL_Log("Generating scene geometry...");

    // Generate procedural mesh
    MeshData *mesh = generate_procedural_mesh(config);
    if (!mesh) {
        SDL_Log("Failed to generate procedural mesh");
        return false;
    }

    // Add ceiling light meshes if provided
    uint32_t light_count = 0;
    float light_positions[MAX_STATIC_LIGHTS][3];
    float light_colors[MAX_STATIC_LIGHTS][3];

    // Scan for light cells in the map
    const size_t palette_count = sizeof(g_light_color_palette) / sizeof(g_light_color_palette[0]);
    for (int z = 0; z < config->map_height; z++) {
        for (int x = 0; x < config->map_width; x++) {
            int cell = config->map_data[z * config->map_width + x];
            if (cell == LIGHT_FLOOR_CELL) {
                if (light_count < MAX_STATIC_LIGHTS) {
                    light_positions[light_count][0] = (float)x + 0.5f;
                    light_positions[light_count][1] = CEILING_LIGHT_HEIGHT;
                    light_positions[light_count][2] = (float)z + 0.5f;

                    if (palette_count > 0) {
                        const float *palette = g_light_color_palette[light_count % palette_count];
                        light_colors[light_count][0] = palette[0] * CEILING_LIGHT_INTENSITY;
                        light_colors[light_count][1] = palette[1] * CEILING_LIGHT_INTENSITY;
                        light_colors[light_count][2] = palette[2] * CEILING_LIGHT_INTENSITY;
                    } else {
                        light_colors[light_count][0] = CEILING_LIGHT_INTENSITY;
                        light_colors[light_count][1] = CEILING_LIGHT_INTENSITY;
                        light_colors[light_count][2] = CEILING_LIGHT_INTENSITY;
                    }
                    light_count++;
                } else {
                    SDL_Log("WARNING: Max static lights reached, ignoring cell (%d,%d)", x, z);
                }
            }
        }
    }

    // Add ceiling light mesh instances
    if (config->ceiling_light_gltf_path && light_count > 0) {
        MeshData *ceiling_mesh = load_ceiling_light_mesh(config->ceiling_light_gltf_path, builder->arena);
        if (ceiling_mesh) {
            // Add ceiling light instances to the mesh
            MeshGenContext ctx = {0};
            ctx.positions = mesh->positions;
            ctx.uvs = mesh->uvs;
            ctx.normals = mesh->normals;
            ctx.surface_types = mesh->surface_types;
            ctx.triangle_ids = mesh->triangle_ids;
            ctx.indices = mesh->indices;
            ctx.position_idx = mesh->position_count;
            ctx.uv_idx = mesh->uv_count;
            ctx.normal_idx = mesh->normal_count;
            ctx.surface_idx = mesh->vertex_count;
            ctx.triangle_idx = mesh->vertex_count;
            ctx.index_idx = mesh->index_count;
            ctx.index_offset = (uint16_t)mesh->vertex_count;

            const float light_scale = CEILING_LIGHT_MODEL_SCALE;
            for (uint32_t i = 0; i < light_count; i++) {
                add_mesh_instance(&ctx, ceiling_mesh, light_scale,
                                  light_positions[i][0],
                                  light_positions[i][1],
                                  light_positions[i][2],
                                  CEILING_LIGHT_SURFACE_TYPE);
            }

            // Update mesh data
            mesh->position_count = ctx.position_idx;
            mesh->uv_count = ctx.uv_idx;
            mesh->normal_count = ctx.normal_idx;
            mesh->vertex_count = ctx.surface_idx;
            mesh->index_count = ctx.index_idx;
        }
    }

    // Convert MeshData to SceneVertex format
    builder->vertex_count = mesh->vertex_count;
    builder->index_count = mesh->index_count;
    builder->light_count = light_count;

    builder->vertices = (SceneVertex *)arena_alloc(builder->arena,
                                                    sizeof(SceneVertex) * builder->vertex_count);
    builder->indices = (uint16_t *)arena_alloc(builder->arena,
                                               sizeof(uint16_t) * builder->index_count);
    builder->lights = (SceneLight *)arena_alloc(builder->arena,
                                                sizeof(SceneLight) * builder->light_count);

    if (!builder->vertices || !builder->indices || !builder->lights) {
        SDL_Log("Failed to allocate scene buffers");
        return false;
    }

    // Convert vertices
    for (uint32_t i = 0; i < builder->vertex_count; i++) {
        SceneVertex *v = &builder->vertices[i];
        v->position[0] = mesh->positions[i * 3 + 0];
        v->position[1] = mesh->positions[i * 3 + 1];
        v->position[2] = mesh->positions[i * 3 + 2];
        v->surface_type = mesh->surface_types[i];
        v->uv[0] = mesh->uvs[i * 2 + 0];
        v->uv[1] = mesh->uvs[i * 2 + 1];
        v->normal[0] = mesh->normals[i * 3 + 0];
        v->normal[1] = mesh->normals[i * 3 + 1];
        v->normal[2] = mesh->normals[i * 3 + 2];
    }

    // Copy indices
    base_memcpy(builder->indices, mesh->indices, sizeof(uint16_t) * builder->index_count);

    // Copy lights
    for (uint32_t i = 0; i < builder->light_count; i++) {
        SceneLight *l = &builder->lights[i];
        l->position[0] = light_positions[i][0];
        l->position[1] = light_positions[i][1];
        l->position[2] = light_positions[i][2];
        l->pad0 = 0.0f;
        l->color[0] = light_colors[i][0];
        l->color[1] = light_colors[i][1];
        l->color[2] = light_colors[i][2];
        l->pad1 = 0.0f;
    }

    // Collect texture paths
    builder->texture_count = 0;
    const char *texture_paths[] = {
        config->floor_texture_path,
        config->wall_texture_path,
        config->ceiling_texture_path,
        config->window_texture_path,
        config->sphere_texture_path,
        config->book_texture_path,
        config->chair_texture_path,
        config->ceiling_light_texture_path,
    };
    uint32_t surface_ids[] = {0, 1, 2, 3, 4, 5, 6, 7};

    for (size_t i = 0; i < sizeof(texture_paths) / sizeof(texture_paths[0]); i++) {
        if (texture_paths[i]) {
            builder->texture_count++;
        }
    }

    if (builder->texture_count > 0) {
        builder->texture_paths = (const char **)arena_alloc(builder->arena,
                                                             sizeof(const char *) * builder->texture_count);
        builder->surface_type_ids = (uint32_t *)arena_alloc(builder->arena,
                                                             sizeof(uint32_t) * builder->texture_count);

        uint32_t idx = 0;
        for (size_t i = 0; i < sizeof(texture_paths) / sizeof(texture_paths[0]); i++) {
            if (texture_paths[i]) {
                builder->texture_paths[idx] = texture_paths[i];
                builder->surface_type_ids[idx] = surface_ids[i];
                idx++;
            }
        }
    }

    SDL_Log("Scene generation complete: %u vertices, %u indices, %u lights, %u textures",
            builder->vertex_count, builder->index_count, builder->light_count, builder->texture_count);

    return true;
}

uint64_t scene_builder_serialize(SceneBuilder *builder, uint8_t **out_blob) {
    if (!builder || !out_blob) {
        SDL_Log("Invalid arguments to scene_builder_serialize");
        return 0;
    }

    // Phase 1: Calculate sizes
    uint64_t vertex_size = sizeof(SceneVertex) * builder->vertex_count;
    uint64_t index_size = sizeof(uint16_t) * builder->index_count;
    uint64_t light_size = sizeof(SceneLight) * builder->light_count;
    uint64_t texture_size = sizeof(SceneTexture) * builder->texture_count;

    // Calculate string arena size (null-terminated paths)
    uint64_t string_size = 0;
    for (uint32_t i = 0; i < builder->texture_count; i++) {
        if (builder->texture_paths[i]) {
            string_size += base_strlen(builder->texture_paths[i]) + 1;
        }
    }

    uint64_t total_size = sizeof(SceneHeader) + vertex_size + index_size +
                          light_size + texture_size + string_size;

    // Allocate blob
    uint8_t *blob = (uint8_t *)arena_alloc(builder->arena, (size_t)total_size);
    if (!blob) {
        SDL_Log("Failed to allocate serialization blob");
        return 0;
    }

    // Phase 2: Write data with offsets
    SceneHeader *header = (SceneHeader *)blob;
    header->magic = SCENE_MAGIC;
    header->version = SCENE_VERSION;
    header->total_size = total_size;

    uint64_t offset = sizeof(SceneHeader);

    // Write vertices
    header->vertex_offset = offset;
    header->vertex_size = vertex_size;
    header->vertex_count = builder->vertex_count;
    base_memcpy(blob + offset, builder->vertices, (size_t)vertex_size);
    offset += vertex_size;

    // Write indices
    header->index_offset = offset;
    header->index_size = index_size;
    header->index_count = builder->index_count;
    base_memcpy(blob + offset, builder->indices, (size_t)index_size);
    offset += index_size;

    // Write lights
    header->light_offset = offset;
    header->light_size = light_size;
    header->light_count = builder->light_count;
    base_memcpy(blob + offset, builder->lights, (size_t)light_size);
    offset += light_size;

    // Write textures
    header->texture_offset = offset;
    header->texture_size = texture_size;
    header->texture_count = builder->texture_count;

    SceneTexture *textures = (SceneTexture *)(blob + offset);
    offset += texture_size;

    // Write string arena
    header->string_offset = offset;
    header->string_size = string_size;

    uint64_t string_offset_cursor = 0;
    for (uint32_t i = 0; i < builder->texture_count; i++) {
        const char *path = builder->texture_paths[i];
        if (path) {
            size_t len = base_strlen(path);
            base_memcpy(blob + offset + string_offset_cursor, path, len + 1);

            textures[i].path_offset = string_offset_cursor;
            textures[i].surface_type_id = builder->surface_type_ids[i];
            textures[i].pad = 0;

            string_offset_cursor += len + 1;
        }
    }

    *out_blob = blob;
    SDL_Log("Serialized scene: %llu bytes", (unsigned long long)total_size);
    return total_size;
}

bool scene_builder_save(SceneBuilder *builder, const char *path) {
    if (!builder || !path) {
        SDL_Log("Invalid arguments to scene_builder_save");
        return false;
    }

    uint8_t *blob = NULL;
    uint64_t size = scene_builder_serialize(builder, &blob);
    if (!blob || size == 0) {
        SDL_Log("Failed to serialize scene");
        return false;
    }

    SDL_IOStream *file = SDL_IOFromFile(path, "wb");
    if (!file) {
        SDL_Log("Failed to open file for writing: %s", path);
        return false;
    }

    size_t written = SDL_WriteIO(file, blob, size);
    SDL_CloseIO(file);

    if (written != size) {
        SDL_Log("Failed to write complete scene to file: %s", path);
        return false;
    }

    SDL_Log("Saved scene to: %s (%llu bytes)", path, (unsigned long long)size);
    return true;
}

void scene_builder_free(SceneBuilder *builder) {
    if (!builder) {
        return;
    }

    if (builder->owns_arena && builder->arena) {
        arena_free(builder->arena);
    }

    // If arena was provided externally, caller is responsible for freeing it
}
