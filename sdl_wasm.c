// SDL3 WebAssembly Implementation
// Provides SDL3 GPU API bindings to JavaScript/WebGPU

#include "SDL3/SDL.h"
#include <stdint.h>
#include <stddef.h>

#define WASM_IMPORT(module, name) __attribute__((import_module(module), import_name(name)))

// JavaScript host functions providing SDL3 GPU API
WASM_IMPORT("sdl", "init")
uint32_t sdl_host_init(uint32_t flags);

WASM_IMPORT("sdl", "quit")
void sdl_host_quit(void);

WASM_IMPORT("sdl", "create_gpu_device")
uint32_t sdl_host_create_gpu_device(uint32_t shader_format, uint32_t debug);

WASM_IMPORT("sdl", "destroy_gpu_device")
void sdl_host_destroy_gpu_device(uint32_t device);

WASM_IMPORT("sdl", "create_window")
uint32_t sdl_host_create_window(uint32_t title_ptr, uint32_t title_len, int32_t w, int32_t h, uint32_t flags);

WASM_IMPORT("sdl", "destroy_window")
void sdl_host_destroy_window(uint32_t window);

WASM_IMPORT("sdl", "claim_window_for_gpu_device")
uint32_t sdl_host_claim_window_for_gpu_device(uint32_t device, uint32_t window);

WASM_IMPORT("sdl", "release_window_from_gpu_device")
void sdl_host_release_window_from_gpu_device(uint32_t device, uint32_t window);

WASM_IMPORT("sdl", "get_window_size_in_pixels")
void sdl_host_get_window_size_in_pixels(uint32_t window, uint32_t w_ptr, uint32_t h_ptr);

WASM_IMPORT("sdl", "create_gpu_shader")
uint32_t sdl_host_create_gpu_shader(uint32_t device, uint32_t info_ptr);

WASM_IMPORT("sdl", "release_gpu_shader")
void sdl_host_release_gpu_shader(uint32_t device, uint32_t shader);

WASM_IMPORT("sdl", "get_gpu_swapchain_texture_format")
uint32_t sdl_host_get_gpu_swapchain_texture_format(uint32_t device, uint32_t window);

WASM_IMPORT("sdl", "create_gpu_graphics_pipeline")
uint32_t sdl_host_create_gpu_graphics_pipeline(uint32_t device, uint32_t info_ptr);

WASM_IMPORT("sdl", "release_gpu_graphics_pipeline")
void sdl_host_release_gpu_graphics_pipeline(uint32_t device, uint32_t pipeline);

WASM_IMPORT("sdl", "acquire_gpu_command_buffer")
uint32_t sdl_host_acquire_gpu_command_buffer(uint32_t device);

WASM_IMPORT("sdl", "wait_and_acquire_gpu_swapchain_texture")
uint32_t sdl_host_wait_and_acquire_gpu_swapchain_texture(uint32_t cmdbuf, uint32_t window, uint32_t texture_out_ptr, uint32_t w_ptr, uint32_t h_ptr);

WASM_IMPORT("sdl", "begin_gpu_render_pass")
uint32_t sdl_host_begin_gpu_render_pass(uint32_t cmdbuf, uint32_t color_targets_ptr, uint32_t num_color_targets, uint32_t depth_stencil_ptr);

WASM_IMPORT("sdl", "end_gpu_render_pass")
void sdl_host_end_gpu_render_pass(uint32_t pass);

WASM_IMPORT("sdl", "bind_gpu_graphics_pipeline")
void sdl_host_bind_gpu_graphics_pipeline(uint32_t pass, uint32_t pipeline);

WASM_IMPORT("sdl", "push_gpu_fragment_uniform_data")
void sdl_host_push_gpu_fragment_uniform_data(uint32_t cmdbuf, uint32_t slot, uint32_t data_ptr, uint32_t length);

WASM_IMPORT("sdl", "draw_gpu_primitives")
void sdl_host_draw_gpu_primitives(uint32_t pass, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);

WASM_IMPORT("sdl", "submit_gpu_command_buffer")
void sdl_host_submit_gpu_command_buffer(uint32_t cmdbuf);

WASM_IMPORT("sdl", "poll_event")
uint32_t sdl_host_poll_event(uint32_t event_ptr);

WASM_IMPORT("sdl", "get_mouse_state")
uint32_t sdl_host_get_mouse_state(uint32_t x_ptr, uint32_t y_ptr);

WASM_IMPORT("sdl", "get_error")
uint32_t sdl_host_get_error(uint32_t buffer_ptr, uint32_t buffer_size);

WASM_IMPORT("sdl", "log")
void sdl_host_log(uint32_t msg_ptr, uint32_t msg_len);

// String utilities
static size_t wasm_strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

// SDL3 API implementations
bool SDL_Init(Uint32 flags) {
    return sdl_host_init(flags) != 0;
}

void SDL_Quit(void) {
    sdl_host_quit();
}

SDL_GPUDevice* SDL_CreateGPUDevice(SDL_GPUShaderFormat format, bool debug, const char* name) {
    (void)name; // Ignored for now
    uint32_t handle = sdl_host_create_gpu_device((uint32_t)format, debug ? 1 : 0);
    return (SDL_GPUDevice*)(uintptr_t)handle;
}

void SDL_DestroyGPUDevice(SDL_GPUDevice* device) {
    sdl_host_destroy_gpu_device((uint32_t)(uintptr_t)device);
}

SDL_Window* SDL_CreateWindow(const char* title, int w, int h, Uint32 flags) {
    uint32_t title_len = title ? wasm_strlen(title) : 0;
    uint32_t handle = sdl_host_create_window(
        (uint32_t)(uintptr_t)title,
        title_len,
        w, h, flags
    );
    return (SDL_Window*)(uintptr_t)handle;
}

void SDL_DestroyWindow(SDL_Window* window) {
    sdl_host_destroy_window((uint32_t)(uintptr_t)window);
}

bool SDL_ClaimWindowForGPUDevice(SDL_GPUDevice* device, SDL_Window* window) {
    return sdl_host_claim_window_for_gpu_device(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)window
    ) != 0;
}

void SDL_ReleaseWindowFromGPUDevice(SDL_GPUDevice* device, SDL_Window* window) {
    sdl_host_release_window_from_gpu_device(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)window
    );
}

bool SDL_GetWindowSizeInPixels(SDL_Window* window, int* w, int* h) {
    sdl_host_get_window_size_in_pixels(
        (uint32_t)(uintptr_t)window,
        (uint32_t)(uintptr_t)w,
        (uint32_t)(uintptr_t)h
    );
    return true;
}

SDL_GPUShader* SDL_CreateGPUShader(SDL_GPUDevice* device, const SDL_GPUShaderCreateInfo* info) {
    uint32_t handle = sdl_host_create_gpu_shader(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)info
    );
    return (SDL_GPUShader*)(uintptr_t)handle;
}

void SDL_ReleaseGPUShader(SDL_GPUDevice* device, SDL_GPUShader* shader) {
    sdl_host_release_gpu_shader(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)shader
    );
}

SDL_GPUTextureFormat SDL_GetGPUSwapchainTextureFormat(SDL_GPUDevice* device, SDL_Window* window) {
    return (SDL_GPUTextureFormat)sdl_host_get_gpu_swapchain_texture_format(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)window
    );
}

SDL_GPUGraphicsPipeline* SDL_CreateGPUGraphicsPipeline(SDL_GPUDevice* device, const SDL_GPUGraphicsPipelineCreateInfo* info) {
    uint32_t handle = sdl_host_create_gpu_graphics_pipeline(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)info
    );
    return (SDL_GPUGraphicsPipeline*)(uintptr_t)handle;
}

void SDL_ReleaseGPUGraphicsPipeline(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline) {
    sdl_host_release_gpu_graphics_pipeline(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)pipeline
    );
}

SDL_GPUCommandBuffer* SDL_AcquireGPUCommandBuffer(SDL_GPUDevice* device) {
    uint32_t handle = sdl_host_acquire_gpu_command_buffer((uint32_t)(uintptr_t)device);
    return (SDL_GPUCommandBuffer*)(uintptr_t)handle;
}

bool SDL_WaitAndAcquireGPUSwapchainTexture(SDL_GPUCommandBuffer* cmdbuf, SDL_Window* window, SDL_GPUTexture** texture, Uint32* w, Uint32* h) {
    return sdl_host_wait_and_acquire_gpu_swapchain_texture(
        (uint32_t)(uintptr_t)cmdbuf,
        (uint32_t)(uintptr_t)window,
        (uint32_t)(uintptr_t)texture,
        (uint32_t)(uintptr_t)w,
        (uint32_t)(uintptr_t)h
    ) != 0;
}

SDL_GPURenderPass* SDL_BeginGPURenderPass(SDL_GPUCommandBuffer* cmdbuf, const SDL_GPUColorTargetInfo* colorTargets, Uint32 numColorTargets, void* depthStencilTarget) {
    uint32_t handle = sdl_host_begin_gpu_render_pass(
        (uint32_t)(uintptr_t)cmdbuf,
        (uint32_t)(uintptr_t)colorTargets,
        numColorTargets,
        (uint32_t)(uintptr_t)depthStencilTarget
    );
    return (SDL_GPURenderPass*)(uintptr_t)handle;
}

void SDL_EndGPURenderPass(SDL_GPURenderPass* pass) {
    sdl_host_end_gpu_render_pass((uint32_t)(uintptr_t)pass);
}

void SDL_BindGPUGraphicsPipeline(SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline) {
    sdl_host_bind_gpu_graphics_pipeline(
        (uint32_t)(uintptr_t)pass,
        (uint32_t)(uintptr_t)pipeline
    );
}

void SDL_PushGPUFragmentUniformData(SDL_GPUCommandBuffer* cmdbuf, Uint32 slot, const void* data, Uint32 length) {
    sdl_host_push_gpu_fragment_uniform_data(
        (uint32_t)(uintptr_t)cmdbuf,
        slot,
        (uint32_t)(uintptr_t)data,
        length
    );
}

void SDL_DrawGPUPrimitives(SDL_GPURenderPass* pass, Uint32 vertexCount, Uint32 instanceCount, Uint32 firstVertex, Uint32 firstInstance) {
    sdl_host_draw_gpu_primitives(
        (uint32_t)(uintptr_t)pass,
        vertexCount,
        instanceCount,
        firstVertex,
        firstInstance
    );
}

void SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer* cmdbuf) {
    sdl_host_submit_gpu_command_buffer((uint32_t)(uintptr_t)cmdbuf);
}

bool SDL_PollEvent(SDL_Event* event) {
    return sdl_host_poll_event((uint32_t)(uintptr_t)event) != 0;
}

Uint32 SDL_GetMouseState(float* x, float* y) {
    return sdl_host_get_mouse_state(
        (uint32_t)(uintptr_t)x,
        (uint32_t)(uintptr_t)y
    );
}

const char* SDL_GetError(void) {
    static char error_buffer[256];
    sdl_host_get_error((uint32_t)(uintptr_t)error_buffer, sizeof(error_buffer));
    return error_buffer;
}

void SDL_Log(const char* fmt, ...) {
    // For now, just pass the format string directly
    // A full implementation would need to handle variadic args
    if (fmt) {
        uint32_t len = wasm_strlen(fmt);
        sdl_host_log((uint32_t)(uintptr_t)fmt, len);
    }
}

size_t SDL_strlen(const char* str) {
    return wasm_strlen(str);
}
