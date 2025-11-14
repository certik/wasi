const MAX_STATIC_LIGHTS: u32 = 16u;

struct SceneUniforms {
    mvp: mat4x4f,
    cameraPos: vec4f,
    cameraDir: vec4f,
    fogColor: vec4f,
    staticLights: array<vec4f, MAX_STATIC_LIGHTS>,
    staticLightColors: array<vec4f, MAX_STATIC_LIGHTS>,
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

struct StaticLightContribution {
    diffuse: vec3f,
    specular: vec3f,
};

struct MaterialProperties {
    shininess: f32,
    specularStrength: f32,
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

fn get_debug_height(uv: vec2f) -> f32 {
    // Debug: extrude region where 0.4 <= u,v <= 0.6 (center of wall)
    // Make it gradually increase from edges to center for smoother look
    if (uv.x >= 0.4 && uv.x <= 0.6 && uv.y >= 0.4 && uv.y <= 0.6) {
        // Distance from center of the square
        let center = vec2f(0.5, 0.5);
        let dist = length(uv - center);
        let max_dist = 0.141;  // sqrt(0.1^2 + 0.1^2)
        // Smooth falloff from center (1.0) to edges (0.0)
        return clamp(1.0 - (dist / max_dist), 0.0, 1.0);
    }
    return 0.0;
}

fn parallax_occlusion(uv: vec2f, view_tangent: vec3f, height_scale: f32, steps: i32) -> vec2f {
    let layer_depth = 1.0 / f32(steps);
    var current_depth = 0.0;
    // Note: we march in the OPPOSITE direction of view (towards surface)
    let parallax_dir = -view_tangent.xy / max(abs(view_tangent.z), 0.01) * height_scale;
    var current_uv = uv;
    var prev_uv = uv;
    var prev_depth_map = get_debug_height(uv);

    for (var i = 0; i < steps; i = i + 1) {
        current_depth += layer_depth;
        current_uv += parallax_dir * layer_depth;
        let depth_map = get_debug_height(current_uv);

        // Found intersection
        if (current_depth >= depth_map) {
            // Linear interpolation for smoother result
            let after_depth = depth_map - current_depth;
            let before_depth = prev_depth_map - (current_depth - layer_depth);
            let weight = after_depth / (after_depth - before_depth);
            return mix(current_uv, prev_uv, weight);
        }

        prev_uv = current_uv;
        prev_depth_map = depth_map;
    }
    return current_uv;
}

fn get_material_properties(surface_type: f32) -> MaterialProperties {
    if (surface_type < 0.5) {
        return MaterialProperties(12.0, 0.15); // Floor: mostly matte
    } else if (surface_type < 1.5) {
        return MaterialProperties(18.0, 0.2); // Walls
    } else if (surface_type < 2.5) {
        return MaterialProperties(14.0, 0.18); // Ceiling tiles
    } else if (surface_type < 3.5) {
        return MaterialProperties(28.0, 0.35); // Checker panels / accents
    } else if (surface_type < 4.5) {
        return MaterialProperties(48.0, 0.8); // Sphere
    } else if (surface_type < 5.5) {
        return MaterialProperties(36.0, 0.4); // Book
    }
    return MaterialProperties(30.0, 0.5); // Chair and others
}

fn compute_static_lighting(normal: vec3f, world_pos: vec3f, view_dir: vec3f, material: MaterialProperties) -> StaticLightContribution {
    let light_count = clamp(i32(uniforms.staticLightParams.x), 0, i32(MAX_STATIC_LIGHTS));
    let range = uniforms.staticLightParams.y;
    if (range <= 0.0 || light_count == 0) {
        return StaticLightContribution(vec3f(0.0), vec3f(0.0));
    }

    var total_diffuse = vec3f(0.0);
    var total_specular = vec3f(0.0);
    let shininess = max(material.shininess, 1.0);
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
        let H = normalize(dir + view_dir);
        let spec = pow(max(dot(normal, H), 0.0), shininess);
        let light_color = uniforms.staticLightColors[u32(i)].xyz;
        total_diffuse += ndotl * attenuation * light_color;
        total_specular += material.specularStrength * spec * attenuation * light_color * 0.5;
    }
    return StaticLightContribution(total_diffuse, total_specular);
}

fn compute_flashlight(normal: vec3f, world_pos: vec3f, frag_coord: vec4f, view_dir: vec3f, material: MaterialProperties) -> FlashlightContribution {
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
    let half_dir = normalize(uniforms.flashlightDir.xyz + view_dir);
    let spec = pow(max(dot(normal, half_dir), 0.0), max(material.shininess, 1.0));
    var beam = base_intensity + focus * ndotl * distance_atten * uniforms.flashlightParams.y * 0.5;
    beam = min(beam, uniforms.flashlightParams.y * 0.7);
    let specular = spec * focus * distance_atten * 0.15 * uniforms.flashlightParams.y * material.specularStrength;
    return FlashlightContribution(beam, specular);
}

@fragment
fn main_(input: FragmentInput, @builtin(position) frag_coord: vec4f) -> @location(0) vec4f {
    // Calculate view direction early for POM
    var n = normalize(input.normal);
    var view_dir = uniforms.cameraPos.xyz - input.worldPos;
    let view_len = length(view_dir);
    if (view_len > 0.0001) {
        view_dir = view_dir / view_len;
    } else {
        view_dir = vec3f(0.0, 0.0, 1.0);
    }

    // WORKAROUND: Check if normal is facing camera, flip if not
    if (dot(n, view_dir) < 0.0) {
        n = -n;  // Flip the normal to face camera
    }

    // Use camera direction for parallax
    let cam_dir = uniforms.cameraDir.xyz;

    // Construct tangent space basis from normal
    // For walls, we know Y is always up, so bitangent = (0, 1, 0)
    let bitangent = vec3f(0.0, 1.0, 0.0);

    // Tangent is perpendicular to both normal and bitangent
    let tangent = normalize(cross(bitangent, n));

    // Transform camera direction to tangent space
    let cam_tangent = vec3f(
        dot(cam_dir, tangent),
        dot(cam_dir, bitangent),
        dot(cam_dir, n)
    );

    // Apply full parallax occlusion mapping for nice depth
    var uv = input.uv;
    if (input.surfaceType >= 0.5 && input.surfaceType < 1.5) {  // Walls only
        // Apply POM to all walls (both N/S and E/W)
        uv = parallax_occlusion(input.uv, cam_tangent, 0.15, 32);
    }

    // Sample textures unconditionally (required for uniform control flow)
    let floorColor = textureSample(floorTexture, sharedSampler, input.uv);
    let wallColor = textureSample(wallTexture, sharedSampler, uv);
    let ceilingColor = textureSample(ceilingTexture, sharedSampler, input.uv);
    let sphereColor = textureSample(sphereTexture, sharedSampler, input.uv);
    let bookColor = textureSample(bookTexture, sharedSampler, input.uv);
    let chairColor = textureSample(chairTexture, sharedSampler, input.uv);

    var baseColor: vec3f;
    if (input.surfaceType < 0.5) {
        // Floor: use sampled texture
        baseColor = floorColor.rgb;
    } else if (input.surfaceType < 1.5) {
        // Walls: use POM texture
        baseColor = wallColor.rgb;

        // DEBUG: Show grid in debug region if enabled (press G to toggle)
        if (uniforms.staticLightParams.w > 0.5) {
            if (input.uv.x >= 0.4 && input.uv.x <= 0.6 && input.uv.y >= 0.4 && input.uv.y <= 0.6) {
                // Show normal orientation for debugging
                baseColor = vec3f(
                    n.x * 0.5 + 0.5,
                    n.y * 0.5 + 0.5,
                    n.z * 0.5 + 0.5
                );

                // Draw grid on OFFSET uv
                let grid_size = 0.05;
                let grid_x = fract(uv.x / grid_size);
                let grid_y = fract(uv.y / grid_size);
                if (grid_x < 0.1 || grid_y < 0.1) {
                    baseColor = vec3f(1.0, 1.0, 0.0);  // Yellow grid
                }

                // Draw border of original debug region in red
                let border_thickness = 0.01;
                if (input.uv.x < 0.4 + border_thickness || input.uv.x > 0.6 - border_thickness ||
                    input.uv.y < 0.4 + border_thickness || input.uv.y > 0.6 - border_thickness) {
                    baseColor = vec3f(1.0, 0.0, 0.0);  // Red border
                }
            }
        }
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

    let material = get_material_properties(input.surfaceType);
    let staticLight = compute_static_lighting(n, input.worldPos, view_dir, material);
    let flashlight = compute_flashlight(n, input.worldPos, frag_coord, view_dir, material);
    if (uniforms.screenParams.w > 0.5) {
        let mapped = normalize(input.normal) * 0.5 + vec3f(0.5);
        return vec4f(mapped, 1.0);
    }
    let ambient = uniforms.staticLightParams.z;
    let fogFactor = exp(-distance(input.worldPos, uniforms.cameraPos.xyz) * 0.08);
    var color = baseColor * (ambient + flashlight.diffuse);
    color += baseColor * staticLight.diffuse;
    color += staticLight.specular;
    color += flashlight.specular * vec3f(1.0, 0.95, 0.85);
    color = mix(uniforms.fogColor.xyz, color, fogFactor);
    return vec4f(color, 1.0);
}
