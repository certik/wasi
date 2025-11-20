# AGENTS.md

This file provides guidance to LLMs when working with code in this repository.

## Project Overview

This is a cross-platform standalone project that demonstrates building C programs without standard library dependencies (`-nostdlib`). The same source code compiles to:
- **WebAssembly** (WASI)
- **Linux native** (raw syscalls)
- **macOS native** (libSystem)
- **Windows native** (kernel32.dll)

The project uses WASI as its core API layer, implementing a minimal C standard library on top. This approach provides security through sandboxing and a smaller, more curated API surface compared to POSIX.

## Build Commands

All builds use pixi with platform-specific environments:

**WebAssembly:**
```bash
pixi r build_wasm
pixi r build_game_wasm
pixi r test_wasm       # requires wasmtime
pixi r test_base_wasm  # requires wasmtime
```

**Linux:**
```bash
pixi r build_linux
pixi r test_linux
pixi r test_game
```

**macOS:**
```bash
pixi r build_macos  # macOS only
pixi r test_macos
pixi r test_game
```

**Windows:**
```bash
pixi run -e windows build_windows  # Windows only, requires MSVC
pixi run -e windows test_windows
pixi r test_game
```

**Run all tests:**
```bash
pixi run all                    # Linux + WASM
pixi run all_with_macos         # Linux + WASM + macOS
pixi run all_with_windows       # Linux + WASM + Windows
pixi run all_platforms          # All four platforms
```

## Code Architecture

### Three-Layer Design

1. **Platform API Layer** (`platform/platform.h`): Platform-independent interface
   - Memory management: `wasi_heap_base()`, `wasi_heap_size()`, `wasi_heap_grow()`
   - I/O: `wasi_fd_write()`, `wasi_proc_exit()`
   - Single point of system interaction

2. **Platform Implementations** (`platform/`):
   - `platform_wasm.c`: WASM/WASI using Clang intrinsics
   - `platform_linux.c`: Linux x86_64 syscalls
   - `platform_macos.c`: macOS libSystem wrappers
   - `platform_windows.c`: Windows kernel32 API

3. **Application Layer** (`stdlib/`, `base/`):
   - Custom minimal C standard library (`string.c`, `stdio.c`, `stdlib.c`)
   - Memory allocators: buddy allocator (`base/buddy.c`), arena allocator (`base/arena.c`)

### Memory Allocator Stack

- **Buddy Allocator** (`base/buddy.h`, `base/buddy.c`): Manages heap using buddy system algorithm
  - Built on top of WASI memory primitives
  - Initialized in `_start` via `buddy_init()`

- **Arena Allocator** (`base/arena.h`, `base/arena.c`): Fast bump-pointer allocation
  - Built on top of buddy allocator
  - API: `arena_new()`, `arena_alloc()`, `arena_get_pos()`, `arena_reset()`, `arena_free()`
  - Supports position save/restore for temporary allocations

### Single-Source Compilation

`tests.c` is the main entry point that compiles for all platforms using conditional compilation (`#ifdef __wasi__`, etc.). The build system includes the appropriate `platform/platform_*.c` file for each platform. Tests are organized into:
- `test_stdlib()`: Tests for standard library functions
- `test_base()`: Tests for WASI heap operations, buddy allocator, and arena allocator

## Platform-Specific Notes

- **WASM**: Uses `--target=wasm32-wasi`, exports `_start` and `__heap_base`
- **Linux**: Uses `-nostdlib`, raw syscalls, no libc
- **macOS**: Uses `-nostdlib -lSystem`, entry point `__start`
- **Windows**: Uses `/kernel /nodefaultlib`, links with `kernel32.lib`, entry point `_start`

## Directory Structure

- `base/`: Core platform-independent abstractions (arena, buddy)
- `platform/`: Platform-specific implementations (WASM, Linux, macOS, Windows)
- `stdlib/`: Minimal C standard library implementations
- `old/`: Legacy code (not part of current build)
- `tests.c`: Main test program

## CI

GitHub Actions tests all four platforms on push/PR to main. See `.github/workflows/CI.yml`.
