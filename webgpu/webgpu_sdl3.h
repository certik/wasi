#pragma once

#include <webgpu/webgpu.h>

// ============================================================================
// WebGPU SDL3 Context Management
// ============================================================================

// Set the WebGPU device (must be called before rendering)
void webgpu_sdl3_set_device(WGPUDevice device);

// Set the WebGPU surface (must be called before rendering)
void webgpu_sdl3_set_surface(WGPUSurface surface);

// Cleanup WebGPU resources
void webgpu_sdl3_cleanup(void);
