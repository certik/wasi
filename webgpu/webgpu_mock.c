#include <base/base_types.h>
#include <webgpu/webgpu.h>
#include <base/exit.h>

WGPUInstance wgpuCreateInstance(WGPUInstanceDescriptor const * descriptor) {
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

uint32_t wasm_webgpu_get_preferred_canvas_format(void) {
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return 0;
}
