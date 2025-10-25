# GM Refactoring Plan: Moving Logic from HTML to C

**Goal**: Make `gm.html` absolutely minimal by moving as much logic as possible to `gm.c`.

## Current State Analysis

The `gm.html` file is currently ~1150 lines and contains:
- WebGPU enum mapping tables and conversion functions
- Complex WebGPU descriptor parsing from WASM memory
- The massive `createWasmWebGPUHost` function (~550 lines)
- Game initialization and event handling logic
- Helper functions that could be in C

## Refactoring Priorities

### High Priority - Maximum Impact

1. **Move all enum mappings to C**
   - `textureFormatStringToEnum` / `textureFormatEnumToString`
   - All WebGPU enum mapping tables (15+ maps)
   - Export C functions like:
     ```c
     const char* gm_texture_format_to_string(uint32_t enum_value);
     uint32_t gm_texture_format_from_string(const char* name);
     const char* gm_vertex_format_to_string(uint32_t enum_value);
     // etc. for all enum types
     ```

2. **Create C functions that build complete pipeline descriptors**
   - Instead of JS parsing WASM memory structures
   - Return descriptors in JavaScript-friendly format
   - Possibly as JSON strings or structured memory

3. **Move `GMBufferSlot` and `GMResource` enums to C**
   - Currently defined in JavaScript (lines 50-62)
   - Export as constants from WASM

### Medium Priority - Good Cleanup

4. **Move descriptor reading logic to C**
   - `readBindGroupLayoutEntries()`
   - `readBindGroupEntries()`
   - `readProgrammableStage()`
   - `readVertexBuffers()`
   - `readVertexState()`
   - `readColorTargets()`
   - `readFragmentState()`
   - `readPrimitiveState()`
   - `readDepthStencilState()`
   - Have C functions prepare data in JS-friendly format instead

5. **Create a single C initialization function**
   - Consolidate the 200+ lines of `init()` boilerplate
   - Handle all setup that doesn't strictly require browser APIs

### Low Priority - Nice to Have

6. **Move key mapping to C**
   - Arrow key special characters (lines 1093-1097)
   - Consolidate with existing key handling

7. **Consolidate texture loading URLs and logic**
   - Move URL construction to C exports
   - Keep actual fetch/loading in JS (requires browser APIs)

## What Should Stay in HTML

These require browser/DOM APIs and cannot be moved to C:

- The `createWasmWebGPUHost` import wrapper (simplified version)
- Actual WebGPU API calls (`device.createBuffer`, `queue.writeBuffer`, etc.)
- Texture loading from URLs (`fetch`, `createImageBitmap`)
- The `platformAPI` object for WASM→JS callbacks
- Canvas/DOM manipulation (`canvas.getContext`, `canvas.requestPointerLock`)
- Animation frame loop trigger (`requestAnimationFrame`)
- Event listeners (`addEventListener` for keyboard, mouse, resize)

## Implementation Strategy

1. **Start with enum mappings** - Clear win, easy to test
2. **Refactor descriptor functions** - Will significantly reduce JS complexity
3. **Consolidate initialization** - Final cleanup to minimize `init()` function
4. **Test thoroughly** - Ensure `pixi r test_gm_linux`, `pixi r test_gm_wasm`, and `pixi r serve` all work

## Testing Checklist

After each change:
- [ ] `pixi r test_gm_linux` - Native Linux build works
- [ ] `pixi r test_gm_wasm` - WASM build compiles
- [ ] `pixi r serve` - Browser version renders correctly

## Expected Outcome

Reduce `gm.html` from ~1150 lines to ~300-400 lines containing only:
- Minimal WebGPU host wrapper
- Platform API callbacks
- DOM/Canvas setup
- Event handlers
- Render loop trigger

All game logic, data structures, and WebGPU configuration should live in `gm.c`.
