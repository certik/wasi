#ifndef SDL_H
#define SDL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Basic types
typedef uint8_t Uint8;
typedef uint32_t Uint32;
typedef int32_t Sint32;

// Opaque types
typedef struct SDL_Window SDL_Window;
typedef struct SDL_GPUDevice SDL_GPUDevice;
typedef struct SDL_GPUGraphicsPipeline SDL_GPUGraphicsPipeline;
typedef struct SDL_GPUShader SDL_GPUShader;
typedef struct SDL_GPUCommandBuffer SDL_GPUCommandBuffer;
typedef struct SDL_GPUTexture SDL_GPUTexture;
typedef struct SDL_GPURenderPass SDL_GPURenderPass;

// Color type
typedef struct SDL_FColor {
    float r;
    float g;
    float b;
    float a;
} SDL_FColor;

// Event types
typedef enum {
    SDL_EVENT_QUIT = 0x100,
    SDL_EVENT_KEY_DOWN = 0x300,
} SDL_EventType;

typedef struct SDL_KeyboardEvent {
    Uint32 type;
    Uint32 key;
} SDL_KeyboardEvent;

typedef union SDL_Event {
    Uint32 type;
    SDL_KeyboardEvent key;
} SDL_Event;

// Key codes
typedef enum {
    SDLK_ESCAPE = 27,
    SDLK_Q = 'q',
} SDL_Keycode;

// Init flags
#define SDL_INIT_VIDEO 0x00000020

// Window flags
#define SDL_WINDOW_RESIZABLE 0x00000020

// GPU enums
typedef enum {
    SDL_GPU_SHADERFORMAT_MSL,
} SDL_GPUShaderFormat;

typedef enum {
    SDL_GPU_SHADERSTAGE_VERTEX,
    SDL_GPU_SHADERSTAGE_FRAGMENT,
} SDL_GPUShaderStage;

typedef enum {
    SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
} SDL_GPUPrimitiveType;

typedef enum {
    SDL_GPU_LOADOP_CLEAR,
} SDL_GPULoadOp;

typedef enum {
    SDL_GPU_STOREOP_STORE,
} SDL_GPUStoreOp;

typedef enum {
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
} SDL_GPUTextureFormat;

// GPU structures
typedef struct SDL_GPUShaderCreateInfo {
    const Uint8* code;
    size_t code_size;
    const char* entrypoint;
    SDL_GPUShaderFormat format;
    SDL_GPUShaderStage stage;
    Uint32 num_samplers;
    Uint32 num_uniform_buffers;
    Uint32 num_storage_buffers;
    Uint32 num_storage_textures;
} SDL_GPUShaderCreateInfo;

typedef struct SDL_GPUColorTargetDescription {
    SDL_GPUTextureFormat format;
} SDL_GPUColorTargetDescription;

typedef struct SDL_GPUGraphicsPipelineTargetInfo {
    Uint32 num_color_targets;
    SDL_GPUColorTargetDescription* color_target_descriptions;
} SDL_GPUGraphicsPipelineTargetInfo;

typedef struct SDL_GPUGraphicsPipelineCreateInfo {
    SDL_GPUShader* vertex_shader;
    SDL_GPUShader* fragment_shader;
    SDL_GPUGraphicsPipelineTargetInfo target_info;
    SDL_GPUPrimitiveType primitive_type;
} SDL_GPUGraphicsPipelineCreateInfo;

typedef struct SDL_GPUColorTargetInfo {
    SDL_GPUTexture* texture;
    SDL_FColor clear_color;
    SDL_GPULoadOp load_op;
    SDL_GPUStoreOp store_op;
} SDL_GPUColorTargetInfo;

// Function declarations
bool SDL_Init(Uint32 flags);
void SDL_Quit(void);
const char* SDL_GetError(void);
void SDL_Log(const char* fmt, ...);

SDL_GPUDevice* SDL_CreateGPUDevice(SDL_GPUShaderFormat format, bool debug, const char* name);
void SDL_DestroyGPUDevice(SDL_GPUDevice* device);

SDL_Window* SDL_CreateWindow(const char* title, int w, int h, Uint32 flags);
void SDL_DestroyWindow(SDL_Window* window);
bool SDL_ClaimWindowForGPUDevice(SDL_GPUDevice* device, SDL_Window* window);
void SDL_ReleaseWindowFromGPUDevice(SDL_GPUDevice* device, SDL_Window* window);
bool SDL_GetWindowSizeInPixels(SDL_Window* window, int* w, int* h);

SDL_GPUShader* SDL_CreateGPUShader(SDL_GPUDevice* device, const SDL_GPUShaderCreateInfo* info);
void SDL_ReleaseGPUShader(SDL_GPUDevice* device, SDL_GPUShader* shader);

SDL_GPUTextureFormat SDL_GetGPUSwapchainTextureFormat(SDL_GPUDevice* device, SDL_Window* window);
SDL_GPUGraphicsPipeline* SDL_CreateGPUGraphicsPipeline(SDL_GPUDevice* device, const SDL_GPUGraphicsPipelineCreateInfo* info);
void SDL_ReleaseGPUGraphicsPipeline(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline);

SDL_GPUCommandBuffer* SDL_AcquireGPUCommandBuffer(SDL_GPUDevice* device);
bool SDL_WaitAndAcquireGPUSwapchainTexture(SDL_GPUCommandBuffer* cmdbuf, SDL_Window* window, SDL_GPUTexture** texture, Uint32* w, Uint32* h);
void SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer* cmdbuf);

SDL_GPURenderPass* SDL_BeginGPURenderPass(SDL_GPUCommandBuffer* cmdbuf, const SDL_GPUColorTargetInfo* colorTargets, Uint32 numColorTargets, void* depthStencilTarget);
void SDL_EndGPURenderPass(SDL_GPURenderPass* pass);
void SDL_BindGPUGraphicsPipeline(SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline);
void SDL_PushGPUFragmentUniformData(SDL_GPUCommandBuffer* cmdbuf, Uint32 slot, const void* data, Uint32 length);
void SDL_DrawGPUPrimitives(SDL_GPURenderPass* pass, Uint32 vertexCount, Uint32 instanceCount, Uint32 firstVertex, Uint32 firstInstance);

bool SDL_PollEvent(SDL_Event* event);
Uint32 SDL_GetMouseState(float* x, float* y);

size_t SDL_strlen(const char* str);

#endif // SDL_H
