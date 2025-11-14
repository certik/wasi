const MAX_STATIC_LIGHTS: u32 = 16u;

struct SceneUniforms {
    mvp: mat4x4f,
    cameraPos: vec4f,
    fogColor: vec4f,
    staticLights: array<vec4f, MAX_STATIC_LIGHTS>,
    staticLightParams: vec4f,
    flashlightPos: vec4f,
    flashlightDir: vec4f,
    flashlightParams: vec4f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) surfaceType: f32,
    @location(1) uv: vec2f,
    @location(2) normal: vec3f,
    @location(3) worldPos: vec3f,
};

// SDL3 SPIRV requirement: fragment textures in set 2, uniforms in set 3
// Use single shared sampler to reduce binding complexity
@group(2) @binding(0) var floorTexture: texture_2d<f32>;
@group(2) @binding(1) var wallTexture: texture_2d<f32>;
@group(2) @binding(2) var ceilingTexture: texture_2d<f32>;
@group(2) @binding(3) var sphereTexture: texture_2d<f32>;
@group(2) @binding(4) var bookTexture: texture_2d<f32>;
@group(2) @binding(5) var chairTexture: texture_2d<f32>;
@group(2) @binding(6) var sharedSampler: sampler;
@group(3) @binding(0) var<uniform> uniforms: SceneUniforms;

fn checker(uv: vec2f) -> f32 {
    let scaled = floor(uv * 4.0);
    let v = (scaled.x + scaled.y) % 2.0;
    return select(0.7, 1.0, v < 0.5);
}

fn compute_static_lighting(normal: vec3f, world_pos: vec3f) -> f32 {
    let light_count = clamp(i32(uniforms.staticLightParams.x), 0, i32(MAX_STATIC_LIGHTS));
    let range = uniforms.staticLightParams.y;
    if (range <= 0.0 || light_count == 0) {
        return 0.0;
    }

    var total = 0.0;
    for (var i = 0; i < light_count; i = i + 1) {
        let light = uniforms.staticLights[u32(i)];
        let to_light = light.xyz - world_pos;
        let dist = length(to_light);
        if (dist > 0.0001 && dist < range) {
            let dir = to_light / dist;
            let ndotl = max(dot(normal, dir), 0.0);
            if (ndotl > 0.0) {
                let attenuation = pow(max(1.0 - dist / range, 0.0), 2.0);
                total += ndotl * attenuation * light.w;
            }
        }
    }
    return total;
}

fn compute_flashlight(normal: vec3f, world_pos: vec3f) -> f32 {
    if (uniforms.flashlightParams.x < 0.5) {
        return 0.0;
    }

    let light_vec = uniforms.flashlightPos.xyz - world_pos;
    let dist = length(light_vec);
    if (dist <= 0.0001 || dist > uniforms.flashlightPos.w) {
        return 0.0;
    }

    let dir_to_light = light_vec / dist;
    let ndotl = max(dot(normal, dir_to_light), 0.0);
    if (ndotl <= 0.0) {
        return 0.0;
    }

    let cutoff = uniforms.flashlightDir.w;
    let softness = clamp(uniforms.flashlightParams.z, 0.05, 0.95);
    let inner = mix(cutoff, 1.0, softness);
    let dir_from_light = -dir_to_light;
    let spot = dot(dir_from_light, uniforms.flashlightDir.xyz);
    if (spot <= cutoff) {
        return 0.0;
    }

    let denom = max(inner - cutoff, 0.001);
    var focus = clamp((spot - cutoff) / denom, 0.0, 1.0);
    focus = focus * focus;
    let attenuation = pow(max(1.0 - dist / uniforms.flashlightPos.w, 0.0), 2.0);
    return ndotl * focus * attenuation * uniforms.flashlightParams.y;
}

@fragment
fn main_(input: VertexOutput) -> @location(0) vec4f {
    // Sample textures unconditionally (required for uniform control flow)
    let floorColor = textureSample(floorTexture, sharedSampler, input.uv);
    let wallColor = textureSample(wallTexture, sharedSampler, input.uv);
    let ceilingColor = textureSample(ceilingTexture, sharedSampler, input.uv);
    let sphereColor = textureSample(sphereTexture, sharedSampler, input.uv);
    let bookColor = textureSample(bookTexture, sharedSampler, input.uv);
    let chairColor = textureSample(chairTexture, sharedSampler, input.uv);

    var baseColor: vec3f;
    if (input.surfaceType < 0.5) {
        // Floor: use sampled texture
        baseColor = floorColor.rgb;
    } else if (input.surfaceType < 1.5) {
        baseColor = wallColor.rgb;
    } else if (input.surfaceType < 2.5) {
        baseColor = ceilingColor.rgb;
    } else if (input.surfaceType < 3.5) {
        baseColor = vec3f(0.7, 0.5, 0.3) * checker(input.uv);
    } else if (input.surfaceType < 4.5) {
        // Sphere: use sphere texture
        baseColor = sphereColor.rgb;
    } else if (input.surfaceType < 5.5) {
        // Book mesh
        baseColor = bookColor.rgb;
    } else {
        // Chair mesh
        baseColor = chairColor.rgb;
    }

    let n = normalize(input.normal);
    let staticLight = compute_static_lighting(n, input.worldPos);
    let flashlightLight = compute_flashlight(n, input.worldPos);
    let ambient = uniforms.staticLightParams.z;
    var lighting = ambient + staticLight + flashlightLight;
    lighting = clamp(lighting, ambient, 6.0);
    let fogFactor = exp(-distance(input.worldPos, uniforms.cameraPos.xyz) * 0.08);
    var color = baseColor * lighting;
    color = mix(uniforms.fogColor.xyz, color, fogFactor);
    return vec4f(color, 1.0);
}
