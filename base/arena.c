/**
 * @file standalone_arena.c
 * @brief A simple, standalone C program with an Arena allocator.
 *
 * This program does not link against the standard C library (`-nostdlib`) on Linux and WASM.
 * On macOS, it links dynamically against libSystem (libc equivalent) but avoids using standard library functions beyond mmap, mprotect, writev, and _exit.
 * On Windows, it uses MSVC and calls Windows API directly (VirtualAlloc, WriteFile, etc.) without C runtime.
 * It provides four implementations chosen at compile time:
 * 1. WebAssembly (WASM) using WASI system calls.
 * 2. Linux using raw syscalls.
 * 3. macOS using libc wrappers for syscalls.
 * 4. Windows using Windows API directly.
 *
 * The program implements an Arena allocator on top of a heap that is managed
 * either by the WASM runtime, by `mmap` on Linux/macOS, or by `VirtualAlloc` on Windows. It then allocates a few
 * strings onto the arena and prints them to stdout using the `fd_write` syscall.
 *
 * --- Compilation Instructions ---
 *
 * For WebAssembly (WASI):
 * clang --target=wasm32-wasi -nostdlib -Wl,--no-entry -Wl,--export=__heap_base -Wl,--export=_start -Wl,--initial-memory=131072 -o arena.wasm standalone_arena.c
 *
 * To Run with wasmtime:
 * wasmtime arena.wasm
 *
 * For Linux (x86_64):
 * gcc -nostdlib -o arena_linux standalone_arena.c
 *
 * To Run on Linux:
 * ./arena_linux
 *
 * For macOS (x86_64 or arm64):
 * clang -nostdlib -o arena_macos standalone_arena.c -lSystem -Wl,-e,__start
 *
 * To Run on macOS:
 * ./arena_macos
 *
 * For Windows (MSVC):
 * cl /nologo /GS- /Gs0 /kernel /c standalone_arena.c /Fo:standalone_arena.obj
 * link /nologo /subsystem:console /nodefaultlib /entry:_start kernel32.lib standalone_arena.obj /out:arena_windows.exe
 *
 * To Run on Windows:
 * arena_windows.exe
 */

#include <arena.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <base_io.h>
#include <wasi.h>
#include <string.h>



// --- Arena Allocator Implementation ---

// Function to print error and exit
static void allocation_error(void) {
    const char* err_str = "Error: Failed to grow memory for arena allocation.\n";
    ciovec_t iov = { .buf = err_str, .buf_len = strlen(err_str) };
    write_all(1, &iov, 1); // Write to stdout; alternatively use fd=2 for stderr
    proc_exit(1);
}

/**
 * @brief Initializes an Arena allocator.
 *
 * It sets up the arena to use the available memory from `__heap_base` up to
 * the current memory size. If no memory is available, it tries to grow it.
 *
 * @param arena Pointer to the Arena struct to initialize.
 */
void arena_init(Arena* arena) {
    size_t current_size = heap_size();
    if (current_size == 0) {
        if (heap_grow(WASM_PAGE_SIZE) == NULL) { // Try to allocate one page
            allocation_error();
        }
        current_size = heap_size();
    }

    arena->base = (uint8_t*)heap_base();
    arena->capacity = current_size;
    arena->offset = 0;
}

/**
 * @brief Allocates a block of memory from the arena.
 *
 * This is a simple bump allocator. If the arena is out of space, it will
 * attempt to grow the underlying heap.
 *
 * @param arena Pointer to the Arena.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory (always succeeds or exits).
 */
void* arena_alloc(Arena* arena, size_t size) {
    if (arena->base == NULL) allocation_error();

    // Simple alignment to 8 bytes
    size = (size + 7) & ~7;

    if (arena->offset + size > arena->capacity) {
        size_t needed_bytes = (arena->offset + size) - arena->capacity;
        size_t pages_to_grow = (needed_bytes + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;

        if (heap_grow(pages_to_grow*WASM_PAGE_SIZE) == NULL) {
            allocation_error();
        }
        arena->capacity += pages_to_grow * WASM_PAGE_SIZE;
    }

    void* ptr = arena->base + arena->offset;
    arena->offset += size;
    return ptr;
}

/**
 * @brief Resets the arena, effectively "freeing" all allocations.
 *
 * @param arena Pointer to the Arena.
 */
void arena_reset(Arena* arena) {
    arena->offset = 0;
}
