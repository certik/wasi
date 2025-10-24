#include <base/base_types.h>
#include <webgpu/webgpu.h>

#define WASM_IMPORT(module, name) __attribute__((import_module(module), import_name(name)))

// JavaScript host functions providing WebGPU access
WASM_IMPORT("webgpu", "create_instance")
uint32_t webgpu_host_create_instance(uint32_t descriptor_ptr);

WASM_IMPORT("webgpu", "surface_get_preferred_format")
uint32_t webgpu_host_surface_get_preferred_format(void);

WASM_IMPORT("webgpu", "device_create_buffer")
uint32_t webgpu_host_device_create_buffer(uint32_t device_handle, uint32_t size_low,
        uint32_t size_high, uint32_t usage, uint32_t mapped_at_creation);

WASM_IMPORT("webgpu", "queue_write_buffer")
void webgpu_host_queue_write_buffer(uint32_t queue_handle, uint32_t buffer_handle,
        uint32_t offset_low, uint32_t offset_high, uint32_t data_ptr, uint32_t size);

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

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device,
        WGPUBufferDescriptor const * descriptor) {
    uint64_t size = descriptor->size;
    uint32_t size_low = (uint32_t)(size & 0xFFFFFFFFu);
    uint32_t size_high = (uint32_t)(size >> 32);
    uint32_t handle = webgpu_host_device_create_buffer(
            (uint32_t)(uintptr_t)device,
            size_low,
            size_high,
            (uint32_t)descriptor->usage,
            descriptor->mappedAtCreation ? 1u : 0u);
    return (WGPUBuffer)(uintptr_t)handle;
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset,
        void const * data, size_t size) {
    if (data == NULL || size == 0) {
        return;
    }
    uint32_t offset_low = (uint32_t)(bufferOffset & 0xFFFFFFFFu);
    uint32_t offset_high = (uint32_t)(bufferOffset >> 32);
    webgpu_host_queue_write_buffer(
            (uint32_t)(uintptr_t)queue,
            (uint32_t)(uintptr_t)buffer,
            offset_low,
            offset_high,
            (uint32_t)(uintptr_t)data,
            (uint32_t)size);
}
