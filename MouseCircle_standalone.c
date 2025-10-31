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
#include <base/base_math.h>

// Embedded Metal Shaders (compiled MSL)
static const char* VertexShaderMSL =
"#include <metal_stdlib>\n"
"#include <simd/simd.h>\n"
"\n"
"using namespace metal;\n"
"\n"
"struct VertexInput {\n"
"    float3 position [[attribute(0)]];\n"
"    float3 normal [[attribute(1)]];\n"
"    float2 uv [[attribute(2)]];\n"
"};\n"
"\n"
"struct Uniforms {\n"
"    float4x4 mvp;\n"
"    float4x4 normal_matrix;\n"
"    float4 light_dir;\n"
"};\n"
"\n"
"struct VertexOutput {\n"
"    float4 position [[position]];\n"
"    float3 normal;\n"
"    float2 uv;\n"
"};\n"
"\n"
"vertex VertexOutput main0(VertexInput in [[stage_in]], constant Uniforms& uniforms [[buffer(1)]]) {\n"
"    VertexOutput out;\n"
"    float4 pos = float4(in.position, 1.0);\n"
"    out.position = uniforms.mvp * pos;\n"
"    float4 transformed_normal = uniforms.normal_matrix * float4(in.normal, 0.0);\n"
"    out.normal = normalize(transformed_normal.xyz);\n"
"    out.uv = in.uv;\n"
"    return out;\n"
"}\n";

static const char* FragmentShaderMSL =
"#include <metal_stdlib>\n"
"#include <simd/simd.h>\n"
"\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float4x4 mvp;\n"
"    float4x4 normal_matrix;\n"
"    float4 light_dir;\n"
"};\n"
"\n"
"struct VertexOutput {\n"
"    float4 position [[position]];\n"
"    float3 normal;\n"
"    float2 uv;\n"
"};\n"
"\n"
"fragment float4 main0(VertexOutput input [[stage_in]], constant Uniforms& uniforms [[buffer(1)]]) {\n"
"    float3 light_dir = normalize(uniforms.light_dir.xyz);\n"
"    float diffuse = max(dot(input.normal, light_dir), 0.0);\n"
"    float3 base_color = float3(0.8, 0.3, 0.2);\n"
"    float3 color = base_color * (0.2 + 0.8 * diffuse);\n"
"    return float4(color, 1.0);\n"
"}\n";

typedef struct CubeVertex {
    float position[3];
    float normal[3];
    float uv[2];
} CubeVertex;

typedef struct CubeUniforms {
    float mvp[16];
    float normal_matrix[16];
    float light_dir[4];
} CubeUniforms;

typedef struct MouseCircleApp {
    SDL_Window* window;
    SDL_GPUDevice* device;
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* vertex_buffer;
    CubeUniforms uniforms;
    bool quit_requested;
    int frame_count;
    float rotation;
    Uint64 previous_counter;
    double time_accumulator;
    int fps_frames;
    Uint32 vertex_count;
} MouseCircleApp;

static MouseCircleApp g_App;
static bool g_buddy_initialized = false;

void ensure_heap_initialized(void);

static inline void mat4_identity(float *m) {
    for (int i = 0; i < 16; i++) {
        m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

static void mat4_multiply(float *out, const float *a, const float *b) {
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            out[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
}

static void mat4_rotate_x(float *m, float angle) {
    float c = __builtin_cosf(angle);
    float s = __builtin_sinf(angle);
    mat4_identity(m);
    m[5] = c;
    m[9] = -s;
    m[6] = s;
    m[10] = c;
}

static void mat4_rotate_y(float *m, float angle) {
    float c = __builtin_cosf(angle);
    float s = __builtin_sinf(angle);
    mat4_identity(m);
    m[0] = c;
    m[8] = s;
    m[2] = -s;
    m[10] = c;
}

static void mat4_translate(float *m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

static void mat4_perspective(float *m, float fov_degrees, float aspect, float znear, float zfar) {
    const float pi = 3.14159265358979323846f;
    float fov_radians = fov_degrees * (pi / 180.0f);
    float f = 1.0f / __builtin_tanf(fov_radians * 0.5f);

    for (int i = 0; i < 16; i++) {
        m[i] = 0.0f;
    }
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zfar + znear) / (znear - zfar);
    m[11] = -1.0f;
    m[14] = (2.0f * zfar * znear) / (znear - zfar);
}

static void mat4_copy(float *dest, const float *src) {
    for (int i = 0; i < 16; i++) {
        dest[i] = src[i];
    }
}

static void vec3_normalize(float v[3]) {
    float length_sq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (length_sq > 0.0f) {
        float inv_len = 1.0f / __builtin_sqrtf(length_sq);
        v[0] *= inv_len;
        v[1] *= inv_len;
        v[2] *= inv_len;
    }
}

static const CubeVertex g_CubeVertices[] = {
    // Front face
    {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    // Back face
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f,-1.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f,-1.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f,-1.0f}, {0.0f, 0.0f}},
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f,-1.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f,-1.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f,-1.0f}, {0.0f, 1.0f}},
    // Left face
    {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.0f,  1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    // Right face
    {{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    // Top face
    {{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    // Bottom face
    {{-1.0f, -1.0f, -1.0f}, {0.0f,-1.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f,  1.0f}, {0.0f,-1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-1.0f, -1.0f,  1.0f}, {0.0f,-1.0f, 0.0f}, {0.0f, 1.0f}},
    {{-1.0f, -1.0f, -1.0f}, {0.0f,-1.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f, -1.0f}, {0.0f,-1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 1.0f, -1.0f,  1.0f}, {0.0f,-1.0f, 0.0f}, {1.0f, 1.0f}},
};

static int Update(MouseCircleApp* app, float delta_time);

// Initialize SDL, GPU device, window and graphics pipeline
static int Init(MouseCircleApp* app)
{
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return -1;
    }

    println(str_lit("Init: after SDL_Init"));

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

    println(str_lit("Init: after SDL_CreateGPUDevice"));

    // Create window
    app->window = SDL_CreateWindow("MouseCircle", 640, 480, SDL_WINDOW_RESIZABLE);
    if (app->window == NULL)
    {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    println(str_lit("Init: after SDL_CreateWindow"));

    // Claim window for GPU
    if (!SDL_ClaimWindowForGPUDevice(app->device, app->window))
    {
        SDL_Log("GPUClaimWindow failed: %s", SDL_GetError());
        return -1;
    }

    println(str_lit("Init: after SDL_ClaimWindowForGPUDevice"));

    // Create vertex shader
    SDL_GPUShaderCreateInfo vertexShaderInfo = {
        .code = (const Uint8*)VertexShaderMSL,
        .code_size = SDL_strlen(VertexShaderMSL),
        .entrypoint = "main0",
        .format = SDL_GPU_SHADERFORMAT_MSL,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_uniform_buffers = 1,
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

    println(str_lit("Init: after SDL_CreateGPUShader vertex"));

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

    println(str_lit("Init: after SDL_CreateGPUShader fragment"));

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

    println(str_lit("Init: after SDL_CreateGPUGraphicsPipeline"));

    // Clean up shader resources
    SDL_ReleaseGPUShader(app->device, vertexShader);
    SDL_ReleaseGPUShader(app->device, fragmentShader);

    // Initialize uniform values
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);

    SDL_GPUBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = (Uint32)sizeof(g_CubeVertices),
        .initial_data = g_CubeVertices
    };

    app->vertex_buffer = SDL_CreateGPUBuffer(app->device, &buffer_info);
    if (!app->vertex_buffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return -1;
    }

    println(str_lit("Init: after SDL_CreateGPUBuffer"));

    app->vertex_count = (Uint32)(sizeof(g_CubeVertices) / sizeof(g_CubeVertices[0]));
    app->rotation = 0.0f;
    app->previous_counter = SDL_GetPerformanceCounter();
    app->time_accumulator = 0.0;
    app->fps_frames = 0;
    Update(app, 0.0f);

    SDL_Log("Initialized rotating cube demo (%dx%d)", width, height);

    return 0;
}

// Update function - called each frame
static int Update(MouseCircleApp* app, float delta_time)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    float aspect = (height != 0) ? ((float)width / (float)height) : 1.0f;

    app->rotation += delta_time;

    println(str_lit("Update: delta {} aspect {}"), (double)delta_time, (double)aspect);

    float model_y[16];
    float model_x[16];
    float model[16];
    float view[16];
    float proj[16];
    float vp[16];

    mat4_rotate_y(model_y, app->rotation);
    mat4_rotate_x(model_x, app->rotation * 0.5f);
    mat4_multiply(model, model_y, model_x);

    mat4_translate(view, 0.0f, 0.0f, -5.0f);
    mat4_perspective(proj, 60.0f, aspect, 0.1f, 100.0f);
    mat4_multiply(vp, proj, view);
    mat4_multiply(app->uniforms.mvp, vp, model);

    mat4_copy(app->uniforms.normal_matrix, model);
    app->uniforms.normal_matrix[3] = 0.0f;
    app->uniforms.normal_matrix[7] = 0.0f;
    app->uniforms.normal_matrix[11] = 0.0f;
    app->uniforms.normal_matrix[12] = 0.0f;
    app->uniforms.normal_matrix[13] = 0.0f;
    app->uniforms.normal_matrix[14] = 0.0f;
    app->uniforms.normal_matrix[15] = 1.0f;

    float light_dir[3] = {0.5f, 1.0f, -0.6f};
    vec3_normalize(light_dir);
    app->uniforms.light_dir[0] = light_dir[0];
    app->uniforms.light_dir[1] = light_dir[1];
    app->uniforms.light_dir[2] = light_dir[2];
    app->uniforms.light_dir[3] = 0.0f;

    println(str_lit("Update: computed uniforms"));

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

    println(str_lit("Draw: acquired command buffer"));

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
        SDL_BindGPUVertexBuffer(renderPass, 0, app->vertex_buffer, 0, sizeof(CubeVertex));
        SDL_PushGPUFragmentUniformData(cmdbuf, 0, &app->uniforms, sizeof(CubeUniforms));
        SDL_DrawGPUPrimitives(renderPass, app->vertex_count, 1, 0, 0);
        SDL_EndGPURenderPass(renderPass);
        println(str_lit("Draw: finished render pass"));
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    println(str_lit("Draw: submitted command buffer"));

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

    if (app->vertex_buffer != NULL)
    {
        SDL_DestroyGPUBuffer(app->device, app->vertex_buffer);
        app->vertex_buffer = NULL;
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

    Uint64 now = SDL_GetPerformanceCounter();
    float delta_time = 0.0f;
    Uint64 freq = SDL_GetPerformanceFrequency();
    if (app->previous_counter != 0 && freq != 0) {
        delta_time = (float)((double)(now - app->previous_counter) / (double)freq);
    }
    app->previous_counter = now;

    println(str_lit("Iterate: delta {}"), (double)delta_time);

    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
        SDL_AppResult event_result = SDL_AppEvent(app, &evt);
        if (event_result != SDL_APP_CONTINUE) {
            return event_result;
        }
    }

    println(str_lit("Iterate: events processed"));

    if (Update(app, delta_time) < 0) {
        SDL_Log("Update failed!");
        return SDL_APP_FAILURE;
    }

    if (Draw(app) < 0) {
        SDL_Log("Draw failed!");
        return SDL_APP_FAILURE;
    }

    app->frame_count++;
    app->time_accumulator += delta_time;
    app->fps_frames++;

    if (app->time_accumulator >= 1.0) {
        double fps = (double)app->fps_frames / app->time_accumulator;
        SDL_Log("FPS: %.2f", fps);
        app->time_accumulator = 0.0;
        app->fps_frames = 0;
    }

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
