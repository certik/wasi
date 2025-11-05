struct SceneUniforms {
    mvp: mat4x4f,
    cameraPos: vec4f,
    fogColor: vec4f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) surfaceType: f32,
    @location(1) uv: vec2f,
    @location(2) normal: vec3f,
    @location(3) worldPos: vec3f,
};

@group(0) @binding(0) var<uniform> uniforms: SceneUniforms;
@group(1) @binding(0) var floorTexture: texture_2d<f32>;
@group(1) @binding(1) var floorSampler: sampler;
@group(1) @binding(2) var wallTexture: texture_2d<f32>;
@group(1) @binding(3) var wallSampler: sampler;
@group(1) @binding(4) var ceilingTexture: texture_2d<f32>;
@group(1) @binding(5) var ceilingSampler: sampler;

fn checker(uv: vec2f) -> f32 {
    let scaled = floor(uv * 4.0);
    let v = (scaled.x + scaled.y) % 2.0;
    return select(0.7, 1.0, v < 0.5);
}

@fragment
fn main(input: VertexOutput) -> @location(0) vec4f {
    // Sample textures unconditionally (required for uniform control flow)
    let floorColor = textureSample(floorTexture, floorSampler, input.uv);
    let wallColor = textureSample(wallTexture, wallSampler, input.uv);
    let ceilingColor = textureSample(ceilingTexture, ceilingSampler, input.uv);

    var baseColor: vec3f;
    if (input.surfaceType < 0.5) {
        // Floor: use sampled texture
        baseColor = floorColor.rgb;
    } else if (input.surfaceType < 1.5) {
        baseColor = wallColor.rgb;
    } else if (input.surfaceType < 2.5) {
        baseColor = ceilingColor.rgb;
    } else {
        baseColor = vec3f(0.7, 0.5, 0.3) * checker(input.uv);
    }

    let n = normalize(input.normal);
    let lightDir = normalize(vec3f(0.35, 1.0, 0.45));
    let diff = max(dot(n, lightDir), 0.15);
    let fogFactor = exp(-distance(input.worldPos, uniforms.cameraPos.xyz) * 0.08);
    var color = baseColor * diff;
    color = mix(uniforms.fogColor.xyz, color, fogFactor);
    return vec4f(color, 1.0);
}
