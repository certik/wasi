#include <wasi.h>
#include <stdlib.h>

#include <buddy.h>

#define WASI(name) __attribute__((__import_module__("wasi_snapshot_preview1"), __import_name__(#name))) name

uint32_t WASI(fd_write)(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);
void WASI(proc_exit)(int status);

#undef WASI

// =============================================================================
// == WebAssembly (WASI) Implementation
// =============================================================================

// __heap_base is a special symbol provided by the wasm-ld linker. It marks
// the end of the static data section and the beginning of the linear memory
// heap that we can manage. It is declared as an external variable.
//extern uint8_t __heap_base;

// Wrapper around the `memory.size` WASM instruction.
// The argument `0` is required for the current memory space.
// Returns the pointer to the last allocated byte plus one.
size_t wasi_heap_size() {
    return WASM_PAGE_SIZE * __builtin_wasm_memory_size(0)
        - (size_t)wasi_heap_base();
}

static inline uintptr_t align(uintptr_t val, uintptr_t alignment) {
  return (val + alignment - 1) & ~(alignment - 1);
}

// Wrapper around the `memory.grow` WASM instruction.
// Attempts to grow the linear memory by `num_pages`.
// Returns the previous size in pages on success, or -1 on failure.
void* wasi_heap_grow(size_t num_bytes) {
    size_t num_pages = align(num_bytes, WASM_PAGE_SIZE) / WASM_PAGE_SIZE;
    size_t prev_size = __builtin_wasm_memory_grow(0, num_pages);
    if (prev_size == (size_t)(-1)) {
        return NULL;
    }
    return (void*)(prev_size * WASM_PAGE_SIZE);
}


extern uint8_t* __heap_base;

void* wasi_heap_base() {
    return &__heap_base;
}

void wasi_proc_exit(int status) {
    proc_exit(status);
}

uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    return fd_write(fd, iovs, iovs_len, nwritten);
}

// For WASI, the entry point is `_start`, which we define to call our `main` function.
int main();

void _start() {
    buddy_init();
    int status = main();
    wasi_proc_exit(status);
}
