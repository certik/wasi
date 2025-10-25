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

// Game state and logic
typedef struct {
    // Camera state
    float camera_x, camera_y, camera_z;
    float yaw, pitch;
    float target_yaw, target_pitch;

    // Movement parameters
    float person_height;
    float turn_speed;
    float mouse_sensitivity;
    float orientation_smoothing;
    float fov;
    float move_speed;
    float collision_radius;

    // Game flags
    int map_visible;
    int map_relative_mode;
    int hud_visible;
    int textures_enabled;
    int triangle_mode;
    int debug_mode;
    int horizontal_movement;

    // Input state (keys are indexed by ASCII code, max 256)
    uint8_t keys[256];
    float mouse_delta_x;
    float mouse_delta_y;

    // Map data for collision detection
    int *map_data;
    int map_width;
    int map_height;

    // Performance tracking
    float fps;
    float avg_frame_time;
    float avg_js_time;
    float avg_gpu_copy_time;
    float avg_gpu_render_time;
    uint32_t frame_count;
    double last_fps_update_time;  // Time of last FPS calculation
    uint32_t fps_frame_count;     // Frames since last FPS calculation

    // Platform event tracking
    uint32_t last_resize_id;      // Last observed resize flag
} GameState;

// Initialize game state with starting position and map
void gm_init_game_state(GameState *state, int *map, int width, int height,
                        float start_x, float start_z, float start_yaw);

// Input management
void gm_set_key_state(GameState *state, uint8_t key_code, int pressed);
void gm_add_mouse_delta(GameState *state, float dx, float dy);

// Per-frame update
void gm_update_frame(GameState *state, float canvas_width, float canvas_height);

// Get uniform data for rendering (returns float array for main uniforms)
const float* gm_get_uniform_data(const GameState *state, float canvas_width, float canvas_height);

// Overlay text building (returns glyph array and metadata)
typedef struct {
    uint32_t *glyph_data;  // Array of glyph codes
    uint32_t length;       // Number of glyphs
    uint32_t max_line_length;  // Max chars per line
} OverlayTextResult;

const OverlayTextResult* gm_build_overlay_text(const GameState *state);

// Get overlay uniform data (returns float array for overlay uniforms)
const float* gm_get_overlay_uniform_data(const GameState *state, float canvas_width,
                                         float canvas_height, uint32_t text_length,
                                         uint32_t max_line_length);

// Update performance metrics
void gm_update_perf_metrics(GameState *state, float frame_time, float js_time,
                           float gpu_copy_time, float gpu_render_time);

// Main engine entry point invoked by the host each frame.
void gm_frame(void);
