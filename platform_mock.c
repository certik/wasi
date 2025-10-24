#include "platform.h"
#include <base/base_types.h>
#include <base/exit.h>

// Mock implementations of platform API for native builds
// These functions should only be called in WASM builds where JavaScript provides the implementation

void platform_get_canvas_size(int *width, int *height) {
    (void)width;
    (void)height;
    FATAL_ERROR("platform_get_canvas_size is not supported on native builds");
}

double platform_get_time(void) {
    FATAL_ERROR("platform_get_time is not supported on native builds");
    return 0.0;
}

void platform_request_animation_frame(void) {
    FATAL_ERROR("platform_request_animation_frame is not supported on native builds");
}

double platform_render_frame(uint32_t uniformDataPtr, uint32_t overlayUniformDataPtr,
                              uint32_t overlayTextDataPtr, uint32_t overlayTextLength,
                              double *js_time_out, double *gpu_copy_time_out,
                              double *gpu_render_time_out) {
    (void)uniformDataPtr;
    (void)overlayUniformDataPtr;
    (void)overlayTextDataPtr;
    (void)overlayTextLength;
    (void)js_time_out;
    (void)gpu_copy_time_out;
    (void)gpu_render_time_out;
    FATAL_ERROR("platform_render_frame is not supported on native builds");
    return 0.0;
}
