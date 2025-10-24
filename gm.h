#pragma once

#include <base/base_types.h>

// Map dimensions
#define MAP_WIDTH 10
#define MAP_HEIGHT 10

// Map cell types
// 0 = floor, 1 = wall, 2 = wall with north/south window, 3 = wall with east/west window
// 5-8 = starting position with direction: 5=North, 6=East, 7=South, 8=West

// Math constants
#define PI 3.14159265358979323846

// Mesh generation constants
#define WALL_HEIGHT 2.0f
#define CHECKER_SIZE 4.0f

// Mesh data structure returned by generate_mesh
// All arrays are allocated in a flat memory region for easy WASM access
typedef struct {
    float *positions;       // Vertex positions (x,y,z), size = position_count * 3
    float *uvs;            // Texture coordinates (u,v), size = uv_count * 2
    float *normals;        // Surface normals (x,y,z), size = normal_count * 3
    float *surface_types;  // Surface type per vertex, size = vertex_count
    float *triangle_ids;   // Triangle ID per vertex, size = vertex_count
    uint16_t *indices;     // Triangle indices, size = index_count

    uint32_t position_count;  // Number of floats in positions array
    uint32_t uv_count;        // Number of floats in uvs array
    uint32_t normal_count;    // Number of floats in normals array
    uint32_t vertex_count;    // Number of vertices (for surface_types and triangle_ids)
    uint32_t index_count;     // Number of indices
} MeshData;

// Find starting position and direction in the map
// Returns: 1 if found, 0 if not found
// Outputs: startX, startZ (position), startYaw (direction in radians), and modifies map
int find_start_position(int *map, int width, int height,
                       float *startX, float *startZ, float *startYaw);

// Generate mesh geometry from map
// Returns: Pointer to MeshData structure in WASM linear memory
// Note: The returned pointer and all internal arrays point to WASM memory
MeshData* generate_mesh(int *map, int width, int height);

// Returns the preferred WebGPU canvas texture format for the current platform.
uint32_t gm_get_preferred_canvas_format(void);

// WebGPU integration helpers
void gm_register_webgpu_handles(uint32_t device_handle, uint32_t queue_handle);
int gm_create_gpu_buffers(void);
uint32_t gm_get_gpu_buffer_table(void);
uint32_t gm_get_uniform_float_count(void);
uint32_t gm_get_uniform_buffer_size(void);
