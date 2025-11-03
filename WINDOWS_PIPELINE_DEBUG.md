# Windows D3D12 Graphics Pipeline Creation Error - Debug Report

**Date**: November 2, 2025  
**Issue**: MouseCircle application fails to create graphics pipeline on Windows with D3D12 backend  
**Error**: `ERROR: Could not create graphics pipeline state! Error Code: The parameter is incorrect. (0x80070057)`

## Problem Description

When running `pixi r test_mousecircle_windows`, the application builds successfully but fails at runtime when attempting to create the scene graphics pipeline. The error occurs in `SDL_CreateGPUGraphicsPipeline()` with error code 0x80070057 (E_INVALIDARG), indicating an invalid parameter.

```
Using direct3d12 backend, loading shaders from shaders/DXIL/
ERROR: Could not create graphics pipeline state! Error Code: The parameter is incorrect. (0x80070057)
Failed to create scene pipeline: Could not create graphics pipeline state! Error Code: The parameter is incorrect. (0x80070057)
```

## Initial Analysis

The overlay pipeline (2D HUD) creates successfully, while the scene pipeline (3D world) fails. Key differences:
- **Scene pipeline**: 4 vertex attributes, depth testing enabled, backface culling, uses uniform buffers
- **Overlay pipeline**: 2 vertex attributes, no depth testing, no culling, no uniform buffers

## Attempted Fixes

### 1. Shader Entry Point Names ❌

**Problem Found**: Code specified entry points as `main_vertex`, `main_fragment`, `overlay_vertex`, `overlay_fragment`, but HLSL shaders use `main`.

**Fix Applied**:
```c
// Changed from:
.entrypoint = "main_vertex"
// To:
.entrypoint = "main"
```

**Files Modified**: `MouseCircle_standalone.c` lines ~1491, 1509, 1522, 1536

**Result**: No change - error persists.

---

### 2. Fragment Shader Uniform Buffer Declaration ❌

**Problem Found**: Fragment shader accesses `ConstantBuffer<SceneUniforms>` but `num_uniform_buffers` was not explicitly set for the fragment shader.

**Fix Applied**:
```c
// Scene fragment shader creation:
shader_info.num_uniform_buffers = 1;  // Added this line
```

**Files Modified**: `MouseCircle_standalone.c` line ~1511

**Result**: No change - error persists.

---

### 3. HLSL Semantic Naming Convention ❌

**Problem Found**: Hand-written scene shaders used old-style D3D11 semantics (POSITION, TEXCOORD, NORMAL) while auto-generated overlay shaders used LOC0-3 semantics.

**Original Scene Vertex Shader**:
```hlsl
struct VertexInput {
    float3 position : POSITION;
    float surfaceType : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 normal : NORMAL;
};
```

**Fix Applied**:
```hlsl
struct VertexInput {
    float3 position : LOC0;
    float surfaceType : LOC1;
    float2 uv : LOC2;
    float3 normal : LOC3;
};

struct VertexOutput {
    float surfaceType : LOC0;
    float2 uv : LOC1;
    float3 normal : LOC2;
    float3 worldPos : LOC3;
    float4 position : SV_Position;
};
```

**Files Modified**: 
- `shaders/HLSL/mousecircle_scene_vertex.hlsl`
- `shaders/HLSL/mousecircle_scene_fragment.hlsl`

**Recompiled**: Used DXC to generate new DXIL binaries:
```bash
dxc.exe -T vs_6_0 -E main -Fo shaders/DXIL/mousecircle_scene_vertex.dxil shaders/HLSL/mousecircle_scene_vertex.hlsl
dxc.exe -T ps_6_0 -E main -Fo shaders/DXIL/mousecircle_scene_fragment.dxil shaders/HLSL/mousecircle_scene_fragment.hlsl
```

**Result**: No change - error persists.

---

### 4. Depth Clip Configuration ❌

**Hypothesis**: Maybe D3D12 has issues with depth clipping enabled.

**Fix Attempted**:
```c
rasterizer_state.enable_depth_clip = false;  // Changed from true
```

**Files Modified**: `MouseCircle_standalone.c` line ~1370

**Result**: No change - error persists. Reverted.

---

### 5. Depth Testing Disabled ❌

**Hypothesis**: Maybe depth/stencil state configuration is the issue.

**Fix Attempted**:
```c
depth_state.enable_depth_test = false;  // Changed from true
depth_state.enable_depth_write = false; // Changed from true
```

**Files Modified**: `MouseCircle_standalone.c` lines ~1359-1360

**Result**: No change - error persists. Reverted.

---

### 6. Fragment Shader Uniform Buffer Count Experiment ❌

**Hypothesis**: Maybe only vertex shader should declare uniform buffers.

**Fix Attempted**:
```c
// Fragment shader creation:
shader_info.num_uniform_buffers = 0;  // Changed from 1
```

**Files Modified**: `MouseCircle_standalone.c` line ~1511

**Result**: No change - error persists. Reverted back to 1.

---

### 7. Shader Regeneration from WGSL ⚠️

**Discovery**: Hand-written HLSL shaders might have subtle issues. Attempted to regenerate from WGSL sources.

**Process**:
1. Built Rust shader compiler: `cd scripts && cargo build --release`
2. Ran compiler: `./scripts/target/release/shader_compiler.exe shaders/WGSL/mousecircle_scene_vertex.wgsl`
3. Inspected generated files

**Problem Found**: Naga v26.0 HLSL backend has a bug - generates incomplete HLSL files missing:
- The `main()` function implementation
- The `ConstantBuffer<>` declaration

**Evidence**: 
- MSL output is complete and correct
- SPIRV output generates successfully  
- Only HLSL output is truncated

**Generated HLSL (incomplete)**:
```hlsl
struct VertexOutput_main {
    float surfaceType : LOC0;
    float2 uv : LOC1;
    float3 normal : LOC2;
    float3 worldPos : LOC3;
    float4 position : SV_Position;
};
// File ends here - missing main() function and ConstantBuffer
```

**Working MSL Reference**:
```metal
vertex main_Output main_vertex(
  main_Input varyings [[stage_in]]
, constant SceneUniforms& uniforms [[buffer(0)]]
) {
    // ... implementation exists
}
```

**Resolution**: Manually wrote complete HLSL based on MSL output.

---

### 8. Manual HLSL Shader Creation ✅ (Shaders compile, but pipeline still fails)

**Approach**: Manually created correct HLSL shaders based on the working MSL output.

**Final Scene Vertex Shader** (`shaders/HLSL/mousecircle_scene_vertex.hlsl`):
```hlsl
// Manually fixed scene vertex shader for mousecircle (D3D12/DXIL)

struct SceneUniforms {
    row_major float4x4 mvp;
    float4 cameraPos;
    float4 fogColor;
};

struct VertexInput {
    float3 position : LOC0;
    float surfaceType : LOC1;
    float2 uv : LOC2;
    float3 normal : LOC3;
};

struct VertexOutput {
    float surfaceType : LOC0;
    float2 uv : LOC1;
    float3 normal : LOC2;
    float3 worldPos : LOC3;
    float4 position : SV_Position;
};

ConstantBuffer<SceneUniforms> uniforms : register(b0);

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    float4 world = float4(input.position, 1.0);
    output.position = mul(uniforms.mvp, world);
    output.surfaceType = input.surfaceType;
    output.uv = input.uv;
    output.normal = input.normal;
    output.worldPos = input.position;
    return output;
}
```

**Final Scene Fragment Shader** (`shaders/HLSL/mousecircle_scene_fragment.hlsl`):
```hlsl
// Manually fixed scene fragment shader for mousecircle (D3D12/DXIL)

struct SceneUniforms {
    row_major float4x4 mvp;
    float4 cameraPos;
    float4 fogColor;
};

struct VertexOutput {
    float surfaceType : LOC0;
    float2 uv : LOC1;
    float3 normal : LOC2;
    float3 worldPos : LOC3;
    float4 position : SV_Position;
};

ConstantBuffer<SceneUniforms> uniforms : register(b0);

float checker(float2 uv)
{
    float2 scaled = floor(uv * 4.0);
    float v = fmod(scaled.x + scaled.y, 2.0);
    return (v < 0.5) ? 1.0 : 0.7;
}

float4 main(VertexOutput input) : SV_Target0
{
    float3 baseColor;
    if (input.surfaceType < 0.5) {
        baseColor = float3(0.1, 0.1, 0.9);
    } else if (input.surfaceType < 1.5) {
        baseColor = float3(0.9, 0.2, 0.2);
    } else if (input.surfaceType < 2.5) {
        baseColor = float3(0.9, 0.9, 0.2);
    } else {
        baseColor = float3(0.7, 0.5, 0.3);
    }
    
    float3 n = normalize(input.normal);
    float3 lightDir = normalize(float3(0.35, 1.0, 0.45));
    float diff = max(dot(n, lightDir), 0.15);
    float fogFactor = exp(-distance(input.worldPos, uniforms.cameraPos.xyz) * 0.08);
    float3 color = baseColor * checker(input.uv) * diff;
    color = lerp(uniforms.fogColor.xyz, color, fogFactor);
    return float4(color, 1.0);
}
```

**Compilation**: Both shaders compile successfully with DXC without errors.

**Result**: Shaders are now correct and match working pattern, but **pipeline creation still fails** with same error.

---

## Current State

### What Works ✅
- Application builds successfully
- Overlay pipeline creates and works
- Shaders compile without errors
- Shader semantics match working overlay pattern
- Uniform buffer declarations are correct

### What Doesn't Work ❌
- Scene pipeline creation fails with E_INVALIDARG (0x80070057)
- Error is consistent across all attempted fixes

### Pipeline Configuration Comparison

**Scene Pipeline** (fails):
```c
- 4 vertex attributes: FLOAT3, FLOAT, FLOAT2, FLOAT3 at offsets 0, 12, 16, 24
- Vertex buffer pitch: sizeof(MapVertex) = 36 bytes
- Depth testing: enabled
- Depth write: enabled
- Backface culling: enabled
- Depth format: D16_UNORM
- Uniform buffers: vertex=1, fragment=1
```

**Overlay Pipeline** (works):
```c
- 2 vertex attributes: FLOAT2, FLOAT4 at offsets 0, 8
- Vertex buffer pitch: sizeof(OverlayVertex) = 24 bytes
- Depth testing: disabled
- Depth write: disabled  
- No culling
- Same depth format: D16_UNORM (even though not used)
- Uniform buffers: vertex=0, fragment=0
```

## Possible Root Causes

### 1. SDL3 D3D12 Backend Bug
The error occurs in SDL3's D3D12 backend during pipeline creation. Given that:
- Shaders are correct and compile successfully
- Configuration mirrors working overlay pipeline pattern
- Error code is generic "invalid parameter"

This suggests SDL3 might have a bug in how it validates or creates D3D12 pipelines with specific combinations of features (depth testing + uniform buffers + 4 attributes).

### 2. Shader Reflection Mismatch
D3D12 uses shader reflection to validate pipeline state. The compiled DXIL might have metadata that SDL3 interprets incorrectly, causing validation to fail.

### 3. Uniform Buffer Binding Configuration
Despite `num_uniform_buffers` being set, there might be missing binding configuration that D3D12 requires but SDL3 doesn't expose in its API.

### 4. Vertex Attribute Layout Issue
The 4-attribute layout with specific formats might trigger a validation path in SDL3's D3D12 backend that has a bug.

## Recommended Next Steps

1. **Test on macOS/Linux**: Run the same code on Metal (macOS) or Vulkan (Linux) backends to isolate if this is Windows/D3D12 specific.

2. **Simplify Scene Pipeline**: Create a minimal test pipeline with:
   - 1 vertex attribute
   - No uniform buffers
   - No depth testing
   Then incrementally add features to identify which triggers the error.

3. **Check SDL3 Version**: Verify SDL3 version and check for known issues or updates related to D3D12 pipeline creation.

4. **Enable D3D12 Debug Layer**: Use D3D12 debug layer to get more detailed error messages:
   ```c
   // Enable before device creation
   SDL_SetHint(SDL_HINT_RENDER_DIRECT3D11_DEBUG, "1");
   ```

5. **Compare with SDL3 Examples**: Find SDL3 GPU examples that use D3D12 with uniform buffers and compare pipeline configuration.

6. **Binary Search Pipeline Features**: Disable features one at a time (depth test, culling, etc.) to find the minimal reproduction case.

## Files Modified

### C Code
- `MouseCircle_standalone.c`:
  - Line ~1491: Changed vertex shader entry point to "main"
  - Line ~1509: Changed fragment shader entry point to "main", added num_uniform_buffers = 1
  - Line ~1522: Changed overlay vertex shader entry point to "main"
  - Line ~1536: Changed overlay fragment shader entry point to "main"

### Shaders
- `shaders/HLSL/mousecircle_scene_vertex.hlsl`: Completely rewritten with LOC semantics
- `shaders/HLSL/mousecircle_scene_fragment.hlsl`: Completely rewritten with LOC semantics
- `shaders/DXIL/mousecircle_scene_vertex.dxil`: Recompiled from updated HLSL
- `shaders/DXIL/mousecircle_scene_fragment.dxil`: Recompiled from updated HLSL

## Build Commands Used

```bash
# Rebuild and test
pixi run -e windows test_mousecircle_windows

# Shader compilation
"C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe" -T vs_6_0 -E main -Fo shaders/DXIL/mousecircle_scene_vertex.dxil shaders/HLSL/mousecircle_scene_vertex.hlsl

"C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe" -T ps_6_0 -E main -Fo shaders/DXIL/mousecircle_scene_fragment.dxil shaders/HLSL/mousecircle_scene_fragment.hlsl

# Shader regeneration from WGSL
cd scripts && cargo build --release
./scripts/target/release/shader_compiler.exe shaders/WGSL/mousecircle_scene_vertex.wgsl
./scripts/target/release/shader_compiler.exe shaders/WGSL/mousecircle_scene_fragment.wgsl
```

## Known Issues Discovered

### Naga v26.0 HLSL Backend Bug
The Rust shader compiler using Naga v26.0 generates incomplete HLSL output:
- Missing function implementations
- Missing ConstantBuffer declarations
- MSL and SPIRV outputs are correct

**Workaround**: Manually write HLSL based on MSL output and compile with DXC.

## Conclusion

Despite fixing multiple shader-related issues (entry points, semantics, uniform buffers) and ensuring the HLSL matches the working overlay pattern, the D3D12 pipeline creation continues to fail with a generic "invalid parameter" error. The issue likely lies in:

1. An SDL3 D3D12 backend bug with specific feature combinations
2. A configuration requirement not exposed by SDL3's API
3. A subtle validation issue in how D3D12 interprets the shader metadata

The problem requires either:
- Testing on alternative backends (Metal/Vulkan) to isolate the issue
- Deeper investigation with D3D12 debug tools
- Consultation with SDL3 maintainers or D3D12 documentation
- Creating a minimal reproduction case to identify the exact trigger

All code changes made are valid improvements (correct entry points, proper semantics, complete shaders), but they do not resolve the underlying pipeline creation failure on Windows D3D12.
