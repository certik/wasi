#include "platform.h"
#include "gm.h"
#include <base/base_types.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <webgpu/webgpu.h>
#include <string.h>
#include <stdlib.h>

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
// Texture Loading
// ============================================================================

#define MAX_TEXTURE_REQUESTS 16

typedef struct {
    int active;
    WGPUTextureView texture_view;
    WGPUTexture texture;
} TextureRequest;

static TextureRequest g_texture_requests[MAX_TEXTURE_REQUESTS] = {0};
static uint32_t g_next_request_handle = 1;

// Extract filename from URL (simple version - just get the last path component)
static const char* extract_filename(const char* url) {
    const char* last_slash = strrchr(url, '/');
    return last_slash ? last_slash + 1 : url;
}

uint32_t platform_request_texture_load(const char* url, uint32_t url_len) {
    if (!url || url_len == 0 || !g_platform_state.host_config_ready) {
        return 0;
    }

    // Find free slot
    uint32_t slot = 0;
    for (uint32_t i = 0; i < MAX_TEXTURE_REQUESTS; i++) {
        if (!g_texture_requests[i].active) {
            slot = i;
            break;
        }
    }

    // Extract filename from URL
    const char* filename = extract_filename(url);

    // Load image using SDL_image
    SDL_Surface* surface = IMG_Load(filename);
    if (!surface) {
        return 0;
    }

    // Convert to RGBA if needed
    SDL_Surface* rgba_surface = surface;
    if (surface->format != SDL_PIXELFORMAT_RGBA32 &&
        surface->format != SDL_PIXELFORMAT_ABGR8888) {
        rgba_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if (!rgba_surface) {
            return 0;
        }
    }

    // Create WebGPU texture
    WGPUDevice device = (WGPUDevice)g_platform_state.device_handle;
    WGPUQueue queue = (WGPUQueue)g_platform_state.queue_handle;

    WGPUTextureDescriptor texture_desc = {
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = {rgba_surface->w, rgba_surface->h, 1},
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texture_desc);
    if (!texture) {
        SDL_DestroySurface(rgba_surface);
        return 0;
    }

    // Upload texture data
    WGPUTexelCopyTextureInfo destination = {
        .texture = texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All,
    };

    WGPUTexelCopyBufferLayout data_layout = {
        .offset = 0,
        .bytesPerRow = 4 * rgba_surface->w,
        .rowsPerImage = rgba_surface->h,
    };

    WGPUExtent3D write_size = {rgba_surface->w, rgba_surface->h, 1};

    wgpuQueueWriteTexture(queue, &destination, rgba_surface->pixels,
                         rgba_surface->h * rgba_surface->pitch,
                         &data_layout, &write_size);

    // Create texture view
    WGPUTextureViewDescriptor view_desc = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    };

    WGPUTextureView texture_view = wgpuTextureCreateView(texture, &view_desc);

    SDL_DestroySurface(rgba_surface);

    if (!texture_view) {
        wgpuTextureRelease(texture);
        return 0;
    }

    // Store in slot
    g_texture_requests[slot].active = 1;
    g_texture_requests[slot].texture = texture;
    g_texture_requests[slot].texture_view = texture_view;

    uint32_t handle = g_next_request_handle++;
    return handle ? ((slot + 1) | (handle << 16)) : 0;
}

int platform_poll_texture_load(uint32_t request_handle, uint32_t *texture_view_handle_out) {
    if (request_handle == 0 || !texture_view_handle_out) {
        return -1;
    }

    // Extract slot from handle
    uint32_t slot = (request_handle & 0xFFFF) - 1;
    if (slot >= MAX_TEXTURE_REQUESTS || !g_texture_requests[slot].active) {
        return -1;
    }

    // Texture is ready (synchronous loading)
    *texture_view_handle_out = (uint32_t)(uintptr_t)g_texture_requests[slot].texture_view;
    return 1;  // Ready
}

void platform_cancel_texture_load(uint32_t request_handle) {
    if (request_handle == 0) {
        return;
    }

    uint32_t slot = (request_handle & 0xFFFF) - 1;
    if (slot >= MAX_TEXTURE_REQUESTS || !g_texture_requests[slot].active) {
        return;
    }

    // Clean up
    if (g_texture_requests[slot].texture_view) {
        wgpuTextureViewRelease(g_texture_requests[slot].texture_view);
    }
    if (g_texture_requests[slot].texture) {
        wgpuTextureRelease(g_texture_requests[slot].texture);
    }

    g_texture_requests[slot].active = 0;
    g_texture_requests[slot].texture_view = NULL;
    g_texture_requests[slot].texture = NULL;
}
