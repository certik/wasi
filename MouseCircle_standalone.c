/*
 * Standalone MouseCircle Example for SDL3 GPU
 *
 * This example draws a red circle that follows the mouse cursor.
 *
 * Compile with:
 *   clang MouseCircle_standalone.c -o MouseCircle_standalone \
 *     -I$CONDA_PREFIX/include \
 *     -L$CONDA_PREFIX/lib \
 *     -Wl,-rpath,$CONDA_PREFIX/lib \
 *     -lSDL3 \
 *     -framework Metal -framework CoreGraphics -framework AppKit
 *
 * Run with:
 *   ./MouseCircle_standalone
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>

// Embedded Metal Shaders (compiled MSL)
static const char* VertexShaderMSL =
"#include <metal_stdlib>\n"
"#include <simd/simd.h>\n"
"\n"
"using namespace metal;\n"
"\n"
"struct VertexOutput {\n"
"    float4 position [[position]];\n"
"    float2 uv [[user(locn0)]];\n"
"};\n"
"\n"
"vertex VertexOutput main0(uint vertex_index [[vertex_id]]) {\n"
"    VertexOutput output;\n"
"\n"
"    // Full-screen triangle\n"
"    float x = float(int(vertex_index) - 1);\n"
"    float y = float(int(vertex_index & 1u) * 2 - 1);\n"
"    output.position = float4(x, y, 0.0, 1.0);\n"
"\n"
"    // Convert to UV coordinates (0,0 to 1,1)\n"
"    output.uv = float2((x + 1.0) * 0.5, (1.0 - y) * 0.5);\n"
"\n"
"    return output;\n"
"}\n";

static const char* FragmentShaderMSL =
"#include <metal_stdlib>\n"
"#include <simd/simd.h>\n"
"\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float2 mouse_pos;\n"
"    float2 resolution;\n"
"};\n"
"\n"
"struct VertexOutput {\n"
"    float4 position [[position]];\n"
"    float2 uv [[user(locn0)]];\n"
"};\n"
"\n"
"fragment float4 main0(\n"
"    VertexOutput input [[stage_in]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    // Convert UV to screen coordinates\n"
"    float2 screen_pos = input.uv * uniforms.resolution;\n"
"\n"
"    // Calculate distance from mouse position\n"
"    float dist = length(screen_pos - uniforms.mouse_pos);\n"
"\n"
"    // Draw a circle with radius 50 pixels\n"
"    float circle_radius = 50.0;\n"
"    float4 circle_color = float4(1.0, 0.0, 0.0, 1.0); // Red\n"
"    float4 bg_color = float4(0.0, 1.0, 0.0, 1.0); // Green\n"
"\n"
"    // Smooth circle edge\n"
"    float edge_smoothness = 2.0;\n"
"    float t = smoothstep(circle_radius + edge_smoothness, circle_radius - edge_smoothness, dist);\n"
"\n"
"    return mix(bg_color, circle_color, t);\n"
"}\n";

// Uniform data structure
typedef struct MouseCircleUniforms
{
    float mouse_x;
    float mouse_y;
    float resolution_x;
    float resolution_y;
} MouseCircleUniforms;

// Global state
static SDL_Window* g_Window = NULL;
static SDL_GPUDevice* g_Device = NULL;
static SDL_GPUGraphicsPipeline* g_Pipeline = NULL;
static MouseCircleUniforms g_UniformValues = {0};

// Initialize SDL, GPU device, window and graphics pipeline
static int Init(void)
{
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return -1;
    }

    // Create GPU device
    g_Device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL,
        true,
        NULL);

    if (g_Device == NULL)
    {
        SDL_Log("GPUCreateDevice failed: %s", SDL_GetError());
        return -1;
    }

    // Create window
    g_Window = SDL_CreateWindow("MouseCircle", 640, 480, SDL_WINDOW_RESIZABLE);
    if (g_Window == NULL)
    {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    // Claim window for GPU
    if (!SDL_ClaimWindowForGPUDevice(g_Device, g_Window))
    {
        SDL_Log("GPUClaimWindow failed: %s", SDL_GetError());
        return -1;
    }

    // Create vertex shader
    SDL_GPUShaderCreateInfo vertexShaderInfo = {
        .code = (const Uint8*)VertexShaderMSL,
        .code_size = SDL_strlen(VertexShaderMSL),
        .entrypoint = "main0",
        .format = SDL_GPU_SHADERFORMAT_MSL,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_uniform_buffers = 0,
        .num_storage_buffers = 0,
        .num_storage_textures = 0
    };
    SDL_GPUShader* vertexShader = SDL_CreateGPUShader(g_Device, &vertexShaderInfo);
    if (vertexShader == NULL)
    {
        SDL_Log("Failed to create vertex shader: %s", SDL_GetError());
        return -1;
    }

    // Create fragment shader
    SDL_GPUShaderCreateInfo fragmentShaderInfo = {
        .code = (const Uint8*)FragmentShaderMSL,
        .code_size = SDL_strlen(FragmentShaderMSL),
        .entrypoint = "main0",
        .format = SDL_GPU_SHADERFORMAT_MSL,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 0,
        .num_uniform_buffers = 1,
        .num_storage_buffers = 0,
        .num_storage_textures = 0
    };
    SDL_GPUShader* fragmentShader = SDL_CreateGPUShader(g_Device, &fragmentShaderInfo);
    if (fragmentShader == NULL)
    {
        SDL_Log("Failed to create fragment shader: %s", SDL_GetError());
        return -1;
    }

    // Create graphics pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                .format = SDL_GetGPUSwapchainTextureFormat(g_Device, g_Window)
            }},
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
    };

    g_Pipeline = SDL_CreateGPUGraphicsPipeline(g_Device, &pipelineCreateInfo);
    if (g_Pipeline == NULL)
    {
        SDL_Log("Failed to create pipeline: %s", SDL_GetError());
        return -1;
    }

    // Clean up shader resources
    SDL_ReleaseGPUShader(g_Device, vertexShader);
    SDL_ReleaseGPUShader(g_Device, fragmentShader);

    // Initialize uniform values
    int width, height;
    SDL_GetWindowSizeInPixels(g_Window, &width, &height);
    g_UniformValues.mouse_x = width / 2.0f;
    g_UniformValues.mouse_y = height / 2.0f;
    g_UniformValues.resolution_x = (float)width;
    g_UniformValues.resolution_y = (float)height;

    SDL_Log("Move the mouse to see the circle follow!");

    return 0;
}

// Update function - called each frame
static int Update(void)
{
    // Update mouse position
    float mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    g_UniformValues.mouse_x = mouse_x;
    g_UniformValues.mouse_y = mouse_y;

    // Update resolution in case window was resized
    int width, height;
    SDL_GetWindowSizeInPixels(g_Window, &width, &height);
    g_UniformValues.resolution_x = (float)width;
    g_UniformValues.resolution_y = (float)height;

    return 0;
}

// Draw function - called each frame
static int Draw(void)
{
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(g_Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, g_Window, &swapchainTexture, NULL, NULL))
    {
        SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

    if (swapchainTexture != NULL)
    {
        SDL_GPUColorTargetInfo colorTargetInfo = {
            .texture = swapchainTexture,
            .clear_color = (SDL_FColor){ 0.0f, 0.0f, 1.0f, 1.0f },
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
        SDL_BindGPUGraphicsPipeline(renderPass, g_Pipeline);
        SDL_PushGPUFragmentUniformData(cmdbuf, 0, &g_UniformValues, sizeof(MouseCircleUniforms));
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

// Cleanup function
static void Quit(void)
{
    if (g_Pipeline != NULL)
    {
        SDL_ReleaseGPUGraphicsPipeline(g_Device, g_Pipeline);
        g_Pipeline = NULL;
    }

    if (g_Window != NULL && g_Device != NULL)
    {
        SDL_ReleaseWindowFromGPUDevice(g_Device, g_Window);
    }

    if (g_Window != NULL)
    {
        SDL_DestroyWindow(g_Window);
        g_Window = NULL;
    }

    if (g_Device != NULL)
    {
        SDL_DestroyGPUDevice(g_Device);
        g_Device = NULL;
    }

    SDL_Quit();
}

// Main function
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Initialize
    if (Init() < 0)
    {
        SDL_Log("Init failed!");
        Quit();
        return 1;
    }

    // Main loop
    bool quit = false;
    while (!quit)
    {
        // Handle events
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
        {
            if (evt.type == SDL_EVENT_QUIT)
            {
                quit = true;
            }
            else if (evt.type == SDL_EVENT_KEY_DOWN)
            {
                if (evt.key.key == SDLK_ESCAPE)
                {
                    quit = true;
                } else if (evt.key.key == SDLK_Q) {
                    quit = true;
                }
            }
        }

        // Update
        if (Update() < 0)
        {
            SDL_Log("Update failed!");
            break;
        }

        // Draw
        if (Draw() < 0)
        {
            SDL_Log("Draw failed!");
            break;
        }
    }

    // Cleanup
    Quit();

    return 0;
}
