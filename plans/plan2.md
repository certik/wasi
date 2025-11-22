# Radiance Cascades Implementation Plan (Updated for Correct Probe-Based Approach)
**Goals**: Add diffuse global illumination using Radiance Cascades while keeping the renderer portable across Metal/Direct3D/WebGPU targets and the existing SDL GPU pipeline. This provides multi-bounce indirect light and soft ambient from emissive/static lights without using voxels for the core GI storage or propagation. Instead, rely on a hierarchical probe grid system with ray marching via a signed distance field (SDF) for scene queries to enable deterministic, noise-free sampling.

**Context**: Current lighting in `game.c` and `shaders/*/mousecircle_scene_fragment.*` is direct-only (static point lights + flashlight). Radiance Cascades approximate global illumination by placing probes on multi-resolution 3D grids (cascades), casting short ray intervals from each probe to sample radiance, and propagating/merging results hierarchically. No voxels are used for radiance storage—radiance is stored directionally per probe in 3D textures. Scene traversal for ray casting uses a separate 3D SDF volume (a grid of signed distances to geometry) for efficient marching, which is generated from the scene meshes but is not part of the GI data itself. This keeps memory low and avoids light leaks common in voxel methods. For 3D extension, probes form a cubic lattice; ray directions are sampled hemispherically or spherically.

**Key Differences from Original Plan**: Removed all voxelization references. Replaced with probe grid setup, SDF generation for ray marching acceleration, interval-based ray casting from probes, and hierarchical merging (angular occlusion and spatial interpolation) for propagation. This aligns with the core technique's probe-based hierarchy, ensuring no noise and real-time performance on target platforms.

## Phase 0: Research and API Envelope
- Verify 3D texture + storage texture limits on all targets (Metal, D3D12/Vulkan via SDL GPU, WebGPU/WASM). Aim for a memory budget under ~32–64 MB for cascade textures (e.g., RGBA16F format) and SDF volume.
- Decide radiance storage format: Use RGBA16F for radiance intervals (RGB for color, A for visibility/occlusion). Store directional data using a fixed number of intervals per probe (e.g., base=4 or 16 directions, packed into texture slices or arrays). Avoid SH for now; favor simple interval encoding for deterministic sampling.
- Lock cascade count and resolution: e.g., 4–6 cascades with probe grids like 64x64x64 (fine, near camera, small spacing ~0.5m), doubling spacing per cascade (coarser far-field, up to 100m+ extents). Use world-space distances for intervals (e.g., length = pow(base, cascadeIndex) * spacing).
- Research WebGPU compatibility for 3D texture sampling and compute dispatches; ensure bind group limits are not exceeded (e.g., max 4–8 textures per shader).

## Phase 1: Scene Bounds and Inputs
- Extend `SceneHeader`/`Scene` to carry world AABB (compute in `scene_builder.c` during mesh emit) for cascade placement and SDF bounds clamping.
- Define inputs for probe sampling: Albedo, normal, emissive from materials and `SceneLight` data. No opacity channel needed beyond SDF (which implies occupancy).
- Add per-frame constants to `SceneUniforms`: Cascade origins (camera-relative, snapped to grid for stability), probe spacings, interval counts, base factor (e.g., 4), cascade index offsets, and debug toggles.
- Prepare for SDF generation: Add a flag in scene data to mark static geometry for SDF baking (dynamic objects can be approximated or ignored in initial impl).

## Phase 2: GPU Resources and Pipelines
- Allocate resources: 3D textures per cascade (double-buffered for ping-pong merging if needed) for radiance storage (e.g., array<vec4f> per direction, or packed into slices). Create a separate 3D SDF texture (R32F for distances) and optional normal/albedo atlas (if direct sampling from scene buffers isn't feasible).
- Create once in `engine_create` with descriptors compatible across backends. Use storage textures for read/write in compute.
- Add SDL GPU compute pipelines (WGSL/HLSL/MSL) for:
  1) SDF generation (compute pass to bake distances from meshes).
  2) Probe initialization and ray interval casting (sample emissives/lights via marching).
  3) Hierarchical merging/propagation (angular occlusion and spatial interpolation across cascades).
  4) Optional downsample/mipmap for smoother interpolation.
- Update shader bundling (`scripts/src/main.rs` and `shaders_manifest.txt` if needed) to include new compute shaders. Ensure WGSL compatibility for WebGPU (e.g., use @workgroup_size(4,4,4) for 3D dispatches).

## Phase 3: SDF Generation and Probe Initialization
- Implement a compute-based SDF baker that consumes scene vertex/index buffers from `engine_upload_scene` (no CPU-side duplication). Use a flood-fill or jump-flood algorithm to compute signed distances in the 3D volume: Dispatch workgroups per slice, iteratively propagate min distances to geometry (positive outside, negative inside for optional refraction/transparency).
- Resolution: Match finest cascade (e.g., 64x64x64 voxels for SDF, ~4–8 MB in R32F). Dilate surfaces slightly to reduce leaks.
- Initialize probes: For each cascade, compute probe positions in world space (grid centered on camera AABB). No "voxelization"—probes are just lattice points.
- Inject direct light/emissives: For each probe in the coarsest cascade first (bottom-up build), cast initial ray intervals to sample static point lights (`SceneLight`) as spherical contributions and emissive materials via SDF marching hits.

## Phase 4: Ray Interval Casting and Propagation
- For each cascade (starting from coarsest to finest or vice versa, depending on build order): Dispatch compute for each probe to cast fixed ray intervals.
  - Directions: Evenly spaced on hemisphere/sphere (e.g., base^ (cascadeIndex +1) rays, offset by 2π / count).
  - Marching: From probe position, step along direction using SDF values (jump by sampled distance, check for hits within interval length). If hit, sample albedo/normal/emissive at hit point (via atlas or reprojection); accumulate radiance (linear space) and set visibility=0 if occluded.
  - If no hit in interval, leave open for merging.
- Propagation/merging (key to multi-bounce):
  - Angular merging: For each probe, combine sub-intervals recursively (e.g., branch into 4 sub-directions per level); occlude far radiance by near visibility (radiance_far *= visibility_near).
  - Spatial merging: For a probe in cascade i, interpolate radiance from 8 nearest probes in coarser cascade i+1 using trilinear weights (based on fractional grid coords). This upsamples coarse data to fill fine gaps, approximating bounces.
  - Apply 4–6 iterations if needed for diffusion, but core method uses single-pass hierarchy for efficiency.
  - Temporal stability: Use camera-snapped origins; blend with history (90% weight) in radiance textures to reduce flicker on movement.
- Schedule incrementally (e.g., update one cascade per frame or async dispatch) for stable frame times.

## Phase 5: Shading Integration
- Extend `shaders/*/mousecircle_scene_fragment.*` to query cascades for irradiance:
  - Compute world position/normal from G-buffer.
  - Find enclosing cascade based on distance; interpolate from 8 nearest probes using trilinear (spatial) and angular lookup (direction towards normal).
  - Blend merged radiance across cascades (weighted by distance/penumbra); combine with direct lighting and clamp for energy conservation.
- Update bind groups/resource tables in `engine_render` to bind cascade 3D textures, SDF, and samplers. Ensure WebGPU bind layouts stay under limit (group cascades into arrays if needed).
- Add fallback path (toggle) to disable GI on low-end platforms or if 3D textures exceed limits (e.g., drop to 2D screen-space cascades).

## Phase 6: Runtime Control and Debugging
- Expose debug overlays in `game.c`: Probe grid visualization (as points/spheres), radiance heatmap per cascade, interval count sliders, and toggle for cascade boundaries.
- Add GPU timing scopes for SDF bake, ray casting, merging, and shading to the HUD perf panel.
- Provide a console/flag to freeze cascades, visualize ray marches, and capture screenshots for quality checks.

## Phase 7: Testing and Validation
- Create a test scene with high contrast (bright room, dark corridor) and a moving flashlight to validate indirect bounces and response.
- Add automated capture harness (offscreen render at fixed camera path) to compare with and without cascades; store SSIM or histogram metrics to detect regressions.
- Stress-test camera teleport, rapid rotation, and large scenes to ensure merging handles realignment without flicker or artifacts (e.g., popping intervals).

## Phase 8: Performance/Quality Tuning
- Profile memory and bandwidth; adjust cascade counts, probe resolutions, interval bases (4–16), and march steps (8–16) to hit 60–120 fps on baseline (WASM + Metal).
- Experiment with interval vs. SH storage; pick the fastest with good GI quality (intervals for accuracy, SH for compactness if memory tight).
- Consider future extensions: Hemispherical sampling for grounded scenes, dynamic emissive support (re-cast probes near changes), and SSR integration for reflections (trace screen-space rays, fallback to cascade queries for off-screen).

This updated plan ensures a faithful implementation of Radiance Cascades, leveraging probes and hierarchical merging for efficient GI without voxel grids for radiance. If your engine lacks easy mesh traversal, the SDF provides a portable acceleration structure. Start with a 2D prototype in shaders for validation before full 3D.
