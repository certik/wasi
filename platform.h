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

// Get resize flag (increments when canvas resizes)
PLATFORM_IMPORT("get_resize_flag")
uint32_t platform_get_resize_flag(void);

// Get visibility state (1 = visible, 0 = hidden)
PLATFORM_IMPORT("get_visibility")
int platform_get_visibility(void);

// ============================================================================
// Engine Callbacks (Host <-> Game Module)
// ============================================================================

// Fetch shared WebGPU handles, texture views, and preferred formats from host.
// Returns 1 when the host configuration has been populated, 0 if not ready yet.
PLATFORM_IMPORT("get_host_config")
int platform_get_host_config(GMHostConfig *config);

// Retrieve the current input snapshot (keyboard + mouse) from the host.
PLATFORM_IMPORT("get_input_state")
void platform_get_input_state(GMInputSnapshot *snapshot);
