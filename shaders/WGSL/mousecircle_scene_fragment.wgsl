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
    screenParams: vec4f,
};

struct FragmentInput {
    @location(0) surfaceType: f32,
    @location(1) uv: vec2f,
    @location(2) normal: vec3f,
    @location(3) worldPos: vec3f,
};

struct FlashlightContribution {
    diffuse: f32,
    specular: f32,
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

fn compute_flashlight(normal: vec3f, world_pos: vec3f, frag_coord: vec4f) -> FlashlightContribution {
    if (uniforms.flashlightParams.x < 0.5) {
        return FlashlightContribution(0.0, 0.0);
    }

    let width = uniforms.screenParams.x;
    let height = uniforms.screenParams.y;
    let min_dim = uniforms.screenParams.z;
    if (width <= 0.0 || height <= 0.0 || min_dim <= 0.0) {
        return FlashlightContribution(0.0, 0.0);
    }

    let pixel = frag_coord.xy;
    let center = vec2f(width * 0.5, height * 0.5);
    let delta = (pixel - center) / min_dim;
    let radius = uniforms.flashlightParams.w;
    if (radius <= 0.0) {
        return FlashlightContribution(0.0, 0.0);
    }
    let dist = length(delta);
    if (dist > radius) {
        return FlashlightContribution(0.0, 0.0);
    }

    let falloff = clamp(dist / radius, 0.0, 1.0);
    let base_intensity = (1.0 - falloff * falloff) * uniforms.flashlightParams.y * 0.35;

    if (uniforms.flashlightDir.w <= 0.0) {
        return FlashlightContribution(base_intensity, 0.0);
    }

    let to_fragment = world_pos - uniforms.flashlightPos.xyz;
    let dist_along_axis = dot(to_fragment, uniforms.flashlightDir.xyz);
    if (dist_along_axis <= 0.0 || dist_along_axis > uniforms.flashlightPos.w) {
        return FlashlightContribution(base_intensity, 0.0);
    }

    let dir_norm = normalize(to_fragment);
    let spot = dot(dir_norm, uniforms.flashlightDir.xyz);
    let cutoff = uniforms.flashlightDir.w;
    if (spot <= cutoff) {
        return FlashlightContribution(base_intensity, 0.0);
    }

    let focus = pow((spot - cutoff) / max(1.0 - cutoff, 0.001), 2.0);
    let ndotl = max(dot(normal, uniforms.flashlightDir.xyz), 0.0);
    let distance_atten = clamp(1.0 - dist_along_axis / uniforms.flashlightPos.w, 0.0, 1.0);
    let spec_dir = normalize(uniforms.flashlightDir.xyz + vec3f(0.0, 1.0, 0.0));
    let spec = pow(max(dot(normal, spec_dir), 0.0), 24.0);
    var beam = base_intensity + focus * ndotl * distance_atten * uniforms.flashlightParams.y * 0.5;
    beam = min(beam, uniforms.flashlightParams.y * 0.7);
    let specular = spec * focus * distance_atten * 0.15 * uniforms.flashlightParams.y;
    return FlashlightContribution(beam, specular);
}

@fragment
fn main_(input: FragmentInput, @builtin(position) frag_coord: vec4f) -> @location(0) vec4f {
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
    let flashlight = compute_flashlight(n, input.worldPos, frag_coord);
    if (uniforms.screenParams.w > 0.5) {
        let mapped = normalize(input.normal) * 0.5 + vec3f(0.5);
        return vec4f(mapped, 1.0);
    }
    let ambient = uniforms.staticLightParams.z;
    var lighting = ambient + staticLight;
    lighting = clamp(lighting, ambient, 6.0);
    let fogFactor = exp(-distance(input.worldPos, uniforms.cameraPos.xyz) * 0.08);
    var color = baseColor * (lighting + flashlight.diffuse);
    color += flashlight.specular * vec3f(1.0, 0.95, 0.85);
    color = mix(uniforms.fogColor.xyz, color, fogFactor);
    return vec4f(color, 1.0);
}
