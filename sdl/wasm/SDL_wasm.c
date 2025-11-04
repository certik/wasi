// SDL3 WebAssembly Implementation
// Provides SDL3 GPU API bindings to JavaScript/WebGPU

#include "SDL3/SDL.h"
#include <stdint.h>
#include <stddef.h>
#include <base/stdarg.h>
#include <base/numconv.h>
#include <base/mem.h>

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

WASM_IMPORT("sdl", "push_gpu_vertex_uniform_data")
void sdl_host_push_gpu_vertex_uniform_data(uint32_t cmdbuf, uint32_t slot, uint32_t data_ptr, uint32_t length);

WASM_IMPORT("sdl", "draw_gpu_indexed_primitives")
void sdl_host_draw_gpu_indexed_primitives(uint32_t pass, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);

WASM_IMPORT("sdl", "create_gpu_buffer")
uint32_t sdl_host_create_gpu_buffer(uint32_t device, uint32_t info_ptr);

WASM_IMPORT("sdl", "release_gpu_buffer")
void sdl_host_release_gpu_buffer(uint32_t device, uint32_t buffer);

WASM_IMPORT("sdl", "create_gpu_transfer_buffer")
uint32_t sdl_host_create_gpu_transfer_buffer(uint32_t device, uint32_t info_ptr);

WASM_IMPORT("sdl", "release_gpu_transfer_buffer")
void sdl_host_release_gpu_transfer_buffer(uint32_t device, uint32_t transfer_buffer);

WASM_IMPORT("sdl", "map_gpu_transfer_buffer")
uint32_t sdl_host_map_gpu_transfer_buffer(uint32_t device, uint32_t transfer_buffer, uint32_t cycle);

WASM_IMPORT("sdl", "unmap_gpu_transfer_buffer")
void sdl_host_unmap_gpu_transfer_buffer(uint32_t device, uint32_t transfer_buffer);

WASM_IMPORT("sdl", "begin_gpu_copy_pass")
uint32_t sdl_host_begin_gpu_copy_pass(uint32_t cmdbuf);

WASM_IMPORT("sdl", "upload_to_gpu_buffer")
void sdl_host_upload_to_gpu_buffer(uint32_t copy_pass, uint32_t source_ptr, uint32_t destination_ptr, uint32_t cycle);

WASM_IMPORT("sdl", "end_gpu_copy_pass")
void sdl_host_end_gpu_copy_pass(uint32_t copy_pass);

WASM_IMPORT("sdl", "bind_gpu_vertex_buffers")
void sdl_host_bind_gpu_vertex_buffers(uint32_t pass, uint32_t first_slot, uint32_t bindings_ptr, uint32_t num_bindings);

WASM_IMPORT("sdl", "bind_gpu_index_buffer")
void sdl_host_bind_gpu_index_buffer(uint32_t pass, uint32_t binding_ptr, uint32_t index_element_size);

WASM_IMPORT("sdl", "create_gpu_texture")
uint32_t sdl_host_create_gpu_texture(uint32_t device, uint32_t info_ptr);

WASM_IMPORT("sdl", "release_gpu_texture")
void sdl_host_release_gpu_texture(uint32_t device, uint32_t texture);

WASM_IMPORT("sdl", "create_gpu_sampler")
uint32_t sdl_host_create_gpu_sampler(uint32_t device, uint32_t info_ptr);

WASM_IMPORT("sdl", "release_gpu_sampler")
void sdl_host_release_gpu_sampler(uint32_t device, uint32_t sampler);

WASM_IMPORT("sdl", "bind_gpu_vertex_samplers")
void sdl_host_bind_gpu_vertex_samplers(uint32_t pass, uint32_t first_slot, uint32_t bindings_ptr, uint32_t num_bindings);

WASM_IMPORT("sdl", "bind_gpu_fragment_samplers")
void sdl_host_bind_gpu_fragment_samplers(uint32_t pass, uint32_t first_slot, uint32_t bindings_ptr, uint32_t num_bindings);

WASM_IMPORT("sdl", "set_window_relative_mouse_mode")
uint32_t sdl_host_set_window_relative_mouse_mode(uint32_t window, uint32_t enabled);

WASM_IMPORT("sdl", "get_ticks")
uint32_t sdl_host_get_ticks(void);

WASM_IMPORT("sdl", "get_asset_image_info")
uint32_t sdl_host_get_asset_image_info(uint32_t path_ptr, uint32_t path_len, uint32_t width_ptr, uint32_t height_ptr);

WASM_IMPORT("sdl", "copy_asset_image_rgba")
uint32_t sdl_host_copy_asset_image_rgba(uint32_t path_ptr, uint32_t path_len, uint32_t dest_ptr, uint32_t dest_len);

WASM_IMPORT("sdl", "upload_to_gpu_texture")
void sdl_host_upload_to_gpu_texture(uint32_t copy_pass, uint32_t source_ptr, uint32_t destination_ptr, uint32_t cycle);

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

const char* SDL_GetGPUDeviceDriver(SDL_GPUDevice* device) {
    (void)device;
    return "wgsl";
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

SDL_GPURenderPass* SDL_BeginGPURenderPass(SDL_GPUCommandBuffer* cmdbuf, const SDL_GPUColorTargetInfo* colorTargets, Uint32 numColorTargets, const SDL_GPUDepthStencilTargetInfo* depthStencilTarget) {
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
    if (!fmt) return;

    char buffer[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len > 0 && len < (int)sizeof(buffer)) {
        sdl_host_log((uint32_t)(uintptr_t)buffer, (uint32_t)len);
    }
}

size_t SDL_strlen(const char* str) {
    return wasm_strlen(str);
}

void* SDL_memcpy(void* dst, const void* src, size_t len) {
    return base_memcpy(dst, src, len);
}

// Managed main bridging for WASM builds
static void *wasm_appstate = NULL;
static SDL_AppResult wasm_last_result = SDL_APP_SUCCESS;

__attribute__((export_name("app_init")))
int app_init(void) {
    void *state = NULL;
    SDL_AppResult result = SDL_AppInit(&state, 0, NULL);
    wasm_last_result = result;

    if (result == SDL_APP_FAILURE) {
        wasm_appstate = NULL;
        return -1;
    }

    if (result == SDL_APP_SUCCESS) {
        SDL_AppQuit(state, result);
        wasm_appstate = NULL;
        return 1;
    }

    wasm_appstate = state;
    return 0;
}

static int process_pending_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        SDL_AppResult event_result = SDL_AppEvent(wasm_appstate, &event);
        if (event_result != SDL_APP_CONTINUE) {
            wasm_last_result = event_result;
            return (event_result == SDL_APP_SUCCESS) ? 1 : -1;
        }
    }
    return 0;
}

__attribute__((export_name("app_iterate")))
int app_iterate(void) {
    if (!wasm_appstate) {
        return -1;
    }

    int event_status = process_pending_events();
    if (event_status != 0) {
        return event_status;
    }

    SDL_AppResult result = SDL_AppIterate(wasm_appstate);
    if (result == SDL_APP_CONTINUE) {
        return 0;
    }

    wasm_last_result = result;
    return (result == SDL_APP_SUCCESS) ? 1 : -1;
}

__attribute__((export_name("app_quit")))
int app_quit(void) {
    if (!wasm_appstate) {
        return (wasm_last_result == SDL_APP_SUCCESS) ? 0 : -1;
    }

    SDL_AppQuit(wasm_appstate, wasm_last_result);
    wasm_appstate = NULL;
    return (wasm_last_result == SDL_APP_SUCCESS) ? 0 : -1;
}

// New SDL GPU functions
void SDL_PushGPUVertexUniformData(SDL_GPUCommandBuffer* cmdbuf, Uint32 slot, const void* data, Uint32 length) {
    sdl_host_push_gpu_vertex_uniform_data(
        (uint32_t)(uintptr_t)cmdbuf,
        slot,
        (uint32_t)(uintptr_t)data,
        length
    );
}

void SDL_DrawGPUIndexedPrimitives(SDL_GPURenderPass* pass, Uint32 indexCount, Uint32 instanceCount, Uint32 firstIndex, Sint32 vertexOffset, Uint32 firstInstance) {
    sdl_host_draw_gpu_indexed_primitives(
        (uint32_t)(uintptr_t)pass,
        indexCount,
        instanceCount,
        firstIndex,
        vertexOffset,
        firstInstance
    );
}

SDL_GPUBuffer* SDL_CreateGPUBuffer(SDL_GPUDevice* device, const SDL_GPUBufferCreateInfo* info) {
    uint32_t handle = sdl_host_create_gpu_buffer(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)info
    );
    return (SDL_GPUBuffer*)(uintptr_t)handle;
}

void SDL_ReleaseGPUBuffer(SDL_GPUDevice* device, SDL_GPUBuffer* buffer) {
    sdl_host_release_gpu_buffer(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)buffer
    );
}

SDL_GPUTransferBuffer* SDL_CreateGPUTransferBuffer(SDL_GPUDevice* device, const SDL_GPUTransferBufferCreateInfo* info) {
    uint32_t handle = sdl_host_create_gpu_transfer_buffer(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)info
    );
    return (SDL_GPUTransferBuffer*)(uintptr_t)handle;
}

void SDL_ReleaseGPUTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer) {
    sdl_host_release_gpu_transfer_buffer(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)buffer
    );
}

void* SDL_MapGPUTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer, bool cycle) {
    uint32_t ptr = sdl_host_map_gpu_transfer_buffer(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)buffer,
        cycle ? 1 : 0
    );
    return (void*)(uintptr_t)ptr;
}

void SDL_UnmapGPUTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer) {
    sdl_host_unmap_gpu_transfer_buffer(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)buffer
    );
}

SDL_GPUCopyPass* SDL_BeginGPUCopyPass(SDL_GPUCommandBuffer* cmdbuf) {
    uint32_t handle = sdl_host_begin_gpu_copy_pass((uint32_t)(uintptr_t)cmdbuf);
    return (SDL_GPUCopyPass*)(uintptr_t)handle;
}

void SDL_UploadToGPUBuffer(SDL_GPUCopyPass* copy_pass, const SDL_GPUTransferBufferLocation* source, const SDL_GPUBufferRegion* destination, bool cycle) {
    sdl_host_upload_to_gpu_buffer(
        (uint32_t)(uintptr_t)copy_pass,
        (uint32_t)(uintptr_t)source,
        (uint32_t)(uintptr_t)destination,
        cycle ? 1 : 0
    );
}

void SDL_UploadToGPUTexture(SDL_GPUCopyPass* copy_pass, const SDL_GPUTextureTransferInfo* source, const SDL_GPUTextureRegion* destination, bool cycle) {
    sdl_host_upload_to_gpu_texture(
        (uint32_t)(uintptr_t)copy_pass,
        (uint32_t)(uintptr_t)source,
        (uint32_t)(uintptr_t)destination,
        cycle ? 1 : 0
    );
}

void SDL_EndGPUCopyPass(SDL_GPUCopyPass* copy_pass) {
    sdl_host_end_gpu_copy_pass((uint32_t)(uintptr_t)copy_pass);
}

void SDL_BindGPUVertexBuffers(SDL_GPURenderPass* pass, Uint32 firstSlot, const SDL_GPUBufferBinding* bindings, Uint32 numBindings) {
    sdl_host_bind_gpu_vertex_buffers(
        (uint32_t)(uintptr_t)pass,
        firstSlot,
        (uint32_t)(uintptr_t)bindings,
        numBindings
    );
}

void SDL_BindGPUIndexBuffer(SDL_GPURenderPass* pass, const SDL_GPUBufferBinding* binding, SDL_GPUIndexElementSize indexElementSize) {
    sdl_host_bind_gpu_index_buffer(
        (uint32_t)(uintptr_t)pass,
        (uint32_t)(uintptr_t)binding,
        (uint32_t)indexElementSize
    );
}

SDL_GPUTexture* SDL_CreateGPUTexture(SDL_GPUDevice* device, const SDL_GPUTextureCreateInfo* info) {
    uint32_t handle = sdl_host_create_gpu_texture(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)info
    );
    return (SDL_GPUTexture*)(uintptr_t)handle;
}

void SDL_ReleaseGPUTexture(SDL_GPUDevice* device, SDL_GPUTexture* texture) {
    sdl_host_release_gpu_texture(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)texture
    );
}

SDL_GPUSampler* SDL_CreateGPUSampler(SDL_GPUDevice* device, const SDL_GPUSamplerCreateInfo* info) {
    uint32_t handle = sdl_host_create_gpu_sampler(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)info
    );
    return (SDL_GPUSampler*)(uintptr_t)handle;
}

void SDL_ReleaseGPUSampler(SDL_GPUDevice* device, SDL_GPUSampler* sampler) {
    sdl_host_release_gpu_sampler(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)sampler
    );
}

void SDL_BindGPUVertexSamplers(SDL_GPURenderPass* pass, Uint32 firstSlot, const SDL_GPUTextureSamplerBinding* bindings, Uint32 numBindings) {
    sdl_host_bind_gpu_vertex_samplers(
        (uint32_t)(uintptr_t)pass,
        firstSlot,
        (uint32_t)(uintptr_t)bindings,
        numBindings
    );
}

void SDL_BindGPUFragmentSamplers(SDL_GPURenderPass* pass, Uint32 firstSlot, const SDL_GPUTextureSamplerBinding* bindings, Uint32 numBindings) {
    sdl_host_bind_gpu_fragment_samplers(
        (uint32_t)(uintptr_t)pass,
        firstSlot,
        (uint32_t)(uintptr_t)bindings,
        numBindings
    );
}

bool SDL_SetWindowRelativeMouseMode(SDL_Window* window, bool enabled) {
    return sdl_host_set_window_relative_mouse_mode(
        (uint32_t)(uintptr_t)window,
        enabled ? 1 : 0
    ) != 0;
}

// SDL_snprintf wrapper - calls base/ snprintf implementation
#include <base/numconv.h>
#include <base/stdarg.h>

int SDL_snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);

    // For now, we'll implement a simple version that forwards to snprintf
    // We need to reconstruct the call since we can't directly forward variadic args
    // This is a simplified implementation that handles the common cases

    if (size == 0) {
        va_end(args);
        return 0;
    }

    size_t pos = 0;
    const char* p = format;
    char temp_buf[32];

    while (*p && pos < size - 1) {
        if (*p == '%' && *(p + 1)) {
            p++;

            // Check for precision specifier
            int precision = -1;
            if (*p == '.') {
                p++;
                precision = 0;
                while (*p >= '0' && *p <= '9') {
                    precision = precision * 10 + (*p - '0');
                    p++;
                }
            }

            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);
                    size_t len = int_to_str(val, temp_buf);
                    size_t copy_len = (pos + len < size - 1) ? len : (size - 1 - pos);
                    for (size_t i = 0; i < copy_len; i++) {
                        str[pos++] = temp_buf[i];
                    }
                    break;
                }
                case 'f': {
                    double val = va_arg(args, double);
                    if (precision < 0) precision = 6;
                    size_t len = double_to_str(val, temp_buf, precision);
                    size_t copy_len = (pos + len < size - 1) ? len : (size - 1 - pos);
                    for (size_t i = 0; i < copy_len; i++) {
                        str[pos++] = temp_buf[i];
                    }
                    break;
                }
                case 's': {
                    char* s = va_arg(args, char*);
                    if (s == NULL) s = "(null)";
                    while (*s && pos < size - 1) {
                        str[pos++] = *s++;
                    }
                    break;
                }
                case '%': {
                    str[pos++] = '%';
                    break;
                }
                default:
                    break;
            }
            p++;
        } else {
            str[pos++] = *p++;
        }
    }

    str[pos] = '\0';
    va_end(args);
    return (int)pos;
}

Uint32 SDL_GetTicks(void) {
    return sdl_host_get_ticks();
}
