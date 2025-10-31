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

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>

#include <base/io.h>
#include <base/buddy.h>
#include <base/mem.h>

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

typedef struct MouseCircleApp {
    SDL_Window* window;
    SDL_GPUDevice* device;
    SDL_GPUGraphicsPipeline* pipeline;
    MouseCircleUniforms uniforms;
    bool quit_requested;
    int frame_count;
} MouseCircleApp;

static MouseCircleApp g_App;
static bool g_buddy_initialized = false;

void ensure_heap_initialized(void);

// Initialize SDL, GPU device, window and graphics pipeline
static int Init(MouseCircleApp* app)
{
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return -1;
    }

    // Create GPU device
    app->device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL,
        true,
        NULL);

    if (app->device == NULL)
    {
        SDL_Log("GPUCreateDevice failed: %s", SDL_GetError());
        return -1;
    }

    // Create window
    app->window = SDL_CreateWindow("MouseCircle", 640, 480, SDL_WINDOW_RESIZABLE);
    if (app->window == NULL)
    {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    // Claim window for GPU
    if (!SDL_ClaimWindowForGPUDevice(app->device, app->window))
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
    SDL_GPUShader* vertexShader = SDL_CreateGPUShader(app->device, &vertexShaderInfo);
    if (vertexShader == NULL)
    {
        SDL_Log("Failed to create vertex shader: %s", SDL_GetError());
        return -1;
    }
    println(str_lit("Vertex shader created. Handle: {}"), (uint64_t)vertexShader);

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
    SDL_GPUShader* fragmentShader = SDL_CreateGPUShader(app->device, &fragmentShaderInfo);
    if (fragmentShader == NULL)
    {
        SDL_Log("Failed to create fragment shader: %s", SDL_GetError());
        return -1;
    }
    println(str_lit("Fragment shader created. Handle: {}"), (uint64_t)fragmentShader);

    // Create graphics pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                .format = SDL_GetGPUSwapchainTextureFormat(app->device, app->window)
            }},
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
    };

    app->pipeline = SDL_CreateGPUGraphicsPipeline(app->device, &pipelineCreateInfo);
    if (app->pipeline == NULL)
    {
        SDL_Log("Failed to create pipeline: %s", SDL_GetError());
        return -1;
    }

    // Clean up shader resources
    SDL_ReleaseGPUShader(app->device, vertexShader);
    SDL_ReleaseGPUShader(app->device, fragmentShader);

    // Initialize uniform values
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    app->uniforms.mouse_x = width / 2.0f;
    app->uniforms.mouse_y = height / 2.0f;
    app->uniforms.resolution_x = (float)width;
    app->uniforms.resolution_y = (float)height;

    SDL_Log("Move the mouse to see the circle follow!");

    return 0;
}

// Update function - called each frame
static int Update(MouseCircleApp* app)
{
    // Update mouse position
    float mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    app->uniforms.mouse_x = mouse_x;
    app->uniforms.mouse_y = mouse_y;

    // Update resolution in case window was resized
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    app->uniforms.resolution_x = (float)width;
    app->uniforms.resolution_y = (float)height;

    return 0;
}

// Draw function - called each frame
static int Draw(MouseCircleApp* app)
{
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(app->device);
    if (cmdbuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, app->window, &swapchainTexture, NULL, NULL))
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
        SDL_BindGPUGraphicsPipeline(renderPass, app->pipeline);
        SDL_PushGPUFragmentUniformData(cmdbuf, 0, &app->uniforms, sizeof(MouseCircleUniforms));
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

// Cleanup function
static void Quit(MouseCircleApp* app)
{
    if (app->pipeline != NULL)
    {
        SDL_ReleaseGPUGraphicsPipeline(app->device, app->pipeline);
        app->pipeline = NULL;
    }

    if (app->window != NULL && app->device != NULL)
    {
        SDL_ReleaseWindowFromGPUDevice(app->device, app->window);
    }

    if (app->window != NULL)
    {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }

    if (app->device != NULL)
    {
        SDL_DestroyGPUDevice(app->device);
        app->device = NULL;
    }

    SDL_Quit();
}

static const int MAX_FRAMES = 300;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!g_buddy_initialized) {
        ensure_heap_initialized();
        buddy_init();
        g_buddy_initialized = true;
    }

    base_memset(&g_App, 0, sizeof(g_App));

    if (Init(&g_App) < 0) {
        SDL_Log("Init failed!");
        Quit(&g_App);
        return SDL_APP_FAILURE;
    }

    g_App.quit_requested = false;
    g_App.frame_count = 0;

    *appstate = &g_App;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    MouseCircleApp *app = (MouseCircleApp *)appstate;
    if (!app || !event) {
        return SDL_APP_FAILURE;
    }

    if (event->type == SDL_EVENT_QUIT) {
        app->quit_requested = true;
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_Q) {
            app->quit_requested = true;
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    MouseCircleApp *app = (MouseCircleApp *)appstate;
    if (!app) {
        return SDL_APP_FAILURE;
    }

    println(str_lit("Main loop"));

#ifdef __wasi__
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
        SDL_AppEvent(app, &evt);
    }
#endif

    if (Update(app) < 0) {
        SDL_Log("Update failed!");
        return SDL_APP_FAILURE;
    }

    if (Draw(app) < 0) {
        SDL_Log("Draw failed!");
        return SDL_APP_FAILURE;
    }

    app->frame_count++;
    if (app->quit_requested || app->frame_count >= MAX_FRAMES) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    MouseCircleApp *app = (MouseCircleApp *)appstate;
    if (!app) {
        return;
    }

    Quit(app);
}

#ifdef __wasi__
static void *g_wasm_appstate = NULL;
static SDL_AppResult g_wasm_last_result = SDL_APP_SUCCESS;

__attribute__((export_name("app_init")))
int app_init(void)
{
    void *state = NULL;
    SDL_AppResult result = SDL_AppInit(&state, 0, NULL);
    if (result == SDL_APP_FAILURE) {
        return -1;
    }

    g_wasm_appstate = state;
    g_wasm_last_result = result;

    if (result == SDL_APP_SUCCESS) {
        return 1;
    }

    return 0;
}

__attribute__((export_name("app_iterate")))
int app_iterate(void)
{
    if (!g_wasm_appstate) {
        return -1;
    }

    SDL_AppResult result = SDL_AppIterate(g_wasm_appstate);
    if (result == SDL_APP_CONTINUE) {
        return 0;
    }

    g_wasm_last_result = result;
    return (result == SDL_APP_SUCCESS) ? 1 : -1;
}

__attribute__((export_name("app_quit")))
int app_quit(void)
{
    if (!g_wasm_appstate) {
        return 0;
    }

    SDL_AppQuit(g_wasm_appstate, g_wasm_last_result);
    g_wasm_appstate = NULL;
    return (g_wasm_last_result == SDL_APP_SUCCESS) ? 0 : -1;
}
#endif
