# Overlay Flickering Bug

## Problem Description

The overlay rendering system (HUD text and minimap) exhibits flickering artifacts. The flickering appears to be caused by stale vertex data being rendered.

## Current Workaround

Adding a `memset()` call at the beginning of `build_overlay()` fixes the issue:

```c
// In build_overlay() after preparing text lines
base_memset(app->overlay_cpu_vertices, 0, sizeof(OverlayVertex) * 20000);
```

This clears ~480KB of the 1.1MB vertex buffer before building the overlay. However, this is expensive and **hides the actual bug** rather than fixing it.

**Working commit:** a716391 (has memset workaround)

## Architecture Overview

### Overlay Rendering Pipeline

1. **CPU Buffer:** `overlay_cpu_vertices[MAX_OVERLAY_VERTICES]` (48000 vertices max)
   - Static array in `GameApp` struct
   - Built every frame by `build_overlay()`

2. **Vertex Count:** `overlay_vertex_count`
   - Tracks actual number of vertices used
   - Set at end of `build_overlay()`

3. **Update Flow** (in `update_game()`):
   ```c
   if (app->overlay_dirty && app->overlay_vertex_count > 0) {
       // Copy ONLY overlay_vertex_count vertices to transfer buffer
       base_memmove(mapped, app->overlay_cpu_vertices,
                    sizeof(OverlayVertex) * app->overlay_vertex_count);
   }
   ```

4. **Upload Flow** (in `render_game()`):
   ```c
   if (app->overlay_dirty) {
       // Upload ONLY overlay_vertex_count vertices to GPU
       dst.size = app->overlay_vertex_count > 0
           ? sizeof(OverlayVertex) * app->overlay_vertex_count
           : sizeof(OverlayVertex);
       SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
   }
   ```

5. **Draw Flow** (in `render_game()`):
   ```c
   if (app->overlay_vertex_count > 0) {
       // Draw ONLY overlay_vertex_count vertices
       SDL_DrawGPUPrimitives(render_pass, app->overlay_vertex_count, 1, 0, 0);
   }
   ```

### Vertex Building Functions

- `append_quad()` - Adds 6 vertices for a quad (2 triangles)
- `append_convex_quad()` - Adds 6 vertices for a convex quad
- `append_triangle()` - Adds 3 vertices for a triangle
- `append_glyph()` - Calls `append_quad()` for each pixel in a glyph

All functions return `offset` unchanged if buffer is full (`offset + N > max`).

## The Mystery

**Key Question:** If we only copy, upload, and draw `overlay_vertex_count` vertices, why does stale data beyond that count matter?

The system should only ever access vertices `[0, overlay_vertex_count)`. Vertices beyond that index should be irrelevant. Yet the memset "fixes" the issue by clearing them.

### Potential Explanations

1. **Race Condition**: `overlay_vertex_count` might be inconsistent between update/render
   - However, both run sequentially in the same thread

2. **Off-by-One Error**: Perhaps `overlay_vertex_count` is wrong by 1 in some code path
   - Would need to audit all uses of the count

3. **GPU Buffer State**: The GPU vertex buffer might not be properly sized/cleared
   - Buffer is created with `sizeof(OverlayVertex) * MAX_OVERLAY_VERTICES`
   - Upload size uses `overlay_vertex_count` but buffer is larger

4. **Transfer Buffer Reuse**: The transfer buffer might contain old data
   - We call `MapGPUTransferBuffer()` then `memmove()` only `overlay_vertex_count` vertices
   - The transfer buffer is `MAX_OVERLAY_VERTICES` in size
   - **HYPOTHESIS**: Old data beyond `overlay_vertex_count` remains in transfer buffer and gets uploaded

5. **Upload Size Mismatch**: The upload might be uploading more than requested
   - SDL bug?
   - Or we're misunderstanding the API?

## Attempts That Didn't Work

### Attempt 1: Error Handling in `append_glyph()` (Commit b57ced3)

**Theory:** Partial glyph writes when buffer is full might leave inconsistent data.

**Implementation:**
```c
uint32_t new_offset = append_quad(verts, offset, max, x0, y0, x1, y1, color);
// If append_quad failed (buffer full), stop trying to add more pixels
if (new_offset == offset) {
    return offset;
}
offset = new_offset;
```

**Result:** Flicker still occurred. This suggests the issue isn't about partial glyph writes.

### Attempt 2: Remove memset to expose root cause

**Result:** Flicker confirmed without memset, proving memset is masking the issue.

## Historical Context

### Flicker Fix Timeline

- **bbe3165**: Added memset at start of `build_overlay()` clearing entire buffer
  - Used `sizeof(app->overlay_cpu_vertices)`
- **62ba003 ("X")**: Changed render upload to always run if dirty (even if count=0)
- **a716391 ("Better")**: Moved memset to after text line preparation, reduced size to 20000 vertices
  - More efficient, only clears what we typically use
- **b57ced3**: Attempted to fix by handling append errors + removed memset (didn't work)

## Data Points

- `MAX_OVERLAY_VERTICES = 48000`
- `OverlayVertex` size = 24 bytes (2 floats position + 4 floats color)
- Typical usage appears to be < 20000 vertices based on memset optimization
- Full buffer = 1.1 MB, memset clears 480 KB

## Next Steps for Investigation

1. **Add logging** to track `overlay_vertex_count` changes frame-by-frame
   - Log in `build_overlay()`, `update_game()`, `render_game()`
   - Check if count is ever inconsistent

2. **Verify transfer buffer behavior**
   - Log what's in transfer buffer beyond `overlay_vertex_count`
   - Check if MapGPUTransferBuffer returns buffer with old data

3. **Check GPU buffer state**
   - Verify actual upload size vs. requested size
   - Check if GPU buffer retains old data across frames

4. **Test with HUD toggle**
   - Disable HUD (sets `overlay_vertex_count = 0`)
   - Re-enable HUD
   - Does stale data from before disable appear?

5. **Audit overlay_vertex_count usage**
   - Verify it's set correctly in all code paths
   - Check for any off-by-one errors

6. **Read SDL GPU documentation**
   - Understand exact semantics of `SDL_UploadToGPUBuffer`
   - Check if we need to clear regions we're not writing to

## Code References

- `build_overlay()`: game.c:1168
- `update_game()` overlay copy: game.c:1788-1795
- `render_game()` overlay upload: game.c:1811-1826
- `render_game()` overlay draw: game.c:1861-1869
- Buffer initialization: game.c:1619-1637
