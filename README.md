# Standalone Arena Allocator Project

This project demonstrates a standalone C program with an Arena allocator that compiles and runs on:

* **Linux native** (using raw syscalls, `-nostdlib`)
* **WebAssembly** (using WASI)
* **macOS native** (using libSystem but minimal libc functions, `-nostdlib`)

The program implements an Arena allocator on top of a heap managed either by the WASM runtime or by `mmap` on Linux/macOS. It allocates a few strings onto the arena and prints them to stdout.

## Quick Start

### Build and Test All Platforms

Run all tests for supported platforms:

```bash
pixi run all
```

### Platform-Specific Builds

**Linux native:**
```bash
pixi run -e linux build_linux
pixi run -e linux test_linux
```

**WebAssembly:**
```bash
pixi run -e wasm build_wasm
pixi run -e wasm test_wasm  # Requires wasmtime installed separately
```

**macOS native:**
```bash
pixi run -e macos build_macos  # On macOS only
pixi run -e macos test_macos   # On macOS only
```

### Additional Tasks

```bash
pixi run build_all              # Build Linux + WASM
pixi run build_all_with_macos   # Build all three platforms (macOS only)
pixi run all_with_macos         # Test all three platforms (macOS only)
```

## About the Implementation

The `standalone_arena.c` file contains:

1. **Platform-agnostic Arena allocator** - Works across all target platforms
2. **Platform-specific syscall implementations**:
   - WASI syscalls for WebAssembly
   - Raw Linux syscalls (x86_64)
   - macOS libSystem wrappers
3. **Memory management** using `mmap`/`mprotect` on native platforms and WASM linear memory
4. **Custom implementations** of basic functions (`strlen`, `strcpy`, `memcpy`) to avoid libc dependencies

## Current Status

✅ **Linux native**: Fully working, builds and runs successfully  
✅ **WebAssembly**: Builds successfully, tested compilation  
⚠️ **macOS native**: Configuration ready, requires macOS to test  

The builds create optimized standalone executables that demonstrate cross-platform arena allocation without standard library dependencies.

## Continuous Integration

The project includes GitHub Actions CI that tests all three platforms:

- **Linux**: Tests on `ubuntu-latest` using `pixi run -e linux test_linux`
- **macOS**: Tests on `macos-latest` using `pixi run -e macos test_macos`  
- **WebAssembly**: Tests on `ubuntu-latest` using `pixi run -e wasm build_wasm` + `wasmtime`

All builds use pixi for dependency management and consistent build environments.

## Installation Requirements

- **pixi** (for dependency management)
- **wasmtime** (optional, for running WebAssembly - install separately)

Install wasmtime separately if you want to test WASM locally:
```bash
curl https://wasmtime.dev/install.sh -sSf | bash
```

## Project Architecture Benefits

This project demonstrates several key advantages:

1. **Single Source File**: The same `standalone_arena.c` compiles for all platforms using conditional compilation
2. **Minimal Dependencies**: Uses `-nostdlib` to eliminate runtime dependencies
3. **Cross-Platform Memory Management**: Unified Arena allocator interface across WASM and native platforms
4. **Efficient Syscalls**: Direct syscall usage on native platforms for minimal overhead
5. **Web-Ready**: WASM builds can run in browsers or standalone WASM runtimes

## Motivation

The POSIX API has over 1,000 functions. The WASI API has around 60. WASI/WASM
is more modern API, with security and sandboxing as primary concerns. This
"deny by default" approach neccessitates a smaller, more carefuly curated set
of APIs to minimize the potential attack surface.

Our motivation is in a simpler, yet powerful API and to make it easy to export
programs to the web as well as any platform with minimal overhead.

We use WASM/WASI as our API, and build a lean subset of the C standard library
on top. C programs can use most of the C standard library as usual, and
everything will just work. We also provide extensions, such as arena allocator,
vector / dict data structures, etc.

Then on the backend side we implement JavaScript driver to be able to call such
a program, as well as native implementation to be able to run the program
natively. It runs in wasmtime/wasmer out of the box.

Compared to emscripten, we tightly control the API, so there are no unnecessary calls back and forth with JavaScript.
