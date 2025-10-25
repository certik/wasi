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

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device,
        WGPUBufferDescriptor const * descriptor) {
    (void)device;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device,
        WGPUShaderModuleDescriptor const * descriptor) {
    (void)device;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset,
        void const * data, size_t size) {
    (void)queue;
    (void)buffer;
    (void)bufferOffset;
    (void)data;
    (void)size;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device,
        WGPUBindGroupDescriptor const * descriptor) {
    (void)device;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device,
        WGPUBindGroupLayoutDescriptor const * descriptor) {
    (void)device;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device,
        WGPUPipelineLayoutDescriptor const * descriptor) {
    (void)device;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device,
        WGPURenderPipelineDescriptor const * descriptor) {
    (void)device;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device,
        WGPUSamplerDescriptor const * descriptor) {
    (void)device;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device,
        WGPUCommandEncoderDescriptor const * descriptor) {
    (void)device;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder,
        WGPURenderPassDescriptor const * descriptor) {
    (void)commandEncoder;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder,
        WGPURenderPipeline pipeline) {
    (void)renderPassEncoder;
    (void)pipeline;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder,
        uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount,
        uint32_t const * dynamicOffsets) {
    (void)renderPassEncoder;
    (void)groupIndex;
    (void)group;
    (void)dynamicOffsetCount;
    (void)dynamicOffsets;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder,
        uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    (void)renderPassEncoder;
    (void)slot;
    (void)buffer;
    (void)offset;
    (void)size;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder,
        WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
    (void)renderPassEncoder;
    (void)buffer;
    (void)format;
    (void)offset;
    (void)size;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder,
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
        int32_t baseVertex, uint32_t firstInstance) {
    (void)renderPassEncoder;
    (void)indexCount;
    (void)instanceCount;
    (void)firstIndex;
    (void)baseVertex;
    (void)firstInstance;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder,
        uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
        uint32_t firstInstance) {
    (void)renderPassEncoder;
    (void)vertexCount;
    (void)instanceCount;
    (void)firstVertex;
    (void)firstInstance;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder) {
    (void)renderPassEncoder;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder,
        WGPUCommandBufferDescriptor const * descriptor) {
    (void)commandEncoder;
    (void)descriptor;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount,
        WGPUCommandBuffer const * commands) {
    (void)queue;
    (void)commandCount;
    (void)commands;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
}

WGPUTextureView wgpu_context_get_current_texture_view(void) {
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}

WGPUTextureView wgpu_get_depth_texture_view(uint32_t width, uint32_t height) {
    (void)width;
    (void)height;
    FATAL_ERROR("WebGPU is not supported on this platform build.");
    return NULL;
}
