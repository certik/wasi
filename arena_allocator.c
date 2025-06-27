// clang --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-all -o arena.wasm arena_allocator.c

#include <stdint.h>
#include <stddef.h>

// Define the page size for WebAssembly (64KiB)
#define WASM_PAGE_SIZE 65536

// Arena allocator structure
typedef struct {
    uint8_t* base;      // Base pointer of the arena
    size_t size;        // Total size of the arena in bytes
    size_t offset;      // Current offset for allocation
} Arena;

// Initialize the arena with an initial number of pages
void arena_init(Arena* arena, size_t initial_pages) {
    // Get the current memory size in pages
    size_t current_pages = __builtin_wasm_memory_size(0);
    
    // If not enough pages, grow memory
    if (current_pages < initial_pages) {
        int32_t result = __builtin_wasm_memory_grow(0, initial_pages - current_pages);
        if (result == -1) {
            // Handle memory growth failure
            arena->base = NULL;
            arena->size = 0;
            arena->offset = 0;
            return;
        }
    }
    
    arena->base = (uint8_t*)0; // WebAssembly memory starts at address 0
    arena->size = initial_pages * WASM_PAGE_SIZE;
    arena->offset = 0;
}

// Allocate memory from the arena
void* arena_alloc(Arena* arena, size_t size) {
    // Ensure alignment to 8 bytes for safety
    size = (size + 7) & ~7;
    
    // Check if there's enough space
    if (arena->offset + size > arena->size) {
        // Calculate additional pages needed
        size_t additional_bytes = arena->offset + size - arena->size;
        size_t additional_pages = (additional_bytes + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;
        
        // Grow memory
        int32_t result = __builtin_wasm_memory_grow(0, additional_pages);
        if (result == -1) {
            return NULL; // Memory growth failed
        }
        
        // Update arena size
        arena->size += additional_pages * WASM_PAGE_SIZE;
    }
    
    // Allocate memory
    void* ptr = arena->base + arena->offset;
    arena->offset += size;
    return ptr;
}

// Reset the arena (reuse memory without freeing)
void arena_reset(Arena* arena) {
    arena->offset = 0;
}

// Example usage
int main() {
    Arena arena;
    arena_init(&arena, 1); // Initialize with 1 page (64KiB)
    
    if (arena.base == NULL) {
        return 1; // Initialization failed
    }
    
    // Allocate some memory
    int* numbers = (int*)arena_alloc(&arena, 100 * sizeof(int));
    if (numbers == NULL) {
        return 1; // Allocation failed
    }
    
    // Use the allocated memory
    for (int i = 0; i < 100; i++) {
        numbers[i] = i;
    }
    
    // Allocate more memory, triggering memory.grow
    char* buffer = (char*)arena_alloc(&arena, 70000); // Exceeds initial page
    if (buffer == NULL) {
        return 1; // Allocation failed
    }
    
    // Reset arena for reuse
    arena_reset(&arena);
    
    return 0;
}
