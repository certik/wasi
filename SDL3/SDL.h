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
typedef struct SDL_GPUCopyPass SDL_GPUCopyPass;
typedef struct SDL_GPUBuffer SDL_GPUBuffer;
typedef struct SDL_GPUTransferBuffer SDL_GPUTransferBuffer;

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
    SDL_EVENT_KEY_UP = 0x301,
    SDL_EVENT_MOUSE_MOTION = 0x400,
} SDL_EventType;

typedef struct SDL_KeyboardEvent {
    Uint32 type;
    Uint32 key;
} SDL_KeyboardEvent;

typedef struct SDL_MouseMotionEvent {
    Uint32 type;
    float xrel;
    float yrel;
} SDL_MouseMotionEvent;

typedef union SDL_Event {
    Uint32 type;
    SDL_KeyboardEvent key;
    SDL_MouseMotionEvent motion;
} SDL_Event;

// Key codes
typedef enum {
    SDLK_ESCAPE = 27,
    SDLK_SPACE = ' ',
    SDLK_W = 'w',
    SDLK_A = 'a',
    SDLK_S = 's',
    SDLK_D = 'd',
    SDLK_Q = 'q',
    SDLK_LSHIFT = 1073742049,
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
    SDL_GPU_TEXTUREFORMAT_D16_UNORM,
} SDL_GPUTextureFormat;

typedef enum {
    SDL_GPU_TEXTUREUSAGE_SAMPLER = 1 << 0,
    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET = 1 << 1,
    SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET = 1 << 2,
} SDL_GPUTextureUsageFlags;

typedef enum {
    SDL_GPU_BUFFERUSAGE_VERTEX = 1 << 0,
    SDL_GPU_BUFFERUSAGE_INDEX = 1 << 1,
} SDL_GPUBufferUsageFlags;

typedef enum {
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
    SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
} SDL_GPUVertexElementFormat;

typedef enum {
    SDL_GPU_VERTEXINPUTRATE_VERTEX,
    SDL_GPU_VERTEXINPUTRATE_INSTANCE,
} SDL_GPUVertexInputRate;

typedef enum {
    SDL_GPU_INDEXELEMENTSIZE_16BIT,
    SDL_GPU_INDEXELEMENTSIZE_32BIT,
} SDL_GPUIndexElementSize;

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
    SDL_GPUTextureFormat depth_stencil_format;
    bool has_depth_stencil_target;
} SDL_GPUGraphicsPipelineTargetInfo;

typedef struct SDL_GPUVertexAttribute {
    Uint32 location;
    Uint32 buffer_slot;
    SDL_GPUVertexElementFormat format;
    Uint32 offset;
} SDL_GPUVertexAttribute;

typedef struct SDL_GPUVertexBufferDescription {
    Uint32 slot;
    Uint32 pitch;
    SDL_GPUVertexInputRate input_rate;
} SDL_GPUVertexBufferDescription;

typedef struct SDL_GPUVertexInputState {
    const SDL_GPUVertexBufferDescription* vertex_buffer_descriptions;
    Uint32 num_vertex_buffers;
    const SDL_GPUVertexAttribute* vertex_attributes;
    Uint32 num_vertex_attributes;
} SDL_GPUVertexInputState;

typedef enum {
    SDL_GPU_COMPAREOP_LESS,
    SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
} SDL_GPUCompareOp;

typedef struct SDL_GPUDepthStencilState {
    bool enable_depth_test;
    bool enable_depth_write;
    SDL_GPUCompareOp compare_op;
} SDL_GPUDepthStencilState;

typedef struct SDL_GPUGraphicsPipelineCreateInfo {
    SDL_GPUShader* vertex_shader;
    SDL_GPUShader* fragment_shader;
    SDL_GPUVertexInputState vertex_input_state;
    SDL_GPUDepthStencilState depth_stencil_state;
    SDL_GPUGraphicsPipelineTargetInfo target_info;
    SDL_GPUPrimitiveType primitive_type;
} SDL_GPUGraphicsPipelineCreateInfo;

typedef struct SDL_GPUColorTargetInfo {
    SDL_GPUTexture* texture;
    SDL_FColor clear_color;
    SDL_GPULoadOp load_op;
    SDL_GPUStoreOp store_op;
} SDL_GPUColorTargetInfo;

typedef struct SDL_GPUDepthStencilTargetInfo {
    SDL_GPUTexture* texture;
    float clear_depth;
    SDL_GPULoadOp load_op;
    SDL_GPUStoreOp store_op;
} SDL_GPUDepthStencilTargetInfo;

typedef struct SDL_GPUTextureCreateInfo {
    SDL_GPUTextureUsageFlags usage;
    SDL_GPUTextureFormat format;
    Uint32 width;
    Uint32 height;
    Uint32 layer_count_or_depth;
    Uint32 num_levels;
} SDL_GPUTextureCreateInfo;

typedef struct SDL_GPUBufferCreateInfo {
    SDL_GPUBufferUsageFlags usage;
    Uint32 size;
} SDL_GPUBufferCreateInfo;

typedef struct SDL_GPUTransferBufferCreateInfo {
    SDL_GPUBufferUsageFlags usage;
    Uint32 size;
} SDL_GPUTransferBufferCreateInfo;

typedef struct SDL_GPUBufferBinding {
    SDL_GPUBuffer* buffer;
    Uint32 offset;
} SDL_GPUBufferBinding;

typedef struct SDL_GPUTransferBufferLocation {
    SDL_GPUTransferBuffer* transfer_buffer;
    Uint32 offset;
} SDL_GPUTransferBufferLocation;

typedef struct SDL_GPUBufferRegion {
    SDL_GPUBuffer* buffer;
    Uint32 offset;
    Uint32 size;
} SDL_GPUBufferRegion;

// App callbacks
typedef enum SDL_AppResult {
    SDL_APP_FAILURE = -1,
    SDL_APP_SUCCESS = 0,
    SDL_APP_CONTINUE = 1
} SDL_AppResult;

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

SDL_GPUTexture* SDL_CreateGPUTexture(SDL_GPUDevice* device, const SDL_GPUTextureCreateInfo* info);
void SDL_ReleaseGPUTexture(SDL_GPUDevice* device, SDL_GPUTexture* texture);

SDL_GPURenderPass* SDL_BeginGPURenderPass(SDL_GPUCommandBuffer* cmdbuf, const SDL_GPUColorTargetInfo* colorTargets, Uint32 numColorTargets, const SDL_GPUDepthStencilTargetInfo* depthStencilTarget);
void SDL_EndGPURenderPass(SDL_GPURenderPass* pass);
void SDL_BindGPUGraphicsPipeline(SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline);
void SDL_PushGPUFragmentUniformData(SDL_GPUCommandBuffer* cmdbuf, Uint32 slot, const void* data, Uint32 length);
void SDL_PushGPUVertexUniformData(SDL_GPUCommandBuffer* cmdbuf, Uint32 slot, const void* data, Uint32 length);
void SDL_DrawGPUPrimitives(SDL_GPURenderPass* pass, Uint32 vertexCount, Uint32 instanceCount, Uint32 firstVertex, Uint32 firstInstance);
void SDL_DrawGPUIndexedPrimitives(SDL_GPURenderPass* pass, Uint32 indexCount, Uint32 instanceCount, Uint32 firstIndex, Sint32 vertexOffset, Uint32 firstInstance);

SDL_GPUBuffer* SDL_CreateGPUBuffer(SDL_GPUDevice* device, const SDL_GPUBufferCreateInfo* info);
void SDL_ReleaseGPUBuffer(SDL_GPUDevice* device, SDL_GPUBuffer* buffer);
SDL_GPUTransferBuffer* SDL_CreateGPUTransferBuffer(SDL_GPUDevice* device, const SDL_GPUTransferBufferCreateInfo* info);
void SDL_ReleaseGPUTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer);
void* SDL_MapGPUTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer, bool cycle);
void SDL_UnmapGPUTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBuffer* buffer);

SDL_GPUCopyPass* SDL_BeginGPUCopyPass(SDL_GPUCommandBuffer* cmdbuf);
void SDL_UploadToGPUBuffer(SDL_GPUCopyPass* copy_pass, const SDL_GPUTransferBufferLocation* source, const SDL_GPUBufferRegion* destination, bool cycle);
void SDL_EndGPUCopyPass(SDL_GPUCopyPass* copy_pass);

void SDL_BindGPUVertexBuffers(SDL_GPURenderPass* pass, Uint32 firstSlot, const SDL_GPUBufferBinding* bindings, Uint32 numBindings);
void SDL_BindGPUIndexBuffer(SDL_GPURenderPass* pass, const SDL_GPUBufferBinding* binding, SDL_GPUIndexElementSize indexElementSize);

bool SDL_PollEvent(SDL_Event* event);
Uint32 SDL_GetMouseState(float* x, float* y);
bool SDL_SetWindowRelativeMouseMode(SDL_Window* window, bool enabled);

size_t SDL_strlen(const char* str);

// Managed main callback prototypes (implemented by the app when using SDL_MAIN_USE_CALLBACKS)
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]);
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
SDL_AppResult SDL_AppIterate(void *appstate);
void SDL_AppQuit(void *appstate, SDL_AppResult result);

#endif // SDL_H
