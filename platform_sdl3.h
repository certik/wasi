#pragma once

#include <base/base_types.h>
#include <SDL3/SDL.h>

// ============================================================================
// Platform SDL3 Initialization and State Management
// ============================================================================

// Initialize platform state with SDL window
void platform_sdl3_init(SDL_Window *window);

// Set WebGPU host configuration
void platform_sdl3_set_host_config(uint32_t device, uint32_t queue, uint32_t color_format);

// Window event handlers
void platform_sdl3_on_window_resized(int width, int height);
void platform_sdl3_on_window_minimized(void);
void platform_sdl3_on_window_restored(void);

// Input event handlers
void platform_sdl3_set_key_state(uint8_t key_code, int pressed);
void platform_sdl3_add_mouse_delta(float dx, float dy);
void platform_sdl3_reset_mouse_delta(void);
