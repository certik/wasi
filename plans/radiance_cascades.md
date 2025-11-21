# Radiance Cascades Implementation Plan

Goals: add diffuse global illumination using radiance cascades while keeping the renderer portable across Metal/Direct3D/WebGPU targets and the existing SDL GPU pipeline.

Context: current lighting in `game.c` and `shaders/*/mousecircle_scene_fragment.*` is direct-only (static point lights + flashlight). Cascades will provide multi-bounce indirect light and soft ambient from emissive/static lights.

## Phase 0: Research and API envelope
- Verify 3D texture + storage texture limits on all targets we support (Metal, D3D12/Vulkan via SDL GPU, WebGPU/WASM). Keep memory budget under ~64–96 MB for cascades.
- Decide radiance storage format (RGB10A2 or RGBA16F) and whether to store SH (L0/L1) or octahedral-encoded directional radiance. Favor RGBA16F + octahedral for simplicity.
- Lock cascade count and resolution (e.g., 3 cascades at 64³, 64³, 32³ with world extents 20 m, 60 m, 180 m around the camera).

## Phase 1: Scene bounds and inputs
- Extend `SceneHeader`/`Scene` to carry world AABB (compute in `scene_builder.c` during mesh emit) so voxelization can clamp to content without scanning at runtime.
- Define lighting channels produced during voxelization: albedo, normal, emissive, opacity (1 byte each where possible). Emissive comes from `SceneLight` data and optionally emissive materials.
- Add per-frame constants to `SceneUniforms` for cascade transforms: origin, voxel size, cascade index offsets, and toggle for debug visualization.

## Phase 2: GPU resources and pipelines
- Allocate cascade 3D textures (double-buffered for propagation) and an occupancy/albedo/normal atlas. Create once in `engine_create` with descriptors compatible across backends.
- Add SDL GPU compute pipelines (WGSL/HLSL/MSL) for:
  1) Voxelization raster/compute pass.
  2) Light injection from point/spot lights.
  3) Radiance propagation (Jacobi or push-pull across cascades).
  4) Mipmap/downsample step for cone tracing.
- Update shader bundling (`scripts/src/main.rs` and `shaders_manifest.txt` if needed) to include the new compute shaders.

## Phase 3: Voxelization and light injection
- Implement a compute-based voxelizer that consumes scene vertex/index buffers from `engine_upload_scene` (no CPU-side duplication). Use conservative voxelization or surface dilation to avoid leaks.
- Write albedo/normal into G-buffer volume; mark opacity/occupancy. Inject emissive from:
  - Static point lights (`SceneLight`) as spherical splats.
  - Flashlight (treat as spot emission in near cascade).
- Schedule voxelization incrementally (one cascade per frame) to keep frame time stable.

## Phase 4: Radiance propagation
- Implement cascade-local propagation (e.g., 4–6 iterations of 6-neighbor diffusion with normal-weighted bleeding) into a ping-pong radiance volume.
- Add cross-cascade blending: downsample fine levels into coarser cascades and optionally upsample coarse to fill gaps when the camera moves quickly.
- Maintain temporal stability with camera-relative cascade origins snapped to voxel size; apply a history weight to reduce flicker.

## Phase 5: Shading integration
- Extend `shaders/*/mousecircle_scene_fragment.*` to sample cascades via cone tracing:
  - Compute world position/normal from G-buffer.
  - Trace 3–4 cones with increasing aperture and step counts; blend cascades based on distance.
  - Combine with existing direct lighting; add energy conservation clamp.
- Update bind groups/resource tables in `engine_render` to bind cascade textures and samplers. Ensure WebGPU bind layouts stay under limit.
- Add a fallback path (toggle) to disable GI on platforms lacking 3D storage textures.

## Phase 6: Runtime control and debugging
- Expose debug overlays in `game.c`: cascade occupancy slice viewer, radiance heatmap, cone count sliders, and toggle for cascade visualization.
- Add GPU timing scopes for voxelization, propagation, and shading to the HUD perf panel.
- Provide a console/flag to freeze cascades and capture screenshots for comparisons.

## Phase 7: Testing and validation
- Create a test scene with high contrast (bright room, dark corridor) and a moving flashlight to validate indirect response.
- Add automated capture harness (offscreen render at fixed camera path) to compare with and without cascades; store SSIM or histogram metrics to detect regressions.
- Stress-test camera teleport, rapid rotation, and large windows to ensure cascade realignment does not flicker or leak.

## Phase 8: Performance/quality tuning
- Profile memory and bandwidth; adjust cascade resolutions, iteration count, and cone steps to hit 60 fps on baseline (WASM + Metal).
- Experiment with SH vs. octahedral storage; pick the fastest that maintains acceptable GI quality.
- Consider future extensions: cascaded shadowed cones (using depth dither), dynamic emissive objects, and up-front denoising if noise appears.
