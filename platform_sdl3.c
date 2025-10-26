#include "platform.h"
#include "gm.h"
#include <base/base_types.h>
#include <SDL3/SDL.h>

// ============================================================================
// Platform State (shared with gm_sdl.c)
// ============================================================================

typedef struct {
    SDL_Window *window;
    int window_width;
    int window_height;
    uint32_t resize_flag;
    int is_visible;
    double start_time_ms;

    // WebGPU handles
    uintptr_t device_handle;
    uintptr_t queue_handle;
    uint32_t preferred_color_format;
    int host_config_ready;

    // Input state
    uint8_t key_states[256];
    float mouse_delta_x;
    float mouse_delta_y;
} PlatformState;

static PlatformState g_platform_state = {0};

// ============================================================================
// Platform State Management (called from gm_sdl.c)
// ============================================================================

void platform_sdl3_init(SDL_Window *window) {
    g_platform_state.window = window;
    SDL_GetWindowSize(window, &g_platform_state.window_width, &g_platform_state.window_height);
    g_platform_state.resize_flag = 0;
    g_platform_state.is_visible = 1;
    g_platform_state.start_time_ms = (double)SDL_GetTicks();
    g_platform_state.host_config_ready = 0;

    // Clear input state
    for (int i = 0; i < 256; i++) {
        g_platform_state.key_states[i] = 0;
    }
    g_platform_state.mouse_delta_x = 0.0f;
    g_platform_state.mouse_delta_y = 0.0f;
}

void platform_sdl3_set_host_config(uintptr_t device, uintptr_t queue, uint32_t color_format) {
    g_platform_state.device_handle = device;
    g_platform_state.queue_handle = queue;
    g_platform_state.preferred_color_format = color_format;
    g_platform_state.host_config_ready = 1;
}

void platform_sdl3_on_window_resized(int width, int height) {
    g_platform_state.window_width = width;
    g_platform_state.window_height = height;
    g_platform_state.resize_flag++;
}

void platform_sdl3_on_window_minimized(void) {
    g_platform_state.is_visible = 0;
}

void platform_sdl3_on_window_restored(void) {
    g_platform_state.is_visible = 1;
}

void platform_sdl3_set_key_state(uint8_t key_code, int pressed) {
    // key_code is uint8_t, so always < 256
    g_platform_state.key_states[key_code] = pressed ? 1 : 0;
}

void platform_sdl3_add_mouse_delta(float dx, float dy) {
    g_platform_state.mouse_delta_x += dx;
    g_platform_state.mouse_delta_y += dy;
}

void platform_sdl3_reset_mouse_delta(void) {
    g_platform_state.mouse_delta_x = 0.0f;
    g_platform_state.mouse_delta_y = 0.0f;
}

// ============================================================================
// Platform API Implementation (platform.h)
// ============================================================================

void platform_get_canvas_size(int *width, int *height) {
    *width = g_platform_state.window_width;
    *height = g_platform_state.window_height;
}

double platform_get_time(void) {
    double current_time = (double)SDL_GetTicks();
    return current_time - g_platform_state.start_time_ms;
}

uint32_t platform_get_resize_flag(void) {
    return g_platform_state.resize_flag;
}

int platform_get_visibility(void) {
    return g_platform_state.is_visible;
}

int platform_get_host_config(GMHostConfig *config) {
    if (!g_platform_state.host_config_ready) {
        return 0;
    }

    config->device_handle = g_platform_state.device_handle;
    config->queue_handle = g_platform_state.queue_handle;
    config->preferred_color_format = g_platform_state.preferred_color_format;
    return 1;
}

void platform_get_input_state(GMInputSnapshot *snapshot) {
    // Copy key states
    for (int i = 0; i < 256; i++) {
        snapshot->key_states[i] = g_platform_state.key_states[i];
    }

    // Copy mouse delta
    snapshot->mouse_delta_x = g_platform_state.mouse_delta_x;
    snapshot->mouse_delta_y = g_platform_state.mouse_delta_y;
}

// ============================================================================
// Texture Loading (Stub implementations for now)
// ============================================================================

uint32_t platform_request_texture_load(const char* url, uint32_t url_len) {
    (void)url;
    (void)url_len;
    // TODO: Implement async texture loading using SDL_Image or stb_image
    return 0;  // Return 0 = failure for now
}

int platform_poll_texture_load(uint32_t request_handle, uint32_t *texture_view_handle_out) {
    (void)request_handle;
    (void)texture_view_handle_out;
    // TODO: Implement texture loading polling
    return -1;  // Return -1 = error (not implemented)
}

void platform_cancel_texture_load(uint32_t request_handle) {
    (void)request_handle;
    // TODO: Implement texture loading cancellation
}
