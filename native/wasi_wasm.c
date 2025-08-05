#include <wasi.h>

// =============================================================================
// == WebAssembly (WASI) Implementation
// =============================================================================

// __heap_base is a special symbol provided by the wasm-ld linker. It marks
// the end of the static data section and the beginning of the linear memory
// heap that we can manage. It is declared as an external variable.
//extern uint8_t __heap_base;

#include <stdlib.h>

// Wrapper around the `memory.size` WASM instruction.
// The argument `0` is required for the current memory space.
// Returns the pointer to the last allocated byte plus one.
size_t heap_size() {
    return WASM_PAGE_SIZE * __builtin_wasm_memory_size(0)
        - (size_t)heap_base();
}

static inline uintptr_t align(uintptr_t val, uintptr_t alignment) {
  return (val + alignment - 1) & ~(alignment - 1);
}

// Wrapper around the `memory.grow` WASM instruction.
// Attempts to grow the linear memory by `num_pages`.
// Returns the previous size in pages on success, or -1 on failure.
void* heap_grow(size_t num_bytes) {
    size_t num_pages = align(num_bytes, WASM_PAGE_SIZE) / WASM_PAGE_SIZE;
    size_t prev_size = __builtin_wasm_memory_grow(0, num_pages);
    if (prev_size == (size_t)(-1)) {
        return NULL;
    }
    return (void*)(prev_size * WASM_PAGE_SIZE);
}


extern uint8_t* __heap_base;

void* heap_base() {
    return &__heap_base;
}

// For WASI, the entry point is `_start`, which we define to call our `main` function.
int main();

void _start() {
    int status = main();
    proc_exit(status);
}
