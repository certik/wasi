#include <webgpu/webgpu.h>
#include <base/base_types.h>

// ============================================================================
// WebGPU Context State (shared with gm_sdl.c)
// ============================================================================

typedef struct {
    WGPUDevice device;
    WGPUSwapChain swap_chain;

    // Depth texture cache
    WGPUTexture depth_texture;
    WGPUTextureView depth_texture_view;
    uint32_t depth_texture_width;
    uint32_t depth_texture_height;
} WebGPUContext;

static WebGPUContext g_wgpu_context = {0};

// ============================================================================
// Context Management (called from gm_sdl.c)
// ============================================================================

void webgpu_sdl3_set_device(WGPUDevice device) {
    g_wgpu_context.device = device;
}

void webgpu_sdl3_set_swap_chain(WGPUSwapChain swap_chain) {
    g_wgpu_context.swap_chain = swap_chain;
}

void webgpu_sdl3_cleanup(void) {
    // Release depth texture resources
    if (g_wgpu_context.depth_texture_view != NULL) {
        wgpuTextureViewRelease(g_wgpu_context.depth_texture_view);
        g_wgpu_context.depth_texture_view = NULL;
    }
    if (g_wgpu_context.depth_texture != NULL) {
        wgpuTextureRelease(g_wgpu_context.depth_texture);
        g_wgpu_context.depth_texture = NULL;
    }
    g_wgpu_context.depth_texture_width = 0;
    g_wgpu_context.depth_texture_height = 0;
}

// ============================================================================
// WebGPU Context Functions (required by gm.c)
// ============================================================================

WGPUTextureView wgpu_context_get_current_texture_view(void) {
    if (g_wgpu_context.swap_chain == NULL) {
        return NULL;
    }

    return wgpuSwapChainGetCurrentTextureView(g_wgpu_context.swap_chain);
}

WGPUTextureView wgpu_get_depth_texture_view(uint32_t width, uint32_t height) {
    // Check if we need to recreate the depth texture (size changed)
    if (g_wgpu_context.depth_texture != NULL) {
        if (g_wgpu_context.depth_texture_width != width ||
            g_wgpu_context.depth_texture_height != height) {
            // Size changed, release old depth texture
            wgpuTextureViewRelease(g_wgpu_context.depth_texture_view);
            wgpuTextureRelease(g_wgpu_context.depth_texture);
            g_wgpu_context.depth_texture = NULL;
            g_wgpu_context.depth_texture_view = NULL;
        }
    }

    // Create depth texture if needed
    if (g_wgpu_context.depth_texture == NULL) {
        WGPUTextureDescriptor depth_texture_desc = {
            .nextInChain = NULL,
            .label = WGPU_STRING_VIEW("Depth Texture"),
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = width,
                .height = height,
                .depthOrArrayLayers = 1
            },
            .format = WGPUTextureFormat_Depth24Plus,
            .mipLevelCount = 1,
            .sampleCount = 1,
            .viewFormatCount = 0,
            .viewFormats = NULL
        };

        g_wgpu_context.depth_texture = wgpuDeviceCreateTexture(
            g_wgpu_context.device,
            &depth_texture_desc
        );

        if (g_wgpu_context.depth_texture == NULL) {
            return NULL;
        }

        // Create texture view
        WGPUTextureViewDescriptor view_desc = {
            .nextInChain = NULL,
            .label = WGPU_STRING_VIEW("Depth Texture View"),
            .format = WGPUTextureFormat_Depth24Plus,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All
        };

        g_wgpu_context.depth_texture_view = wgpuTextureCreateView(
            g_wgpu_context.depth_texture,
            &view_desc
        );

        g_wgpu_context.depth_texture_width = width;
        g_wgpu_context.depth_texture_height = height;
    }

    return g_wgpu_context.depth_texture_view;
}
