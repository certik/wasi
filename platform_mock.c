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

uint32_t platform_get_resize_flag(void) {
    FATAL_ERROR("platform_get_resize_flag is not supported on native builds");
    return 0;
}

int platform_get_visibility(void) {
    FATAL_ERROR("platform_get_visibility is not supported on native builds");
    return 0;
}

int platform_get_host_config(GMHostConfig *config) {
    (void)config;
    FATAL_ERROR("platform_get_host_config is not supported on native builds");
    return 0;
}

void platform_get_input_state(GMInputSnapshot *snapshot) {
    (void)snapshot;
    FATAL_ERROR("platform_get_input_state is not supported on native builds");
}
