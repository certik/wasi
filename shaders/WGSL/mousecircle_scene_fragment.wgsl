const MAX_STATIC_LIGHTS: u32 = 16u;
const MATERIAL_COUNT: u32 = 7u;
const PI: f32 = 3.14159265359;

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

struct FragmentInput {
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
@group(2) @binding(6) var debugAlbedoTexture: texture_2d<f32>;
@group(2) @binding(7) var normalTexture: texture_2d<f32>;
@group(2) @binding(8) var metallicRoughnessTexture: texture_2d<f32>;
@group(2) @binding(9) var emissiveTexture: texture_2d<f32>;
@group(2) @binding(10) var heightTexture: texture_2d<f32>;
@group(2) @binding(11) var sharedSampler: sampler;
@group(3) @binding(0) var<uniform> uniforms: SceneUniforms;

fn checker(uv: vec2f) -> f32 {
    let scaled = floor(uv * 4.0);
    let v = (scaled.x + scaled.y) % 2.0;
    return select(0.7, 1.0, v < 0.5);
}

fn fresnel_schlick(cos_theta: f32, f0: vec3f) -> vec3f {
    let clamped = clamp(1.0 - cos_theta, 0.0, 1.0);
    return f0 + (vec3f(1.0) - f0) * pow(clamped, 5.0);
}

fn distribution_ggx(n: vec3f, h: vec3f, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let n_dot_h = max(dot(n, h), 0.0);
    let n_dot_h2 = n_dot_h * n_dot_h;
    let denom = n_dot_h2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

fn geometry_schlick_ggx(n_dot_v: f32, roughness: f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;
    return n_dot_v / (n_dot_v * (1.0 - k) + k);
}

fn geometry_smith(n: vec3f, v: vec3f, l: vec3f, roughness: f32) -> f32 {
    let n_dot_v = max(dot(n, v), 0.0);
    let n_dot_l = max(dot(n, l), 0.0);
    let ggx1 = geometry_schlick_ggx(n_dot_v, roughness);
    let ggx2 = geometry_schlick_ggx(n_dot_l, roughness);
    return ggx1 * ggx2;
}

fn cook_torrance_brdf(
    n: vec3f,
    v: vec3f,
    l: vec3f,
    albedo: vec3f,
    metallic: f32,
    roughness: f32,
    f0: vec3f,
    light_color: vec3f,
    light_intensity: f32
) -> vec3f {
    let h = normalize(v + l);
    let n_dot_v = max(dot(n, v), 0.0001);
    let n_dot_l = max(dot(n, l), 0.0001);
    let h_dot_v = max(dot(h, v), 0.0);

    let f = fresnel_schlick(h_dot_v, f0);
    let d = distribution_ggx(n, h, roughness);
    let g = geometry_smith(n, v, l, roughness);
    let specular = (d * g * f) / (4.0 * n_dot_v * n_dot_l);

    let ks = f;
    let kd = (vec3f(1.0) - ks) * (1.0 - metallic);
    let diffuse = albedo / PI;

    return (kd * diffuse + specular) * light_color * light_intensity * n_dot_l;
}

fn compute_static_lighting(
    normal: vec3f,
    world_pos: vec3f,
    view_dir: vec3f,
    albedo: vec3f,
    metallic: f32,
    roughness: f32,
    f0: vec3f
) -> vec3f {
    let light_count = clamp(i32(uniforms.staticLightParams.x), 0, i32(MAX_STATIC_LIGHTS));
    let range = uniforms.staticLightParams.y;
    if (range <= 0.0 || light_count == 0) {
        return vec3f(0.0);
    }

    var total = vec3f(0.0);
    for (var i = 0; i < light_count; i = i + 1) {
        let light_data = uniforms.staticLights[u32(i)];
        let to_light = light_data.xyz - world_pos;
        let dist = length(to_light);
        if (dist <= 0.0001) {
            continue;
        }
        let dir = to_light / dist;
        let ndotl = max(dot(normal, dir), 0.0);
        if (ndotl <= 0.0) {
            continue;
        }
        var range_falloff = 1.0;
        if (range > 0.0) {
            if (dist > range) {
                continue;
            }
            range_falloff = clamp(1.0 - dist / range, 0.0, 1.0);
        }
        let inv = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        let attenuation = inv * range_falloff;
        let light_color = uniforms.staticLightColors[u32(i)].xyz;
        total += cook_torrance_brdf(normal, view_dir, dir, albedo, metallic, roughness, f0, light_color, attenuation);
    }
    return total;
}

fn compute_flashlight(
    normal: vec3f,
    world_pos: vec3f,
    frag_coord: vec4f,
    view_dir: vec3f,
    albedo: vec3f,
    metallic: f32,
    roughness: f32,
    f0: vec3f
) -> vec3f {
    if (uniforms.flashlightParams.x < 0.5) {
        return vec3f(0.0);
    }

    let width = uniforms.screenParams.x;
    let height = uniforms.screenParams.y;
    let min_dim = uniforms.screenParams.z;
    if (width <= 0.0 || height <= 0.0 || min_dim <= 0.0) {
        return vec3f(0.0);
    }

    let pixel = frag_coord.xy;
    let center = vec2f(width * 0.5, height * 0.5);
    let delta = (pixel - center) / min_dim;
    let radius = uniforms.flashlightParams.w;
    if (radius <= 0.0) {
        return vec3f(0.0);
    }
    let dist = length(delta);
    if (dist > radius) {
        return vec3f(0.0);
    }

    let falloff = clamp(dist / radius, 0.0, 1.0);
    let base_intensity = (1.0 - falloff * falloff) * uniforms.flashlightParams.y * 0.35;

    if (uniforms.flashlightDir.w <= 0.0) {
        return albedo * base_intensity;
    }

    let to_fragment = world_pos - uniforms.flashlightPos.xyz;
    let dist_along_axis = dot(to_fragment, uniforms.flashlightDir.xyz);
    if (dist_along_axis <= 0.0 || dist_along_axis > uniforms.flashlightPos.w) {
        return albedo * base_intensity;
    }

    let dir_norm = normalize(to_fragment);
    let spot = dot(dir_norm, uniforms.flashlightDir.xyz);
    let cutoff = uniforms.flashlightDir.w;
    if (spot <= cutoff) {
        return albedo * base_intensity;
    }

    let focus = pow((spot - cutoff) / max(1.0 - cutoff, 0.001), 2.0);
    let ndotl = max(dot(normal, uniforms.flashlightDir.xyz), 0.0);
    let distance_atten = clamp(1.0 - dist_along_axis / uniforms.flashlightPos.w, 0.0, 1.0);
    var beam = base_intensity + focus * ndotl * distance_atten * uniforms.flashlightParams.y * 0.5;
    beam = min(beam, uniforms.flashlightParams.y * 0.7);
    let light_color = vec3f(1.0, 0.95, 0.85);
    return cook_torrance_brdf(normal, view_dir, dir_norm, albedo, metallic, roughness, f0, light_color, beam);
}

fn compute_material_index(surface_type: f32) -> u32 {
    let rounded = floor(surface_type + 0.5);
    let clamped = clamp(rounded, 0.0, f32(MATERIAL_COUNT - 1u));
    return u32(clamped);
}

fn parallax_occlusion(uv: vec2f, view_dir_ts: vec3f, height_scale: f32, steps: i32) -> vec2f {
    var current_uv = uv;
    let num_steps = max(steps, 1);
    let layer_depth = 1.0 / f32(num_steps);
    let view_dir = normalize(view_dir_ts);
    let denom = max(view_dir.z, 0.0001);
    let delta = view_dir.xy / denom * height_scale;
    var current_depth = 0.0;
    var prev_uv = uv;
    var prev_depth_map = textureSample(heightTexture, sharedSampler, uv).r;
    for (var i = 0; i < num_steps; i = i + 1) {
        current_uv -= delta * layer_depth;
        current_depth += layer_depth;
        let depth_map = textureSample(heightTexture, sharedSampler, current_uv).r;
        if (depth_map < current_depth) {
            let prev_layer_depth = current_depth - layer_depth;
            let after = depth_map - current_depth;
            let before = prev_depth_map - prev_layer_depth;
            let weight = clamp(before / (before - after + 0.0001), 0.0, 1.0);
            return mix(current_uv, prev_uv, weight);
        }
        prev_uv = current_uv;
        prev_depth_map = depth_map;
    }
    return current_uv;
}

fn build_tbn(normal: vec3f) -> mat3x3f {
    var up = vec3f(0.0, 1.0, 0.0);
    if (abs(normal.y) > 0.9) {
        up = vec3f(0.0, 0.0, 1.0);
    }
    let tangent = normalize(cross(up, normal));
    let bitangent = cross(normal, tangent);
    return mat3x3f(tangent, bitangent, normal);
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
    var sampledAlbedo = textureSample(debugAlbedoTexture, sharedSampler, input.uv).rgb;
    var sampledNormal = textureSample(normalTexture, sharedSampler, input.uv).rgb * 2.0 - 1.0;
    var sampledMR = textureSample(metallicRoughnessTexture, sharedSampler, input.uv).rg;
    var sampledEmissive = textureSample(emissiveTexture, sharedSampler, input.uv).rgb;

    var baseColor: vec3f;
    if (input.surfaceType < 0.5) {
        // Floor: use sampled texture
        baseColor = floorColor.rgb;
    } else if (input.surfaceType < 1.5) {
        baseColor = sampledAlbedo;
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

    var n = normalize(input.normal);
    let material = uniforms.materials[compute_material_index(input.surfaceType)];
    var view_dir = uniforms.cameraPos.xyz - input.worldPos;
    let view_len = length(view_dir);
    if (view_len > 0.0001) {
        view_dir = view_dir / view_len;
    } else {
        view_dir = vec3f(0.0, 0.0, 1.0);
    }

    let use_pbr_debug = input.surfaceType > 0.5 && input.surfaceType < 1.5;
    var metallic = material.metalness;
    var roughness = max(material.roughness, 0.04);
    var emissive = vec3f(0.0);

    if (use_pbr_debug) {
        let tbn = build_tbn(n);
        let view_ts = transpose(tbn) * view_dir;
        let camera_distance = distance(uniforms.cameraPos.xyz, input.worldPos);
        var uv = input.uv;
        if (camera_distance < 6.0) {
            uv = parallax_occlusion(uv, view_ts, 0.05, 32);
        }
        sampledAlbedo = textureSample(debugAlbedoTexture, sharedSampler, uv).rgb;
        sampledNormal = textureSample(normalTexture, sharedSampler, uv).rgb * 2.0 - 1.0;
        sampledMR = textureSample(metallicRoughnessTexture, sharedSampler, uv).rg;
        sampledEmissive = textureSample(emissiveTexture, sharedSampler, uv).rgb;
        baseColor = sampledAlbedo;
        n = normalize(tbn * sampledNormal);
        metallic = clamp(sampledMR.r * material.metalness, 0.0, 1.0);
        roughness = clamp(sampledMR.g * material.roughness, 0.04, 1.0);
        emissive = sampledEmissive * material.emissiveIntensity;
    } else {
        metallic = material.metalness;
        roughness = max(material.roughness, 0.04);
        emissive = vec3f(0.0);
    }

    if (uniforms.screenParams.w > 0.5) {
        let mapped = n * 0.5 + vec3f(0.5);
        return vec4f(mapped, 1.0);
    }

    let dielectric_ior = max(material.ior, 1.0);
    let dielectric_f0 = pow((dielectric_ior - 1.0) / (dielectric_ior + 1.0), 2.0);
    let base_reflectance = vec3f(dielectric_f0);
    let f0 = mix(base_reflectance, baseColor, vec3f(metallic));
    let staticLight = compute_static_lighting(n, input.worldPos, view_dir, baseColor, metallic, roughness, f0);
    let flashlight = compute_flashlight(n, input.worldPos, frag_coord, view_dir, baseColor, metallic, roughness, f0);
    let ambient = uniforms.staticLightParams.z;
    let fogFactor = exp(-distance(input.worldPos, uniforms.cameraPos.xyz) * 0.08);
    var color = baseColor * ambient * (1.0 - metallic);
    color += staticLight;
    color += flashlight;
    color += emissive;
    color = color / (color + vec3f(1.0));
    color = pow(color, vec3f(1.0 / 2.2));
    color = mix(uniforms.fogColor.xyz, color, fogFactor);
    return vec4f(color, 1.0);
}
