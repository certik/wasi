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

WASM_IMPORT("webgpu", "device_create_shader_module")
uint32_t webgpu_host_device_create_shader_module(uint32_t device_handle,
        uint32_t code_ptr, uint32_t code_length);

WASM_IMPORT("webgpu", "device_create_bind_group_layout")
uint32_t webgpu_host_device_create_bind_group_layout(uint32_t device_handle,
        uint32_t entries_ptr, uint32_t entry_count);

WASM_IMPORT("webgpu", "device_create_pipeline_layout")
uint32_t webgpu_host_device_create_pipeline_layout(uint32_t device_handle,
        uint32_t layouts_ptr, uint32_t layout_count);

WASM_IMPORT("webgpu", "device_create_render_pipeline")
uint32_t webgpu_host_device_create_render_pipeline(uint32_t device_handle,
        uint32_t descriptor_ptr);

WASM_IMPORT("webgpu", "device_create_bind_group")
uint32_t webgpu_host_device_create_bind_group(uint32_t device_handle,
        uint32_t descriptor_ptr);

WASM_IMPORT("webgpu", "device_create_sampler")
uint32_t webgpu_host_device_create_sampler(uint32_t device_handle,
        uint32_t descriptor_ptr);

// Command encoding and rendering
WASM_IMPORT("webgpu", "device_create_command_encoder")
uint32_t webgpu_host_device_create_command_encoder(uint32_t device_handle);

WASM_IMPORT("webgpu", "command_encoder_begin_render_pass")
uint32_t webgpu_host_command_encoder_begin_render_pass(uint32_t encoder_handle,
        uint32_t descriptor_ptr);

WASM_IMPORT("webgpu", "render_pass_set_pipeline")
void webgpu_host_render_pass_set_pipeline(uint32_t pass_handle, uint32_t pipeline_handle);

WASM_IMPORT("webgpu", "render_pass_set_bind_group")
void webgpu_host_render_pass_set_bind_group(uint32_t pass_handle, uint32_t group_index,
        uint32_t bind_group_handle);

WASM_IMPORT("webgpu", "render_pass_set_vertex_buffer")
void webgpu_host_render_pass_set_vertex_buffer(uint32_t pass_handle, uint32_t slot,
        uint32_t buffer_handle);

WASM_IMPORT("webgpu", "render_pass_set_index_buffer")
void webgpu_host_render_pass_set_index_buffer(uint32_t pass_handle, uint32_t buffer_handle,
        uint32_t format);

WASM_IMPORT("webgpu", "render_pass_draw_indexed")
void webgpu_host_render_pass_draw_indexed(uint32_t pass_handle, uint32_t index_count);

WASM_IMPORT("webgpu", "render_pass_draw")
void webgpu_host_render_pass_draw(uint32_t pass_handle, uint32_t vertex_count);

WASM_IMPORT("webgpu", "render_pass_end")
void webgpu_host_render_pass_end(uint32_t pass_handle);

WASM_IMPORT("webgpu", "command_encoder_finish")
uint32_t webgpu_host_command_encoder_finish(uint32_t encoder_handle);

WASM_IMPORT("webgpu", "queue_submit")
void webgpu_host_queue_submit(uint32_t queue_handle, uint32_t command_buffer_handle);

// Context operations
WASM_IMPORT("webgpu", "context_get_current_texture_view")
uint32_t webgpu_host_context_get_current_texture_view(void);

WASM_IMPORT("webgpu", "get_depth_texture_view")
uint32_t webgpu_host_get_depth_texture_view(uint32_t width, uint32_t height);

typedef enum {
    GM_WASM_RESOURCE_BUFFER = 0,
    GM_WASM_RESOURCE_SAMPLER = 1,
    GM_WASM_RESOURCE_TEXTURE_VIEW = 2,
    GM_WASM_RESOURCE_STORAGE_TEXTURE = 3,
} GMWasmResourceType;

typedef struct __attribute__((packed)) {
    uint32_t binding;
    uint32_t visibility;
    uint32_t resource_type;
    uint32_t buffer_type;
    uint32_t buffer_has_dynamic_offset;
    uint64_t buffer_min_binding_size;
    uint32_t sampler_type;
    uint32_t texture_sample_type;
    uint32_t texture_view_dimension;
    uint32_t texture_multisampled;
    uint32_t storage_texture_access;
    uint32_t storage_texture_format;
    uint32_t storage_texture_view_dimension;
} GMWasmBindGroupLayoutEntry;

typedef struct __attribute__((packed)) {
    uint32_t binding;
    uint32_t resource_type;
    uint32_t handle;
    uint64_t offset;
    uint64_t size;
} GMWasmBindGroupEntry;

typedef struct __attribute__((packed)) {
    uint32_t layout_handle;
    uint32_t entry_count;
    uint32_t entries_ptr;
} GMWasmBindGroupDescriptor;

typedef struct __attribute__((packed)) {
    uint32_t module_handle;
    uint32_t entry_point_ptr;
    uint32_t entry_point_length;
} GMWasmProgrammableStage;

typedef struct __attribute__((packed)) {
    uint32_t shader_location;
    uint32_t offset;
    uint32_t format;
} GMWasmVertexAttribute;

typedef struct __attribute__((packed)) {
    uint32_t array_stride;
    uint32_t step_mode;
    uint32_t attribute_count;
    uint32_t attributes_ptr;
} GMWasmVertexBufferLayout;

typedef struct __attribute__((packed)) {
    uint32_t stage_ptr;
    uint32_t buffer_count;
    uint32_t buffers_ptr;
} GMWasmVertexState;

typedef struct __attribute__((packed)) {
    uint32_t stage_ptr;
    uint32_t target_count;
    uint32_t targets_ptr;
} GMWasmFragmentState;

typedef struct __attribute__((packed)) {
    uint32_t format;
    uint32_t blend_enabled;
    uint32_t color_src_factor;
    uint32_t color_dst_factor;
    uint32_t color_operation;
    uint32_t alpha_src_factor;
    uint32_t alpha_dst_factor;
    uint32_t alpha_operation;
    uint32_t write_mask;
} GMWasmColorTargetState;

typedef struct __attribute__((packed)) {
    uint32_t format;
    uint32_t depth_write_enabled;
    uint32_t depth_compare;
} GMWasmDepthStencilState;

typedef struct __attribute__((packed)) {
    uint32_t topology;
    uint32_t cull_mode;
    uint32_t front_face;
    uint32_t strip_index_format;
} GMWasmPrimitiveState;

typedef struct __attribute__((packed)) {
    uint32_t layout_handle;
    uint32_t vertex_state_ptr;
    uint32_t fragment_state_ptr;
    uint32_t primitive_state_ptr;
    uint32_t depth_stencil_state_ptr;
    uint32_t multisample_count;
    uint32_t alpha_to_coverage_enabled;
} GMWasmRenderPipelineDescriptor;

typedef struct __attribute__((packed)) {
    uint32_t mag_filter;
    uint32_t min_filter;
    uint32_t mipmap_filter;
    uint32_t address_mode_u;
    uint32_t address_mode_v;
    uint32_t address_mode_w;
    float lod_min_clamp;
    float lod_max_clamp;
    uint32_t compare;
} GMWasmSamplerDescriptor;

typedef struct __attribute__((packed)) {
    uint32_t texture_view_handle;
    uint32_t load_op;
    uint32_t store_op;
    float clear_r;
    float clear_g;
    float clear_b;
    float clear_a;
} GMWasmColorAttachment;

typedef struct __attribute__((packed)) {
    uint32_t texture_view_handle;
    uint32_t depth_load_op;
    uint32_t depth_store_op;
    float depth_clear_value;
} GMWasmDepthStencilAttachment;

typedef struct __attribute__((packed)) {
    uint32_t color_attachment_ptr;
    uint32_t color_attachment_count;
    uint32_t depth_stencil_attachment_ptr;
} GMWasmRenderPassDescriptor;

static uint32_t gm_wasm_string_length(WGPUStringView view) {
    if (view.data == NULL) {
        return 0;
    }
    if (view.length == WGPU_STRLEN) {
        uint32_t len = 0;
        while (view.data[len] != '\0') {
            len++;
        }
        return len;
    }
    return (uint32_t)view.length;
}

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

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device,
        WGPUShaderModuleDescriptor const * descriptor) {
    if (descriptor == NULL || descriptor->nextInChain == NULL) {
        return NULL;
    }
    const WGPUShaderSourceWGSL *wgsl = (const WGPUShaderSourceWGSL*)descriptor->nextInChain;
    if (wgsl->chain.sType != WGPUSType_ShaderSourceWGSL || wgsl->code.data == NULL) {
        return NULL;
    }
    size_t code_length = wgsl->code.length;
    if (code_length == WGPU_STRLEN) {
        const char *ptr = wgsl->code.data;
        code_length = 0;
        while (ptr[code_length] != '\0') {
            code_length++;
        }
    }
    uint32_t handle = webgpu_host_device_create_shader_module(
            (uint32_t)(uintptr_t)device,
            (uint32_t)(uintptr_t)wgsl->code.data,
            (uint32_t)code_length);
    return (WGPUShaderModule)(uintptr_t)handle;
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

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device,
        WGPUBindGroupLayoutDescriptor const * descriptor) {
    if (descriptor == NULL || descriptor->entries == NULL || descriptor->entryCount == 0) {
        return NULL;
    }

    uint32_t count = descriptor->entryCount;
    GMWasmBindGroupLayoutEntry entries[count];
    for (uint32_t i = 0; i < count; i++) {
        const WGPUBindGroupLayoutEntry *src = &descriptor->entries[i];
        GMWasmBindGroupLayoutEntry *dst = &entries[i];
        *dst = (GMWasmBindGroupLayoutEntry){0};
        dst->binding = src->binding;
        dst->visibility = (uint32_t)src->visibility;

        if (src->buffer.type != WGPUBufferBindingType_Undefined &&
                src->buffer.type != WGPUBufferBindingType_BindingNotUsed) {
            dst->resource_type = GM_WASM_RESOURCE_BUFFER;
            dst->buffer_type = (uint32_t)src->buffer.type;
            dst->buffer_has_dynamic_offset = src->buffer.hasDynamicOffset ? 1u : 0u;
            dst->buffer_min_binding_size = src->buffer.minBindingSize;
        } else if (src->sampler.type != WGPUSamplerBindingType_Undefined &&
                src->sampler.type != WGPUSamplerBindingType_BindingNotUsed) {
            dst->resource_type = GM_WASM_RESOURCE_SAMPLER;
            dst->sampler_type = (uint32_t)src->sampler.type;
        } else if (src->texture.sampleType != WGPUTextureSampleType_Undefined) {
            dst->resource_type = GM_WASM_RESOURCE_TEXTURE_VIEW;
            dst->texture_sample_type = (uint32_t)src->texture.sampleType;
            dst->texture_view_dimension = (uint32_t)src->texture.viewDimension;
            dst->texture_multisampled = src->texture.multisampled ? 1u : 0u;
        } else if (src->storageTexture.access != WGPUStorageTextureAccess_Undefined) {
            dst->resource_type = GM_WASM_RESOURCE_STORAGE_TEXTURE;
            dst->storage_texture_access = (uint32_t)src->storageTexture.access;
            dst->storage_texture_format = (uint32_t)src->storageTexture.format;
            dst->storage_texture_view_dimension = (uint32_t)src->storageTexture.viewDimension;
        }
    }

    uint32_t handle = webgpu_host_device_create_bind_group_layout(
            (uint32_t)(uintptr_t)device,
            (uint32_t)(uintptr_t)entries,
            count);
    return (WGPUBindGroupLayout)(uintptr_t)handle;
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device,
        WGPUPipelineLayoutDescriptor const * descriptor) {
    if (descriptor == NULL || descriptor->bindGroupLayouts == NULL ||
            descriptor->bindGroupLayoutCount == 0) {
        return NULL;
    }

    uint32_t count = descriptor->bindGroupLayoutCount;
    uint32_t handles[count];
    for (uint32_t i = 0; i < count; i++) {
        handles[i] = (uint32_t)(uintptr_t)descriptor->bindGroupLayouts[i];
    }

    uint32_t handle = webgpu_host_device_create_pipeline_layout(
            (uint32_t)(uintptr_t)device,
            (uint32_t)(uintptr_t)handles,
            count);
    return (WGPUPipelineLayout)(uintptr_t)handle;
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device,
        WGPUSamplerDescriptor const * descriptor) {
    if (descriptor == NULL) {
        return NULL;
    }

    GMWasmSamplerDescriptor desc = {
        .mag_filter = (uint32_t)descriptor->magFilter,
        .min_filter = (uint32_t)descriptor->minFilter,
        .mipmap_filter = (uint32_t)descriptor->mipmapFilter,
        .address_mode_u = (uint32_t)descriptor->addressModeU,
        .address_mode_v = (uint32_t)descriptor->addressModeV,
        .address_mode_w = (uint32_t)descriptor->addressModeW,
        .lod_min_clamp = descriptor->lodMinClamp,
        .lod_max_clamp = descriptor->lodMaxClamp,
        .compare = (uint32_t)descriptor->compare,
    };

    uint32_t handle = webgpu_host_device_create_sampler(
            (uint32_t)(uintptr_t)device,
            (uint32_t)(uintptr_t)&desc);
    return (WGPUSampler)(uintptr_t)handle;
}

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device,
        WGPUBindGroupDescriptor const * descriptor) {
    if (descriptor == NULL || descriptor->layout == NULL ||
            descriptor->entries == NULL || descriptor->entryCount == 0) {
        return NULL;
    }

    uint32_t count = descriptor->entryCount;
    GMWasmBindGroupEntry entries[count];
    for (uint32_t i = 0; i < count; i++) {
        const WGPUBindGroupEntry *src = &descriptor->entries[i];
        GMWasmBindGroupEntry *dst = &entries[i];
        *dst = (GMWasmBindGroupEntry){0};
        dst->binding = src->binding;

        if (src->buffer != NULL) {
            dst->resource_type = GM_WASM_RESOURCE_BUFFER;
            dst->handle = (uint32_t)(uintptr_t)src->buffer;
            dst->offset = src->offset;
            dst->size = src->size;
        } else if (src->sampler != NULL) {
            dst->resource_type = GM_WASM_RESOURCE_SAMPLER;
            dst->handle = (uint32_t)(uintptr_t)src->sampler;
        } else if (src->textureView != NULL) {
            dst->resource_type = GM_WASM_RESOURCE_TEXTURE_VIEW;
            dst->handle = (uint32_t)(uintptr_t)src->textureView;
        }
    }

    GMWasmBindGroupDescriptor desc = {
        .layout_handle = (uint32_t)(uintptr_t)descriptor->layout,
        .entry_count = count,
        .entries_ptr = (uint32_t)(uintptr_t)entries,
    };

    uint32_t handle = webgpu_host_device_create_bind_group(
            (uint32_t)(uintptr_t)device,
            (uint32_t)(uintptr_t)&desc);
    return (WGPUBindGroup)(uintptr_t)handle;
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device,
        WGPURenderPipelineDescriptor const * descriptor) {
    if (descriptor == NULL) {
        return NULL;
    }

    GMWasmProgrammableStage vertex_stage = {
        .module_handle = (uint32_t)(uintptr_t)descriptor->vertex.module,
        .entry_point_ptr = (uint32_t)(uintptr_t)descriptor->vertex.entryPoint.data,
        .entry_point_length = gm_wasm_string_length(descriptor->vertex.entryPoint),
    };

    size_t vertex_buffer_count = descriptor->vertex.bufferCount;
    if (vertex_buffer_count == 0) {
        vertex_buffer_count = 1;
    }
    GMWasmVertexBufferLayout vertex_buffers[vertex_buffer_count];

    size_t total_vertex_attributes = 0;
    for (size_t i = 0; i < descriptor->vertex.bufferCount; i++) {
        total_vertex_attributes += descriptor->vertex.buffers[i].attributeCount;
    }
    if (total_vertex_attributes == 0) {
        total_vertex_attributes = 1;
    }
    GMWasmVertexAttribute vertex_attributes[total_vertex_attributes];

    size_t attr_index = 0;
    for (size_t i = 0; i < descriptor->vertex.bufferCount; i++) {
        const WGPUVertexBufferLayout *src = &descriptor->vertex.buffers[i];
        GMWasmVertexBufferLayout *dst = &vertex_buffers[i];
        dst->array_stride = (uint32_t)src->arrayStride;
        dst->step_mode = (uint32_t)src->stepMode;
        dst->attribute_count = (uint32_t)src->attributeCount;
        dst->attributes_ptr = (uint32_t)(uintptr_t)&vertex_attributes[attr_index];

        for (size_t j = 0; j < src->attributeCount; j++) {
            const WGPUVertexAttribute *attr = &src->attributes[j];
            GMWasmVertexAttribute *out = &vertex_attributes[attr_index++];
            out->shader_location = attr->shaderLocation;
            out->offset = (uint32_t)attr->offset;
            out->format = (uint32_t)attr->format;
        }
    }

    // If there were no buffers defined, zero out the placeholder entry.
    if (descriptor->vertex.bufferCount == 0) {
        vertex_buffers[0] = (GMWasmVertexBufferLayout){0};
        vertex_attributes[0] = (GMWasmVertexAttribute){0};
        vertex_buffers[0].attributes_ptr = (uint32_t)(uintptr_t)&vertex_attributes[0];
    }

    GMWasmVertexState vertex_state = {
        .stage_ptr = (uint32_t)(uintptr_t)&vertex_stage,
        .buffer_count = (uint32_t)descriptor->vertex.bufferCount,
        .buffers_ptr = (uint32_t)(uintptr_t)vertex_buffers,
    };

    GMWasmProgrammableStage fragment_stage = {0};
    GMWasmColorTargetState color_targets_buffer[4];
    GMWasmFragmentState fragment_state = {0};
    if (descriptor->fragment != NULL && descriptor->fragment->targetCount > 0 &&
            descriptor->fragment->targets != NULL) {
        fragment_stage.module_handle = (uint32_t)(uintptr_t)descriptor->fragment->module;
        fragment_stage.entry_point_ptr = (uint32_t)(uintptr_t)descriptor->fragment->entryPoint.data;
        fragment_stage.entry_point_length = gm_wasm_string_length(descriptor->fragment->entryPoint);

        size_t target_count = descriptor->fragment->targetCount;
        if (target_count > 4) {
            target_count = 4;
        }
        for (size_t i = 0; i < target_count; i++) {
            const WGPUColorTargetState *src = &descriptor->fragment->targets[i];
            GMWasmColorTargetState *dst = &color_targets_buffer[i];
            *dst = (GMWasmColorTargetState){0};
            dst->format = (uint32_t)src->format;
            dst->write_mask = (uint32_t)src->writeMask;
            if (src->blend != NULL) {
                dst->blend_enabled = 1u;
                dst->color_src_factor = (uint32_t)src->blend->color.srcFactor;
                dst->color_dst_factor = (uint32_t)src->blend->color.dstFactor;
                dst->color_operation = (uint32_t)src->blend->color.operation;
                dst->alpha_src_factor = (uint32_t)src->blend->alpha.srcFactor;
                dst->alpha_dst_factor = (uint32_t)src->blend->alpha.dstFactor;
                dst->alpha_operation = (uint32_t)src->blend->alpha.operation;
            }
        }

        fragment_state.stage_ptr = (uint32_t)(uintptr_t)&fragment_stage;
        fragment_state.target_count = (uint32_t)target_count;
        fragment_state.targets_ptr = (uint32_t)(uintptr_t)color_targets_buffer;
    }

    GMWasmPrimitiveState primitive_state = {
        .topology = (uint32_t)descriptor->primitive.topology,
        .cull_mode = (uint32_t)descriptor->primitive.cullMode,
        .front_face = (uint32_t)descriptor->primitive.frontFace,
        .strip_index_format = (uint32_t)descriptor->primitive.stripIndexFormat,
    };

    GMWasmDepthStencilState depth_state = {0};
    uint32_t depth_state_ptr = 0;
    if (descriptor->depthStencil != NULL) {
        depth_state.format = (uint32_t)descriptor->depthStencil->format;
        depth_state.depth_write_enabled = descriptor->depthStencil->depthWriteEnabled ? 1u : 0u;
        depth_state.depth_compare = (uint32_t)descriptor->depthStencil->depthCompare;
        depth_state_ptr = (uint32_t)(uintptr_t)&depth_state;
    }

    GMWasmRenderPipelineDescriptor desc = {
        .layout_handle = (uint32_t)(uintptr_t)descriptor->layout,
        .vertex_state_ptr = (uint32_t)(uintptr_t)&vertex_state,
        .fragment_state_ptr = (fragment_state.stage_ptr != 0) ?
            (uint32_t)(uintptr_t)&fragment_state : 0u,
        .primitive_state_ptr = (uint32_t)(uintptr_t)&primitive_state,
        .depth_stencil_state_ptr = depth_state_ptr,
        .multisample_count = descriptor->multisample.count,
        .alpha_to_coverage_enabled = descriptor->multisample.alphaToCoverageEnabled ? 1u : 0u,
    };

    uint32_t handle = webgpu_host_device_create_render_pipeline(
            (uint32_t)(uintptr_t)device,
            (uint32_t)(uintptr_t)&desc);
    return (WGPURenderPipeline)(uintptr_t)handle;
}

// Command encoder and rendering operations
WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device,
        WGPUCommandEncoderDescriptor const * descriptor) {
    (void)descriptor;
    uint32_t handle = webgpu_host_device_create_command_encoder(
            (uint32_t)(uintptr_t)device);
    return (WGPUCommandEncoder)(uintptr_t)handle;
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder,
        WGPURenderPassDescriptor const * descriptor) {
    if (descriptor == NULL) {
        return NULL;
    }

    // Convert color attachments
    uint32_t color_count = descriptor->colorAttachmentCount;
    if (color_count > 4) {
        color_count = 4;
    }
    GMWasmColorAttachment color_attachments[color_count > 0 ? color_count : 1];
    for (uint32_t i = 0; i < color_count; i++) {
        const WGPURenderPassColorAttachment *src = &descriptor->colorAttachments[i];
        GMWasmColorAttachment *dst = &color_attachments[i];
        dst->texture_view_handle = (uint32_t)(uintptr_t)src->view;
        dst->load_op = (uint32_t)src->loadOp;
        dst->store_op = (uint32_t)src->storeOp;
        dst->clear_r = src->clearValue.r;
        dst->clear_g = src->clearValue.g;
        dst->clear_b = src->clearValue.b;
        dst->clear_a = src->clearValue.a;
    }

    // Convert depth-stencil attachment
    GMWasmDepthStencilAttachment depth_attachment = {0};
    uint32_t depth_ptr = 0;
    if (descriptor->depthStencilAttachment != NULL) {
        const WGPURenderPassDepthStencilAttachment *src = descriptor->depthStencilAttachment;
        depth_attachment.texture_view_handle = (uint32_t)(uintptr_t)src->view;
        depth_attachment.depth_load_op = (uint32_t)src->depthLoadOp;
        depth_attachment.depth_store_op = (uint32_t)src->depthStoreOp;
        depth_attachment.depth_clear_value = src->depthClearValue;
        depth_ptr = (uint32_t)(uintptr_t)&depth_attachment;
    }

    GMWasmRenderPassDescriptor desc = {
        .color_attachment_ptr = (uint32_t)(uintptr_t)color_attachments,
        .color_attachment_count = color_count,
        .depth_stencil_attachment_ptr = depth_ptr,
    };

    uint32_t handle = webgpu_host_command_encoder_begin_render_pass(
            (uint32_t)(uintptr_t)commandEncoder,
            (uint32_t)(uintptr_t)&desc);
    return (WGPURenderPassEncoder)(uintptr_t)handle;
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder,
        WGPURenderPipeline pipeline) {
    webgpu_host_render_pass_set_pipeline(
            (uint32_t)(uintptr_t)renderPassEncoder,
            (uint32_t)(uintptr_t)pipeline);
}

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder,
        uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount,
        uint32_t const * dynamicOffsets) {
    (void)dynamicOffsetCount;
    (void)dynamicOffsets;
    webgpu_host_render_pass_set_bind_group(
            (uint32_t)(uintptr_t)renderPassEncoder,
            groupIndex,
            (uint32_t)(uintptr_t)group);
}

void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder,
        uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    (void)offset;
    (void)size;
    webgpu_host_render_pass_set_vertex_buffer(
            (uint32_t)(uintptr_t)renderPassEncoder,
            slot,
            (uint32_t)(uintptr_t)buffer);
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder,
        WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
    (void)offset;
    (void)size;
    webgpu_host_render_pass_set_index_buffer(
            (uint32_t)(uintptr_t)renderPassEncoder,
            (uint32_t)(uintptr_t)buffer,
            (uint32_t)format);
}

void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder,
        uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
        int32_t baseVertex, uint32_t firstInstance) {
    (void)instanceCount;
    (void)firstIndex;
    (void)baseVertex;
    (void)firstInstance;
    webgpu_host_render_pass_draw_indexed(
            (uint32_t)(uintptr_t)renderPassEncoder,
            indexCount);
}

void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder,
        uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
        uint32_t firstInstance) {
    (void)instanceCount;
    (void)firstVertex;
    (void)firstInstance;
    webgpu_host_render_pass_draw(
            (uint32_t)(uintptr_t)renderPassEncoder,
            vertexCount);
}

void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder) {
    webgpu_host_render_pass_end((uint32_t)(uintptr_t)renderPassEncoder);
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder,
        WGPUCommandBufferDescriptor const * descriptor) {
    (void)descriptor;
    uint32_t handle = webgpu_host_command_encoder_finish(
            (uint32_t)(uintptr_t)commandEncoder);
    return (WGPUCommandBuffer)(uintptr_t)handle;
}

void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount,
        WGPUCommandBuffer const * commands) {
    if (commandCount > 0 && commands != NULL) {
        webgpu_host_queue_submit(
                (uint32_t)(uintptr_t)queue,
                (uint32_t)(uintptr_t)commands[0]);
    }
}

// Context operations (platform-specific)
WGPUTextureView wgpu_context_get_current_texture_view(void) {
    uint32_t handle = webgpu_host_context_get_current_texture_view();
    return (WGPUTextureView)(uintptr_t)handle;
}

WGPUTextureView wgpu_get_depth_texture_view(uint32_t width, uint32_t height) {
    uint32_t handle = webgpu_host_get_depth_texture_view(width, height);
    return (WGPUTextureView)(uintptr_t)handle;
}

void wgpuBindGroupRelease(WGPUBindGroup bindGroup) {
    // In WASM, bind groups are just handles managed by JavaScript.
    // Release is a no-op since JS side manages the WebGPU object lifecycle.
    (void)bindGroup;
}
