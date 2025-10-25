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

int platform_get_host_config(GMHostConfig *config) {
    (void)config;
    FATAL_ERROR("platform_get_host_config is not supported on native builds");
    return 0;
}

void platform_register_uniform_info(uint32_t uniform_float_count,
        uint32_t overlay_uniform_float_count, uint32_t overlay_text_capacity,
        uint32_t map_cell_count) {
    (void)uniform_float_count;
    (void)overlay_uniform_float_count;
    (void)overlay_text_capacity;
    (void)map_cell_count;
    FATAL_ERROR("platform_register_uniform_info is not supported on native builds");
}

void platform_register_gpu_buffers(uint32_t handles_ptr, uint32_t count) {
    (void)handles_ptr;
    (void)count;
    FATAL_ERROR("platform_register_gpu_buffers is not supported on native builds");
}

void platform_register_bind_groups(uint32_t handles_ptr, uint32_t count) {
    (void)handles_ptr;
    (void)count;
    FATAL_ERROR("platform_register_bind_groups is not supported on native builds");
}

void platform_register_render_pipelines(uint32_t handles_ptr, uint32_t count) {
    (void)handles_ptr;
    (void)count;
    FATAL_ERROR("platform_register_render_pipelines is not supported on native builds");
}

void platform_register_mesh_info(uint32_t vertex_count, uint32_t index_count) {
    (void)vertex_count;
    (void)index_count;
    FATAL_ERROR("platform_register_mesh_info is not supported on native builds");
}

void platform_get_input_state(GMInputSnapshot *snapshot) {
    (void)snapshot;
    FATAL_ERROR("platform_get_input_state is not supported on native builds");
}
