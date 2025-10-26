#include <SDL3/SDL.h>
#include <webgpu/webgpu.h>
#include <gm.h>
#include <platform_sdl3.h>
#include <webgpu/webgpu_sdl3.h>
#include <base/io.h>
#include <base/buddy.h>

// ============================================================================
// WebGPU Helpers
// ============================================================================

// SDL3 provides wgpuInstanceCreateSurface via SDL_GetWGPUInstance
// We'll use wgpu-native's C API

static void request_adapter_callback(WGPURequestAdapterStatus status,
                                     WGPUAdapter adapter,
                                     WGPUStringView message,
                                     void* userdata1,
                                     void* userdata2) {
    (void)userdata2;
    if (status == WGPURequestAdapterStatus_Success) {
        *(WGPUAdapter*)userdata1 = adapter;
    } else {
        println(str_lit("Failed to request adapter: {}"), str_from_cstr_view((char*)message.data));
    }
}

static void request_device_callback(WGPURequestDeviceStatus status,
                                    WGPUDevice device,
                                    WGPUStringView message,
                                    void* userdata1,
                                    void* userdata2) {
    (void)userdata2;
    if (status == WGPURequestDeviceStatus_Success) {
        *(WGPUDevice*)userdata1 = device;
    } else {
        println(str_lit("Failed to request device: {}"), str_from_cstr_view((char*)message.data));
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

void wasi_start(int argc, char** argv);

int main(int argc, char** argv) {
    wasi_start(argc, argv);
    return 0;
}

int main2(void) {
    println(str_lit("Initializing SDL3..."));

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        println(str_lit("Failed to initialize SDL: {}"), str_from_cstr_view((char*)SDL_GetError()));
        return 1;
    }

    // Create window
    const int WINDOW_WIDTH = 1024;
    const int WINDOW_HEIGHT = 768;
    SDL_Window *window = SDL_CreateWindow(
        "GM - SDL3 + WebGPU",
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        println(str_lit("Failed to create window: {}"), str_from_cstr_view((char*)SDL_GetError()));
        SDL_Quit();
        return 1;
    }

    // Initialize platform state
    platform_sdl3_init(window);

    println(str_lit("Initializing WebGPU..."));

    // Create WebGPU instance
    WGPUInstanceDescriptor instance_desc = {
        .nextInChain = NULL
    };
    WGPUInstance instance = wgpuCreateInstance(&instance_desc);
    if (!instance) {
        println(str_lit("Failed to create WebGPU instance"));
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create WebGPU surface from SDL window
    // For macOS, we need to get the native window handle and create a Metal surface
    // SDL3 provides properties to get the native window
    #ifdef __APPLE__
        // Get the NSWindow from SDL
        void *nswindow = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
        if (!nswindow) {
            println(str_lit("Failed to get NSWindow from SDL"));
            wgpuInstanceRelease(instance);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        // Create Metal layer and surface
        extern void* wgpu_create_metal_surface(void* instance, void* ns_window);
        WGPUSurface surface = (WGPUSurface)wgpu_create_metal_surface((void*)instance, nswindow);
    #else
        #error "Only macOS is currently supported"
    #endif

    if (!surface) {
        println(str_lit("Failed to create WebGPU surface"));
        wgpuInstanceRelease(instance);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Request adapter
    WGPURequestAdapterOptions adapter_options = {
        .nextInChain = NULL,
        .compatibleSurface = surface,
        .powerPreference = WGPUPowerPreference_HighPerformance,
        .backendType = WGPUBackendType_Undefined,
        .forceFallbackAdapter = false
    };

    WGPUAdapter adapter = NULL;
    WGPURequestAdapterCallbackInfo adapter_callback_info = {
        .nextInChain = NULL,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = request_adapter_callback,
        .userdata1 = &adapter,
        .userdata2 = NULL
    };
    WGPUFuture adapter_future = wgpuInstanceRequestAdapter(instance, &adapter_options, adapter_callback_info);

    // Wait for adapter request to complete
    WGPUFutureWaitInfo wait_info = {
        .future = adapter_future
    };
    wgpuInstanceWaitAny(instance, 1, &wait_info, UINT64_MAX);

    if (!adapter) {
        println(str_lit("Failed to get WebGPU adapter"));
        wgpuSurfaceRelease(surface);
        wgpuInstanceRelease(instance);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Get surface capabilities
    WGPUSurfaceCapabilities surface_caps = {0};
    WGPUStatus caps_status = wgpuSurfaceGetCapabilities(surface, adapter, &surface_caps);
    if (caps_status != WGPUStatus_Success || surface_caps.formatCount == 0) {
        println(str_lit("Failed to get surface capabilities"));
        wgpuAdapterRelease(adapter);
        wgpuSurfaceRelease(surface);
        wgpuInstanceRelease(instance);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    WGPUTextureFormat surface_format = surface_caps.formats[0];
    wgpuSurfaceCapabilitiesFreeMembers(surface_caps);

    // Request device
    WGPUDeviceDescriptor device_desc = {
        .nextInChain = NULL,
        .label = {.data = "Main Device", .length = WGPU_STRLEN},
        .requiredFeatureCount = 0,
        .requiredFeatures = NULL,
        .requiredLimits = NULL,
        .defaultQueue = {
            .nextInChain = NULL,
            .label = {.data = "Main Queue", .length = WGPU_STRLEN}
        },
        .deviceLostCallbackInfo = {0},
        .uncapturedErrorCallbackInfo = {0}
    };

    WGPUDevice device = NULL;
    WGPURequestDeviceCallbackInfo device_callback_info = {
        .nextInChain = NULL,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = request_device_callback,
        .userdata1 = &device,
        .userdata2 = NULL
    };
    WGPUFuture device_future = wgpuAdapterRequestDevice(adapter, &device_desc, device_callback_info);

    // Wait for device request to complete
    WGPUFutureWaitInfo device_wait_info = {
        .future = device_future
    };
    wgpuInstanceWaitAny(instance, 1, &device_wait_info, UINT64_MAX);

    if (!device) {
        println(str_lit("Failed to get WebGPU device"));
        wgpuAdapterRelease(adapter);
        wgpuSurfaceRelease(surface);
        wgpuInstanceRelease(instance);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    WGPUQueue queue = wgpuDeviceGetQueue(device);

    // Configure surface
    WGPUSurfaceConfiguration surface_config = {
        .nextInChain = NULL,
        .device = device,
        .format = surface_format,
        .usage = WGPUTextureUsage_RenderAttachment,
        .viewFormatCount = 0,
        .viewFormats = NULL,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
        .presentMode = WGPUPresentMode_Fifo
    };
    wgpuSurfaceConfigure(surface, &surface_config);

    println(str_lit("WebGPU initialized successfully"));

    // Set up WebGPU context
    webgpu_sdl3_set_device(device);
    webgpu_sdl3_set_surface(surface);

    // Set up platform host config
    platform_sdl3_set_host_config(
        (uint32_t)(uintptr_t)device,
        (uint32_t)(uintptr_t)queue,
        (uint32_t)surface_format
    );

    println(str_lit("Starting main loop..."));

    // Main loop
    int running = 1;
    SDL_Event event;

    while (running) {
        // Reset mouse delta at the start of each frame
        platform_sdl3_reset_mouse_delta();

        // Poll events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = 0;
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                    int width, height;
                    SDL_GetWindowSize(window, &width, &height);
                    platform_sdl3_on_window_resized(width, height);

                    // Reconfigure surface
                    surface_config.width = width;
                    surface_config.height = height;
                    wgpuSurfaceConfigure(surface, &surface_config);
                    break;
                }

                case SDL_EVENT_WINDOW_MINIMIZED:
                    platform_sdl3_on_window_minimized();
                    break;

                case SDL_EVENT_WINDOW_RESTORED:
                case SDL_EVENT_WINDOW_SHOWN:
                    platform_sdl3_on_window_restored();
                    break;

                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP: {
                    SDL_Keycode key = event.key.key;
                    int pressed = (event.type == SDL_EVENT_KEY_DOWN);

                    // Map SDL keycodes to ASCII/DOM-like codes
                    uint8_t key_code = 0;
                    if (key >= 'a' && key <= 'z') {
                        key_code = (uint8_t)key;
                    } else if (key >= '0' && key <= '9') {
                        key_code = (uint8_t)key;
                    } else {
                        // Map arrow keys to DOM keycodes
                        switch (key) {
                            case SDLK_LEFT:  key_code = 37; break;
                            case SDLK_UP:    key_code = 38; break;
                            case SDLK_RIGHT: key_code = 39; break;
                            case SDLK_DOWN:  key_code = 40; break;
                            default: break;
                        }
                    }

                    if (key_code != 0) {
                        platform_sdl3_set_key_state(key_code, pressed);
                    }
                    break;
                }

                case SDL_EVENT_MOUSE_MOTION: {
                    float dx = event.motion.xrel;
                    float dy = event.motion.yrel;
                    platform_sdl3_add_mouse_delta(dx, dy);
                    break;
                }

                default:
                    break;
            }
        }

        // Call game frame
        gm_frame();

        // Present
        wgpuSurfacePresent(surface);

        // Process WebGPU queue
        #ifdef __EMSCRIPTEN__
        emscripten_sleep(0);
        #endif
    }

    println(str_lit("Shutting down..."));

    // Cleanup
    webgpu_sdl3_cleanup();
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
    SDL_DestroyWindow(window);
    SDL_Quit();

    println(str_lit("Goodbye!"));
    return 0;
}
