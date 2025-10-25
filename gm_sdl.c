#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <webgpu/webgpu.h>
#include <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Uniforms struct for shader (align to 16 bytes for GPU)
typedef struct {
    float mouse[2];
    float resolution[2];
    float radius;
    float padding;  // Pad to 16 bytes
} Uniforms;

// WGSL shader source
const char* shaderSource = 
"@vertex\n"
"fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {\n"
"    var pos = vec2<f32>(0.0, 0.0);\n"
"    if (in_vertex_index == 0u) {\n"
"        pos = vec2<f32>(-1.0, 1.0);\n"
"    } else if (in_vertex_index == 1u) {\n"
"        pos = vec2<f32>(-1.0, -3.0);\n"
"    } else {\n"
"        pos = vec2<f32>(3.0, 1.0);\n"
"    }\n"
"    return vec4<f32>(pos, 0.0, 1.0);\n"
"}\n"
"@fragment\n"
"fn fs_main(@builtin(position) pos: vec4<f32>) -> @location(0) vec4<f32> {\n"
"    let frag_coord = vec2<f32>(pos.x, uniforms.resolution.y - pos.y);\n"
"    let dist = distance(frag_coord, uniforms.mouse);\n"
"    if (dist < uniforms.radius) {\n"
"        return vec4<f32>(1.0, 0.0, 0.0, 1.0);\n"
"    }\n"
"    return vec4<f32>(0.0, 0.0, 0.0, 1.0);\n"
"}";

// Callback data for async requests
typedef struct {
    WGPUAdapter adapter;
    WGPUDevice device;
    bool done;
} CallbackData;

// Adapter callback
static void adapter_callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
    CallbackData* data = (CallbackData*)userdata;
    if (status == WGPURequestAdapterStatus_Success) {
        data->adapter = wgpuAdapterReference(adapter);
    } else {
        printf("Failed to get adapter: %s\n", message ? message : "Unknown error");
    }
    data->done = true;
}

// Device callback
static void device_callback(WGPURequestStatus status, WGPUDevice device, const char* message, void* userdata) {
    CallbackData* data = (CallbackData*)userdata;
    if (status == WGPURequestStatus_Success) {
        data->device = wgpuDeviceReference(device);
    } else {
        printf("Failed to get device: %s\n", message ? message : "Unknown error");
    }
    data->done = true;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("WebGPU Circle Draw SDL 3", 640, 480, 0);
    if (!window) {
        SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Get window size
    int width = 640, height = 480;
    SDL_GetWindowSize(window, &width, &height);

    // Create WebGPU instance
    WGPUInstanceDescriptor instanceDesc = {0};
    WGPUInstance instance = wgpuCreateInstance(&instanceDesc);
    if (!instance) {
        SDL_Log("Failed to create WebGPU instance");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create surface (macOS-specific)
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo) || wmInfo.subsystem != SDL_SYSWM_COCOA) {
        SDL_Log("Failed to get WM info");
        wgpuInstanceRelease(instance);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    NSWindow* nsWindow = (__bridge NSWindow*)wmInfo.info.cocoa.window;
    NSView* nsView = [nsWindow contentView];
    [nsView setWantsLayer: YES];
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    [nsView setLayer: metalLayer];

    WGPUSurfaceDescriptorFromCoreAnimationLayer fromCALayer = {
        .chain = {.sType = WGPUSType_SurfaceDescriptorFromCoreAnimationLayer},
        .layer = (__bridge CFTypeRef)metalLayer
    };
    WGPUSurfaceDescriptor surfaceDesc = {
        .nextInChain = (const WGPUChainedStruct*)&fromCALayer.chain
    };
    WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);
    if (!surface) {
        SDL_Log("Failed to create surface");
        wgpuInstanceRelease(instance);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Request adapter (async with poll)
    CallbackData cbData = {0};
    WGPURequestAdapterOptions adapterOpts = {.compatibleSurface = surface};
    wgpuInstanceRequestAdapter(instance, &adapterOpts, adapter_callback, &cbData);
    while (!cbData.done) {
        SDL_Delay(10);
    }
    if (!cbData.adapter) {
        SDL_Log("No adapter");
        goto cleanup;
    }

    // Get preferred format
    WGPUTextureFormat swapChainFormat = wgpuSurfaceGetPreferredFormat(surface, cbData.adapter);

    // Request device
    WGPUDeviceDescriptor deviceDesc = {0};
    wgpuAdapterRequestDevice(cbData.adapter, &deviceDesc, device_callback, &cbData);
    while (!cbData.done) {
        SDL_Delay(10);
    }
    if (!cbData.device) {
        SDL_Log("No device");
        goto cleanup;
    }
    WGPUDevice device = cbData.device;
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    // Create swap chain
    WGPUSwapChainDescriptor swapChainDesc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = swapChainFormat,
        .width = (uint32_t)width,
        .height = (uint32_t)height,
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = WGPUCompositeAlphaMode_Auto
    };
    WGPUSwapChain swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);

    // Create uniform buffer
    WGPUBufferDescriptor uniformBufDesc = {
        .size = sizeof(Uniforms),
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = false
    };
    WGPUBuffer uniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufDesc);

    // Bind group layout
    WGPUBindGroupLayoutEntry bglEntry = {
        .binding = 0,
        .visibility = WGPUShaderStage_Fragment,
        .buffer = {.type = WGPUBufferBindingType_Uniform, .hasDynamicOffset = false, .minBindingSize = 0}
    };
    WGPUBindGroupLayoutDescriptor bglDesc = {.entryCount = 1, .entries = &bglEntry};
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    // Pipeline layout
    WGPUPipelineLayoutDescriptor plDesc = {.bindGroupLayoutCount = 1, .bindGroupLayouts = &bindGroupLayout};
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    // Shader module
    WGPUShaderModuleWGSLDescriptor wgslDesc = {
        .chain = {.sType = WGPUSType_ShaderModuleWGSLDescriptor},
        .code = shaderSource
    };
    WGPUShaderModuleDescriptor smDesc = {.nextInChain = &wgslDesc.chain};
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &smDesc);

    // Render pipeline
    WGPURenderPipelineDescriptor pipelineDesc = {
        .layout = pipelineLayout,
        .vertex = {
            .module = shaderModule,
            .entryPoint = "vs_main"
        },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace = WGPUFrontFace_CW,
            .cullMode = WGPUCullMode_None
        },
        .fragment = & (WGPUFragmentState) {
            .module = shaderModule,
            .entryPoint = "fs_main",
            .targetCount = 1,
            .targets = &(WGPUColorTargetState) {
                .format = swapChainFormat,
                .writeMask = WGPUColorWriteMask_All
            }
        },
        .multisample = {
            .count = 1,
            .mask = 0xFFFFFFFF,
            .alphaToCoverageEnabled = false
        }
    };
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    // Bind group
    WGPUBindGroupEntry bgEntry = {.binding = 0, .buffer = {.buffer = uniformBuffer, .offset = 0, .size = 0}};
    WGPUBindGroupDescriptor bgDesc = {.layout = bindGroupLayout, .entryCount = 1, .entries = &bgEntry};
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    // Event loop vars
    bool quit = false;
    SDL_Event event;
    int mouse_x = width / 2, mouse_y = height / 2;
    Uniforms uniforms = {{(float)mouse_x, (float)mouse_y}, {(float)width, (float)height}, 20.0f, 0.0f};

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    quit = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.keysym.sym == SDLK_Q) {
                        quit = true;
                    }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    mouse_x = event.motion.x;
                    mouse_y = event.motion.y;
                    break;
            }
        }

        // Update uniforms
        uniforms.mouse[0] = (float)mouse_x;
        uniforms.mouse[1] = (float)mouse_y;
        wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &uniforms, sizeof(Uniforms));

        // Get next texture
        WGPUTextureView nextView = wgpuSwapChainGetCurrentTextureView(swapChain);
        if (!nextView) continue;

        // Render pass
        WGPURenderPassColorAttachment colorAttachment = {
            .view = nextView,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = {0.0f, 0.0f, 0.0f, 1.0f}
        };
        WGPURenderPassDescriptor renderPassDesc = {
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment
        };

        WGPUCommandEncoderDescriptor encoderDesc = {0};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

        wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroup, 0, NULL);
        wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(renderPass);

        WGPUCommandBufferDescriptor cmdBufDesc = {0};
        WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufDesc);
        wgpuQueueSubmit(queue, 1, &cmdBuffer);

        wgpuSwapChainPresent(swapChain);
        wgpuTextureViewRelease(nextView);

        wgpuCommandBufferRelease(cmdBuffer);
        wgpuCommandEncoderRelease(encoder);
    }

cleanup:
    // Cleanup
    if (bindGroup) wgpuBindGroupRelease(bindGroup);
    if (pipeline) wgpuRenderPipelineRelease(pipeline);
    if (shaderModule) wgpuShaderModuleRelease(shaderModule);
    if (pipelineLayout) wgpuPipelineLayoutRelease(pipelineLayout);
    if (bindGroupLayout) wgpuBindGroupLayoutRelease(bindGroupLayout);
    if (uniformBuffer) wgpuBufferRelease(uniformBuffer);
    if (swapChain) wgpuSwapChainRelease(swapChain);
    if (queue) wgpuQueueRelease(queue);
    if (device) wgpuDeviceRelease(device);
    if (cbData.adapter) wgpuAdapterRelease(cbData.adapter);
    if (surface) wgpuSurfaceRelease(surface);
    if (instance) wgpuInstanceRelease(instance);

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
