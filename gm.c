#include <gm.h>
#include <webgpu/webgpu.h>

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
