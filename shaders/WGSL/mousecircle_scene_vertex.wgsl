struct SceneUniforms {
    mvp: mat4x4f,
    cameraPos: vec4f,
    fogColor: vec4f,
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

@group(0) @binding(0) var<uniform> uniforms: SceneUniforms;

@vertex
fn main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    let world = vec4f(input.position, 1.0);
    output.position = uniforms.mvp * world;
    output.surfaceType = input.surfaceType;
    output.uv = input.uv;
    output.normal = input.normal;
    output.worldPos = input.position;
    return output;
}

