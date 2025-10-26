#pragma once

#include <webgpu/webgpu.h>

// ============================================================================
// WebGPU SDL3 Context Management
// ============================================================================

// Set the WebGPU device (must be called before rendering)
void webgpu_sdl3_set_device(WGPUDevice device);

// Set the WebGPU swap chain (must be called before rendering)
void webgpu_sdl3_set_swap_chain(WGPUSwapChain swap_chain);

// Cleanup WebGPU resources
void webgpu_sdl3_cleanup(void);
