#include <gm.h>
#include <webgpu/webgpu.h>
#include <platform.h>
#include <base/scratch.h>
#include <base/format.h>
#include <base/base_string.h>
#include <base/base_io.h>
#include <base/base_math.h>

// Math functions for WASM (imported from JavaScript)
#ifdef __wasm__
// These will be imported from JavaScript, overriding the builtins
__attribute__((import_module("env"), import_name("cosf")))
extern float cosf(float x);
__attribute__((import_module("env"), import_name("sinf")))
extern float sinf(float x);
#endif

#include "gm_shaders.inc"

#define GM_UNIFORM_FLOAT_COUNT 12
#define GM_OVERLAY_UNIFORM_FLOAT_COUNT 24
#define GM_OVERLAY_TEXT_CAPACITY 256
#define GM_MAP_CELL_COUNT (MAP_WIDTH * MAP_HEIGHT)

typedef enum {
    GM_BUFFER_POSITIONS = 0,
    GM_BUFFER_UVS,
    GM_BUFFER_SURFACE_TYPES,
    GM_BUFFER_TRIANGLE_IDS,
    GM_BUFFER_NORMALS,
    GM_BUFFER_INDICES,
    GM_BUFFER_UNIFORM,
    GM_BUFFER_OVERLAY_UNIFORM,
    GM_BUFFER_OVERLAY_TEXT,
    GM_BUFFER_OVERLAY_MAP,
    GM_BUFFER_OVERLAY_VERTEX,
    GM_BUFFER_COUNT
} GMBufferSlot;

static WGPUDevice g_wgpu_device = NULL;
static WGPUQueue g_wgpu_queue = NULL;
static WGPUBuffer g_gpu_buffers[GM_BUFFER_COUNT];

typedef enum {
    GM_SHADER_MAIN_VS = 0,
    GM_SHADER_MAIN_FS,
    GM_SHADER_OVERLAY_VS,
    GM_SHADER_OVERLAY_FS,
    GM_SHADER_COUNT
} GMShaderSlot;

static WGPUShaderModule g_shader_modules[GM_SHADER_COUNT];

typedef enum {
    GM_BIND_GROUP_LAYOUT_MAIN = 0,
    GM_BIND_GROUP_LAYOUT_OVERLAY,
    GM_BIND_GROUP_LAYOUT_COUNT
} GMBindGroupLayoutSlot;

typedef enum {
    GM_PIPELINE_LAYOUT_MAIN = 0,
    GM_PIPELINE_LAYOUT_OVERLAY,
    GM_PIPELINE_LAYOUT_COUNT
} GMPipelineLayoutSlot;

typedef enum {
    GM_RENDER_PIPELINE_MAIN = 0,
    GM_RENDER_PIPELINE_OVERLAY,
    GM_RENDER_PIPELINE_COUNT
} GMRenderPipelineSlot;

typedef enum {
    GM_BIND_GROUP_MAIN = 0,
    GM_BIND_GROUP_OVERLAY,
    GM_BIND_GROUP_COUNT
} GMBindGroupSlot;

static WGPUBindGroupLayout g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_COUNT];
static WGPUPipelineLayout g_pipeline_layouts[GM_PIPELINE_LAYOUT_COUNT];
static WGPURenderPipeline g_render_pipelines[GM_RENDER_PIPELINE_COUNT];
static WGPUBindGroup g_bind_groups[GM_BIND_GROUP_COUNT];
static WGPUTextureView g_wall_texture_view = NULL;
static WGPUTextureView g_floor_texture_view = NULL;
static WGPUTextureView g_ceiling_texture_view = NULL;
static WGPUSampler g_main_sampler = NULL;

static const float g_overlay_vertex_data[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f,
};

// Find starting position and direction in the map
// Returns: 1 if found, 0 if not found
// Outputs: startX, startZ (position), startYaw (direction in radians), and modifies map
int find_start_position(int *map, int width, int height,
                       float *startX, float *startZ, float *startYaw) {
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            int cell = map[z * width + x];
            if (cell >= 5 && cell <= 8) {
                *startX = (float)x + 0.5f;
                *startZ = (float)z + 0.5f;

                // Set yaw based on direction
                if (cell == 5) {
                    *startYaw = -PI / 2.0f;  // North
                } else if (cell == 6) {
                    *startYaw = 0.0f;        // East
                } else if (cell == 7) {
                    *startYaw = PI / 2.0f;   // South
                } else { // cell == 8
                    *startYaw = PI;          // West
                }

                // Clear the marker
                map[z * width + x] = 0;

                return 1;
            }
        }
    }
    return 0;
}

// ============================================================================
// Mesh Generation
// ============================================================================

// Context for building mesh geometry
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
    uint32_t vertex_idx;
    uint32_t index_idx;

    uint32_t index_offset;
    uint32_t triangle_counter;

    float inv_wall_height;
    float window_bottom;
    float window_top;
    float window_margin;
} MeshGenContext;

// Helper: Check if cell is solid
static inline int is_solid_cell(int value) {
    return value == 1 || value == 2 || value == 3;
}

// Helper: Push values to position array
static inline void push_position(MeshGenContext *ctx, float x, float y, float z) {
    ctx->positions[ctx->position_idx++] = x;
    ctx->positions[ctx->position_idx++] = y;
    ctx->positions[ctx->position_idx++] = z;
    ctx->vertex_idx++;
}

// Helper: Push values to UV array
static inline void push_uv(MeshGenContext *ctx, float u, float v) {
    ctx->uvs[ctx->uv_idx++] = u;
    ctx->uvs[ctx->uv_idx++] = v;
}

// Helper: Push values to normal array
static inline void push_normal(MeshGenContext *ctx, float x, float y, float z) {
    ctx->normals[ctx->normal_idx++] = x;
    ctx->normals[ctx->normal_idx++] = y;
    ctx->normals[ctx->normal_idx++] = z;
}

// Helper: Push surface type
static inline void push_surface_type(MeshGenContext *ctx, float type) {
    ctx->surface_types[ctx->vertex_idx - 1] = type;
}

// Helper: Push triangle ID
static inline void push_triangle_id(MeshGenContext *ctx, float id) {
    ctx->triangle_ids[ctx->vertex_idx - 1] = id;
}

// Helper: Push index
static inline void push_index(MeshGenContext *ctx, uint16_t idx) {
    ctx->indices[ctx->index_idx++] = idx;
}

// Push a quad for north-facing wall segment
static void push_north_segment_range(MeshGenContext *ctx, float x0, float x1, float z,
                                     float y0, float y1, int normalize_u, float surface_type) {
    float v0 = y0 * ctx->inv_wall_height;
    float v1 = y1 * ctx->inv_wall_height;
    float u_span = normalize_u ? 1.0f : (x1 - x0);

    uint16_t base = ctx->index_offset;

    // 4 vertices for quad
    push_position(ctx, x0, y0, z);
    push_uv(ctx, 0.0f, v0);
    push_normal(ctx, 0.0f, 0.0f, -1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x1, y0, z);
    push_uv(ctx, u_span, v0);
    push_normal(ctx, 0.0f, 0.0f, -1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x0, y1, z);
    push_uv(ctx, 0.0f, v1);
    push_normal(ctx, 0.0f, 0.0f, -1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x1, y1, z);
    push_uv(ctx, u_span, v1);
    push_normal(ctx, 0.0f, 0.0f, -1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    // 6 indices for 2 triangles
    push_index(ctx, base + 0);
    push_index(ctx, base + 1);
    push_index(ctx, base + 2);
    push_index(ctx, base + 1);
    push_index(ctx, base + 3);
    push_index(ctx, base + 2);

    ctx->index_offset += 4;
    ctx->triangle_counter += 2;
}

// Push a quad for south-facing wall segment
static void push_south_segment_range(MeshGenContext *ctx, float x0, float x1, float z,
                                      float y0, float y1, int normalize_u, float surface_type) {
    float v0 = y0 * ctx->inv_wall_height;
    float v1 = y1 * ctx->inv_wall_height;
    float u_span = normalize_u ? 1.0f : (x1 - x0);

    uint16_t base = ctx->index_offset;

    push_position(ctx, x0, y0, z + 1.0f);
    push_uv(ctx, 0.0f, v0);
    push_normal(ctx, 0.0f, 0.0f, 1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x1, y0, z + 1.0f);
    push_uv(ctx, u_span, v0);
    push_normal(ctx, 0.0f, 0.0f, 1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x0, y1, z + 1.0f);
    push_uv(ctx, 0.0f, v1);
    push_normal(ctx, 0.0f, 0.0f, 1.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x1, y1, z + 1.0f);
    push_uv(ctx, u_span, v1);
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

// Push a quad for west-facing wall segment
static void push_west_segment_range(MeshGenContext *ctx, float x, float z0, float z1,
                                     float y0, float y1, int normalize_u, float surface_type) {
    float v0 = y0 * ctx->inv_wall_height;
    float v1 = y1 * ctx->inv_wall_height;
    float u_span = normalize_u ? 1.0f : (z1 - z0);

    uint16_t base = ctx->index_offset;

    push_position(ctx, x, y0, z0);
    push_uv(ctx, 0.0f, v0);
    push_normal(ctx, -1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x, y0, z1);
    push_uv(ctx, u_span, v0);
    push_normal(ctx, -1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x, y1, z0);
    push_uv(ctx, 0.0f, v1);
    push_normal(ctx, -1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x, y1, z1);
    push_uv(ctx, u_span, v1);
    push_normal(ctx, -1.0f, 0.0f, 0.0f);
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

// Push a quad for east-facing wall segment
static void push_east_segment_range(MeshGenContext *ctx, float x, float z0, float z1,
                                     float y0, float y1, int normalize_u, float surface_type) {
    float v0 = y0 * ctx->inv_wall_height;
    float v1 = y1 * ctx->inv_wall_height;
    float u_span = normalize_u ? 1.0f : (z1 - z0);

    uint16_t base = ctx->index_offset;

    push_position(ctx, x, y0, z0);
    push_uv(ctx, 0.0f, v0);
    push_normal(ctx, 1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)ctx->triangle_counter);

    push_position(ctx, x, y0, z1);
    push_uv(ctx, u_span, v0);
    push_normal(ctx, 1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, (float)(ctx->triangle_counter + 1));

    push_position(ctx, x, y1, z0);
    push_uv(ctx, 0.0f, v1);
    push_normal(ctx, 1.0f, 0.0f, 0.0f);
    push_surface_type(ctx, surface_type);
    push_triangle_id(ctx, 0.0f);

    push_position(ctx, x, y1, z1);
    push_uv(ctx, u_span, v1);
    push_normal(ctx, 1.0f, 0.0f, 0.0f);
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

// Push a horizontal quad (floor or ceiling segment)
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

// Convenience functions for full-cell segments
static void push_north_segment(MeshGenContext *ctx, float x, float z, float y0, float y1) {
    push_north_segment_range(ctx, x, x + 1.0f, z, y0, y1, 0, 1.0f);
}

static void push_south_segment(MeshGenContext *ctx, float x, float z, float y0, float y1) {
    push_south_segment_range(ctx, x, x + 1.0f, z, y0, y1, 0, 1.0f);
}

static void push_west_segment(MeshGenContext *ctx, float x, float z, float y0, float y1) {
    push_west_segment_range(ctx, x, z, z + 1.0f, y0, y1, 0, 1.0f);
}

static void push_east_segment(MeshGenContext *ctx, float x, float z, float y0, float y1) {
    push_east_segment_range(ctx, x + 1.0f, z, z + 1.0f, y0, y1, 0, 1.0f);
}

// Static buffers for mesh data (reused across calls)
// Max estimate: 10x10 map with windows = ~500 quads = ~2000 vertices, ~3000 indices
static float g_positions[6000];  // 2000 vertices * 3
static float g_uvs[4000];        // 2000 vertices * 2
static float g_normals[6000];    // 2000 vertices * 3
static float g_surface_types[2000];
static float g_triangle_ids[2000];
static uint16_t g_indices[6000];  // ~3000 indices max
static MeshData g_mesh_data;

// Generate mesh geometry from map
MeshData* generate_mesh(int *map, int width, int height) {
    MeshGenContext ctx = {0};
    ctx.positions = g_positions;
    ctx.uvs = g_uvs;
    ctx.normals = g_normals;
    ctx.surface_types = g_surface_types;
    ctx.triangle_ids = g_triangle_ids;
    ctx.indices = g_indices;

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
    push_uv(&ctx, (float)width * CHECKER_SIZE, 0.0f);
    push_normal(&ctx, 0.0f, 1.0f, 0.0f);
    push_surface_type(&ctx, 0.0f);
    push_triangle_id(&ctx, (float)(ctx.triangle_counter + 1));

    push_position(&ctx, 0.0f, 0.0f, (float)height);
    push_uv(&ctx, 0.0f, (float)height * CHECKER_SIZE);
    push_normal(&ctx, 0.0f, 1.0f, 0.0f);
    push_surface_type(&ctx, 0.0f);
    push_triangle_id(&ctx, 0.0f);

    push_position(&ctx, (float)width, 0.0f, (float)height);
    push_uv(&ctx, (float)width * CHECKER_SIZE, (float)height * CHECKER_SIZE);
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

            // North face
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

            // South face
            if (z == height - 1 || !is_solid_cell(map[(z + 1) * width + x])) {
                if (is_window_ns) {
                    push_south_segment_range(&ctx, (float)x, (float)(x + 1), (float)z, 0.0f, ctx.window_bottom, 0, 1.0f);
                    push_south_segment_range(&ctx, (float)x, (float)(x + 1), (float)z, ctx.window_top, WALL_HEIGHT, 0, 1.0f);
                    push_south_segment_range(&ctx, (float)x, x_inner0, (float)z, ctx.window_bottom, ctx.window_top, 0, 3.0f);
                    push_south_segment_range(&ctx, x_inner1, (float)(x + 1), (float)z, ctx.window_bottom, ctx.window_top, 0, 3.0f);
                } else {
                    push_south_segment(&ctx, (float)x, (float)z, 0.0f, WALL_HEIGHT);
                }
            }

            // West face
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

            // East face
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

            // Window interior faces
            if (is_window_ns) {
                push_west_segment_range(&ctx, x_inner0, (float)z, (float)(z + 1), ctx.window_bottom, ctx.window_top, 1, 3.0f);
                push_east_segment_range(&ctx, x_inner1, (float)z, (float)(z + 1), ctx.window_bottom, ctx.window_top, 1, 3.0f);
                push_horizontal_fill(&ctx, x_inner0, x_inner1, (float)z, (float)(z + 1), ctx.window_bottom, 0.0f);
                push_horizontal_fill(&ctx, x_inner0, x_inner1, (float)z, (float)(z + 1), ctx.window_top, 2.0f);
            } else if (is_window_ew) {
                push_north_segment_range(&ctx, (float)x, (float)(x + 1), z_inner0, ctx.window_bottom, ctx.window_top, 1, 3.0f);
                push_south_segment_range(&ctx, (float)x, (float)(x + 1), z_inner1 - 1.0f, ctx.window_bottom, ctx.window_top, 1, 3.0f);
                push_horizontal_fill(&ctx, (float)x, (float)(x + 1), z_inner0, z_inner1, ctx.window_bottom, 0.0f);
                push_horizontal_fill(&ctx, (float)x, (float)(x + 1), z_inner0, z_inner1, ctx.window_top, 2.0f);
            }
        }
    }

    // Fill in the mesh data structure
    g_mesh_data.positions = ctx.positions;
    g_mesh_data.uvs = ctx.uvs;
    g_mesh_data.normals = ctx.normals;
    g_mesh_data.surface_types = ctx.surface_types;
    g_mesh_data.triangle_ids = ctx.triangle_ids;
    g_mesh_data.indices = ctx.indices;

    g_mesh_data.position_count = ctx.position_idx;
    g_mesh_data.uv_count = ctx.uv_idx;
    g_mesh_data.normal_count = ctx.normal_idx;
    g_mesh_data.vertex_count = ctx.vertex_idx;
    g_mesh_data.index_count = ctx.index_idx;

    return &g_mesh_data;
}

// Query the preferred canvas texture format via the WebGPU bridge.
uint32_t gm_get_preferred_canvas_format(void) {
    WGPUSurfaceCapabilities caps = WGPU_SURFACE_CAPABILITIES_INIT;

    WGPUStatus status = wgpuSurfaceGetCapabilities(NULL, NULL, &caps);
    if (status != WGPUStatus_Success || caps.formatCount == 0 || caps.formats == NULL) {
        return (uint32_t)WGPUTextureFormat_BGRA8Unorm;
    }

    uint32_t preferred = (uint32_t)caps.formats[0];
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return preferred;
}

void gm_register_webgpu_handles(uint32_t device_handle, uint32_t queue_handle) {
    g_wgpu_device = (WGPUDevice)(uintptr_t)device_handle;
    g_wgpu_queue = (WGPUQueue)(uintptr_t)queue_handle;
}

void gm_register_texture_views(uint32_t wall_view_handle, uint32_t floor_view_handle,
        uint32_t ceiling_view_handle) {
    g_wall_texture_view = (WGPUTextureView)(uintptr_t)wall_view_handle;
    g_floor_texture_view = (WGPUTextureView)(uintptr_t)floor_view_handle;
    g_ceiling_texture_view = (WGPUTextureView)(uintptr_t)ceiling_view_handle;
}

static WGPUShaderModule gm_create_shader_module_from_source(const char *source, size_t length) {
    if (source == NULL || length == 0 || g_wgpu_device == NULL) {
        return NULL;
    }
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code.data = source;
    wgsl.code.length = length;

    WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    desc.nextInChain = (WGPUChainedStruct*)&wgsl;
    return wgpuDeviceCreateShaderModule(g_wgpu_device, &desc);
}

static WGPUBuffer gm_create_buffer_with_data(const void *data, size_t size,
        WGPUBufferUsage usage) {
    if (size == 0) {
        return NULL;
    }

    WGPUBufferDescriptor desc = {
        .nextInChain = NULL,
        .label = WGPU_STRING_VIEW_INIT,
        .usage = usage | WGPUBufferUsage_CopyDst,
        .size = size,
        .mappedAtCreation = WGPU_FALSE,
    };

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(g_wgpu_device, &desc);
    if (buffer != NULL && data != NULL) {
        wgpuQueueWriteBuffer(g_wgpu_queue, buffer, 0, data, size);
    }
    return buffer;
}

int gm_create_gpu_buffers(void) {
    if (g_wgpu_device == NULL || g_wgpu_queue == NULL) {
        return -1;
    }
    if (g_mesh_data.positions == NULL || g_mesh_data.indices == NULL) {
        return -2;
    }

    g_gpu_buffers[GM_BUFFER_POSITIONS] = gm_create_buffer_with_data(
            g_mesh_data.positions,
            (size_t)g_mesh_data.position_count * sizeof(float),
            WGPUBufferUsage_Vertex);
    if (g_gpu_buffers[GM_BUFFER_POSITIONS] == NULL) {
        return -3;
    }

    g_gpu_buffers[GM_BUFFER_UVS] = gm_create_buffer_with_data(
            g_mesh_data.uvs,
            (size_t)g_mesh_data.uv_count * sizeof(float),
            WGPUBufferUsage_Vertex);
    if (g_gpu_buffers[GM_BUFFER_UVS] == NULL) {
        return -4;
    }

    g_gpu_buffers[GM_BUFFER_SURFACE_TYPES] = gm_create_buffer_with_data(
            g_mesh_data.surface_types,
            (size_t)g_mesh_data.vertex_count * sizeof(float),
            WGPUBufferUsage_Vertex);
    if (g_gpu_buffers[GM_BUFFER_SURFACE_TYPES] == NULL) {
        return -5;
    }

    g_gpu_buffers[GM_BUFFER_TRIANGLE_IDS] = gm_create_buffer_with_data(
            g_mesh_data.triangle_ids,
            (size_t)g_mesh_data.vertex_count * sizeof(float),
            WGPUBufferUsage_Vertex);
    if (g_gpu_buffers[GM_BUFFER_TRIANGLE_IDS] == NULL) {
        return -6;
    }

    g_gpu_buffers[GM_BUFFER_NORMALS] = gm_create_buffer_with_data(
            g_mesh_data.normals,
            (size_t)g_mesh_data.normal_count * sizeof(float),
            WGPUBufferUsage_Vertex);
    if (g_gpu_buffers[GM_BUFFER_NORMALS] == NULL) {
        return -7;
    }

    g_gpu_buffers[GM_BUFFER_INDICES] = gm_create_buffer_with_data(
            g_mesh_data.indices,
            (size_t)g_mesh_data.index_count * sizeof(uint16_t),
            WGPUBufferUsage_Index);
    if (g_gpu_buffers[GM_BUFFER_INDICES] == NULL) {
        return -8;
    }

    static float uniform_init[GM_UNIFORM_FLOAT_COUNT] = {0};
    g_gpu_buffers[GM_BUFFER_UNIFORM] = gm_create_buffer_with_data(
            uniform_init,
            sizeof(uniform_init),
            WGPUBufferUsage_Uniform);
    if (g_gpu_buffers[GM_BUFFER_UNIFORM] == NULL) {
        return -9;
    }

    static float overlay_uniform_init[GM_OVERLAY_UNIFORM_FLOAT_COUNT] = {0};
    g_gpu_buffers[GM_BUFFER_OVERLAY_UNIFORM] = gm_create_buffer_with_data(
            overlay_uniform_init,
            sizeof(overlay_uniform_init),
            WGPUBufferUsage_Uniform);
    if (g_gpu_buffers[GM_BUFFER_OVERLAY_UNIFORM] == NULL) {
        return -10;
    }

    g_gpu_buffers[GM_BUFFER_OVERLAY_TEXT] = gm_create_buffer_with_data(
            NULL,
            (size_t)GM_OVERLAY_TEXT_CAPACITY * sizeof(uint32_t),
            WGPUBufferUsage_Storage);
    if (g_gpu_buffers[GM_BUFFER_OVERLAY_TEXT] == NULL) {
        return -11;
    }

    g_gpu_buffers[GM_BUFFER_OVERLAY_MAP] = gm_create_buffer_with_data(
            NULL,
            (size_t)GM_MAP_CELL_COUNT * sizeof(uint32_t),
            WGPUBufferUsage_Storage);
    if (g_gpu_buffers[GM_BUFFER_OVERLAY_MAP] == NULL) {
        return -12;
    }

    g_gpu_buffers[GM_BUFFER_OVERLAY_VERTEX] = gm_create_buffer_with_data(
            g_overlay_vertex_data,
            sizeof(g_overlay_vertex_data),
            WGPUBufferUsage_Vertex);
    if (g_gpu_buffers[GM_BUFFER_OVERLAY_VERTEX] == NULL) {
        return -13;
    }

    return 0;
}

uint32_t gm_get_gpu_buffer_table(void) {
    return (uint32_t)(uintptr_t)g_gpu_buffers;
}

uint32_t gm_get_gpu_buffer_count(void) {
    return GM_BUFFER_COUNT;
}

uint32_t gm_get_uniform_float_count(void) {
    return GM_UNIFORM_FLOAT_COUNT;
}

uint32_t gm_get_uniform_buffer_size(void) {
    return GM_UNIFORM_FLOAT_COUNT * (uint32_t)sizeof(float);
}

int gm_create_shader_modules(void) {
    if (g_wgpu_device == NULL) {
        return -1;
    }

    g_shader_modules[GM_SHADER_MAIN_VS] = gm_create_shader_module_from_source(
            GM_WGSL_MAIN_VS, sizeof(GM_WGSL_MAIN_VS) - 1);
    if (g_shader_modules[GM_SHADER_MAIN_VS] == NULL) {
        return -2;
    }

    g_shader_modules[GM_SHADER_MAIN_FS] = gm_create_shader_module_from_source(
            GM_WGSL_MAIN_FS, sizeof(GM_WGSL_MAIN_FS) - 1);
    if (g_shader_modules[GM_SHADER_MAIN_FS] == NULL) {
        return -3;
    }

    g_shader_modules[GM_SHADER_OVERLAY_VS] = gm_create_shader_module_from_source(
            GM_WGSL_OVERLAY_VS, sizeof(GM_WGSL_OVERLAY_VS) - 1);
    if (g_shader_modules[GM_SHADER_OVERLAY_VS] == NULL) {
        return -4;
    }

    g_shader_modules[GM_SHADER_OVERLAY_FS] = gm_create_shader_module_from_source(
            GM_WGSL_OVERLAY_FS, sizeof(GM_WGSL_OVERLAY_FS) - 1);
    if (g_shader_modules[GM_SHADER_OVERLAY_FS] == NULL) {
        return -5;
    }

    return 0;
}

uint32_t gm_get_shader_module_table(void) {
    return (uint32_t)(uintptr_t)g_shader_modules;
}

uint32_t gm_get_shader_module_count(void) {
    return GM_SHADER_COUNT;
}

int gm_create_bind_group_layouts(void) {
    if (g_wgpu_device == NULL) {
        return -1;
    }

    WGPUBindGroupLayoutEntry main_entries[5] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
            .buffer = {
                .type = WGPUBufferBindingType_Uniform,
                .hasDynamicOffset = WGPU_FALSE,
                .minBindingSize = gm_get_uniform_buffer_size(),
            },
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .sampler = {
                .type = WGPUSamplerBindingType_Filtering,
            },
        },
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .texture = {
                .sampleType = WGPUTextureSampleType_Float,
                .viewDimension = WGPUTextureViewDimension_2D,
                .multisampled = WGPU_FALSE,
            },
        },
        {
            .binding = 3,
            .visibility = WGPUShaderStage_Fragment,
            .texture = {
                .sampleType = WGPUTextureSampleType_Float,
                .viewDimension = WGPUTextureViewDimension_2D,
                .multisampled = WGPU_FALSE,
            },
        },
        {
            .binding = 4,
            .visibility = WGPUShaderStage_Fragment,
            .texture = {
                .sampleType = WGPUTextureSampleType_Float,
                .viewDimension = WGPUTextureViewDimension_2D,
                .multisampled = WGPU_FALSE,
            },
        },
    };

    WGPUBindGroupLayoutDescriptor main_desc = {
        .entryCount = 5,
        .entries = main_entries,
    };

    g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_MAIN] = wgpuDeviceCreateBindGroupLayout(
            g_wgpu_device, &main_desc);
    if (g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_MAIN] == NULL) {
        return -2;
    }

    const uint64_t overlay_uniform_size = (uint64_t)GM_OVERLAY_UNIFORM_FLOAT_COUNT * sizeof(float);
    const uint64_t overlay_text_size = (uint64_t)GM_OVERLAY_TEXT_CAPACITY * sizeof(uint32_t);
    const uint64_t overlay_map_size = (uint64_t)GM_MAP_CELL_COUNT * sizeof(uint32_t);

    WGPUBindGroupLayoutEntry overlay_entries[3] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Fragment,
            .buffer = {
                .type = WGPUBufferBindingType_Uniform,
                .hasDynamicOffset = WGPU_FALSE,
                .minBindingSize = overlay_uniform_size,
            },
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .buffer = {
                .type = WGPUBufferBindingType_ReadOnlyStorage,
                .hasDynamicOffset = WGPU_FALSE,
                .minBindingSize = overlay_text_size,
            },
        },
        {
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .buffer = {
                .type = WGPUBufferBindingType_ReadOnlyStorage,
                .hasDynamicOffset = WGPU_FALSE,
                .minBindingSize = overlay_map_size,
            },
        },
    };

    WGPUBindGroupLayoutDescriptor overlay_desc = {
        .entryCount = 3,
        .entries = overlay_entries,
    };

    g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_OVERLAY] = wgpuDeviceCreateBindGroupLayout(
            g_wgpu_device, &overlay_desc);
    if (g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_OVERLAY] == NULL) {
        return -3;
    }

    return 0;
}

int gm_create_pipeline_layouts(void) {
    if (g_wgpu_device == NULL) {
        return -1;
    }
    if (g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_MAIN] == NULL ||
            g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_OVERLAY] == NULL) {
        return -2;
    }

    WGPUPipelineLayoutDescriptor main_desc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_MAIN],
    };
    g_pipeline_layouts[GM_PIPELINE_LAYOUT_MAIN] = wgpuDeviceCreatePipelineLayout(
            g_wgpu_device, &main_desc);
    if (g_pipeline_layouts[GM_PIPELINE_LAYOUT_MAIN] == NULL) {
        return -3;
    }

    WGPUPipelineLayoutDescriptor overlay_desc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_OVERLAY],
    };
    g_pipeline_layouts[GM_PIPELINE_LAYOUT_OVERLAY] = wgpuDeviceCreatePipelineLayout(
            g_wgpu_device, &overlay_desc);
    if (g_pipeline_layouts[GM_PIPELINE_LAYOUT_OVERLAY] == NULL) {
        return -4;
    }

    return 0;
}

int gm_create_render_pipelines(uint32_t color_format_enum) {
    if (g_wgpu_device == NULL) {
        return -1;
    }
    if (g_pipeline_layouts[GM_PIPELINE_LAYOUT_MAIN] == NULL ||
            g_pipeline_layouts[GM_PIPELINE_LAYOUT_OVERLAY] == NULL) {
        return -2;
    }
    if (g_shader_modules[GM_SHADER_MAIN_VS] == NULL ||
            g_shader_modules[GM_SHADER_MAIN_FS] == NULL ||
            g_shader_modules[GM_SHADER_OVERLAY_VS] == NULL ||
            g_shader_modules[GM_SHADER_OVERLAY_FS] == NULL) {
        return -3;
    }

    WGPUTextureFormat color_format = (WGPUTextureFormat)color_format_enum;

    const char *main_vs_entry = "vs_main";
    const char *main_fs_entry = "fs_main";

    WGPUVertexAttribute main_vertex_attributes[5] = {
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = 0,
            .shaderLocation = 0,
        },
        {
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
            .shaderLocation = 1,
        },
        {
            .format = WGPUVertexFormat_Float32,
            .offset = 0,
            .shaderLocation = 2,
        },
        {
            .format = WGPUVertexFormat_Float32,
            .offset = 0,
            .shaderLocation = 3,
        },
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = 0,
            .shaderLocation = 4,
        },
    };

    WGPUVertexBufferLayout main_vertex_buffers[5] = {
        {
            .arrayStride = sizeof(float) * 3,
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &main_vertex_attributes[0],
        },
        {
            .arrayStride = sizeof(float) * 2,
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &main_vertex_attributes[1],
        },
        {
            .arrayStride = sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &main_vertex_attributes[2],
        },
        {
            .arrayStride = sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &main_vertex_attributes[3],
        },
        {
            .arrayStride = sizeof(float) * 3,
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &main_vertex_attributes[4],
        },
    };

    WGPUVertexState main_vertex_state = {
        .module = g_shader_modules[GM_SHADER_MAIN_VS],
        .entryPoint = {
            .data = main_vs_entry,
            .length = WGPU_STRLEN,
        },
        .bufferCount = 5,
        .buffers = main_vertex_buffers,
    };

    WGPUColorTargetState main_color_target = {
        .format = color_format,
        .writeMask = WGPUColorWriteMask_All,
    };

    WGPUFragmentState main_fragment_state = {
        .module = g_shader_modules[GM_SHADER_MAIN_FS],
        .entryPoint = {
            .data = main_fs_entry,
            .length = WGPU_STRLEN,
        },
        .targetCount = 1,
        .targets = &main_color_target,
    };

    WGPUPrimitiveState main_primitive_state = {
        .topology = WGPUPrimitiveTopology_TriangleList,
        .stripIndexFormat = WGPUIndexFormat_Undefined,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_None,
    };

    WGPUMultisampleState multisample_state = {
        .count = 1,
        .mask = 0xFFFFFFFFu,
        .alphaToCoverageEnabled = WGPU_FALSE,
    };

    WGPUDepthStencilState main_depth_state = {
        .format = WGPUTextureFormat_Depth24Plus,
        .depthWriteEnabled = WGPU_TRUE,
        .depthCompare = WGPUCompareFunction_Less,
    };

    WGPURenderPipelineDescriptor main_desc = {
        .layout = g_pipeline_layouts[GM_PIPELINE_LAYOUT_MAIN],
        .vertex = main_vertex_state,
        .primitive = main_primitive_state,
        .depthStencil = &main_depth_state,
        .multisample = multisample_state,
        .fragment = &main_fragment_state,
    };

    g_render_pipelines[GM_RENDER_PIPELINE_MAIN] = wgpuDeviceCreateRenderPipeline(
            g_wgpu_device, &main_desc);
    if (g_render_pipelines[GM_RENDER_PIPELINE_MAIN] == NULL) {
        return -4;
    }

    const char *overlay_vs_entry = "vertex_main";
    const char *overlay_fs_entry = "fragment_main";

    WGPUVertexAttribute overlay_attribute = {
        .format = WGPUVertexFormat_Float32x2,
        .offset = 0,
        .shaderLocation = 0,
    };

    WGPUVertexBufferLayout overlay_buffer_layout = {
        .arrayStride = sizeof(float) * 2,
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 1,
        .attributes = &overlay_attribute,
    };

    WGPUVertexState overlay_vertex_state = {
        .module = g_shader_modules[GM_SHADER_OVERLAY_VS],
        .entryPoint = {
            .data = overlay_vs_entry,
            .length = WGPU_STRLEN,
        },
        .bufferCount = 1,
        .buffers = &overlay_buffer_layout,
    };

    WGPUBlendState overlay_blend = {
        .color = {
            .srcFactor = WGPUBlendFactor_SrcAlpha,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            .operation = WGPUBlendOperation_Add,
        },
        .alpha = {
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            .operation = WGPUBlendOperation_Add,
        },
    };

    WGPUColorTargetState overlay_target = {
        .format = color_format,
        .blend = &overlay_blend,
        .writeMask = WGPUColorWriteMask_All,
    };

    WGPUFragmentState overlay_fragment_state = {
        .module = g_shader_modules[GM_SHADER_OVERLAY_FS],
        .entryPoint = {
            .data = overlay_fs_entry,
            .length = WGPU_STRLEN,
        },
        .targetCount = 1,
        .targets = &overlay_target,
    };

    WGPUPrimitiveState overlay_primitive_state = {
        .topology = WGPUPrimitiveTopology_TriangleStrip,
        .stripIndexFormat = WGPUIndexFormat_Undefined,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_None,
    };

    WGPUDepthStencilState overlay_depth_state = {
        .format = WGPUTextureFormat_Depth24Plus,
        .depthWriteEnabled = WGPU_FALSE,
        .depthCompare = WGPUCompareFunction_Always,
    };

    WGPURenderPipelineDescriptor overlay_desc = {
        .layout = g_pipeline_layouts[GM_PIPELINE_LAYOUT_OVERLAY],
        .vertex = overlay_vertex_state,
        .primitive = overlay_primitive_state,
        .depthStencil = &overlay_depth_state,
        .multisample = multisample_state,
        .fragment = &overlay_fragment_state,
    };

    g_render_pipelines[GM_RENDER_PIPELINE_OVERLAY] = wgpuDeviceCreateRenderPipeline(
            g_wgpu_device, &overlay_desc);
    if (g_render_pipelines[GM_RENDER_PIPELINE_OVERLAY] == NULL) {
        return -5;
    }

    return 0;
}

int gm_create_bind_groups(void) {
    if (g_wgpu_device == NULL) {
        return -1;
    }
    if (g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_MAIN] == NULL ||
            g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_OVERLAY] == NULL) {
        return -2;
    }
    if (g_wall_texture_view == NULL || g_floor_texture_view == NULL ||
            g_ceiling_texture_view == NULL) {
        return -3;
    }

    if (g_main_sampler == NULL) {
        WGPUSamplerDescriptor sampler_desc = {
            .magFilter = WGPUFilterMode_Linear,
            .minFilter = WGPUFilterMode_Linear,
            .mipmapFilter = WGPUMipmapFilterMode_Linear,
            .addressModeU = WGPUAddressMode_Repeat,
            .addressModeV = WGPUAddressMode_Repeat,
            .addressModeW = WGPUAddressMode_Repeat,
        };
        g_main_sampler = wgpuDeviceCreateSampler(g_wgpu_device, &sampler_desc);
        if (g_main_sampler == NULL) {
            return -4;
        }
    }

    const uint64_t uniform_buffer_size = gm_get_uniform_buffer_size();
    WGPUBindGroupEntry main_entries[5] = {
        {
            .binding = 0,
            .buffer = g_gpu_buffers[GM_BUFFER_UNIFORM],
            .offset = 0,
            .size = uniform_buffer_size,
        },
        {
            .binding = 1,
            .sampler = g_main_sampler,
        },
        {
            .binding = 2,
            .textureView = g_wall_texture_view,
        },
        {
            .binding = 3,
            .textureView = g_floor_texture_view,
        },
        {
            .binding = 4,
            .textureView = g_ceiling_texture_view,
        },
    };

    WGPUBindGroupDescriptor main_desc = {
        .layout = g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_MAIN],
        .entryCount = 5,
        .entries = main_entries,
    };

    g_bind_groups[GM_BIND_GROUP_MAIN] = wgpuDeviceCreateBindGroup(
            g_wgpu_device, &main_desc);
    if (g_bind_groups[GM_BIND_GROUP_MAIN] == NULL) {
        return -5;
    }

    const uint64_t overlay_uniform_size = (uint64_t)GM_OVERLAY_UNIFORM_FLOAT_COUNT * sizeof(float);
    const uint64_t overlay_text_size = (uint64_t)GM_OVERLAY_TEXT_CAPACITY * sizeof(uint32_t);
    const uint64_t overlay_map_size = (uint64_t)GM_MAP_CELL_COUNT * sizeof(uint32_t);

    WGPUBindGroupEntry overlay_entries[3] = {
        {
            .binding = 0,
            .buffer = g_gpu_buffers[GM_BUFFER_OVERLAY_UNIFORM],
            .offset = 0,
            .size = overlay_uniform_size,
        },
        {
            .binding = 1,
            .buffer = g_gpu_buffers[GM_BUFFER_OVERLAY_TEXT],
            .offset = 0,
            .size = overlay_text_size,
        },
        {
            .binding = 2,
            .buffer = g_gpu_buffers[GM_BUFFER_OVERLAY_MAP],
            .offset = 0,
            .size = overlay_map_size,
        },
    };

    WGPUBindGroupDescriptor overlay_desc = {
        .layout = g_bind_group_layouts[GM_BIND_GROUP_LAYOUT_OVERLAY],
        .entryCount = 3,
        .entries = overlay_entries,
    };

    g_bind_groups[GM_BIND_GROUP_OVERLAY] = wgpuDeviceCreateBindGroup(
            g_wgpu_device, &overlay_desc);
    if (g_bind_groups[GM_BIND_GROUP_OVERLAY] == NULL) {
        return -6;
    }

    return 0;
}

uint32_t gm_get_bind_group_table(void) {
    return (uint32_t)(uintptr_t)g_bind_groups;
}

uint32_t gm_get_bind_group_count(void) {
    return GM_BIND_GROUP_COUNT;
}

uint32_t gm_get_render_pipeline_table(void) {
    return (uint32_t)(uintptr_t)g_render_pipelines;
}

uint32_t gm_get_render_pipeline_count(void) {
    return GM_RENDER_PIPELINE_COUNT;
}

// ============================================================================
// Game Logic Implementation
// ============================================================================

// Overlay text constants
#define GLYPH_WIDTH 8
#define GLYPH_HEIGHT 8
#define GLYPH_SPACING 0
#define LINE_SPACING 2
#define TEXT_SCALE 4.0f
#define PANEL_PADDING_X 12.0f
#define PANEL_PADDING_Y 12.0f
#define PANEL_MARGIN 10.0f
#define MAP_SCALE 12.0f
#define MAP_GAP 12.0f
#define NEWLINE_CODE 255
#define SPACE_CODE 32
#define PERF_SMOOTHING 0.9f

// Static buffers for overlay text and uniforms
static uint32_t g_overlay_text_buffer[GM_OVERLAY_TEXT_CAPACITY];
static float g_uniform_buffer[GM_UNIFORM_FLOAT_COUNT];
static float g_overlay_uniform_buffer[GM_OVERLAY_UNIFORM_FLOAT_COUNT];

// Convert character to glyph code
static inline uint32_t char_to_glyph_code(char ch) {
    return (uint32_t)(uint8_t)ch;
}

// Utility: clamp pitch to valid range
static inline float clamp_pitch(float pitch) {
    const float max_pitch = PI / 2.0f - 0.01f;
    if (pitch < -max_pitch) return -max_pitch;
    if (pitch > max_pitch) return max_pitch;
    return pitch;
}

// Utility: clamp float to range
static inline float clampf(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

// Collision detection: check if position is walkable
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

// Initialize game state
void gm_init_game_state(GameState *state, int *map, int width, int height,
                       float start_x, float start_z, float start_yaw) {
    // Zero out the struct
    for (int i = 0; i < (int)sizeof(GameState); i++) {
        ((uint8_t*)state)[i] = 0;
    }

    // Camera state
    state->camera_x = start_x;
    state->camera_y = 1.0f;  // person_height
    state->camera_z = start_z;
    state->yaw = start_yaw;
    state->pitch = 0.0f;
    state->target_yaw = start_yaw;
    state->target_pitch = 0.0f;

    // Movement parameters
    state->person_height = 1.0f;
    state->turn_speed = 0.03f;
    state->mouse_sensitivity = 0.002f;
    state->orientation_smoothing = 0.35f;
    state->fov = PI / 3.0f;
    state->move_speed = 0.1f;
    state->collision_radius = 0.2f;

    // Game flags
    state->map_visible = 0;
    state->map_relative_mode = 0;
    state->hud_visible = 1;
    state->textures_enabled = 0;
    state->triangle_mode = 0;
    state->debug_mode = 0;
    state->horizontal_movement = 1;

    // Map data
    state->map_data = map;
    state->map_width = width;
    state->map_height = height;

    // Performance tracking
    state->fps = 0.0f;
    state->avg_frame_time = 0.0f;
    state->avg_js_time = 0.0f;
    state->avg_gpu_copy_time = 0.0f;
    state->avg_gpu_render_time = 0.0f;
    state->frame_count = 0;
    state->last_fps_update_time = 0.0;
    state->fps_frame_count = 0;
}

// Set key state
void gm_set_key_state(GameState *state, uint8_t key_code, int pressed) {
    state->keys[key_code] = pressed ? 1 : 0;
}

// Add mouse delta
void gm_add_mouse_delta(GameState *state, float dx, float dy) {
    state->mouse_delta_x = dx;
    state->mouse_delta_y = dy;
}

// Update camera based on input
static void update_camera(GameState *state) {
    // Apply mouse smoothing
    float yaw_delta = state->target_yaw - state->yaw;
    float pitch_delta = state->target_pitch - state->pitch;
    state->yaw += yaw_delta * state->orientation_smoothing;
    state->pitch += pitch_delta * state->orientation_smoothing;

    // Arrow key rotation
    int arrow_used = 0;
    float yaw_delta_arrows = 0.0f;
    if (state->keys['<']) {  // ArrowLeft is mapped to '<' by JS
        yaw_delta_arrows -= state->turn_speed;
        arrow_used = 1;
    }
    if (state->keys['>']) {  // ArrowRight is mapped to '>' by JS
        yaw_delta_arrows += state->turn_speed;
        arrow_used = 1;
    }
    if (yaw_delta_arrows != 0.0f) {
        state->yaw += yaw_delta_arrows;
        state->target_yaw = state->yaw;
    }

    // Arrow forward/backward movement
    float arrow_forward = 0.0f;
    if (state->keys['^']) {  // ArrowUp is mapped to '^' by JS
        arrow_forward += state->move_speed;
        arrow_used = 1;
    }
    if (state->keys['v']) {  // ArrowDown is mapped to 'v' by JS (lowercase)
        arrow_forward -= state->move_speed;
        arrow_used = 1;
    }

    // Reset pitch if arrow keys used
    if (arrow_used) {
        state->pitch = 0.0f;
        state->target_pitch = 0.0f;
    }

    // Clamp pitch
    state->pitch = clamp_pitch(state->pitch);
    state->target_pitch = clamp_pitch(state->target_pitch);

    // Calculate direction vectors
    float cos_yaw = cosf(state->yaw);
    float sin_yaw = sinf(state->yaw);
    float cos_pitch = cosf(state->pitch);
    float sin_pitch = sinf(state->pitch);

    float dx = 0.0f, dy = 0.0f, dz = 0.0f;

    // Forward direction
    float forward_x, forward_y, forward_z;
    if (state->horizontal_movement) {
        forward_x = cos_yaw;
        forward_y = 0.0f;
        forward_z = sin_yaw;
    } else {
        forward_x = cos_pitch * cos_yaw;
        forward_y = sin_pitch;
        forward_z = cos_pitch * sin_yaw;
    }

    // WASD movement
    if (state->keys['w']) {
        dx += forward_x * state->move_speed;
        dy += forward_y * state->move_speed;
        dz += forward_z * state->move_speed;
    }
    if (state->keys['s']) {
        dx -= forward_x * state->move_speed;
        dy -= forward_y * state->move_speed;
        dz -= forward_z * state->move_speed;
    }
    if (state->keys['a']) {
        dx += sin_yaw * state->move_speed;
        dz -= cos_yaw * state->move_speed;
    }
    if (state->keys['d']) {
        dx -= sin_yaw * state->move_speed;
        dz += cos_yaw * state->move_speed;
    }

    // Arrow forward movement
    if (arrow_forward != 0.0f) {
        dx += cos_yaw * arrow_forward;
        dz += sin_yaw * arrow_forward;
    }

    // Apply movement with collision detection
    float base_y = arrow_used ? state->person_height : state->camera_y;
    float new_y = base_y + dy;

    float candidate_x = state->camera_x + dx;
    float candidate_z = state->camera_z + dz;

    if (is_walkable(state, candidate_x, state->camera_z)) {
        state->camera_x = candidate_x;
    }
    if (is_walkable(state, state->camera_x, candidate_z)) {
        state->camera_z = candidate_z;
    }

    state->camera_y = clampf(new_y, 0.1f, WALL_HEIGHT - 0.1f);
    if (arrow_used) {
        state->camera_y = state->person_height;
    }
}

// Per-frame update
void gm_update_frame(GameState *state, float canvas_width, float canvas_height) {
    (void)canvas_width;
    (void)canvas_height;

    // Update camera from target based on mouse delta
    state->target_yaw += state->mouse_delta_x * state->mouse_sensitivity;
    state->target_pitch = clamp_pitch(state->target_pitch - state->mouse_delta_y * state->mouse_sensitivity);

    // Reset mouse delta for next frame
    state->mouse_delta_x = 0.0f;
    state->mouse_delta_y = 0.0f;

    // Update camera position
    update_camera(state);

    state->frame_count++;
}

// Get uniform data for main rendering
const float* gm_get_uniform_data(const GameState *state, float canvas_width, float canvas_height) {
    g_uniform_buffer[0] = state->camera_x;
    g_uniform_buffer[1] = state->camera_y;
    g_uniform_buffer[2] = state->camera_z;
    g_uniform_buffer[3] = state->yaw;
    g_uniform_buffer[4] = state->pitch;
    g_uniform_buffer[5] = state->fov;
    g_uniform_buffer[6] = canvas_width;
    g_uniform_buffer[7] = canvas_height;
    g_uniform_buffer[8] = state->textures_enabled ? 1.0f : 0.0f;
    g_uniform_buffer[9] = state->triangle_mode ? 1.0f : 0.0f;
    g_uniform_buffer[10] = state->debug_mode ? 1.0f : 0.0f;
    g_uniform_buffer[11] = 0.0f;

    return g_uniform_buffer;
}

// Build overlay text from game state
static OverlayTextResult g_overlay_text_result;

const OverlayTextResult* gm_build_overlay_text(const GameState *state) {
    Scratch scratch = scratch_begin();

    // Calculate direction from yaw
    const char *direction_names[] = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
    int direction_index = (int)((state->yaw / PI * 4.0f) + 8.5f) % 8;
    const char *direction = direction_names[direction_index];

    // Convert angles to degrees
    double yaw_deg = state->yaw * 180.0 / PI;
    double pitch_deg = state->pitch * 180.0 / PI;

    // Format text lines
    string lines[6];
    lines[0] = format(scratch.arena, str_lit("FPS: {}  Frame: {:.2}ms"),
                     (int64_t)state->fps, (double)state->avg_frame_time);
    lines[1] = format(scratch.arena, str_lit("JS: {:.2}ms  GPU Copy: {:.2}ms  GPU Render: {:.2}ms"),
                     (double)state->avg_js_time, (double)state->avg_gpu_copy_time, (double)state->avg_gpu_render_time);
    lines[2] = format(scratch.arena, str_lit("Pos: ({:.2}, {:.2}, {:.2})"),
                     (double)state->camera_x, (double)state->camera_y, (double)state->camera_z);
    lines[3] = format(scratch.arena, str_lit("Dir: {} (yaw: {:.1}°, pitch: {:.1}°)"),
                     str_from_cstr_view((char*)direction), yaw_deg, pitch_deg);
    lines[4] = format(scratch.arena, str_lit("Movement: {} (f)  HUD: {} (h)  Map: {} (m/r)"),
                     str_from_cstr_view(state->horizontal_movement ? "Person" : "Flying"),
                     str_from_cstr_view(state->hud_visible ? "ON" : "OFF"),
                     str_from_cstr_view(state->map_visible ? "ON" : "OFF"));
    lines[5] = format(scratch.arena, str_lit("Textures: {} (t)  Triangles: {} (v)  Debug: {} (b)"),
                     str_from_cstr_view(state->textures_enabled ? "ON" : "OFF"),
                     str_from_cstr_view(state->triangle_mode ? "ON" : "OFF"),
                     str_from_cstr_view(state->debug_mode ? "ON" : "OFF"));

    // Build overlay text buffer
    uint32_t index = 0;
    uint32_t max_line_length = 0;

    for (uint32_t i = 0; i < GM_OVERLAY_TEXT_CAPACITY; i++) {
        g_overlay_text_buffer[i] = SPACE_CODE;
    }

    for (int i = 0; i < 6; i++) {
        if (lines[i].size > max_line_length) {
            max_line_length = lines[i].size;
        }
        for (uint32_t j = 0; j < lines[i].size && index < GM_OVERLAY_TEXT_CAPACITY; j++) {
            g_overlay_text_buffer[index++] = char_to_glyph_code(lines[i].str[j]);
        }
        if (i < 5 && index < GM_OVERLAY_TEXT_CAPACITY) {
            g_overlay_text_buffer[index++] = NEWLINE_CODE;
        }
    }

    scratch_end(scratch);

    g_overlay_text_result.glyph_data = g_overlay_text_buffer;
    g_overlay_text_result.length = index;
    g_overlay_text_result.max_line_length = max_line_length;
    return &g_overlay_text_result;
}

// Get overlay uniform data
const float* gm_get_overlay_uniform_data(const GameState *state, float canvas_width,
                                         float canvas_height, uint32_t text_length,
                                         uint32_t max_line_length) {
    // Calculate text and panel dimensions
    uint32_t line_count = 6;  // Approximate
    float char_advance = GLYPH_WIDTH * TEXT_SCALE;
    float text_width = max_line_length * char_advance;
    float text_height = line_count * GLYPH_HEIGHT * TEXT_SCALE +
                       (line_count > 1 ? (line_count - 1) * LINE_SPACING * TEXT_SCALE : 0.0f);

    float panel_origin_x = PANEL_MARGIN;
    float panel_origin_y = PANEL_MARGIN;
    float map_width_pixels = state->map_width * MAP_SCALE;
    float map_height_pixels = state->map_height * MAP_SCALE;

    float panel_width = PANEL_PADDING_X * 2.0f + text_width;
    if (state->map_visible) {
        if (map_width_pixels > panel_width) {
            panel_width = PANEL_PADDING_X * 2.0f + map_width_pixels;
        }
    }

    float panel_height = PANEL_PADDING_Y * 2.0f + text_height;
    float map_origin_x = panel_origin_x + PANEL_PADDING_X;
    float map_origin_y = panel_origin_y + PANEL_PADDING_Y + text_height +
                        (state->map_visible ? MAP_GAP : 0.0f);
    if (state->map_visible) {
        panel_height += MAP_GAP + map_height_pixels;
    }

    g_overlay_uniform_buffer[0] = canvas_width;
    g_overlay_uniform_buffer[1] = canvas_height;
    g_overlay_uniform_buffer[2] = panel_origin_x;
    g_overlay_uniform_buffer[3] = panel_origin_y;
    g_overlay_uniform_buffer[4] = panel_width;
    g_overlay_uniform_buffer[5] = panel_height;
    g_overlay_uniform_buffer[6] = TEXT_SCALE;
    g_overlay_uniform_buffer[7] = GLYPH_SPACING;
    g_overlay_uniform_buffer[8] = LINE_SPACING;
    g_overlay_uniform_buffer[9] = text_length;
    g_overlay_uniform_buffer[10] = PANEL_PADDING_X;
    g_overlay_uniform_buffer[11] = PANEL_PADDING_Y;
    g_overlay_uniform_buffer[12] = map_origin_x;
    g_overlay_uniform_buffer[13] = map_origin_y;
    g_overlay_uniform_buffer[14] = MAP_SCALE;
    g_overlay_uniform_buffer[15] = state->map_visible ? 1.0f : 0.0f;
    g_overlay_uniform_buffer[16] = state->map_relative_mode ? 1.0f : 0.0f;
    g_overlay_uniform_buffer[17] = (float)state->map_width;
    g_overlay_uniform_buffer[18] = (float)state->map_height;
    g_overlay_uniform_buffer[19] = 0.0f;
    g_overlay_uniform_buffer[20] = state->camera_x;
    g_overlay_uniform_buffer[21] = state->camera_z;
    g_overlay_uniform_buffer[22] = state->yaw;
    g_overlay_uniform_buffer[23] = state->hud_visible ? 1.0f : 0.0f;

    return g_overlay_uniform_buffer;
}

// Update performance metrics
void gm_update_perf_metrics(GameState *state, float frame_time, float js_time,
                           float gpu_copy_time, float gpu_render_time) {
    state->avg_frame_time = state->avg_frame_time * PERF_SMOOTHING + frame_time * (1.0f - PERF_SMOOTHING);
    state->avg_js_time = state->avg_js_time * PERF_SMOOTHING + js_time * (1.0f - PERF_SMOOTHING);
    state->avg_gpu_copy_time = state->avg_gpu_copy_time * PERF_SMOOTHING + gpu_copy_time * (1.0f - PERF_SMOOTHING);
    state->avg_gpu_render_time = state->avg_gpu_render_time * PERF_SMOOTHING + gpu_render_time * (1.0f - PERF_SMOOTHING);
}

// ============================================================================
// Main Render Loop
// ============================================================================

static GameState *g_game_state = NULL;
static GameState g_static_game_state;  // Static instance for WASM to use

// Get a pointer to the static game state (for JavaScript to use)
#ifdef __wasm__
__attribute__((export_name("gm_get_game_state_ptr")))
#endif
GameState* gm_get_game_state_ptr(void) {
    return &g_static_game_state;
}

// Main render frame function - called every frame
void gm_render_frame(GameState *state) {
    // Debug: log first frame
    static int first_frame = 1;
    if (first_frame) {
        writeln(WASI_STDERR_FD, "[C] gm_render_frame called");
        first_frame = 0;
    }

    double frame_start_time = platform_get_time();

    // Calculate FPS every 500ms
    state->fps_frame_count++;
    double time_since_fps_update = frame_start_time - state->last_fps_update_time;
    if (time_since_fps_update >= 500.0) {
        state->fps = (float)(state->fps_frame_count * 1000.0 / time_since_fps_update);
        state->fps_frame_count = 0;
        state->last_fps_update_time = frame_start_time;
    }

    // Get canvas size
    int canvas_width, canvas_height;
    platform_get_canvas_size(&canvas_width, &canvas_height);

    // Update game state
    gm_update_frame(state, (float)canvas_width, (float)canvas_height);

    // Get uniform data
    const float *uniform_data = gm_get_uniform_data(state, (float)canvas_width, (float)canvas_height);

    // Build overlay text
    const OverlayTextResult *overlay_text = gm_build_overlay_text(state);

    // Get overlay uniform data
    const float *overlay_uniform_data = gm_get_overlay_uniform_data(
        state, (float)canvas_width, (float)canvas_height,
        overlay_text->length, overlay_text->max_line_length);

    // Render via platform and get timing
    double js_time, gpu_copy_time, gpu_render_time;
    double total_frame_time = platform_render_frame(
        (uint32_t)(uintptr_t)uniform_data,
        (uint32_t)(uintptr_t)overlay_uniform_data,
        (uint32_t)(uintptr_t)overlay_text->glyph_data,
        overlay_text->length,
        &js_time, &gpu_copy_time, &gpu_render_time);

    // Update performance metrics
    gm_update_perf_metrics(state, (float)total_frame_time, (float)js_time,
                          (float)gpu_copy_time, (float)gpu_render_time);

    // Debug: log every 60 frames
    static int frame_debug_counter = 0;
    frame_debug_counter++;
    if (frame_debug_counter == 60) {
        writeln(WASI_STDERR_FD, "[C] 60 frames rendered");
        frame_debug_counter = 0;
    }

    // Request next frame
    platform_request_animation_frame();
}

// Callback for platform to call on each frame
void gm_on_animation_frame(void) {
    if (g_game_state) {
        gm_render_frame(g_game_state);
    }
}

// Set the active game state
void gm_set_active_game_state(GameState *state) {
    g_game_state = state;
}

// Default map definition
// 1 = wall, 2 = wall with north/south window, 3 = wall with east/west window
// 0 = floor, 5-8 = starting position with direction
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

// Flatten the default map to 1D array  
static int g_default_map_flattened[MAP_HEIGHT * MAP_WIDTH];

// Static buffer for temporary allocations (e.g., for JavaScript to use as output parameters)
static uint8_t g_temp_buffer[4096];

// Export the default map data for JavaScript to use
#ifdef __wasm__
__attribute__((export_name("gm_get_default_map")))
#endif
int* gm_get_default_map(void) {
    // Flatten the map on first call
    static int initialized = 0;
    if (!initialized) {
        for (int z = 0; z < MAP_HEIGHT; z++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                g_default_map_flattened[z * MAP_WIDTH + x] = g_default_map[z][x];
            }
        }
        initialized = 1;
    }
    return g_default_map_flattened;
}

// Export a temporary buffer for JavaScript to use for output parameters
#ifdef __wasm__
__attribute__((export_name("gm_get_temp_buffer")))
#endif
void* gm_get_temp_buffer(void) {
    return g_temp_buffer;
}

// Export map dimensions
#ifdef __wasm__
__attribute__((export_name("gm_get_map_width")))
#endif
int gm_get_map_width(void) {
    return MAP_WIDTH;
}

#ifdef __wasm__
__attribute__((export_name("gm_get_map_height")))
#endif
int gm_get_map_height(void) {
    return MAP_HEIGHT;
}

// Main entry point - called from JavaScript after WASM is loaded
void gm_main(void) {
    // Initialize the default map
    gm_get_default_map();  // Ensure it's flattened
    // The rest of initialization happens in JavaScript via exported functions
    // since it needs to interact with WebGPU
}
