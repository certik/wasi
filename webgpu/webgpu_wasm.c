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

uint32_t wasm_webgpu_get_preferred_canvas_format(void) {
    return webgpu_host_surface_get_preferred_format();
}
