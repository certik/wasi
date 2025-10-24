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
