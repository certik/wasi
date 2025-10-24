#include <base/base_types.h>
#include <webgpu/webgpu.h>
#include <base/exit.h>

WGPUInstance wgpuCreateInstance(WGPUInstanceDescriptor const * descriptor) {
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPUStatus wgpuSurfaceGetCapabilities(WGPUSurface surface,
        WGPUAdapter adapter,
        WGPUSurfaceCapabilities *capabilities) {
    (void)surface;
    (void)adapter;
    (void)capabilities;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return WGPUStatus_Error;
}

void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities surfaceCapabilities) {
    (void)surfaceCapabilities;
}
