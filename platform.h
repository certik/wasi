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

// ============================================================================
// Texture Loading (Async)
// ============================================================================

// Request loading a texture from a URL.
// Returns: request_handle (>0) on success, 0 on failure
// The URL is read from WASM memory at the given pointer/length.
PLATFORM_IMPORT("request_texture_load")
uint32_t platform_request_texture_load(const char* url, uint32_t url_len);

// Poll the status of a texture load request.
// Returns:
//   0 = still loading
//   1 = ready (texture_view_handle_out is populated)
//  -1 = error (load failed)
// On success (1), writes the WebGPU texture view handle to *texture_view_handle_out
PLATFORM_IMPORT("poll_texture_load")
int platform_poll_texture_load(uint32_t request_handle, uint32_t *texture_view_handle_out);

// Cancel a pending texture load request and free resources.
// Safe to call even if request is already complete.
PLATFORM_IMPORT("cancel_texture_load")
void platform_cancel_texture_load(uint32_t request_handle);
