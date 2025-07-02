/*
 * On WASM:
 * clang w2.c -target wasm32 --no-standard-libraries -Wl,--no-entry -Wl,--export-all -o w2.wasm
 * wasmtime w2.wasm
 * On Linux:
 * clang w2.c && ./a.out
 */
#include <stdio.h>
#include <stddef.h> // For size_t
#include <string.h> // For memcpy
#include <stdint.h> // For uintptr_t

// The page size in WebAssembly is fixed at 64KB.
#define WASM_PAGE_SIZE 65536

// This will be our unified memory handle.
// On WASM, it's a real global array mapped to linear memory.
// On Linux, it's a pointer to our mmap'd region.
char* memory;

// The current size of the memory in pages.
// This is now managed by the application logic, not the backend.
size_t current_memory_pages = 1;

// =============================================================================
// PLATFORM-SPECIFIC IMPLEMENTATION
// =============================================================================

#if defined(__wasm32__)
// =============================================================================
// == WEB_ASSEMBLY BACKEND
// =============================================================================

// On WASM, we declare a global array. The `export_name("memory")` attribute is
// a special convention that tells the linker to export the module's entire
// linear memory. We also need to define `memory` as a pointer for type
// compatibility with the Linux build, but the linker will resolve it correctly
// to the start of the memory array.
__attribute__((export_name("memory")))
char wasm_memory_array[WASM_PAGE_SIZE];
char* memory = wasm_memory_array;

// No implementation of __builtin_wasm_memory_grow is needed here,
// as it's provided by the Clang compiler for the wasm32 target.

#else
// =============================================================================
// == LINUX BACKEND
// =============================================================================

#include <sys/mman.h> // For mmap
#include <unistd.h>   // For sysconf, _SC_PAGESIZE

// We'll reserve 4GB of virtual address space, the maximum for a 32-bit WASM module.
const size_t MAX_MEMORY_BYTES = 4UL * 1024 * 1024 * 1024;

// Provide a Linux implementation for the WASM builtin.
// The function signature must match the compiler intrinsic exactly.
size_t __builtin_wasm_memory_grow(size_t memory_index, size_t num_pages_to_add) {
    // We only support the main memory (index 0).
    if (memory_index != 0 || !memory) {
        return (size_t)-1;
    }

    size_t old_pages = current_memory_pages;
    size_t new_total_pages = current_memory_pages + num_pages_to_add;
    size_t new_total_bytes = new_total_pages * WASM_PAGE_SIZE;

    if (new_total_bytes > MAX_MEMORY_BYTES) {
        fprintf(stderr, "Error: Cannot grow memory beyond 4GB reservation.\n");
        return (size_t)-1;
    }

    // Calculate the start of the new region to commit.
    char* new_region_start = memory + (old_pages * WASM_PAGE_SIZE);
    size_t bytes_to_add = num_pages_to_add * WASM_PAGE_SIZE;

    // Commit the new pages by changing their protection flags.
    void* result = mmap(
        new_region_start,
        bytes_to_add,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
        -1, 0
    );

    if (result == MAP_FAILED) {
        perror("mmap commit failed for growth");
        return (size_t)-1;
    }

    // On success, the builtin returns the *previous* number of pages.
    // The caller is responsible for updating the current page count.
    return old_pages;
}

// This function runs automatically before main() to set up our simulated memory.
__attribute__((constructor))
void init_linux_memory() {
    // 1. Reserve 4GB of virtual address space.
    memory = (char*)mmap(NULL, MAX_MEMORY_BYTES, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (memory == MAP_FAILED) {
        perror("mmap reserve failed");
        memory = NULL;
        return;
    }

    // 2. Commit the first page (initial memory).
    void* first_page = mmap(memory, WASM_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if (first_page == MAP_FAILED) {
        perror("mmap commit failed for initial page");
        munmap(memory, MAX_MEMORY_BYTES);
        memory = NULL;
        return;
    }
    
    printf("[Linux Backend] Successfully reserved 4GB and committed 1 page (64KB).\n");
}

#endif

// =============================================================================
// UNIFIED APPLICATION LOGIC (works on both platforms)
// =============================================================================

int main() {
    printf("--- Memory Test Initial State ---\n");
    printf("Initial memory size: %zu pages (%zu bytes).\n", current_memory_pages, current_memory_pages * WASM_PAGE_SIZE);

    // Write to the end of the initial page.
    size_t last_byte_initial = (current_memory_pages * WASM_PAGE_SIZE) - 1;
    memory[last_byte_initial] = 'A';
    printf("Successfully wrote '%c' to last byte of initial memory (offset %zu).\n", memory[last_byte_initial], last_byte_initial);
    
    printf("\n--- Growing Memory ---\n");
    size_t pages_to_add = 2;
    printf("Attempting to grow memory by %zu pages using the standard builtin...\n", pages_to_add);
    
    // Use the standard WASM builtin directly. On Linux, this calls our implementation.
    // The first argument (0) is the memory index, which is always 0 for the main memory.
    size_t old_page_count = __builtin_wasm_memory_grow(0, pages_to_add);

    if (old_page_count == (size_t)-1) {
        printf("Failed to grow memory.\n");
        return 1;
    }

    // The application logic is now responsible for updating the page count.
    current_memory_pages += pages_to_add;

    printf("Memory grown successfully.\n");
    printf("Previous size: %zu pages. New size: %zu pages.\n", old_page_count, current_memory_pages);

    printf("\n--- Memory Test After Growth ---\n");
    
    // Test writing to the newly allocated memory.
    size_t last_byte_new = (current_memory_pages * WASM_PAGE_SIZE) - 1;
    memory[last_byte_new] = 'Z';
    printf("Successfully wrote '%c' to last byte of new memory (offset %zu).\n", memory[last_byte_new], last_byte_new);

    // Verify the old data is still there.
    printf("Verified old data is still present: memory[%zu] = '%c'\n", last_byte_initial, memory[last_byte_initial]);

    return 0;
}

