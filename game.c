/*
 * GM-inspired world renderer using SDL3 GPU (Metal)
 *
 * Builds with:
 *   pixi r build_mousecircle_sdl
 *
 * Run with:
 *   pixi r test_mousecircle_sdl
 */

// Define __gnuc_va_list before SDL headers (needed for wchar.h on Linux with Clang)
#if defined(__clang__) || defined(__GNUC__)
#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
typedef __builtin_va_list __gnuc_va_list;
#endif
#endif

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <base/arena.h>
#include <base/buddy.h>
#include <base/mem.h>
#include <base/mat4.h>
#include <base/base_math.h>
#include <base/base_string.h>
#include <base/io.h>
#include <base/wasi.h>
#include <base/base_io.h>

#include "gm_font_data.h"

#ifndef SDL_arraysize
#define SDL_arraysize(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

// Map dimensions and generation parameters (copied from gm.h)
#define MAP_WIDTH 10
#define MAP_HEIGHT 10
#define PI 3.14159265358979323846
#define WALL_HEIGHT 2.0f
#define CHECKER_SIZE 4.0f

// Overlay layout constants
#define GLYPH_PIXEL_SIZE 1.0f
#define GLYPH_WIDTH 8
#define GLYPH_HEIGHT 8
#define GLYPH_SPACING 1.0f
#define LINE_SPACING 2.0f
#define PANEL_MARGIN 12.0f
#define TEXT_SCALE 2.0f
#define MAP_SCALE 6.0f
#define PERF_SMOOTHING 0.9f

#define MAX_SCENE_VERTICES 4096
#define MAX_OVERLAY_VERTICES 48000
#define KEY_SHIFT 16
#define KEY_CTRL 17

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

typedef struct {
    float camera_x, camera_y, camera_z;
    float yaw, pitch;
    float target_yaw, target_pitch;

    float person_height;
    float turn_speed;
    float mouse_sensitivity;
    float orientation_smoothing;
    float fov;
    float move_speed;
    float collision_radius;

    int map_visible;
    int map_relative_mode;
    int hud_visible;
    int textures_enabled;
    int triangle_mode;
    int debug_mode;
    int horizontal_movement;

    uint8_t keys[256];
    float mouse_delta_x;
    float mouse_delta_y;

    int *map_data;
    int map_width;
    int map_height;

    float fps;
    float avg_frame_time;
    float avg_js_time;
    float avg_gpu_copy_time;
    float avg_gpu_render_time;
    uint32_t frame_count;
    double last_fps_update_time;
    uint32_t fps_frame_count;
    uint32_t last_resize_id;
} GameState;

typedef struct {
    float position[3];
    float surface_type;
    float uv[2];
    float normal[3];
} MapVertex;

typedef struct {
    float position[2];
    float pad[2];
    float color[4];
} OverlayVertex;

static inline OverlayVertex overlay_vertex_make(float x, float y, const float color[4]) {
    return (OverlayVertex){
        {x, y},
        {0.0f, 0.0f},
        {color[0], color[1], color[2], color[3]},
    };
}

_Static_assert(offsetof(OverlayVertex, color) == sizeof(float) * 4, "OverlayVertex color must be 16b aligned");
_Static_assert(sizeof(OverlayVertex) == sizeof(float) * 8, "OverlayVertex unexpected size");

typedef struct {
    mat4 mvp;
    float camera_pos[4];
    float fog_color[4];
} SceneUniforms;

typedef struct {
    SDL_Window *window;
    SDL_GPUDevice *device;
    SDL_GPUGraphicsPipeline *scene_pipeline;
    SDL_GPUGraphicsPipeline *overlay_pipeline;

    SDL_GPUBuffer *scene_vertex_buffer;
    SDL_GPUBuffer *scene_index_buffer;
    SDL_GPUTransferBuffer *scene_vertex_transfer_buffer;
    SDL_GPUTransferBuffer *scene_index_transfer_buffer;

    SDL_GPUBuffer *overlay_vertex_buffer;
    SDL_GPUTransferBuffer *overlay_transfer_buffer;

    SDL_GPUTexture *depth_texture;
    SDL_GPUTexture *floor_texture;
    SDL_GPUTexture *wall_texture;
    SDL_GPUTexture *ceiling_texture;
    SDL_GPUSampler *floor_sampler;

    uint32_t scene_vertex_count;
    uint32_t scene_index_count;

    OverlayVertex overlay_cpu_vertices[MAX_OVERLAY_VERTICES];
    uint32_t overlay_vertex_count;
    bool overlay_dirty;

    SceneUniforms scene_uniforms;

    int window_width;
    int window_height;

    GameState state;

    uint32_t last_ticks;
    bool has_tick_base;
    float frame_time_ms;

    bool quit_requested;
    SDL_GPUShaderFormat shader_format;
    char scene_vertex_path[256];
    char scene_fragment_path[256];
    char overlay_vertex_path[256];
    char overlay_fragment_path[256];

    int test_frames_max;      // Max frames to run (0 = unlimited)
    int test_frames_count;    // Current frame counter

    bool export_obj_mode;     // If true, export OBJ and exit
    char export_obj_path[256]; // Output path for OBJ file
    
    bool minimal_mode;        // If true, skip GPU setup for testing
} GameApp;

static GameApp g_App;
static bool g_buddy_initialized = false;
static Arena *g_shader_arena = NULL;

#define FLOOR_TEXTURE_PATH "assets/WoodFloor007_1K-JPG_Color.jpg"
#define WALL_TEXTURE_PATH "assets/Concrete046_1K-JPG_Color.jpg"
#define CEILING_TEXTURE_PATH "assets/OfficeCeiling001_1K-JPG_Color.jpg"

static string g_scene_vertex_shader = {0};
static string g_scene_fragment_shader = {0};
static string g_overlay_vertex_shader = {0};
static string g_overlay_fragment_shader = {0};

static const char *shader_entrypoint = "main";

static void ensure_runtime_heap(void) {
    if (!g_buddy_initialized) {
        extern void ensure_heap_initialized(void);
        ensure_heap_initialized();
        buddy_init();
        g_buddy_initialized = true;
    }
    if (g_shader_arena == NULL) {
        g_shader_arena = arena_new(64 * 1024);
    }
}

static string load_shader_source(string *cache, const char *path_literal) {
    if (cache->str == NULL) {
        ensure_runtime_heap();
        string path = str_from_cstr_len_view_const(path_literal, base_strlen(path_literal));
        *cache = read_file_ok(g_shader_arena, path);
    }
    return *cache;
}

static SDL_GPUTexture *load_texture_from_path(GameApp *app, const char *path, const char *label) {
    SDL_Log("Loading %s texture from %s", label, path);

    SDL_Surface *surface = IMG_Load(path);
    if (!surface) {
        SDL_Log("Failed to load %s texture: %s", label, SDL_GetError());
        return NULL;
    }

    SDL_Log("Loaded %s texture: %dx%d, format=0x%08x, pitch=%d",
            label, surface->w, surface->h, surface->format, surface->pitch);

    if (surface->format != SDL_PIXELFORMAT_RGBA32 &&
        surface->format != SDL_PIXELFORMAT_ABGR32) {
        SDL_Log("Converting %s texture from format 0x%08x to RGBA32", label, surface->format);
        SDL_Surface *converted_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if (!converted_surface) {
            SDL_Log("Failed to convert %s texture: %s", label, SDL_GetError());
            return NULL;
        }
        surface = converted_surface;
        SDL_Log("Converted %s texture: %dx%d, format=0x%08x, pitch=%d",
                label, surface->w, surface->h, surface->format, surface->pitch);
    }

    int tex_width = surface->w;
    int tex_height = surface->h;
    Uint32 tex_data_size = (Uint32)(tex_width * tex_height * 4);

    SDL_GPUTextureCreateInfo tex_info = {
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = (Uint32)tex_width,
        .height = (Uint32)tex_height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(app->device, &tex_info);
    if (!texture) {
        SDL_Log("Failed to create %s texture: %s", label, SDL_GetError());
        SDL_DestroySurface(surface);
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = tex_data_size,
    };
    SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer(app->device, &transfer_info);
    if (!transfer_buffer) {
        SDL_Log("Failed to create %s texture transfer buffer: %s", label, SDL_GetError());
        SDL_ReleaseGPUTexture(app->device, texture);
        SDL_DestroySurface(surface);
        return NULL;
    }

    unsigned char *mapped = (unsigned char *)SDL_MapGPUTransferBuffer(app->device, transfer_buffer, false);
    if (!mapped) {
        SDL_Log("Failed to map %s texture transfer buffer: %s", label, SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(app->device, transfer_buffer);
        SDL_ReleaseGPUTexture(app->device, texture);
        SDL_DestroySurface(surface);
        return NULL;
    }
    SDL_memcpy(mapped, surface->pixels, tex_data_size);
    SDL_UnmapGPUTransferBuffer(app->device, transfer_buffer);

    SDL_DestroySurface(surface);

    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(app->device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmdbuf);

    SDL_GPUTextureTransferInfo transfer_src = {
        .transfer_buffer = transfer_buffer,
        .offset = 0,
        .pixels_per_row = (Uint32)tex_width,
        .rows_per_layer = (Uint32)tex_height,
    };

    SDL_GPUTextureRegion region = {
        .texture = texture,
        .mip_level = 0,
        .layer = 0,
        .x = 0,
        .y = 0,
        .z = 0,
        .w = (Uint32)tex_width,
        .h = (Uint32)tex_height,
        .d = 1,
    };

    SDL_UploadToGPUTexture(copy_pass, &transfer_src, &region, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmdbuf);
    SDL_ReleaseGPUTransferBuffer(app->device, transfer_buffer);

    SDL_Log("%s texture uploaded successfully", label);
    return texture;
}

static Uint32 shader_code_size(string source) {
    if (source.size == 0) {
        return 0;
    }
    // Return full size for binary shader files (SPIRV, DXIL, etc.)
    // Don't subtract 1 like we would for null-terminated text
    return (Uint32)source.size;
}

// ============================================================================
// Mesh generation helpers copied from gm.c (trimmed to necessary pieces)
// ============================================================================

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

static inline int is_solid_cell(int value) {
    return value != 0;
}

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

static float g_positions_storage[6000];
static float g_uvs_storage[4000];
static float g_normals_storage[6000];
static float g_surface_storage[2000];
static float g_triangle_storage[2000];
static uint16_t g_index_storage[6000];
static MeshData g_mesh_data_storage;
static MapVertex g_temp_vertices[MAX_SCENE_VERTICES];

static MeshData* generate_mesh(int *map, int width, int height) {
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
// Game state management (derived from gm.c, simplified)
// ============================================================================

static inline float clamp_pitch(float pitch) {
    const float max_pitch = PI / 2.0f - 0.01f;
    if (pitch < -max_pitch) {
        return -max_pitch;
    }
    if (pitch > max_pitch) {
        return max_pitch;
    }
    return pitch;
}

static inline float clampf(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static int is_walkable(const GameState *state, float x, float z) {
    int min_x = (int)(x - state->collision_radius);
    int max_x = (int)(x + state->collision_radius);
    int min_z = (int)(z - state->collision_radius);
    int max_z = (int)(z + state->collision_radius);

    for (int tz = min_z; tz <= max_z; tz++) {
        if (tz < 0 || tz >= state->map_height) {
            return 0;
        }
        for (int tx = min_x; tx <= max_x; tx++) {
            if (tx < 0 || tx >= state->map_width) {
                return 0;
            }
            int cell = state->map_data[tz * state->map_width + tx];
            if (cell != 0) {
                return 0;
            }
        }
    }
    return 1;
}

static void gm_init_game_state(GameState *state, int *map, int width, int height,
                               float start_x, float start_z, float start_yaw) {
    base_memset(state, 0, sizeof(GameState));

    state->camera_x = start_x;
    state->camera_y = 1.0f;
    state->camera_z = start_z;
    state->yaw = start_yaw;
    state->pitch = 0.0f;
    state->target_yaw = start_yaw;
    state->target_pitch = 0.0f;

    state->person_height = 1.0f;
    state->turn_speed = 0.03f;
    state->mouse_sensitivity = 0.002f;
    state->orientation_smoothing = 0.35f;
    state->fov = PI / 3.0f;
    state->move_speed = 0.12f;
    state->collision_radius = 0.2f;

    state->map_visible = 1;
    state->map_relative_mode = 0;
    state->hud_visible = 1;
    state->textures_enabled = 0;
    state->triangle_mode = 0;
    state->debug_mode = 0;
    state->horizontal_movement = 1;

    state->map_data = map;
    state->map_width = width;
    state->map_height = height;

    state->fps = 0.0f;
    state->avg_frame_time = 0.0f;
    state->avg_js_time = 0.0f;
    state->avg_gpu_copy_time = 0.0f;
    state->avg_gpu_render_time = 0.0f;
    state->frame_count = 0;
    state->last_fps_update_time = 0.0;
    state->fps_frame_count = 0;
    state->last_resize_id = 0;
}

static void gm_set_key_state(GameState *state, uint8_t key_code, int pressed) {
    state->keys[key_code] = pressed ? 1 : 0;
}

static void gm_add_mouse_delta(GameState *state, float dx, float dy) {
    state->mouse_delta_x += dx;
    state->mouse_delta_y += dy;
}

static void gm_toggle_map_visible(GameState *state) {
    state->map_visible = !state->map_visible;
}

static void gm_toggle_map_relative_mode(GameState *state) {
    state->map_relative_mode = !state->map_relative_mode;
}

static void gm_toggle_hud_visible(GameState *state) {
    state->hud_visible = !state->hud_visible;
}

static void gm_toggle_textures_enabled(GameState *state) {
    state->textures_enabled = !state->textures_enabled;
}

static void gm_toggle_triangle_mode(GameState *state) {
    state->triangle_mode = !state->triangle_mode;
}

static void gm_toggle_debug_mode(GameState *state) {
    state->debug_mode = !state->debug_mode;
}

static void gm_toggle_horizontal_movement(GameState *state) {
    state->horizontal_movement = !state->horizontal_movement;
}

static void gm_handle_key_press(GameState *state, uint8_t key_code) {
    switch (key_code) {
        case 'm':
        case 'M':
            gm_toggle_map_visible(state);
            break;
        case 'r':
        case 'R':
            gm_toggle_map_relative_mode(state);
            break;
        case 'h':
        case 'H':
            gm_toggle_hud_visible(state);
            break;
        case 't':
        case 'T':
            gm_toggle_textures_enabled(state);
            break;
        case 'i':
        case 'I':
            gm_toggle_triangle_mode(state);
            break;
        case 'b':
        case 'B':
            gm_toggle_debug_mode(state);
            break;
        case 'f':
        case 'F':
            gm_toggle_horizontal_movement(state);
            break;
        default:
            break;
    }
}

static void update_camera(GameState *state) {
    float yaw_delta = state->target_yaw - state->yaw;
    float pitch_delta = state->target_pitch - state->pitch;
    state->yaw += yaw_delta * state->orientation_smoothing;
    state->pitch += pitch_delta * state->orientation_smoothing;

    bool arrow_used = false;
    float arrow_forward = 0.0f;

    // Use SDL key codes directly (lower 8 bits match the array index)
    if (state->keys[SDLK_LEFT & 0xFF]) {
        state->target_yaw -= state->turn_speed;
        arrow_used = true;
    }
    if (state->keys[SDLK_RIGHT & 0xFF]) {
        state->target_yaw += state->turn_speed;
        arrow_used = true;
    }
    if (state->keys[SDLK_UP & 0xFF]) {
        arrow_forward += state->move_speed;
        arrow_used = true;
    }
    if (state->keys[SDLK_DOWN & 0xFF]) {
        arrow_forward -= state->move_speed;
        arrow_used = true;
    }

    state->pitch = clamp_pitch(state->pitch);
    state->target_pitch = clamp_pitch(state->target_pitch);

    float cos_yaw = fast_cos(state->yaw);
    float sin_yaw = fast_sin(state->yaw);

    float forward_x = cos_yaw;
    float forward_z = sin_yaw;
    float right_x = -sin_yaw;
    float right_z = cos_yaw;

    float speed_multiplier = state->keys[KEY_SHIFT] ? 2.0f : 1.0f;
    float base_speed = state->move_speed * speed_multiplier;

    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;

    if (state->keys['w']) {
        dx += forward_x * base_speed;
        dz += forward_z * base_speed;
    }
    if (state->keys['s']) {
        dx -= forward_x * base_speed;
        dz -= forward_z * base_speed;
    }
    if (state->keys['a']) {
        dx -= right_x * base_speed;
        dz -= right_z * base_speed;
    }
    if (state->keys['d']) {
        dx += right_x * base_speed;
        dz += right_z * base_speed;
    }

    if (state->keys[' ']) {
        dy += base_speed;
    }
    if (state->keys[KEY_CTRL]) {
        dy -= base_speed;
    }

    if (arrow_forward != 0.0f) {
        dx += forward_x * arrow_forward;
        dz += forward_z * arrow_forward;
    }

    float candidate_x = state->camera_x + dx;
    float candidate_z = state->camera_z + dz;

    if (is_walkable(state, candidate_x, state->camera_z)) {
        state->camera_x = candidate_x;
    }
    if (is_walkable(state, state->camera_x, candidate_z)) {
        state->camera_z = candidate_z;
    }

    float base_y = arrow_used ? state->person_height : state->camera_y;
    float new_y = base_y + dy;
    state->camera_y = clampf(new_y, 0.2f, WALL_HEIGHT - 0.2f);
    if (arrow_used) {
        state->camera_y = state->person_height;
    }
}

static void gm_update_frame(GameState *state, float canvas_width, float canvas_height) {
    (void)canvas_width;
    (void)canvas_height;

    state->target_yaw += state->mouse_delta_x * state->mouse_sensitivity;
    state->target_pitch = clamp_pitch(state->target_pitch - state->mouse_delta_y * state->mouse_sensitivity);

    state->mouse_delta_x = 0.0f;
    state->mouse_delta_y = 0.0f;

    update_camera(state);
    state->frame_count++;
}

// ============================================================================
// Map and starting position
// ============================================================================

static const int g_default_map[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1},
    {1,7,0,0,0,0,0,0,0,1},
    {1,0,1,2,1,0,2,0,0,1},
    {1,0,1,0,0,0,0,1,0,1},
    {1,0,1,0,1,0,0,0,0,1},
    {1,0,1,0,3,0,1,0,0,1},
    {1,0,3,0,1,1,0,0,1,1},
    {1,0,1,0,1,0,0,0,0,1},
    {1,0,0,0,1,0,0,1,0,1},
    {1,1,1,1,1,1,1,1,1,1}
};

static int g_map_data[MAP_WIDTH * MAP_HEIGHT];

static int find_start_position(int *map, int width, int height,
                               float *startX, float *startZ, float *startYaw) {
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            int cell = map[z * width + x];
            if (cell >= 5 && cell <= 8) {
                *startX = (float)x + 0.5f;
                *startZ = (float)z + 0.5f;
                if (cell == 5) {
                    *startYaw = -PI / 2.0f;
                } else if (cell == 6) {
                    *startYaw = 0.0f;
                } else if (cell == 7) {
                    *startYaw = PI / 2.0f;
                } else {
                    *startYaw = PI;
                }
                map[z * width + x] = 0;
                return 1;
            }
        }
    }
    return 0;
}

// ============================================================================
// OBJ export functions
// ============================================================================

// Helper function to write string to file
static bool write_string_to_file(const char *filename, const char *data, size_t length) {
    wasi_fd_t fd = wasi_path_open(filename, base_strlen(filename),
                                    WASI_RIGHTS_WRITE,
                                    WASI_O_CREAT | WASI_O_TRUNC);
    if (fd < 0) {
        SDL_Log("Failed to open file for writing: %s", filename);
        return false;
    }

    ciovec_t iov = { .buf = data, .buf_len = length };
    size_t written;
    uint32_t result = wasi_fd_write(fd, &iov, 1, &written);
    wasi_fd_close(fd);

    if (result != 0 || written != length) {
        SDL_Log("Failed to write to file: %s", filename);
        return false;
    }
    return true;
}

// Simple float to string conversion (enough for OBJ format)
static int float_to_str(float value, char *buf, int buf_size) {
    // Handle negative values
    int pos = 0;
    if (value < 0) {
        buf[pos++] = '-';
        value = -value;
    }

    // Integer part
    int int_part = (int)value;
    float frac_part = value - (float)int_part;

    // Convert integer part
    char temp[32];
    int temp_pos = 0;
    if (int_part == 0) {
        temp[temp_pos++] = '0';
    } else {
        while (int_part > 0 && temp_pos < 32) {
            temp[temp_pos++] = '0' + (int_part % 10);
            int_part /= 10;
        }
    }
    // Reverse temp into buf
    for (int i = temp_pos - 1; i >= 0; i--) {
        if (pos < buf_size - 1) buf[pos++] = temp[i];
    }

    // Decimal point and fractional part (6 digits)
    if (pos < buf_size - 1) buf[pos++] = '.';
    for (int i = 0; i < 6 && pos < buf_size - 1; i++) {
        frac_part *= 10.0f;
        int digit = (int)frac_part;
        buf[pos++] = '0' + digit;
        frac_part -= (float)digit;
    }

    buf[pos] = '\0';
    return pos;
}

// Simple integer to string conversion
static int int_to_str(int value, char *buf, int buf_size) {
    int pos = 0;
    if (value == 0) {
        buf[pos++] = '0';
        buf[pos] = '\0';
        return pos;
    }

    if (value < 0) {
        buf[pos++] = '-';
        value = -value;
    }

    char temp[32];
    int temp_pos = 0;
    while (value > 0 && temp_pos < 32) {
        temp[temp_pos++] = '0' + (value % 10);
        value /= 10;
    }

    // Reverse into buf
    for (int i = temp_pos - 1; i >= 0 && pos < buf_size - 1; i--) {
        buf[pos++] = temp[i];
    }
    buf[pos] = '\0';
    return pos;
}

// Export MTL material file
static bool export_mtl_file(const char *obj_filename) {
    // Create MTL filename from OBJ filename
    char mtl_filename[512];
    int i = 0;
    while (obj_filename[i] && i < 500) {
        mtl_filename[i] = obj_filename[i];
        i++;
    }
    // Replace .obj with .mtl
    if (i >= 4 && mtl_filename[i-4] == '.') {
        mtl_filename[i-3] = 'm';
        mtl_filename[i-2] = 't';
        mtl_filename[i-1] = 'l';
    } else {
        mtl_filename[i++] = '.';
        mtl_filename[i++] = 'm';
        mtl_filename[i++] = 't';
        mtl_filename[i++] = 'l';
    }
    mtl_filename[i] = '\0';

    // Build MTL content
    char mtl_content[2048];
    int pos = 0;

    // Floor material
    const char *floor_mtl = "newmtl floor\nKd 1.0 1.0 1.0\nmap_Kd " FLOOR_TEXTURE_PATH "\n\n";
    for (int j = 0; floor_mtl[j] && pos < sizeof(mtl_content) - 1; j++) {
        mtl_content[pos++] = floor_mtl[j];
    }

    // Wall material
    const char *wall_mtl = "newmtl wall\nKd 1.0 1.0 1.0\nmap_Kd " WALL_TEXTURE_PATH "\n\n";
    for (int j = 0; wall_mtl[j] && pos < sizeof(mtl_content) - 1; j++) {
        mtl_content[pos++] = wall_mtl[j];
    }

    // Ceiling material
    const char *ceiling_mtl = "newmtl ceiling\nKd 1.0 1.0 1.0\nmap_Kd " CEILING_TEXTURE_PATH "\n";
    for (int j = 0; ceiling_mtl[j] && pos < sizeof(mtl_content) - 1; j++) {
        mtl_content[pos++] = ceiling_mtl[j];
    }

    mtl_content[pos] = '\0';

    bool success = write_string_to_file(mtl_filename, mtl_content, pos);
    if (success) {
        SDL_Log("Exported MTL file: %s", mtl_filename);
    }
    return success;
}

// Export mesh to OBJ file
static bool export_mesh_to_obj(MeshData *mesh, const char *filename) {
    if (!mesh || !filename) {
        SDL_Log("export_mesh_to_obj: invalid arguments");
        return false;
    }

    SDL_Log("Exporting mesh to OBJ: %s", filename);
    SDL_Log("Vertices: %u, Indices: %u", mesh->vertex_count, mesh->index_count);

    // Allocate a buffer for the OBJ content using buddy allocator
    // Estimate: ~100 bytes per vertex (v, vt, vn) + ~50 bytes per face
    size_t estimated_size = mesh->vertex_count * 100 + mesh->index_count * 50 / 3;
    if (estimated_size < 64 * 1024) estimated_size = 64 * 1024;  // Minimum 64KB

    ensure_runtime_heap();
    char *obj_buffer = (char *)buddy_alloc(estimated_size);
    if (!obj_buffer) {
        SDL_Log("Failed to allocate memory for OBJ export (needed %zu bytes)", estimated_size);
        return false;
    }

    int pos = 0;

    // Write MTL reference
    const char *mtllib = "# Generated by game.c\nmtllib ";
    for (int i = 0; mtllib[i]; i++) {
        obj_buffer[pos++] = mtllib[i];
    }

    // Extract basename from filename for mtl reference
    int basename_start = 0;
    for (int i = 0; filename[i]; i++) {
        if (filename[i] == '/' || filename[i] == '\\') {
            basename_start = i + 1;
        }
    }
    for (int i = basename_start; filename[i] && pos < (int)estimated_size - 100; i++) {
        obj_buffer[pos++] = filename[i];
    }
    // Replace .obj with .mtl
    if (pos >= 4 && obj_buffer[pos-4] == '.') {
        obj_buffer[pos-3] = 'm';
        obj_buffer[pos-2] = 't';
        obj_buffer[pos-1] = 'l';
    } else {
        obj_buffer[pos++] = '.';
        obj_buffer[pos++] = 'm';
        obj_buffer[pos++] = 't';
        obj_buffer[pos++] = 'l';
    }
    obj_buffer[pos++] = '\n';
    obj_buffer[pos++] = '\n';

    // Write vertices
    for (uint32_t i = 0; i < mesh->vertex_count && pos < (int)estimated_size - 200; i++) {
        obj_buffer[pos++] = 'v';
        obj_buffer[pos++] = ' ';

        char num_buf[64];
        int len;

        len = float_to_str(mesh->positions[i * 3 + 0], num_buf, sizeof(num_buf));
        for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
        obj_buffer[pos++] = ' ';

        len = float_to_str(mesh->positions[i * 3 + 1], num_buf, sizeof(num_buf));
        for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
        obj_buffer[pos++] = ' ';

        len = float_to_str(mesh->positions[i * 3 + 2], num_buf, sizeof(num_buf));
        for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
        obj_buffer[pos++] = '\n';
    }
    obj_buffer[pos++] = '\n';

    // Write texture coordinates
    for (uint32_t i = 0; i < mesh->vertex_count && pos < (int)estimated_size - 200; i++) {
        obj_buffer[pos++] = 'v';
        obj_buffer[pos++] = 't';
        obj_buffer[pos++] = ' ';

        char num_buf[64];
        int len;

        len = float_to_str(mesh->uvs[i * 2 + 0], num_buf, sizeof(num_buf));
        for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
        obj_buffer[pos++] = ' ';

        len = float_to_str(mesh->uvs[i * 2 + 1], num_buf, sizeof(num_buf));
        for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
        obj_buffer[pos++] = '\n';
    }
    obj_buffer[pos++] = '\n';

    // Write normals
    for (uint32_t i = 0; i < mesh->vertex_count && pos < (int)estimated_size - 200; i++) {
        obj_buffer[pos++] = 'v';
        obj_buffer[pos++] = 'n';
        obj_buffer[pos++] = ' ';

        char num_buf[64];
        int len;

        len = float_to_str(mesh->normals[i * 3 + 0], num_buf, sizeof(num_buf));
        for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
        obj_buffer[pos++] = ' ';

        len = float_to_str(mesh->normals[i * 3 + 1], num_buf, sizeof(num_buf));
        for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
        obj_buffer[pos++] = ' ';

        len = float_to_str(mesh->normals[i * 3 + 2], num_buf, sizeof(num_buf));
        for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
        obj_buffer[pos++] = '\n';
    }
    obj_buffer[pos++] = '\n';

    // Write faces (grouped by material based on surface_type)
    // First pass: floor faces (surface_type = 0.0)
    const char *use_floor = "usemtl floor\n";
    for (int i = 0; use_floor[i]; i++) obj_buffer[pos++] = use_floor[i];

    for (uint32_t i = 0; i < mesh->index_count && pos < (int)estimated_size - 300; i += 3) {
        uint16_t i0 = mesh->indices[i + 0];
        uint16_t i1 = mesh->indices[i + 1];
        uint16_t i2 = mesh->indices[i + 2];

        // Check if this face is floor
        float st0 = mesh->surface_types[i0];
        if (st0 < 0.5f) {  // floor
            obj_buffer[pos++] = 'f';
            obj_buffer[pos++] = ' ';

            char num_buf[64];
            int len;

            // Vertex 0 (OBJ indices are 1-based)
            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = ' ';

            // Vertex 1
            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = ' ';

            // Vertex 2
            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '\n';
        }
    }
    obj_buffer[pos++] = '\n';

    // Second pass: wall faces (surface_type = 1.0)
    const char *use_wall = "usemtl wall\n";
    for (int i = 0; use_wall[i]; i++) obj_buffer[pos++] = use_wall[i];

    for (uint32_t i = 0; i < mesh->index_count && pos < (int)estimated_size - 300; i += 3) {
        uint16_t i0 = mesh->indices[i + 0];
        uint16_t i1 = mesh->indices[i + 1];
        uint16_t i2 = mesh->indices[i + 2];

        float st0 = mesh->surface_types[i0];
        if (st0 >= 0.5f && st0 < 1.5f) {  // wall
            obj_buffer[pos++] = 'f';
            obj_buffer[pos++] = ' ';

            char num_buf[64];
            int len;

            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = ' ';

            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = ' ';

            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '\n';
        }
    }
    obj_buffer[pos++] = '\n';

    // Third pass: ceiling faces (surface_type = 2.0)
    const char *use_ceiling = "usemtl ceiling\n";
    for (int i = 0; use_ceiling[i]; i++) obj_buffer[pos++] = use_ceiling[i];

    for (uint32_t i = 0; i < mesh->index_count && pos < (int)estimated_size - 300; i += 3) {
        uint16_t i0 = mesh->indices[i + 0];
        uint16_t i1 = mesh->indices[i + 1];
        uint16_t i2 = mesh->indices[i + 2];

        float st0 = mesh->surface_types[i0];
        if (st0 >= 1.5f) {  // ceiling
            obj_buffer[pos++] = 'f';
            obj_buffer[pos++] = ' ';

            char num_buf[64];
            int len;

            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i0 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = ' ';

            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i1 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = ' ';

            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '/';
            len = int_to_str(i2 + 1, num_buf, sizeof(num_buf));
            for (int j = 0; j < len; j++) obj_buffer[pos++] = num_buf[j];
            obj_buffer[pos++] = '\n';
        }
    }

    obj_buffer[pos] = '\0';

    SDL_Log("Generated OBJ content: %d bytes", pos);

    // Write to file
    bool success = write_string_to_file(filename, obj_buffer, pos);
    if (success) {
        SDL_Log("Successfully exported OBJ file: %s", filename);
    }

    // Free the allocated buffer
    buddy_free(obj_buffer);

    return success;
}

// ============================================================================
// Overlay vertex generation
// ============================================================================

typedef struct {
    float x0, y0;
    float x1, y1;
} Rect;

static inline float to_clip_x(float pixel_x, float width) {
    if (width <= 0.0f) {
        SDL_Log("ERROR: to_clip_x called with invalid width %.2f", width);
        return 0.0f;
    }
    return (pixel_x / width) * 2.0f - 1.0f;
}

static inline float to_clip_y(float pixel_y, float height) {
    if (height <= 0.0f) {
        SDL_Log("ERROR: to_clip_y called with invalid height %.2f", height);
        return 0.0f;
    }
    return 1.0f - (pixel_y / height) * 2.0f;
}

static uint32_t append_quad(OverlayVertex *verts, uint32_t offset, uint32_t max,
                            float x0, float y0, float x1, float y1, const float color[4]) {
    if (offset + 6 > max) {
        SDL_Log("append_quad: buffer full at offset %u", offset);
        return offset;
    }

    // Sanity check coordinates - overlay quads should be small (text pixels or minimap cells)
    // A quad larger than 0.5 in clip space is suspicious (that's 1/4 of the screen)
    float quad_width = (x1 > x0 ? x1 - x0 : x0 - x1);
    float quad_height = (y1 > y0 ? y1 - y0 : y0 - y1);
    if (quad_width > 0.5f || quad_height > 0.5f) {
        SDL_Log("WARNING: Large quad at offset %u: (%.2f,%.2f) to (%.2f,%.2f), size %.2fx%.2f",
                offset, x0, y0, x1, y1, quad_width, quad_height);
    }

    verts[offset + 0] = overlay_vertex_make(x0, y0, color);
    verts[offset + 1] = overlay_vertex_make(x1, y0, color);
    verts[offset + 2] = overlay_vertex_make(x0, y1, color);
    verts[offset + 3] = overlay_vertex_make(x1, y0, color);
    verts[offset + 4] = overlay_vertex_make(x1, y1, color);
    verts[offset + 5] = overlay_vertex_make(x0, y1, color);
    return offset + 6;
}

static uint32_t append_convex_quad(OverlayVertex *verts, uint32_t offset, uint32_t max,
                                   const float points[4][2], const float color[4]) {
    if (offset + 6 > max) {
        return offset;
    }

    // Check for excessively large quads
    float minX = points[0][0], maxX = points[0][0];
    float minY = points[0][1], maxY = points[0][1];
    for (int i = 1; i < 4; i++) {
        if (points[i][0] < minX) minX = points[i][0];
        if (points[i][0] > maxX) maxX = points[i][0];
        if (points[i][1] < minY) minY = points[i][1];
        if (points[i][1] > maxY) maxY = points[i][1];
    }
    float width = maxX - minX;
    float height = maxY - minY;

    if (width > 0.5f || height > 0.5f) {
        SDL_Log("WARNING: Large convex_quad at offset %u: bounds (%.2f,%.2f) to (%.2f,%.2f), size %.2fx%.2f",
                offset, minX, minY, maxX, maxY, width, height);
    }

    static const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) {
        int idx = order[i];
        verts[offset + i] = overlay_vertex_make(points[idx][0], points[idx][1], color);
    }
    return offset + 6;
}

static uint32_t append_triangle(OverlayVertex *verts, uint32_t offset, uint32_t max,
                                const float points[3][2], const float color[4]) {
    if (offset + 3 > max) {
        return offset;
    }

    // Check for excessively large triangles
    float minX = points[0][0], maxX = points[0][0];
    float minY = points[0][1], maxY = points[0][1];
    for (int i = 1; i < 3; i++) {
        if (points[i][0] < minX) minX = points[i][0];
        if (points[i][0] > maxX) maxX = points[i][0];
        if (points[i][1] < minY) minY = points[i][1];
        if (points[i][1] > maxY) maxY = points[i][1];
    }
    float width = maxX - minX;
    float height = maxY - minY;

    // Log if we're near the problematic offset
    if (offset >= 7920 && offset <= 7940) {
        SDL_Log("append_triangle at offset %u: v1=(%.2f,%.2f) v2=(%.2f,%.2f) v3=(%.2f,%.2f)",
                offset, points[0][0], points[0][1], points[1][0], points[1][1], points[2][0], points[2][1]);
    }

    if (width > 0.5f || height > 0.5f) {
        SDL_Log("WARNING: Large triangle at offset %u: bounds (%.2f,%.2f) to (%.2f,%.2f), size %.2fx%.2f",
                offset, minX, minY, maxX, maxY, width, height);
        SDL_Log("  v1=(%.2f,%.2f) v2=(%.2f,%.2f) v3=(%.2f,%.2f)",
                points[0][0], points[0][1], points[1][0], points[1][1], points[2][0], points[2][1]);
    }

    for (int i = 0; i < 3; i++) {
        verts[offset + i] = overlay_vertex_make(points[i][0], points[i][1], color);
    }
    return offset + 3;
}

static uint32_t append_glyph(OverlayVertex *verts, uint32_t offset, uint32_t max,
                             float origin_x, float origin_y, float scale,
                             float canvas_w, float canvas_h, unsigned char ch,
                             const float color[4]) {
    const uint32_t *glyph = GM_FONT_GLYPHS[ch];

    for (int row = 0; row < GLYPH_HEIGHT; row++) {
        uint32_t row_bits = glyph[row];
        for (int col = 0; col < GLYPH_WIDTH; col++) {
            uint32_t mask = 1u << col;
            if (row_bits & mask) {
                float px0 = origin_x + (float)col * scale;
                float py0 = origin_y + (float)row * scale;
                float px1 = px0 + scale;
                float py1 = py0 + scale;
                float x0 = to_clip_x(px0, canvas_w);
                float y0 = to_clip_y(py0, canvas_h);
                float x1 = to_clip_x(px1, canvas_w);
                float y1 = to_clip_y(py1, canvas_h);

                offset = append_quad(verts, offset, max, x0, y0, x1, y1, color);
            }
        }
    }
    return offset;
}

static uint32_t append_text_line(GameApp *app, OverlayVertex *verts, uint32_t offset,
                                 const char *text, float start_x, float start_y,
                                 float scale, float canvas_w, float canvas_h,
                                 const float color[4]) {
    for (const char *ptr = text; *ptr; ptr++) {
        unsigned char ch = (unsigned char)*ptr;
        offset = append_glyph(verts, offset, MAX_OVERLAY_VERTICES,
                              start_x, start_y, scale, canvas_w, canvas_h, ch, color);
        start_x += (GLYPH_WIDTH + GLYPH_SPACING) * scale;
    }
    return offset;
}

static const char *direction_from_yaw(float yaw) {
    static const char *names[] = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
    float angle = yaw;
    float two_pi = (float)(2.0f * PI);
    while (angle < 0.0f) {
        angle += two_pi;
    }
    while (angle >= two_pi) {
        angle -= two_pi;
    }
    int index = (int)((angle + (float)(PI / 8.0f)) / (float)(PI / 4.0f)) & 7;
    return names[index];
}

static void build_overlay(GameApp *app) {
    GameState *state = &app->state;
    app->overlay_vertex_count = 0;

    if (!state->hud_visible) {
        app->overlay_dirty = true;
        return;
    }

    float canvas_w = (float)app->window_width;
    float canvas_h = (float)app->window_height;
    float scale = TEXT_SCALE;
    float origin_x = PANEL_MARGIN;
    float origin_y = PANEL_MARGIN;
    float line_height = (GLYPH_HEIGHT + LINE_SPACING) * scale;
    const float text_color[4] = {1.0f, 1.0f, 1.0f, 0.95f};
    const float map_wall_color[4] = {0.8f, 0.2f, 0.2f, 0.9f};
    const float map_floor_color[4] = {0.1f, 0.1f, 0.15f, 0.8f};
    const float map_player_color[4] = {0.2f, 0.9f, 0.3f, 1.0f};

    char lines[5][96];
    SDL_snprintf(lines[0], sizeof(lines[0]), "FPS %d  FRAME %.2fMS",
                 (int)(state->fps + 0.5f), state->avg_frame_time);
    SDL_snprintf(lines[1], sizeof(lines[1]), "POS X %.2f Y %.2f Z %.2f",
                 state->camera_x, state->camera_y, state->camera_z);
    float yaw_deg = state->yaw * 180.0f / (float)PI;
    float pitch_deg = state->pitch * 180.0f / (float)PI;
    SDL_snprintf(lines[2], sizeof(lines[2]), "DIR %s Y %.1f P %.1f",
                 direction_from_yaw(state->yaw), yaw_deg, pitch_deg);
    SDL_snprintf(lines[3], sizeof(lines[3]), "MODE %s MAP %s HUD %s",
                 state->horizontal_movement ? "WALK" : "FLY",
                 state->map_visible ? "ON" : "OFF",
                 state->hud_visible ? "ON" : "OFF");
    SDL_snprintf(lines[4], sizeof(lines[4]), "TOGGLE M/R/H/T/I/B/F");

    uint32_t offset = 0;
    for (int i = 0; i < 5; i++) {
        char *line = lines[i];
        for (char *p = line; *p; ++p) {
            if (*p >= 'a' && *p <= 'z') {
                *p = (char)(*p - 32);
            }
        }

        float cursor_x = origin_x;
        for (const char *p = line; *p; ++p) {
            offset = append_glyph(app->overlay_cpu_vertices, offset, MAX_OVERLAY_VERTICES,
                                  cursor_x, origin_y + i * line_height,
                                  scale, canvas_w, canvas_h,
                                  (unsigned char)*p, text_color);
            cursor_x += (GLYPH_WIDTH + GLYPH_SPACING) * scale;
        }
    }

    if (state->map_visible) {
        float max_cells = (float)((state->map_width > state->map_height) ? state->map_width : state->map_height);
        float map_pixel_size = max_cells * MAP_SCALE;
        float map_origin_x = state->map_relative_mode ? PANEL_MARGIN : canvas_w - map_pixel_size - PANEL_MARGIN;
        float map_origin_y = PANEL_MARGIN + line_height * 6.0f;
        float map_center_x = map_origin_x + map_pixel_size * 0.5f;
        float map_center_y = map_origin_y + map_pixel_size * 0.5f;

        float cos_yaw = fast_cos(state->yaw);
        float sin_yaw = fast_sin(state->yaw);
        const float corner_offsets[4][2] = {
            {-0.5f, -0.5f},
            { 0.5f, -0.5f},
            { 0.5f,  0.5f},
            {-0.5f,  0.5f}
        };

        for (int z = 0; z < state->map_height; z++) {
            for (int x = 0; x < state->map_width; x++) {
                int cell = state->map_data[z * state->map_width + x];
                const float *color = cell ? map_wall_color : map_floor_color;

                float cell_dx = ((float)x + 0.5f) - state->camera_x;
                float cell_dz = ((float)z + 0.5f) - state->camera_z;

                float points_clip[4][2];
                for (int c = 0; c < 4; c++) {
                    float corner_dx = cell_dx + corner_offsets[c][0];
                    float corner_dz = cell_dz + corner_offsets[c][1];

                    float forward = corner_dx * cos_yaw + corner_dz * sin_yaw;
                    float right = -corner_dx * sin_yaw + corner_dz * cos_yaw;

                    float screen_x = map_center_x + right * MAP_SCALE;
                    float screen_y = map_center_y - forward * MAP_SCALE;

                    points_clip[c][0] = to_clip_x(screen_x, canvas_w);
                    points_clip[c][1] = to_clip_y(screen_y, canvas_h);
                }

                offset = append_convex_quad(app->overlay_cpu_vertices, offset, MAX_OVERLAY_VERTICES,
                                             points_clip, color);
            }
        }

        const float arrow_shape[3][2] = {
            { 0.8f,  0.0f},
            {-0.4f, -0.4f},
            { 0.4f, -0.4f},
        };
        float arrow_points[3][2];
        for (int i = 0; i < 3; i++) {
            float forward = arrow_shape[i][0];
            float right = arrow_shape[i][1];
            float screen_x = map_center_x + right * MAP_SCALE;
            float screen_y = map_center_y - forward * MAP_SCALE;
            arrow_points[i][0] = to_clip_x(screen_x, canvas_w);
            arrow_points[i][1] = to_clip_y(screen_y, canvas_h);
        }
        offset = append_triangle(app->overlay_cpu_vertices, offset, MAX_OVERLAY_VERTICES,
                                 arrow_points, map_player_color);

        struct LabelInfo { char label; float dx; float dz; };
        const struct LabelInfo label_info[4] = {
            {'N', 0.0f, -1.0f},
            {'S', 0.0f,  1.0f},
            {'E', 1.0f,  0.0f},
            {'W', -1.0f, 0.0f},
        };
        float letter_offset_cells = max_cells * 0.5f + 0.8f;
        float glyph_w = (float)GLYPH_WIDTH * TEXT_SCALE;
        float glyph_h = (float)GLYPH_HEIGHT * TEXT_SCALE;
        for (int i = 0; i < 4; i++) {
            float dx = label_info[i].dx * letter_offset_cells;
            float dz = label_info[i].dz * letter_offset_cells;
            float forward = dx * cos_yaw + dz * sin_yaw;
            float right = -dx * sin_yaw + dz * cos_yaw;
            float glyph_x = map_center_x + right * MAP_SCALE - glyph_w * 0.5f;
            float glyph_y = map_center_y - forward * MAP_SCALE - glyph_h * 0.5f;
            offset = append_glyph(app->overlay_cpu_vertices, offset, MAX_OVERLAY_VERTICES,
                                  glyph_x, glyph_y, TEXT_SCALE, canvas_w, canvas_h,
                                  (unsigned char)label_info[i].label, text_color);
        }
    }

    app->overlay_vertex_count = offset;
    app->overlay_dirty = true;
}

// ============================================================================
// Shader sources (Metal Shading Language)
// ============================================================================

// ============================================================================
// GPU resource helpers
// ============================================================================

static bool create_scene_pipeline(GameApp *app, SDL_GPUShader *vertex_shader, SDL_GPUShader *fragment_shader) {
    SDL_GPUVertexAttribute attributes[] = {
        {.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0},
        {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, .offset = sizeof(float) * 3},
        {.location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = sizeof(float) * 4},
        {.location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = sizeof(float) * 6},
    };

    SDL_GPUVertexBufferDescription buffer_desc = {
        .slot = 0,
        .pitch = sizeof(MapVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };

    SDL_GPUVertexInputState vertex_state = {
        .vertex_buffer_descriptions = &buffer_desc,
        .num_vertex_buffers = 1,
        .vertex_attributes = attributes,
        .num_vertex_attributes = SDL_arraysize(attributes),
    };

    SDL_GPUDepthStencilState depth_state = {
        .enable_depth_test = true,
        .enable_depth_write = true,
        .enable_stencil_test = false,
        .compare_op = SDL_GPU_COMPAREOP_LESS,
        .back_stencil_state = {
            .fail_op = SDL_GPU_STENCILOP_KEEP,
            .pass_op = SDL_GPU_STENCILOP_KEEP,
            .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
        },
        .front_stencil_state = {
            .fail_op = SDL_GPU_STENCILOP_KEEP,
            .pass_op = SDL_GPU_STENCILOP_KEEP,
            .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
        },
        .compare_mask = 0,
        .write_mask = 0,
    };

    SDL_GPURasterizerState rasterizer_state = {
        .fill_mode = SDL_GPU_FILLMODE_FILL,
        .cull_mode = SDL_GPU_CULLMODE_BACK,
        .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        .depth_bias_constant_factor = 0.0f,
        .depth_bias_clamp = 0.0f,
        .depth_bias_slope_factor = 0.0f,
        .enable_depth_bias = false,
        .enable_depth_clip = false,
    };

    SDL_GPUMultisampleState multisample_state = {
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .sample_mask = 0,
        .enable_mask = false,
        .enable_alpha_to_coverage = false,
    };

    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = vertex_state,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = rasterizer_state,
        .multisample_state = multisample_state,
        .depth_stencil_state = depth_state,
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){
                {
                    .format = SDL_GetGPUSwapchainTextureFormat(app->device, app->window),
                    .blend_state = {
                        .enable_blend = false,
                        .enable_color_write_mask = false,
                        .color_write_mask = 0,
                        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
                        .color_blend_op = SDL_GPU_BLENDOP_ADD,
                        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
                        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                    },
                }
            },
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .has_depth_stencil_target = true,
        },
        .props = 0,
    };

    app->scene_pipeline = SDL_CreateGPUGraphicsPipeline(app->device, &info);
    return app->scene_pipeline != NULL;
}

static bool create_overlay_pipeline(GameApp *app, SDL_GPUShader *vertex_shader, SDL_GPUShader *fragment_shader) {
    SDL_GPUVertexAttribute attributes[] = {
        {.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(OverlayVertex, position)},
        {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = offsetof(OverlayVertex, color)},
    };

    SDL_GPUVertexBufferDescription buffer_desc = {
        .slot = 0,
        .pitch = sizeof(OverlayVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };

    SDL_GPUVertexInputState vertex_state = {
        .vertex_buffer_descriptions = &buffer_desc,
        .num_vertex_buffers = 1,
        .vertex_attributes = attributes,
        .num_vertex_attributes = SDL_arraysize(attributes),
    };

    SDL_GPUDepthStencilState depth_state = {
        .enable_depth_test = false,
        .enable_depth_write = false,
        .enable_stencil_test = false,
        .compare_op = SDL_GPU_COMPAREOP_LESS,
        .back_stencil_state = {
            .fail_op = SDL_GPU_STENCILOP_KEEP,
            .pass_op = SDL_GPU_STENCILOP_KEEP,
            .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
        },
        .front_stencil_state = {
            .fail_op = SDL_GPU_STENCILOP_KEEP,
            .pass_op = SDL_GPU_STENCILOP_KEEP,
            .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
        },
        .compare_mask = 0,
        .write_mask = 0,
    };

    SDL_GPURasterizerState rasterizer_state = {
        .fill_mode = SDL_GPU_FILLMODE_FILL,
        .cull_mode = SDL_GPU_CULLMODE_NONE,
        .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        .depth_bias_constant_factor = 0.0f,
        .depth_bias_clamp = 0.0f,
        .depth_bias_slope_factor = 0.0f,
        .enable_depth_bias = false,
        .enable_depth_clip = false,
    };

    SDL_GPUMultisampleState multisample_state = {
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .sample_mask = 0,
        .enable_mask = false,
        .enable_alpha_to_coverage = false,
    };

    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = vertex_state,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = rasterizer_state,
        .multisample_state = multisample_state,
        .depth_stencil_state = depth_state,
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){
                {
                    .format = SDL_GetGPUSwapchainTextureFormat(app->device, app->window),
                    .blend_state = {
                        .enable_blend = true,
                        .enable_color_write_mask = false,
                        .color_write_mask = 0,
                        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                        .color_blend_op = SDL_GPU_BLENDOP_ADD,
                        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
                        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                    },
                }
            },
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .has_depth_stencil_target = true,
        },
        .props = 0,
    };

    app->overlay_pipeline = SDL_CreateGPUGraphicsPipeline(app->device, &info);
    return app->overlay_pipeline != NULL;
}

static int complete_gpu_setup(GameApp *app) {
    if (app->device == NULL || app->window == NULL) {
        SDL_Log("GPU setup requires valid device and window");
        return -1;
    }

    if (app->depth_texture == NULL) {
        SDL_GPUTextureCreateInfo depth_info = {
            .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
            .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .width = (Uint32)app->window_width,
            .height = (Uint32)app->window_height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
        };
        app->depth_texture = SDL_CreateGPUTexture(app->device, &depth_info);
        if (app->depth_texture == NULL) {
            SDL_Log("Failed to create depth texture: %s", SDL_GetError());
            return -1;
        }
    }

    string scene_vs_code = load_shader_source(&g_scene_vertex_shader, app->scene_vertex_path);
    SDL_Log("Loaded scene vertex shader: %zu bytes from %s", scene_vs_code.size, app->scene_vertex_path);
    SDL_GPUShaderCreateInfo shader_info = {
        .code = (const Uint8 *)scene_vs_code.str,
        .code_size = shader_code_size(scene_vs_code),
        .entrypoint = shader_entrypoint,
        .format = app->shader_format,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_uniform_buffers = 1,
        .num_storage_buffers = 0,
        .num_storage_textures = 0,
    };

    SDL_GPUShader *scene_vs = SDL_CreateGPUShader(app->device, &shader_info);
    if (!scene_vs) {
        SDL_Log("Failed to create scene vertex shader: %s", SDL_GetError());
        return -1;
    }
    SDL_Log("Created scene vertex shader successfully");

    string scene_fs_code = load_shader_source(&g_scene_fragment_shader, app->scene_fragment_path);
    shader_info.code = (const Uint8 *)scene_fs_code.str;
    shader_info.code_size = shader_code_size(scene_fs_code);
    shader_info.entrypoint = shader_entrypoint;
    shader_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shader_info.num_samplers = 3;
    shader_info.num_uniform_buffers = 1;
    shader_info.num_storage_buffers = 0;
    shader_info.num_storage_textures = 0;
    SDL_GPUShader *scene_fs = SDL_CreateGPUShader(app->device, &shader_info);
    if (!scene_fs) {
        SDL_Log("Failed to create scene fragment shader: %s", SDL_GetError());
        SDL_ReleaseGPUShader(app->device, scene_vs);
        return -1;
    }

    string overlay_vs_code = load_shader_source(&g_overlay_vertex_shader, app->overlay_vertex_path);
    shader_info.code = (const Uint8 *)overlay_vs_code.str;
    shader_info.code_size = shader_code_size(overlay_vs_code);
    shader_info.entrypoint = shader_entrypoint;
    shader_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    shader_info.num_samplers = 0;
    shader_info.num_uniform_buffers = 0;
    SDL_GPUShader *overlay_vs = SDL_CreateGPUShader(app->device, &shader_info);
    if (!overlay_vs) {
        SDL_Log("Failed to create overlay vertex shader: %s", SDL_GetError());
        SDL_ReleaseGPUShader(app->device, scene_vs);
        SDL_ReleaseGPUShader(app->device, scene_fs);
        return -1;
    }

    string overlay_fs_code = load_shader_source(&g_overlay_fragment_shader, app->overlay_fragment_path);
    shader_info.code = (const Uint8 *)overlay_fs_code.str;
    shader_info.code_size = shader_code_size(overlay_fs_code);
    shader_info.entrypoint = shader_entrypoint;
    shader_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shader_info.num_samplers = 0;
    shader_info.num_uniform_buffers = 0;
    SDL_GPUShader *overlay_fs = SDL_CreateGPUShader(app->device, &shader_info);
    if (!overlay_fs) {
        SDL_Log("Failed to create overlay fragment shader: %s", SDL_GetError());
        SDL_ReleaseGPUShader(app->device, scene_vs);
        SDL_ReleaseGPUShader(app->device, scene_fs);
        SDL_ReleaseGPUShader(app->device, overlay_vs);
        return -1;
    }

    if (!create_scene_pipeline(app, scene_vs, scene_fs)) {
        SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(app->device, scene_vs);
        SDL_ReleaseGPUShader(app->device, scene_fs);
        SDL_ReleaseGPUShader(app->device, overlay_vs);
        SDL_ReleaseGPUShader(app->device, overlay_fs);
        return -1;
    }

    if (!create_overlay_pipeline(app, overlay_vs, overlay_fs)) {
        SDL_Log("Failed to create overlay pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(app->device, scene_vs);
        SDL_ReleaseGPUShader(app->device, scene_fs);
        SDL_ReleaseGPUShader(app->device, overlay_vs);
        SDL_ReleaseGPUShader(app->device, overlay_fs);
        return -1;
    }

    SDL_ReleaseGPUShader(app->device, scene_vs);
    SDL_ReleaseGPUShader(app->device, scene_fs);
    SDL_ReleaseGPUShader(app->device, overlay_vs);
    SDL_ReleaseGPUShader(app->device, overlay_fs);

    for (int z = 0; z < MAP_HEIGHT; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            g_map_data[z * MAP_WIDTH + x] = g_default_map[z][x];
        }
    }

    float start_x = 1.5f;
    float start_z = 1.5f;
    float start_yaw = 0.0f;
    find_start_position(g_map_data, MAP_WIDTH, MAP_HEIGHT, &start_x, &start_z, &start_yaw);

    MeshData *mesh = generate_mesh(g_map_data, MAP_WIDTH, MAP_HEIGHT);
    app->scene_vertex_count = mesh->vertex_count;
    app->scene_index_count = mesh->index_count;

    if (mesh->vertex_count > MAX_SCENE_VERTICES) {
        SDL_Log("Mesh vertex count exceeds capacity");
        return -1;
    }
    MapVertex *cpu_vertices = g_temp_vertices;

    for (uint32_t i = 0; i < mesh->vertex_count; i++) {
        cpu_vertices[i].position[0] = mesh->positions[i * 3 + 0];
        cpu_vertices[i].position[1] = mesh->positions[i * 3 + 1];
        cpu_vertices[i].position[2] = mesh->positions[i * 3 + 2];
        cpu_vertices[i].surface_type = mesh->surface_types[i];
        cpu_vertices[i].uv[0] = mesh->uvs[i * 2 + 0];
        cpu_vertices[i].uv[1] = mesh->uvs[i * 2 + 1];
        cpu_vertices[i].normal[0] = mesh->normals[i * 3 + 0];
        cpu_vertices[i].normal[1] = mesh->normals[i * 3 + 1];
        cpu_vertices[i].normal[2] = mesh->normals[i * 3 + 2];
    }

    SDL_GPUBufferCreateInfo vertex_buffer_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(MapVertex) * mesh->vertex_count,
    };
    app->scene_vertex_buffer = SDL_CreateGPUBuffer(app->device, &vertex_buffer_info);
    if (!app->scene_vertex_buffer) {
        SDL_Log("Failed to create scene vertex buffer: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUBufferCreateInfo index_buffer_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = sizeof(uint16_t) * mesh->index_count,
    };
    app->scene_index_buffer = SDL_CreateGPUBuffer(app->device, &index_buffer_info);
    if (!app->scene_index_buffer) {
        SDL_Log("Failed to create scene index buffer: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = vertex_buffer_info.size,
    };
    app->scene_vertex_transfer_buffer = SDL_CreateGPUTransferBuffer(app->device, &transfer_info);
    transfer_info.size = index_buffer_info.size;
    app->scene_index_transfer_buffer = SDL_CreateGPUTransferBuffer(app->device, &transfer_info);

    if (!app->scene_vertex_transfer_buffer || !app->scene_index_transfer_buffer) {
        SDL_Log("Failed to create transfer buffers: %s", SDL_GetError());
        return -1;
    }

    float *mapped_vertices = (float *)SDL_MapGPUTransferBuffer(app->device, app->scene_vertex_transfer_buffer, false);
    if (!mapped_vertices) {
        SDL_Log("Failed to map vertex transfer buffer: %s", SDL_GetError());
        return -1;
    }
    base_memcpy(mapped_vertices, cpu_vertices, vertex_buffer_info.size);
    SDL_UnmapGPUTransferBuffer(app->device, app->scene_vertex_transfer_buffer);

    uint16_t *mapped_indices = (uint16_t *)SDL_MapGPUTransferBuffer(app->device, app->scene_index_transfer_buffer, false);
    if (!mapped_indices) {
        SDL_Log("Failed to map index transfer buffer: %s", SDL_GetError());
        return -1;
    }
    base_memcpy(mapped_indices, mesh->indices, index_buffer_info.size);
    SDL_UnmapGPUTransferBuffer(app->device, app->scene_index_transfer_buffer);

    SDL_GPUCommandBuffer *upload_cmdbuf = SDL_AcquireGPUCommandBuffer(app->device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmdbuf);

    SDL_GPUTransferBufferLocation src = {
        .transfer_buffer = app->scene_vertex_transfer_buffer,
        .offset = 0,
    };
    SDL_GPUBufferRegion dst = {
        .buffer = app->scene_vertex_buffer,
        .offset = 0,
        .size = vertex_buffer_info.size,
    };
    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);

    src.transfer_buffer = app->scene_index_transfer_buffer;
    dst.buffer = app->scene_index_buffer;
    dst.size = index_buffer_info.size;
    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmdbuf);

    SDL_GPUTransferBufferCreateInfo overlay_transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = sizeof(OverlayVertex) * MAX_OVERLAY_VERTICES,
    };
    app->overlay_transfer_buffer = SDL_CreateGPUTransferBuffer(app->device, &overlay_transfer_info);
    if (!app->overlay_transfer_buffer) {
        SDL_Log("Failed to create overlay transfer buffer: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUBufferCreateInfo overlay_buffer_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = overlay_transfer_info.size,
    };
    app->overlay_vertex_buffer = SDL_CreateGPUBuffer(app->device, &overlay_buffer_info);
    if (!app->overlay_vertex_buffer) {
        SDL_Log("Failed to create overlay vertex buffer: %s", SDL_GetError());
        return -1;
    }

    app->floor_texture = load_texture_from_path(app, FLOOR_TEXTURE_PATH, "floor");
    if (!app->floor_texture) {
        return -1;
    }

    app->wall_texture = load_texture_from_path(app, WALL_TEXTURE_PATH, "wall");
    if (!app->wall_texture) {
        SDL_ReleaseGPUTexture(app->device, app->floor_texture);
        app->floor_texture = NULL;
        return -1;
    }

    app->ceiling_texture = load_texture_from_path(app, CEILING_TEXTURE_PATH, "ceiling");
    if (!app->ceiling_texture) {
        SDL_ReleaseGPUTexture(app->device, app->wall_texture);
        app->wall_texture = NULL;
        SDL_ReleaseGPUTexture(app->device, app->floor_texture);
        app->floor_texture = NULL;
        return -1;
    }

    // Create sampler
    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    app->floor_sampler = SDL_CreateGPUSampler(app->device, &sampler_info);
    if (!app->floor_sampler) {
        SDL_Log("Failed to create texture sampler: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(app->device, app->ceiling_texture);
        app->ceiling_texture = NULL;
        SDL_ReleaseGPUTexture(app->device, app->wall_texture);
        app->wall_texture = NULL;
        SDL_ReleaseGPUTexture(app->device, app->floor_texture);
        app->floor_texture = NULL;
        return -1;
    }
    SDL_Log("Scene textures and sampler created successfully");

    GameState *state = &app->state;
    gm_init_game_state(state, g_map_data, MAP_WIDTH, MAP_HEIGHT, start_x, start_z, start_yaw);
    app->last_ticks = SDL_GetTicks();
    app->has_tick_base = false;
    app->frame_time_ms = 0.0f;
    app->quit_requested = false;
    app->overlay_dirty = true;
    SDL_SetWindowRelativeMouseMode(app->window, true);
    return 0;
}
// ============================================================================
// Application lifecycle
// ============================================================================

static int init_game(GameApp *app) {
    ensure_runtime_heap();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }

    // Minimal mode: just create window, no GPU
    if (app->minimal_mode) {
        SDL_Log("Minimal mode: creating window only");
        app->window_width = 1280;
        app->window_height = 720;
        app->window = SDL_CreateWindow("GM SDL (Minimal)", app->window_width, app->window_height, SDL_WINDOW_RESIZABLE);
        if (app->window == NULL) {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            return -1;
        }
        SDL_Log("Minimal mode: window created successfully");
        return 0;
    }

    // Select shader format and backend based on platform (following SDL_gpu_examples pattern)
    SDL_GPUShaderFormat shader_format;
    const char* backend_name;
#if defined(_WIN32)
    shader_format = SDL_GPU_SHADERFORMAT_DXIL;  // D3D12 on Windows
    backend_name = "direct3d12";
#elif defined(__APPLE__)
    shader_format = SDL_GPU_SHADERFORMAT_MSL;   // Metal on macOS
    backend_name = "metal";
#else
    shader_format = SDL_GPU_SHADERFORMAT_SPIRV; // Vulkan on Linux
    backend_name = "vulkan";
#endif

    app->device = SDL_CreateGPUDevice(shader_format, true, backend_name);
    if (app->device == NULL) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return -1;
    }

    app->window_width = 1280;
    app->window_height = 720;

    app->window = SDL_CreateWindow("GM SDL", app->window_width, app->window_height, SDL_WINDOW_RESIZABLE);
    if (app->window == NULL) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    if (!SDL_ClaimWindowForGPUDevice(app->device, app->window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return -1;
    }

    const char *driver = SDL_GetGPUDeviceDriver(app->device);
    const char *shader_dir = NULL;
    const char *shader_ext = NULL;

    if (base_strcmp(driver, "metal") == 0) {
        shader_dir = "shaders/MSL/";
        app->shader_format = SDL_GPU_SHADERFORMAT_MSL;
        shader_ext = ".msl";
    } else if (base_strcmp(driver, "vulkan") == 0) {
        shader_dir = "shaders/SPIRV/";
        app->shader_format = SDL_GPU_SHADERFORMAT_SPIRV;
        shader_ext = ".spv";
    } else if (base_strcmp(driver, "direct3d12") == 0) {
        shader_dir = "shaders/DXIL/";
        app->shader_format = SDL_GPU_SHADERFORMAT_DXIL;
        shader_ext = ".dxil";
#if defined(__wasi__)
    } else if (base_strcmp(driver, "wgsl") == 0) {
        shader_dir = "shaders/WGSL/";
        app->shader_format = SDL_GPU_SHADERFORMAT_WGSL;
        shader_ext = ".wgsl";
#endif
    } else {
        SDL_Log("ERROR: Unsupported GPU driver '%s'", driver);
        return -1;
    }

    SDL_Log("Using %s backend, loading shaders from %s", driver, shader_dir);

    SDL_snprintf(app->scene_vertex_path, sizeof(app->scene_vertex_path),
                 "%smousecircle_scene_vertex%s", shader_dir, shader_ext);
    SDL_snprintf(app->scene_fragment_path, sizeof(app->scene_fragment_path),
                 "%smousecircle_scene_fragment%s", shader_dir, shader_ext);
    SDL_snprintf(app->overlay_vertex_path, sizeof(app->overlay_vertex_path),
                 "%smousecircle_overlay_vertex%s", shader_dir, shader_ext);
    SDL_snprintf(app->overlay_fragment_path, sizeof(app->overlay_fragment_path),
                 "%smousecircle_overlay_fragment%s", shader_dir, shader_ext);

    return complete_gpu_setup(app);
}

static void update_game(GameApp *app) {
    Uint32 now = SDL_GetTicks();
    if (!app->has_tick_base) {
        app->last_ticks = now;
        app->has_tick_base = true;
    }
    Uint32 delta = now - app->last_ticks;
    app->last_ticks = now;
    if (delta == 0) {
        delta = 16;
    }
    app->frame_time_ms = (float)delta;

    GameState *state = &app->state;
    state->avg_frame_time = state->avg_frame_time * PERF_SMOOTHING + app->frame_time_ms * (1.0f - PERF_SMOOTHING);

    state->fps_frame_count++;
    state->last_fps_update_time += delta;
    if (state->last_fps_update_time >= 500.0) {
        state->fps = (float)(state->fps_frame_count * 1000.0 / state->last_fps_update_time);
        state->fps_frame_count = 0;
        state->last_fps_update_time = 0.0;
    }

    gm_update_frame(state, (float)app->window_width, (float)app->window_height);

    mat4 projection = mat4_perspective(state->fov, (float)app->window_width / (float)app->window_height, 0.05f, 100.0f);
    mat4 view = mat4_look_at_fps(state->camera_x, state->camera_y, state->camera_z,
                                 state->yaw, state->pitch);
    mat4 mvp = mat4_multiply(projection, view);

    app->scene_uniforms.mvp = mvp;
    app->scene_uniforms.camera_pos[0] = state->camera_x;
    app->scene_uniforms.camera_pos[1] = state->camera_y;
    app->scene_uniforms.camera_pos[2] = state->camera_z;
    app->scene_uniforms.camera_pos[3] = 1.0f;
    app->scene_uniforms.fog_color[0] = 0.5f;
    app->scene_uniforms.fog_color[1] = 0.65f;
    app->scene_uniforms.fog_color[2] = 0.9f;
    app->scene_uniforms.fog_color[3] = 1.0f;

    build_overlay(app);

    // Debug: Always log vertex count and check for issues
    static uint32_t frame_count = 0;
    frame_count++;

    // Unconditional log to verify this code is running
    if (frame_count <= 3) {
        SDL_Log("=== UPDATE_GAME Frame %u: overlay_vertex_count=%u ===", frame_count, app->overlay_vertex_count);
    }

    // Only copy to transfer buffer if overlay changed (dirty flag is set by build_overlay)
    if (app->overlay_dirty && app->overlay_vertex_count > 0) {
        OverlayVertex *mapped = (OverlayVertex *)SDL_MapGPUTransferBuffer(app->device, app->overlay_transfer_buffer, true);
        if (mapped) {
            base_memcpy(mapped, app->overlay_cpu_vertices, sizeof(OverlayVertex) * app->overlay_vertex_count);
            SDL_UnmapGPUTransferBuffer(app->device, app->overlay_transfer_buffer);
        }
    }
    if (frame_count <= 3) {
        SDL_Log("update_game: END frame %u", frame_count);
    }
}

static int render_game(GameApp *app) {
    SDL_Log("render_game: START");
    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(app->device);
    if (!cmdbuf) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }
    SDL_Log("render_game: Got command buffer");

    SDL_GPUTexture *swapchain_texture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, app->window, &swapchain_texture, NULL, NULL)) {
        SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }
    SDL_Log("render_game: Got swapchain texture");

    // Always upload if dirty, even if vertex_count is 0 (to clear stale data)
    if (app->overlay_dirty) {
        SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmdbuf);
        SDL_GPUTransferBufferLocation src = {
            .transfer_buffer = app->overlay_transfer_buffer,
            .offset = 0,
        };
        SDL_GPUBufferRegion dst = {
            .buffer = app->overlay_vertex_buffer,
            .offset = 0,
            .size = app->overlay_vertex_count > 0 ? sizeof(OverlayVertex) * app->overlay_vertex_count : sizeof(OverlayVertex),
        };
        SDL_UploadToGPUBuffer(copy_pass, &src, &dst, true);
        SDL_EndGPUCopyPass(copy_pass);
        app->overlay_dirty = false;
        SDL_Log("render_game: Uploaded overlay data");
    }

    SDL_Log("render_game: Setting up render pass");
    SDL_GPUColorTargetInfo color_target = {
        .texture = swapchain_texture,
        .clear_color = (SDL_FColor){0.5f, 0.65f, 0.9f, 1.0f},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };

    SDL_GPUDepthStencilTargetInfo depth_target = {
        .texture = app->depth_texture,
        .clear_depth = 1.0f,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };

    SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(cmdbuf, &color_target, 1, &depth_target);
    SDL_Log("render_game: Render pass started");

    SDL_Log("render_game: Binding scene pipeline");
    SDL_BindGPUGraphicsPipeline(render_pass, app->scene_pipeline);
    SDL_Log("render_game: Pushing uniform data");
    SDL_PushGPUVertexUniformData(cmdbuf, 0, &app->scene_uniforms, sizeof(SceneUniforms));
    SDL_PushGPUFragmentUniformData(cmdbuf, 0, &app->scene_uniforms, sizeof(SceneUniforms));
    SDL_Log("render_game: Pipeline and uniforms set");

    // Bind scene textures and sampler (shared sampler for all textures)
    SDL_Log("render_game: About to bind textures");
    if (!app->floor_texture || !app->wall_texture || !app->ceiling_texture || !app->floor_sampler) {
        SDL_Log("ERROR: Missing textures or sampler!");
        SDL_EndGPURenderPass(render_pass);
        return -1;
    }
    
    SDL_GPUTextureSamplerBinding texture_bindings[3] = {
        {
            .texture = app->floor_texture,
            .sampler = app->floor_sampler,
        },
        {
            .texture = app->wall_texture,
            .sampler = app->floor_sampler,  // Use shared sampler
        },
        {
            .texture = app->ceiling_texture,
            .sampler = app->floor_sampler,  // Use shared sampler
        },
    };
    
    SDL_Log("render_game: Binding fragment samplers");
    // Note: On Vulkan, vertex shaders don't typically access textures
    // Only bind to fragment samplers
    SDL_BindGPUFragmentSamplers(render_pass, 0, texture_bindings, SDL_arraysize(texture_bindings));
    SDL_Log("render_game: Fragment samplers bound");

    if (!app->scene_vertex_buffer || !app->scene_index_buffer) {
        SDL_Log("ERROR: Scene buffers not initialized! vertex=%p index=%p", 
                (void*)app->scene_vertex_buffer, (void*)app->scene_index_buffer);
        SDL_EndGPURenderPass(render_pass);
        return -1;
    }

    SDL_Log("render_game: Binding vertex buffers (ptr=%p)", (void*)app->scene_vertex_buffer);
    SDL_GPUBufferBinding vertex_binding = {
        .buffer = app->scene_vertex_buffer,
        .offset = 0,
    };
    SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);
    SDL_Log("render_game: Vertex buffers bound");

    SDL_Log("render_game: Binding index buffer (ptr=%p)", (void*)app->scene_index_buffer);
    SDL_GPUBufferBinding index_binding = {
        .buffer = app->scene_index_buffer,
        .offset = 0,
    };
    SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_Log("render_game: Index buffer bound");

    // TODO: Add back draw call
    #if 0
    SDL_Log("render_game: Drawing scene (index_count=%u)", app->scene_index_count);
    SDL_DrawGPUIndexedPrimitives(render_pass, app->scene_index_count, 1, 0, 0, 0);
    SDL_Log("render_game: Scene drawn");
    #endif

    // Temporarily disable overlay to test scene stability
    if (false && app->overlay_vertex_count > 0) {
        SDL_Log("render_game: Binding overlay pipeline (ptr=%p)", (void*)app->overlay_pipeline);
        if (!app->overlay_pipeline) {
            SDL_Log("ERROR: overlay_pipeline is NULL!");
            SDL_EndGPURenderPass(render_pass);
            return -1;
        }
        SDL_BindGPUGraphicsPipeline(render_pass, app->overlay_pipeline);
        SDL_Log("render_game: Binding overlay vertex buffer");
        SDL_GPUBufferBinding overlay_binding = {
            .buffer = app->overlay_vertex_buffer,
            .offset = 0,
        };
        SDL_BindGPUVertexBuffers(render_pass, 0, &overlay_binding, 1);
        SDL_Log("render_game: Drawing overlay (vertex_count=%u)", app->overlay_vertex_count);
        SDL_DrawGPUPrimitives(render_pass, app->overlay_vertex_count, 1, 0, 0);
        SDL_Log("render_game: Overlay drawn");
    }

    SDL_Log("render_game: Ending render pass");
    SDL_EndGPURenderPass(render_pass);
    SDL_Log("render_game: Submitting command buffer (ptr=%p)", (void*)cmdbuf);
    SDL_SubmitGPUCommandBuffer(cmdbuf);
    SDL_Log("render_game: Command buffer submitted successfully");
    
    // Wait for GPU to finish to avoid race conditions
    SDL_Log("render_game: Waiting for GPU");
    SDL_WaitForGPUIdle(app->device);
    SDL_Log("render_game: GPU idle");
    
    SDL_Log("render_game: END");
    return 0;
}

static void shutdown_game(GameApp *app) {
    if (app->overlay_transfer_buffer) {
        SDL_ReleaseGPUTransferBuffer(app->device, app->overlay_transfer_buffer);
        app->overlay_transfer_buffer = NULL;
    }
    if (app->overlay_vertex_buffer) {
        SDL_ReleaseGPUBuffer(app->device, app->overlay_vertex_buffer);
        app->overlay_vertex_buffer = NULL;
    }
    if (app->scene_vertex_transfer_buffer) {
        SDL_ReleaseGPUTransferBuffer(app->device, app->scene_vertex_transfer_buffer);
        app->scene_vertex_transfer_buffer = NULL;
    }
    if (app->scene_index_transfer_buffer) {
        SDL_ReleaseGPUTransferBuffer(app->device, app->scene_index_transfer_buffer);
        app->scene_index_transfer_buffer = NULL;
    }
    if (app->scene_vertex_buffer) {
        SDL_ReleaseGPUBuffer(app->device, app->scene_vertex_buffer);
        app->scene_vertex_buffer = NULL;
    }
    if (app->scene_index_buffer) {
        SDL_ReleaseGPUBuffer(app->device, app->scene_index_buffer);
        app->scene_index_buffer = NULL;
    }
    if (app->scene_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(app->device, app->scene_pipeline);
        app->scene_pipeline = NULL;
    }
    if (app->overlay_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(app->device, app->overlay_pipeline);
        app->overlay_pipeline = NULL;
    }
    if (app->depth_texture) {
        SDL_ReleaseGPUTexture(app->device, app->depth_texture);
        app->depth_texture = NULL;
    }
    if (app->floor_texture) {
        SDL_ReleaseGPUTexture(app->device, app->floor_texture);
        app->floor_texture = NULL;
    }
    if (app->wall_texture) {
        SDL_ReleaseGPUTexture(app->device, app->wall_texture);
        app->wall_texture = NULL;
    }
    if (app->ceiling_texture) {
        SDL_ReleaseGPUTexture(app->device, app->ceiling_texture);
        app->ceiling_texture = NULL;
    }
    if (app->floor_sampler) {
        SDL_ReleaseGPUSampler(app->device, app->floor_sampler);
        app->floor_sampler = NULL;
    }
    if (app->device && app->window) {
        SDL_ReleaseWindowFromGPUDevice(app->device, app->window);
    }
    if (app->window) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }
    if (app->device) {
        SDL_DestroyGPUDevice(app->device);
        app->device = NULL;
    }
    SDL_Quit();
}

// ============================================================================
// Helper functions
// ============================================================================

// Simple atoi implementation for argument parsing
static int simple_atoi(const char *str) {
    int result = 0;
    int sign = 1;

    if (*str == '-') {
        sign = -1;
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

// ============================================================================
// SDL callbacks
// ============================================================================

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    // Parse command-line arguments
    g_App.test_frames_max = 0;    // 0 = unlimited
    g_App.test_frames_count = 0;
    g_App.export_obj_mode = false;
    g_App.export_obj_path[0] = '\0';
    g_App.minimal_mode = false;

    for (int i = 1; i < argc; i++) {
        if (base_strcmp(argv[i], "--test-frames") == 0 && i + 1 < argc) {
            g_App.test_frames_max = simple_atoi(argv[i + 1]);
            i++;  // Skip the next argument since we consumed it
        } else if (base_strcmp(argv[i], "--minimal") == 0) {
            g_App.minimal_mode = true;
        } else if (base_strcmp(argv[i], "--export-obj") == 0 && i + 1 < argc) {
            g_App.export_obj_mode = true;
            // Copy the filename
            int j = 0;
            while (argv[i + 1][j] && j < 255) {
                g_App.export_obj_path[j] = argv[i + 1][j];
                j++;
            }
            g_App.export_obj_path[j] = '\0';
            i++;  // Skip the next argument since we consumed it
        } else if (argv[i][0] == '-') {
            // Unknown argument starting with '-'
            SDL_Log("Error: Unknown command line argument '%s'", argv[i]);
            SDL_Log("Usage: %s [--test-frames N] [--export-obj FILENAME]", argv[0]);
            return SDL_APP_FAILURE;
        } else {
            // Positional argument (not expected)
            SDL_Log("Error: Unexpected argument '%s'", argv[i]);
            SDL_Log("Usage: %s [--test-frames N] [--export-obj FILENAME]", argv[0]);
            return SDL_APP_FAILURE;
        }
    }

    // Handle export mode: generate mesh and export to OBJ, then exit
    if (g_App.export_obj_mode) {
        SDL_Log("Export mode enabled, output: %s", g_App.export_obj_path);

        // Initialize map data (same as in init_game)
        for (int z = 0; z < MAP_HEIGHT; z++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                g_map_data[z * MAP_WIDTH + x] = g_default_map[z][x];
            }
        }

        // Generate the mesh
        MeshData *mesh = generate_mesh(g_map_data, MAP_WIDTH, MAP_HEIGHT);
        if (!mesh) {
            SDL_Log("Failed to generate mesh");
            return SDL_APP_FAILURE;
        }

        // Export to OBJ file
        bool obj_success = export_mesh_to_obj(mesh, g_App.export_obj_path);
        if (!obj_success) {
            SDL_Log("Failed to export OBJ file");
            return SDL_APP_FAILURE;
        }

        // Export MTL file
        bool mtl_success = export_mtl_file(g_App.export_obj_path);
        if (!mtl_success) {
            SDL_Log("Failed to export MTL file");
            return SDL_APP_FAILURE;
        }

        SDL_Log("Export completed successfully");
        return SDL_APP_SUCCESS;  // Exit successfully without running the game
    }

    int init_status = init_game(&g_App);
    if (init_status < 0) {
        return SDL_APP_FAILURE;
    }

    build_overlay(&g_App);

    *appstate = &g_App;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    GameApp *app = (GameApp *)appstate;
    GameState *state = &app->state;

    if (event->type == SDL_EVENT_QUIT) {
        app->quit_requested = true;
        return SDL_APP_CONTINUE;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        uint32_t key = event->key.key;

        // Store key state using lower 8 bits as index (works for both ASCII and SDL keycodes)
        gm_set_key_state(state, (uint8_t)(key & 0xFF), 1);

        // Handle special key presses (only for printable ASCII keys)
        if (key < 256) {
            gm_handle_key_press(state, (uint8_t)key);
        }

        if (key == SDLK_ESCAPE || key == 'q' || key == 'Q') {
            app->quit_requested = true;
        }
    } else if (event->type == SDL_EVENT_KEY_UP) {
        uint32_t key = event->key.key;

        // Store key state using lower 8 bits as index
        gm_set_key_state(state, (uint8_t)(key & 0xFF), 0);
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        gm_add_mouse_delta(state, event->motion.xrel, event->motion.yrel);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    GameApp *app = (GameApp *)appstate;
    if (!app) {
        return SDL_APP_FAILURE;
    }

    if (app->quit_requested) {
        return SDL_APP_SUCCESS;
    }

    // Minimal mode: just count frames, no rendering
    if (app->minimal_mode) {
        if (app->test_frames_max > 0) {
            app->test_frames_count++;
            SDL_Log("Minimal mode frame %d/%d", app->test_frames_count, app->test_frames_max);
            if (app->test_frames_count >= app->test_frames_max) {
                return SDL_APP_SUCCESS;
            }
        }
        SDL_Delay(16);  // ~60 FPS
        return SDL_APP_CONTINUE;
    }

    update_game(app);
    SDL_Log("SDL_AppIterate: update_game returned");
    
    int render_result = render_game(app);
    SDL_Log("SDL_AppIterate: render_game returned %d", render_result);
    if (render_result < 0) {
        return SDL_APP_FAILURE;
    }

    // Check if we've reached the frame limit for testing
    if (app->test_frames_max > 0) {
        app->test_frames_count++;
        SDL_Log("SDL_AppIterate: Frame %d/%d complete", app->test_frames_count, app->test_frames_max);
        if (app->test_frames_count >= app->test_frames_max) {
            return SDL_APP_SUCCESS;  // Exit after N frames
        }
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result;
    GameApp *app = (GameApp *)appstate;
    if (app) {
        shutdown_game(app);
    }
}
