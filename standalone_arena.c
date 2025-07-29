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

#include <stdint.h>

#define NULL ((void*)0)

// --- Platform-Agnostic Interface ---

#include "wasi.h"

static inline uintptr_t align(uintptr_t val, uintptr_t alignment) {
  return (val + alignment - 1) & ~(alignment - 1);
}

// --- Platform-Specific Implementation ---

#if defined(__wasm__) && defined(__wasm32__)
    #include "native/wasi_wasm.c"
#elif defined(__APPLE__)
    #include "native/wasi_macos.c"
#elif defined(_WIN32) || defined(_WIN64)
    #include "native/wasi_windows.c"
#else
    #include "native/wasi_linux.c"
#endif


// --- Arena Allocator Implementation ---

uint32_t write_all(int fd, ciovec_t* iovs, size_t iovs_len);

typedef struct {
    uint8_t* base;
    size_t capacity;
    size_t offset;
} Arena;

size_t my_strlen(const char* str);

// Function to print error and exit
static void allocation_error(void) {
    const char* err_str = "Error: Failed to grow memory for arena allocation.\n";
    ciovec_t iov = { .buf = err_str, .buf_len = my_strlen(err_str) };
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

/**
 * @brief Writes all data from the iovecs to the specified file descriptor.
 *
 * This function repeatedly calls fd_write until all data is written or an error occurs.
 * It updates the iovecs to skip already-written data.
 *
 * @param fd The file descriptor to write to.
 * @param iovs Array of ciovec_t structures containing the data to write.
 * @param iovs_len Number of iovecs in the array.
 * @return 0 on success, or an error code if fd_write fails.
 */
uint32_t write_all(int fd, ciovec_t* iovs, size_t iovs_len) {
    size_t i;
    size_t nwritten;
    uint32_t ret;

    for (i = 0; i < iovs_len; ) {
        ret = fd_write(fd, &iovs[i], iovs_len - i, &nwritten);
        if (ret != 0) {
            return ret; // Return error code
        }

        // Advance through the iovecs based on how much was written
        while (nwritten > 0 && i < iovs_len) {
            if (nwritten >= iovs[i].buf_len) {
                nwritten -= iovs[i].buf_len;
                i++;
            } else {
                iovs[i].buf = (const uint8_t*)iovs[i].buf + nwritten;
                iovs[i].buf_len -= nwritten;
                nwritten = 0;
            }
        }
    }
    return 0; // Success
}

// Custom strlen to avoid C library dependency
size_t my_strlen(const char* str) {
    const char* s;
    for (s = str; *s; ++s);
    return (s - str);
}

// Custom strcpy to avoid C library dependency
char* my_strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

// Custom memcpy to avoid C library dependency
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}


// --- Main Application Logic ---

int main(void) {
    Arena main_arena;
    arena_init(&main_arena);

    // Allocate and copy some strings onto the arena
    char s1[] = "Hello from the Arena!\n";
    char* p1 = arena_alloc(&main_arena, my_strlen(s1) + 1);
    my_strcpy(p1, s1);

    char s2[] = "This is a standalone C program. ";
    char* p2 = arena_alloc(&main_arena, my_strlen(s2) + 1);
    my_strcpy(p2, s2);

    char s3[] = "It works on WASM, Linux, macOS, and Windows.\n";
    char* p3 = arena_alloc(&main_arena, my_strlen(s3) + 1);
    my_strcpy(p3, s3);

    // Prepare iovecs for write_all
    ciovec_t iovs[] = {
        { .buf = p1, .buf_len = my_strlen(p1) },
        { .buf = p2, .buf_len = my_strlen(p2) },
        { .buf = p3, .buf_len = my_strlen(p3) },
    };

    // Write all data to stdout (fd = 1)
    uint32_t ret = write_all(1, iovs, 3);
    if (ret != 0) {
        return 1; // Return non-zero on write error
    }

    void* hb = heap_base();
    size_t ms1 = heap_size();
    void* mg = heap_grow(4 * WASM_PAGE_SIZE);
    size_t ms2 = heap_size();
    // TODO: print the numbers above, both pointers and size

    return 0; // Success
}
