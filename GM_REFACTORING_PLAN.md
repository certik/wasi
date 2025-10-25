# GM Refactoring Plan: Moving Logic from HTML to C

**Goal**: Make `gm.html` absolutely minimal by moving as much logic as possible to `gm.c`.

## Current State (After Phase 1)

**File sizes:**
- `gm.html`: 1,166 lines total
  - `createWasmWebGPUHost`: 580 lines (WebGPU/WASM bridge - infrastructure)
  - `init` function: 554 lines (game initialization)
  - Remaining: 32 lines (HTML structure, script tags)

**What has been moved to C:**
- ✅ All WebGPU enum mappings (15 types, ~380 lines)
- ✅ 16 enum conversion functions
- ✅ GMResourceType enum definition
- ✅ Enum value export helpers

## Phase 1: Completed ✅

### High Priority - Maximum Impact

1. **✅ Move all enum mappings to C**
   - Added 15 enum mapping tables to gm.c
   - Created 16 `*_to_string()` conversion functions
   - Export functions: `gm_texture_format_to_string()`, `gm_vertex_format_to_string()`, etc.
   - Updated gm.html to use WASM exports where possible
   - Kept inline maps in `createWasmWebGPUHost` (runs before WASM loads)

2. **✅ Export GMBufferSlot and GMResource enums from C**
   - Defined `GMResourceType` enum in gm.c
   - Created helper functions for enum value exports
   - Enums kept in both C and JS (JS needs them before WASM loads)

## Phase 2: In Progress

### Analysis of Remaining gm.html Code

**createWasmWebGPUHost (580 lines):**
- WebGPU API wrapper for WASM
- Descriptor parsing functions (readBindGroupLayoutEntries, etc.)
- Resource handle management
- This is infrastructure - any WASM WebGPU app needs similar code
- **Recommendation**: Keep as-is, this is necessary bridge code

**init function (554 lines):**
- Game-specific initialization
- Texture loading
- Buffer creation
- Event handlers (keyboard, mouse, resize)
- Render loop setup
- **Opportunities for simplification:**
  - Texture loading code could be consolidated
  - Event handler setup could be streamlined
  - Some initialization could be moved to C

### Medium Priority - Good Cleanup

3. **Consolidate texture loading**
   - Current: Separate fetch/load for each texture
   - Potential: Create helper function or move URL handling entirely to C
   - Impact: ~30-50 line reduction

4. **Simplify event handler setup**
   - Current: Inline keydown/keyup/mouse handlers
   - Potential: Extract to named functions
   - Impact: Better organization, ~10-20 line reduction

5. **Move key mapping to C**
   - Current: JavaScript keyMap object for arrow keys
   - Potential: Handle in C's gm_handle_key_press
   - Impact: ~5-10 line reduction

### Low Priority - Nice to Have

6. **Extract helper functions**
   - Create reusable helpers for common patterns
   - Group related initialization code
   - Impact: Better readability

## What Should Stay in HTML

These require browser/DOM APIs and cannot be moved to C:

- ✅ The `createWasmWebGPUHost` wrapper (necessary WebGPU/WASM bridge)
- ✅ Actual WebGPU API calls (`device.createBuffer`, `queue.writeBuffer`)
- ✅ Texture loading from URLs (`fetch`, `createImageBitmap`)
- ✅ The `platformAPI` object for WASM→JS callbacks
- ✅ Canvas/DOM manipulation (`canvas.getContext`, `canvas.requestPointerLock`)
- ✅ Animation frame loop trigger (`requestAnimationFrame`)
- ✅ Event listeners (`addEventListener` for keyboard, mouse, resize)

## Testing Checklist

After each change:
- [x] `pixi r test_gm_linux` - Native Linux build works
- [x] `pixi r test_gm_wasm` - WASM build compiles
- [x] `pixi r serve` - Browser version renders correctly

## Implementation Strategy

### Completed
- [x] Start with enum mappings - Clear win, easy to test ✅
- [x] Export enum values from C ✅
- [x] Test thoroughly ✅

### Next Steps
- [ ] Consolidate texture loading (if worthwhile)
- [ ] Simplify event handlers (refactor for readability)
- [ ] Document why remaining code must stay in HTML

## Expected Outcome

**Realistic goal after Phase 2:**
- Reduce `gm.html` from ~1,166 to ~1,000-1,100 lines
- All game logic and data structures in `gm.c`
- HTML contains only necessary browser API interactions
- Clear separation between infrastructure (WebGPU bridge) and game code

**Current achievement:**
- Moved ~430 lines of enum logic to C
- Created foundation for consistent enum handling
- Improved maintainability and type safety

## Key Insight

The bulk of gm.html (580 lines in `createWasmWebGPUHost`) is **necessary infrastructure** for any WASM WebGPU application. This code provides the bridge between WASM and WebGPU APIs and cannot be eliminated.

The real opportunity is in the game-specific initialization code (~554 lines), where we can:
1. Consolidate repetitive patterns
2. Extract common helpers
3. Move game logic to C where appropriate
4. Improve code organization and readability

Rather than focusing on line count reduction, the goal should be **code quality and maintainability**.
