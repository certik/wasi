#include <gm.h>
#include <webgpu/webgpu.h>

#include "gm_shaders.inc"

#define GM_UNIFORM_FLOAT_COUNT 12

typedef enum {
    GM_BUFFER_POSITIONS = 0,
    GM_BUFFER_UVS,
    GM_BUFFER_SURFACE_TYPES,
    GM_BUFFER_TRIANGLE_IDS,
    GM_BUFFER_NORMALS,
    GM_BUFFER_INDICES,
    GM_BUFFER_UNIFORM,
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
