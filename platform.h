#pragma once

#include <base/base_types.h>

// ============================================================================
// Platform API - SDL-like interface implemented by the host platform
// ============================================================================

// On WASM, these are imported from JavaScript
// On native platforms, these would be implemented using SDL2 or native APIs

#ifdef __wasm__
#define PLATFORM_IMPORT(name) __attribute__((import_module("platform"), import_name(name)))
#else
#define PLATFORM_IMPORT(name)
#endif

typedef struct GMHostConfig GMHostConfig;
typedef struct GMInputSnapshot GMInputSnapshot;

// ============================================================================
// Window / Canvas Management
// ============================================================================

// Get current canvas/window dimensions
PLATFORM_IMPORT("get_canvas_size")
void platform_get_canvas_size(int *width, int *height);

// ============================================================================
// Time / Performance
// ============================================================================

// Get current time in milliseconds (like performance.now())
PLATFORM_IMPORT("get_time")
double platform_get_time(void);

// ============================================================================
// Main Loop
// ============================================================================

// Request animation frame callback
// After calling this, platform will call gm_render_frame() on next frame
PLATFORM_IMPORT("request_animation_frame")
void platform_request_animation_frame(void);

// ============================================================================
// Rendering (GPU Command Encoding)
// ============================================================================

// Render main geometry pass and get timing information
// uniformDataPtr: pointer to uniform buffer data (float array)
// overlayUniformDataPtr: pointer to overlay uniform data (float array)
// overlayTextDataPtr: pointer to overlay text glyph data (uint32 array)
// overlayTextLength: number of glyphs in overlay text
// Returns: total frame time in milliseconds (from start of frame to end of GPU submit)
//
// Timing breakdown is calculated as:
// - JS time: time from frame start to GPU command encoding
// - GPU copy time: time for buffer uploads
// - GPU render time: time for command encoding and submit
PLATFORM_IMPORT("render_frame")
double platform_render_frame(uint32_t uniformDataPtr, uint32_t overlayUniformDataPtr,
                              uint32_t overlayTextDataPtr, uint32_t overlayTextLength,
                              double *js_time_out, double *gpu_copy_time_out,
                              double *gpu_render_time_out);

// ============================================================================
// Engine Callbacks (Host <-> Game Module)
// ============================================================================

// Fetch shared WebGPU handles, texture views, and preferred formats from host.
// Returns 1 when the host configuration has been populated, 0 if not ready yet.
PLATFORM_IMPORT("get_host_config")
int platform_get_host_config(GMHostConfig *config);

// Inform the host about uniform buffer sizes, overlay capacities, etc.
PLATFORM_IMPORT("register_uniform_info")
void platform_register_uniform_info(uint32_t uniform_float_count,
    uint32_t overlay_uniform_float_count, uint32_t overlay_text_capacity,
    uint32_t map_cell_count);

// Share GPU buffer handles with the host for later rendering operations.
PLATFORM_IMPORT("register_gpu_buffers")
void platform_register_gpu_buffers(uint32_t handles_ptr, uint32_t count);

// Share bind group handles with the host.
PLATFORM_IMPORT("register_bind_groups")
void platform_register_bind_groups(uint32_t handles_ptr, uint32_t count);

// Share render pipeline handles with the host.
PLATFORM_IMPORT("register_render_pipelines")
void platform_register_render_pipelines(uint32_t handles_ptr, uint32_t count);

// Provide mesh statistics (vertex/index counts) to the host for draw calls.
PLATFORM_IMPORT("register_mesh_info")
void platform_register_mesh_info(uint32_t vertex_count, uint32_t index_count);

// Retrieve the current input snapshot (keyboard + mouse) from the host.
PLATFORM_IMPORT("get_input_state")
void platform_get_input_state(GMInputSnapshot *snapshot);
