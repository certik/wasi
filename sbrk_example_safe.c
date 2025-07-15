// clang --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export-all -o sbrk_example_safe.wasm sbrk_example_safe.c

#include <stdint.h>
#include <stddef.h>

// Define WebAssembly page size (64KiB)
#define WASM_PAGE_SIZE 65536

// External symbol provided by linker for heap base
extern int __heap_base;

// Global variable to track the program break
static uintptr_t program_break = 0;

// Initialize the heap
void init_heap(void) {
    if (program_break == 0) {
        // Set program_break to __heap_base (after static data and stack)
        program_break = (uintptr_t)&__heap_base;
        
        // Ensure at least one page of memory
        size_t current_pages = __builtin_wasm_memory_size(0);
        size_t heap_base_pages = (program_break + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;
        if (current_pages < heap_base_pages) {
            if (__builtin_wasm_memory_grow(0, heap_base_pages - current_pages) == -1) {
                program_break = 0; // Indicate failure
                return;
            }
        }
    }
}

// Emulate sbrk: Increment break by incr, return previous break
void *sbrk(intptr_t incr) {
    if (program_break == 0 && incr != 0) {
        init_heap();
        if (program_break == 0) {
            return (void*)-1; // Initialization failed
        }
    }
    
    uintptr_t old_break = program_break;
    uintptr_t new_break = program_break + incr;
    size_t current_pages = __builtin_wasm_memory_size(0);
    size_t current_size = current_pages * WASM_PAGE_SIZE;
    
    // Check if new break exceeds current memory
    if (new_break > current_size) {
        size_t additional_bytes = new_break - current_size;
        size_t additional_pages = (additional_bytes + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;
        
        if (__builtin_wasm_memory_grow(0, additional_pages) == -1) {
            return (void*)-1; // Memory growth failed
        }
    }
    
    // Update break and return old break
    program_break = new_break;
    return (void*)old_break;
}

// Example usage: Store and manipulate data
int main(void) {
    // Initialize heap
    init_heap();
    if (program_break == 0) {
        return 1; // Heap initialization failed
    }
    
    // Allocate memory for an array of 10 integers
    int *numbers = (int *)sbrk(10 * sizeof(int));
    if (numbers == (void*)-1) {
        return 1; // Allocation failed
    }
    
    // Store data in the array
    for (int i = 0; i < 10; i++) {
        numbers[i] = i * 10;
    }
    
    // Allocate memory for a string (including null terminator)
    char *message = (char *)sbrk(20);
    if (message == (void*)-1) {
        return 1; // Allocation failed
    }
    
    // Store a string
    const char *text = "Hello, WebAssembly!";
    for (int i = 0; i < 20; i++) {
        message[i] = text[i];
        if (text[i] == '\0') break;
    }
    
    // Verify data (sum of integers)
    volatile int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += numbers[i]; // Sum = 0 + 10 + 20 + ... + 90 = 450
    }
    
    // Return sum and heap base for verification
    return (sum == 450 && program_break > (uintptr_t)&__heap_base) ? 0 : 1;
}
