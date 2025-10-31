/*
 * Rotating Cube Example for SDL3 GPU with Metal
 *
 * This example renders a rotating 3D cube using vertex buffers.
 *
 * Compile with:
 *   pixi r build_mousecircle_sdl
 *
 * Run with:
 *   pixi r test_mousecircle_sdl
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>

#include <base/io.h>
#include <base/buddy.h>
#include <base/mem.h>
#include <base/mat4.h>
#include <base/base_math.h>

// Vertex structure matching our vertex buffer layout
typedef struct {
    float position[3];  // x, y, z
    float color[3];     // r, g, b
} Vertex;

// Embedded Metal Shaders for 3D cube rendering with checkerboard
static const char* VertexShaderMSL =
"#include <metal_stdlib>\n"
"#include <simd/simd.h>\n"
"\n"
"using namespace metal;\n"
"\n"
"struct VertexInput {\n"
"    float3 position [[attribute(0)]];\n"
"    float3 color [[attribute(1)]];\n"
"};\n"
"\n"
"struct VertexOutput {\n"
"    float4 position [[position]];\n"
"    float3 color;\n"
"    float3 world_pos;\n"
"};\n"
"\n"
"struct Uniforms {\n"
"    float4x4 mvp;\n"
"};\n"
"\n"
"vertex VertexOutput main0(\n"
"    VertexInput in [[stage_in]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    VertexOutput out;\n"
"    out.position = uniforms.mvp * float4(in.position, 1.0);\n"
"    out.color = in.color;\n"
"    out.world_pos = in.position;\n"
"    return out;\n"
"}\n";

static const char* FragmentShaderMSL =
"#include <metal_stdlib>\n"
"#include <simd/simd.h>\n"
"\n"
"using namespace metal;\n"
"\n"
"struct VertexOutput {\n"
"    float4 position [[position]];\n"
"    float3 color;\n"
"    float3 world_pos;\n"
"};\n"
"\n"
"fragment float4 main0(VertexOutput in [[stage_in]]) {\n"
"    // Small checkerboard pattern based on world position\n"
"    float checker_size = 0.25;\n"
"    float3 scaled = in.world_pos / checker_size;\n"
"    int check = int(floor(scaled.x) + floor(scaled.y) + floor(scaled.z));\n"
"    float checker = (check & 1) ? 0.8 : 1.0;\n"
"    \n"
"    return float4(in.color * checker, 1.0);\n"
"}\n";

// Uniform data structure (MVP matrix)
typedef struct {
    mat4 mvp;
} CubeUniforms;

typedef struct {
    SDL_Window* window;
    SDL_GPUDevice* device;
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* vertex_buffer;
    SDL_GPUBuffer* index_buffer;
    SDL_GPUTransferBuffer* vertex_transfer_buffer;
    SDL_GPUTransferBuffer* index_transfer_buffer;
    SDL_GPUTexture* depth_texture;
    int window_width;
    int window_height;
    CubeUniforms uniforms;

    // Camera state
    float camera_x, camera_y, camera_z;
    float camera_yaw, camera_pitch;

    // Input state
    bool key_w, key_a, key_s, key_d;
    bool key_space, key_shift, key_ctrl;
    float mouse_delta_x, mouse_delta_y;

    bool quit_requested;
} CubeApp;

static CubeApp g_App;
static bool g_buddy_initialized = false;

void ensure_heap_initialized(void);

// Cube geometry: 24 vertices (4 per face), each with position and color
static const Vertex cube_vertices[] = {
    // Front face (red)
    {{-1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},  // 0
    {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},  // 1
    {{ 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},  // 2
    {{-1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},  // 3

    // Right face (green)
    {{ 1.0f, -1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}},  // 4
    {{ 1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},  // 5
    {{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},  // 6
    {{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}},  // 7

    // Back face (blue)
    {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},  // 8
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},  // 9
    {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},  // 10
    {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},  // 11

    // Left face (yellow)
    {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}},  // 12
    {{-1.0f, -1.0f,  1.0f}, {1.0f, 1.0f, 0.0f}},  // 13
    {{-1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 0.0f}},  // 14
    {{-1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}},  // 15

    // Top face (cyan)
    {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 1.0f}},  // 16
    {{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 1.0f}},  // 17
    {{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 1.0f}},  // 18
    {{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 1.0f}},  // 19

    // Bottom face (magenta)
    {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 1.0f}},  // 20
    {{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 1.0f}},  // 21
    {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 1.0f}},  // 22
    {{-1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 1.0f}},  // 23
};

// Cube indices: 12 triangles (6 faces * 2 triangles per face)
static const Uint32 cube_indices[] = {
    // Front face
    0, 1, 2,  2, 3, 0,
    // Right face
    4, 5, 6,  6, 7, 4,
    // Back face
    8, 9, 10,  10, 11, 8,
    // Left face
    12, 13, 14,  14, 15, 12,
    // Top face
    16, 17, 18,  18, 19, 16,
    // Bottom face
    20, 21, 22,  22, 23, 20,
};

static const Uint32 cube_vertex_count = sizeof(cube_vertices) / sizeof(Vertex);
static const Uint32 cube_index_count = sizeof(cube_indices) / sizeof(Uint32);

// Initialize SDL, GPU device, window and graphics pipeline
static int Init(CubeApp* app)
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

    // Create vertex shader (uses vertex attributes and uniform buffer for MVP)
    SDL_GPUShaderCreateInfo vertexShaderInfo = {
        .code = (const Uint8*)VertexShaderMSL,
        .code_size = SDL_strlen(VertexShaderMSL),
        .entrypoint = "main0",
        .format = SDL_GPU_SHADERFORMAT_MSL,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_uniform_buffers = 1,  // MVP matrix
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

    // Create fragment shader (no uniforms, just interpolated color)
    SDL_GPUShaderCreateInfo fragmentShaderInfo = {
        .code = (const Uint8*)FragmentShaderMSL,
        .code_size = SDL_strlen(FragmentShaderMSL),
        .entrypoint = "main0",
        .format = SDL_GPU_SHADERFORMAT_MSL,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 0,
        .num_uniform_buffers = 0,
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

    // Get window dimensions for depth buffer
    SDL_GetWindowSizeInPixels(app->window, &app->window_width, &app->window_height);

    // Create depth texture
    SDL_GPUTextureCreateInfo depthTextureInfo = {
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        .width = (Uint32)app->window_width,
        .height = (Uint32)app->window_height,
        .layer_count_or_depth = 1,
        .num_levels = 1
    };
    app->depth_texture = SDL_CreateGPUTexture(app->device, &depthTextureInfo);
    if (app->depth_texture == NULL)
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return -1;
    }

    // Configure vertex input layout
    SDL_GPUVertexAttribute vertexAttributes[] = {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,  // position
            .offset = 0
        },
        {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,  // color
            .offset = sizeof(float) * 3
        }
    };

    SDL_GPUVertexBufferDescription vertexBufferDesc = {
        .slot = 0,
        .pitch = sizeof(Vertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX
    };

    SDL_GPUVertexInputState vertexInputState = {
        .vertex_buffer_descriptions = &vertexBufferDesc,
        .num_vertex_buffers = 1,
        .vertex_attributes = vertexAttributes,
        .num_vertex_attributes = 2
    };

    // Configure depth/stencil state
    SDL_GPUDepthStencilState depthStencilState = {
        .enable_depth_test = true,
        .enable_depth_write = true,
        .compare_op = SDL_GPU_COMPAREOP_LESS
    };

    // Create graphics pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
        .vertex_input_state = vertexInputState,
        .depth_stencil_state = depthStencilState,
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                .format = SDL_GetGPUSwapchainTextureFormat(app->device, app->window)
            }},
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
            .has_depth_stencil_target = true
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
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

    // Create vertex buffer
    SDL_GPUBufferCreateInfo vertexBufferInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(cube_vertices)
    };
    app->vertex_buffer = SDL_CreateGPUBuffer(app->device, &vertexBufferInfo);
    if (app->vertex_buffer == NULL)
    {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return -1;
    }

    // Create index buffer
    SDL_GPUBufferCreateInfo indexBufferInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = sizeof(cube_indices)
    };
    app->index_buffer = SDL_CreateGPUBuffer(app->device, &indexBufferInfo);
    if (app->index_buffer == NULL)
    {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        return -1;
    }

    // Create transfer buffers for uploading data
    SDL_GPUTransferBufferCreateInfo vertexTransferInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(cube_vertices)
    };
    app->vertex_transfer_buffer = SDL_CreateGPUTransferBuffer(app->device, &vertexTransferInfo);

    SDL_GPUTransferBufferCreateInfo indexTransferInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = sizeof(cube_indices)
    };
    app->index_transfer_buffer = SDL_CreateGPUTransferBuffer(app->device, &indexTransferInfo);

    // Upload vertex data
    void* vertex_data = SDL_MapGPUTransferBuffer(app->device, app->vertex_transfer_buffer, false);
    base_memcpy(vertex_data, cube_vertices, sizeof(cube_vertices));
    SDL_UnmapGPUTransferBuffer(app->device, app->vertex_transfer_buffer);

    // Upload index data
    void* index_data = SDL_MapGPUTransferBuffer(app->device, app->index_transfer_buffer, false);
    base_memcpy(index_data, cube_indices, sizeof(cube_indices));
    SDL_UnmapGPUTransferBuffer(app->device, app->index_transfer_buffer);

    // Submit transfer commands
    SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(app->device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);

    SDL_GPUTransferBufferLocation vertexSource = {
        .transfer_buffer = app->vertex_transfer_buffer,
        .offset = 0
    };
    SDL_GPUBufferRegion vertexDest = {
        .buffer = app->vertex_buffer,
        .offset = 0,
        .size = sizeof(cube_vertices)
    };
    SDL_UploadToGPUBuffer(copyPass, &vertexSource, &vertexDest, false);

    SDL_GPUTransferBufferLocation indexSource = {
        .transfer_buffer = app->index_transfer_buffer,
        .offset = 0
    };
    SDL_GPUBufferRegion indexDest = {
        .buffer = app->index_buffer,
        .offset = 0,
        .size = sizeof(cube_indices)
    };
    SDL_UploadToGPUBuffer(copyPass, &indexSource, &indexDest, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmd);

    // Initialize camera position (back from the cube)
    app->camera_x = 0.0f;
    app->camera_y = 0.0f;
    app->camera_z = 5.0f;
    app->camera_yaw = 0.0f;
    app->camera_pitch = 0.0f;

    // Initialize input state
    app->key_w = false;
    app->key_a = false;
    app->key_s = false;
    app->key_d = false;
    app->key_space = false;
    app->key_shift = false;
    app->key_ctrl = false;
    app->mouse_delta_x = 0.0f;
    app->mouse_delta_y = 0.0f;

    // Enable relative mouse mode for FPS controls
    SDL_SetWindowRelativeMouseMode(app->window, true);

    SDL_Log("FPS cube demo initialized!");

    return 0;
}

// Update function - called each frame
static int Update(CubeApp* app)
{
    // Camera rotation from mouse input
    float mouse_sensitivity = 0.002f;
    app->camera_yaw += app->mouse_delta_x * mouse_sensitivity;
    app->camera_pitch -= app->mouse_delta_y * mouse_sensitivity;

    // Clamp pitch to avoid gimbal lock
    float max_pitch = 1.5f;
    if (app->camera_pitch > max_pitch) app->camera_pitch = max_pitch;
    if (app->camera_pitch < -max_pitch) app->camera_pitch = -max_pitch;

    // Reset mouse delta (will be updated by events)
    app->mouse_delta_x = 0.0f;
    app->mouse_delta_y = 0.0f;

    // Calculate movement vectors from camera orientation
    float cos_yaw = fast_cos(app->camera_yaw);
    float sin_yaw = fast_sin(app->camera_yaw);

    float forward_x = sin_yaw;
    float forward_z = cos_yaw;
    float right_x = cos_yaw;
    float right_z = -sin_yaw;

    // Camera movement from WASD input
    float move_speed = app->key_shift ? 0.2f : 0.1f;

    if (app->key_w) {
        app->camera_x += forward_x * move_speed;
        app->camera_z += forward_z * move_speed;
    }
    if (app->key_s) {
        app->camera_x -= forward_x * move_speed;
        app->camera_z -= forward_z * move_speed;
    }
    if (app->key_a) {
        app->camera_x -= right_x * move_speed;
        app->camera_z -= right_z * move_speed;
    }
    if (app->key_d) {
        app->camera_x += right_x * move_speed;
        app->camera_z += right_z * move_speed;
    }
    if (app->key_space) {
        app->camera_y += move_speed;
    }
    if (app->key_ctrl) {
        app->camera_y -= move_speed;
    }

    // Get window dimensions for aspect ratio
    int width, height;
    SDL_GetWindowSizeInPixels(app->window, &width, &height);
    float aspect = (float)width / (float)height;

    // Build MVP matrix
    // Projection: perspective with 60 degree FOV
    mat4 projection = mat4_perspective(1.047f, aspect, 0.1f, 100.0f);

    // View: from camera position and orientation
    mat4 view = mat4_look_at_fps(app->camera_x, app->camera_y, app->camera_z,
                                  app->camera_yaw, app->camera_pitch);

    // Model: identity (cube stays at origin)
    mat4 model = mat4_identity();

    // Combine: MVP = projection * view * model
    mat4 vp = mat4_multiply(projection, view);
    app->uniforms.mvp = mat4_multiply(vp, model);

    return 0;
}

// Draw function - called each frame
static int Draw(CubeApp* app)
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
            .clear_color = (SDL_FColor){ 0.1f, 0.1f, 0.15f, 1.0f },  // Dark background
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };

        SDL_GPUDepthStencilTargetInfo depthTargetInfo = {
            .texture = app->depth_texture,
            .clear_depth = 1.0f,
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, &depthTargetInfo);

        // Bind pipeline
        SDL_BindGPUGraphicsPipeline(renderPass, app->pipeline);

        // Push MVP matrix as vertex shader uniform
        SDL_PushGPUVertexUniformData(cmdbuf, 0, &app->uniforms.mvp, sizeof(mat4));

        // Bind vertex buffer
        SDL_GPUBufferBinding vertexBinding = {
            .buffer = app->vertex_buffer,
            .offset = 0
        };
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

        // Bind index buffer
        SDL_GPUBufferBinding indexBinding = {
            .buffer = app->index_buffer,
            .offset = 0
        };
        SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        // Draw the cube
        SDL_DrawGPUIndexedPrimitives(renderPass, cube_index_count, 1, 0, 0, 0);

        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

// Cleanup function
static void Quit(CubeApp* app)
{
    // Release buffers
    if (app->vertex_buffer != NULL)
    {
        SDL_ReleaseGPUBuffer(app->device, app->vertex_buffer);
        app->vertex_buffer = NULL;
    }

    if (app->index_buffer != NULL)
    {
        SDL_ReleaseGPUBuffer(app->device, app->index_buffer);
        app->index_buffer = NULL;
    }

    if (app->vertex_transfer_buffer != NULL)
    {
        SDL_ReleaseGPUTransferBuffer(app->device, app->vertex_transfer_buffer);
        app->vertex_transfer_buffer = NULL;
    }

    if (app->index_transfer_buffer != NULL)
    {
        SDL_ReleaseGPUTransferBuffer(app->device, app->index_transfer_buffer);
        app->index_transfer_buffer = NULL;
    }

    if (app->depth_texture != NULL)
    {
        SDL_ReleaseGPUTexture(app->device, app->depth_texture);
        app->depth_texture = NULL;
    }

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

    *appstate = &g_App;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    CubeApp *app = (CubeApp *)appstate;
    if (!app || !event) {
        return SDL_APP_FAILURE;
    }

    if (event->type == SDL_EVENT_QUIT) {
        app->quit_requested = true;
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_Q) {
            app->quit_requested = true;
        } else if (event->key.key == SDLK_W) {
            app->key_w = true;
        } else if (event->key.key == SDLK_A) {
            app->key_a = true;
        } else if (event->key.key == SDLK_S) {
            app->key_s = true;
        } else if (event->key.key == SDLK_D) {
            app->key_d = true;
        } else if (event->key.key == SDLK_SPACE) {
            app->key_space = true;
        } else if (event->key.key == SDLK_LSHIFT) {
            app->key_shift = true;
        } else if (event->key.key == SDLK_LCTRL) {
            app->key_ctrl = true;
        }
    } else if (event->type == SDL_EVENT_KEY_UP) {
        if (event->key.key == SDLK_W) {
            app->key_w = false;
        } else if (event->key.key == SDLK_A) {
            app->key_a = false;
        } else if (event->key.key == SDLK_S) {
            app->key_s = false;
        } else if (event->key.key == SDLK_D) {
            app->key_d = false;
        } else if (event->key.key == SDLK_SPACE) {
            app->key_space = false;
        } else if (event->key.key == SDLK_LSHIFT) {
            app->key_shift = false;
        } else if (event->key.key == SDLK_LCTRL) {
            app->key_ctrl = false;
        }
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        app->mouse_delta_x += event->motion.xrel;
        app->mouse_delta_y += event->motion.yrel;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    CubeApp *app = (CubeApp *)appstate;
    if (!app) {
        return SDL_APP_FAILURE;
    }

    if (Update(app) < 0) {
        return SDL_APP_FAILURE;
    }

    if (Draw(app) < 0) {
        return SDL_APP_FAILURE;
    }

    if (app->quit_requested) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    CubeApp *app = (CubeApp *)appstate;
    if (!app) {
        return;
    }

    Quit(app);
}
