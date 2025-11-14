const MAX_STATIC_LIGHTS: u32 = 16u;
const MATERIAL_COUNT: u32 = 7u;

struct Material {
    metalness: f32,
    roughness: f32,
    ior: f32,
    emissiveIntensity: f32,
};

struct SceneUniforms {
    mvp: mat4x4f,
    cameraPos: vec4f,
    fogColor: vec4f,
    staticLights: array<vec4f, MAX_STATIC_LIGHTS>,
    staticLightColors: array<vec4f, MAX_STATIC_LIGHTS>,
    staticLightParams: vec4f,
    flashlightPos: vec4f,
    flashlightDir: vec4f,
    flashlightParams: vec4f,
    screenParams: vec4f,
    materials: array<Material, MATERIAL_COUNT>,
};

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) surfaceType: f32,
    @location(2) uv: vec2f,
    @location(3) normal: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) surfaceType: f32,
    @location(1) uv: vec2f,
    @location(2) normal: vec3f,
    @location(3) worldPos: vec3f,
};

// SDL3 SPIRV requirement: vertex uniform buffers must be in set 1
@group(1) @binding(0) var<uniform> uniforms: SceneUniforms;

const DEBUG_EXTRUDE_DISTANCE: f32 = 1.0;

@vertex
fn main_(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    var world_pos = input.position;
    if (input.surfaceType > 0.5 && input.surfaceType < 1.5) {
        let base_tile = floor(input.uv);
        let local_uv = fract(input.uv);
        let tile_matches = base_tile.x == 0.0 && base_tile.y == 0.0;
        if (tile_matches && local_uv.x <= 0.2 && local_uv.y <= 0.2) {
            let normal_dir = normalize(input.normal);
            world_pos += normal_dir * DEBUG_EXTRUDE_DISTANCE;
        }
    }
    let world = vec4f(world_pos, 1.0);
    output.position = uniforms.mvp * world;
    output.surfaceType = input.surfaceType;
    output.uv = input.uv;
    output.normal = input.normal;
    output.worldPos = world_pos;
    return output;
}
