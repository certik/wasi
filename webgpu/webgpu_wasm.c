#include <base/base_types.h>
#include <webgpu/webgpu.h>

#define WASM_IMPORT(module, name) __attribute__((import_module(module), import_name(name)))

// JavaScript host functions providing WebGPU access
WASM_IMPORT("webgpu", "create_instance")
uint32_t webgpu_host_create_instance(uint32_t descriptor_ptr);

WASM_IMPORT("webgpu", "surface_get_preferred_format")
uint32_t webgpu_host_surface_get_preferred_format(void);

WGPUInstance wgpuCreateInstance(WGPUInstanceDescriptor const * descriptor) {
    uint32_t desc_ptr = (uint32_t)(uintptr_t)descriptor;
    uint32_t handle = webgpu_host_create_instance(desc_ptr);
    return (WGPUInstance)(uintptr_t)handle;
}

static WGPUTextureFormat g_surface_formats[1];
static const WGPUPresentMode g_present_modes[1] = {WGPUPresentMode_Fifo};
static const WGPUCompositeAlphaMode g_alpha_modes[1] = {WGPUCompositeAlphaMode_Opaque};

WGPUStatus wgpuSurfaceGetCapabilities(WGPUSurface surface,
        WGPUAdapter adapter,
        WGPUSurfaceCapabilities *capabilities) {
    (void)surface;
    (void)adapter;

    if (capabilities == NULL) {
        return WGPUStatus_Error;
    }

    uint32_t preferred = webgpu_host_surface_get_preferred_format();
    g_surface_formats[0] = (WGPUTextureFormat)preferred;

    capabilities->usages = WGPUTextureUsage_RenderAttachment;
    capabilities->formatCount = 1;
    capabilities->formats = g_surface_formats;
    capabilities->presentModeCount = 1;
    capabilities->presentModes = g_present_modes;
    capabilities->alphaModeCount = 1;
    capabilities->alphaModes = g_alpha_modes;
    return WGPUStatus_Success;
}

void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities surfaceCapabilities) {
    (void)surfaceCapabilities;
}
