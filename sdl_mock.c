#include "SDL3/SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static const char* error_msg = "Mock SDL function called";

// Error handling
const char* SDL_GetError(void) {
    return error_msg;
}

void SDL_Log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "SDL_Log: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// Init/Quit
bool SDL_Init(Uint32 flags) {
    (void)flags;
    fprintf(stderr, "ERROR: SDL_Init called - mock implementation\n");
    return false;
}

void SDL_Quit(void) {
    fprintf(stderr, "ERROR: SDL_Quit called - mock implementation\n");
}

// GPU Device
SDL_GPUDevice* SDL_CreateGPUDevice(SDL_GPUShaderFormat format, bool debug, const char* name) {
    (void)format; (void)debug; (void)name;
    fprintf(stderr, "ERROR: SDL_CreateGPUDevice called - mock implementation\n");
    return NULL;
}

void SDL_DestroyGPUDevice(SDL_GPUDevice* device) {
    (void)device;
    fprintf(stderr, "ERROR: SDL_DestroyGPUDevice called - mock implementation\n");
}

// Window
SDL_Window* SDL_CreateWindow(const char* title, int w, int h, Uint32 flags) {
    (void)title; (void)w; (void)h; (void)flags;
    fprintf(stderr, "ERROR: SDL_CreateWindow called - mock implementation\n");
    return NULL;
}

void SDL_DestroyWindow(SDL_Window* window) {
    (void)window;
    fprintf(stderr, "ERROR: SDL_DestroyWindow called - mock implementation\n");
}

bool SDL_ClaimWindowForGPUDevice(SDL_GPUDevice* device, SDL_Window* window) {
    (void)device; (void)window;
    fprintf(stderr, "ERROR: SDL_ClaimWindowForGPUDevice called - mock implementation\n");
    return false;
}

void SDL_ReleaseWindowFromGPUDevice(SDL_GPUDevice* device, SDL_Window* window) {
    (void)device; (void)window;
    fprintf(stderr, "ERROR: SDL_ReleaseWindowFromGPUDevice called - mock implementation\n");
}

bool SDL_GetWindowSizeInPixels(SDL_Window* window, int* w, int* h) {
    (void)window;
    if (w) *w = 640;
    if (h) *h = 480;
    fprintf(stderr, "ERROR: SDL_GetWindowSizeInPixels called - mock implementation\n");
    return false;
}

// Shaders
SDL_GPUShader* SDL_CreateGPUShader(SDL_GPUDevice* device, const SDL_GPUShaderCreateInfo* info) {
    (void)device; (void)info;
    fprintf(stderr, "ERROR: SDL_CreateGPUShader called - mock implementation\n");
    return NULL;
}

void SDL_ReleaseGPUShader(SDL_GPUDevice* device, SDL_GPUShader* shader) {
    (void)device; (void)shader;
    fprintf(stderr, "ERROR: SDL_ReleaseGPUShader called - mock implementation\n");
}

// Pipeline
SDL_GPUTextureFormat SDL_GetGPUSwapchainTextureFormat(SDL_GPUDevice* device, SDL_Window* window) {
    (void)device; (void)window;
    fprintf(stderr, "ERROR: SDL_GetGPUSwapchainTextureFormat called - mock implementation\n");
    return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
}

SDL_GPUGraphicsPipeline* SDL_CreateGPUGraphicsPipeline(SDL_GPUDevice* device, const SDL_GPUGraphicsPipelineCreateInfo* info) {
    (void)device; (void)info;
    fprintf(stderr, "ERROR: SDL_CreateGPUGraphicsPipeline called - mock implementation\n");
    return NULL;
}

void SDL_ReleaseGPUGraphicsPipeline(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline) {
    (void)device; (void)pipeline;
    fprintf(stderr, "ERROR: SDL_ReleaseGPUGraphicsPipeline called - mock implementation\n");
}

// Command buffer
SDL_GPUCommandBuffer* SDL_AcquireGPUCommandBuffer(SDL_GPUDevice* device) {
    (void)device;
    fprintf(stderr, "ERROR: SDL_AcquireGPUCommandBuffer called - mock implementation\n");
    return NULL;
}

bool SDL_WaitAndAcquireGPUSwapchainTexture(SDL_GPUCommandBuffer* cmdbuf, SDL_Window* window, SDL_GPUTexture** texture, Uint32* w, Uint32* h) {
    (void)cmdbuf; (void)window;
    if (texture) *texture = NULL;
    if (w) *w = 0;
    if (h) *h = 0;
    fprintf(stderr, "ERROR: SDL_WaitAndAcquireGPUSwapchainTexture called - mock implementation\n");
    return false;
}

void SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer* cmdbuf) {
    (void)cmdbuf;
    fprintf(stderr, "ERROR: SDL_SubmitGPUCommandBuffer called - mock implementation\n");
}

// Render pass
SDL_GPURenderPass* SDL_BeginGPURenderPass(SDL_GPUCommandBuffer* cmdbuf, const SDL_GPUColorTargetInfo* colorTargets, Uint32 numColorTargets, void* depthStencilTarget) {
    (void)cmdbuf; (void)colorTargets; (void)numColorTargets; (void)depthStencilTarget;
    fprintf(stderr, "ERROR: SDL_BeginGPURenderPass called - mock implementation\n");
    return NULL;
}

void SDL_EndGPURenderPass(SDL_GPURenderPass* pass) {
    (void)pass;
    fprintf(stderr, "ERROR: SDL_EndGPURenderPass called - mock implementation\n");
}

void SDL_BindGPUGraphicsPipeline(SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline) {
    (void)pass; (void)pipeline;
    fprintf(stderr, "ERROR: SDL_BindGPUGraphicsPipeline called - mock implementation\n");
}

void SDL_PushGPUFragmentUniformData(SDL_GPUCommandBuffer* cmdbuf, Uint32 slot, const void* data, Uint32 length) {
    (void)cmdbuf; (void)slot; (void)data; (void)length;
    fprintf(stderr, "ERROR: SDL_PushGPUFragmentUniformData called - mock implementation\n");
}

void SDL_DrawGPUPrimitives(SDL_GPURenderPass* pass, Uint32 vertexCount, Uint32 instanceCount, Uint32 firstVertex, Uint32 firstInstance) {
    (void)pass; (void)vertexCount; (void)instanceCount; (void)firstVertex; (void)firstInstance;
    fprintf(stderr, "ERROR: SDL_DrawGPUPrimitives called - mock implementation\n");
}

SDL_GPUBuffer* SDL_CreateGPUBuffer(SDL_GPUDevice* device, const SDL_GPUBufferCreateInfo* info) {
    (void)device; (void)info;
    fprintf(stderr, "ERROR: SDL_CreateGPUBuffer called - mock implementation\n");
    return NULL;
}

void SDL_DestroyGPUBuffer(SDL_GPUDevice* device, SDL_GPUBuffer* buffer) {
    (void)device; (void)buffer;
    fprintf(stderr, "ERROR: SDL_DestroyGPUBuffer called - mock implementation\n");
}

void SDL_BindGPUVertexBuffer(SDL_GPURenderPass* pass, Uint32 slot, SDL_GPUBuffer* buffer, size_t offset, Uint32 stride) {
    (void)pass; (void)slot; (void)buffer; (void)offset; (void)stride;
    fprintf(stderr, "ERROR: SDL_BindGPUVertexBuffer called - mock implementation\n");
}

// Events
bool SDL_PollEvent(SDL_Event* event) {
    (void)event;
    fprintf(stderr, "ERROR: SDL_PollEvent called - mock implementation\n");
    return false;
}

Uint64 SDL_GetPerformanceCounter(void) {
    fprintf(stderr, "ERROR: SDL_GetPerformanceCounter called - mock implementation\n");
    return 0;
}

Uint64 SDL_GetPerformanceFrequency(void) {
    fprintf(stderr, "ERROR: SDL_GetPerformanceFrequency called - mock implementation\n");
    return 1;
}

Uint32 SDL_GetMouseState(float* x, float* y) {
    if (x) *x = 0.0f;
    if (y) *y = 0.0f;
    fprintf(stderr, "ERROR: SDL_GetMouseState called - mock implementation\n");
    return 0;
}

// String utilities
size_t SDL_strlen(const char* str) {
    return strlen(str);
}
