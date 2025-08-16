#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <base_io.h>
#include <wasi.h>
#include <arena.h>
#include <string.h>
#include <stdbool.h>
#include <buddy.h>



int main(void) {
    // The underlying buddy allocator must be initialized before the arena can be used.
    buddy_init();

    printf("## Creating a new arena with an initial size of 4KB...\n");
    // Create the arena. The new API returns a pointer to the arena structure.
    arena_t *main_arena = arena_new(4096);
    if (!main_arena) {
        printf("Error: Failed to create the arena.\n");
        return 1;
    }

    printf("Hello World!\n");
    int i = 10;
    printf("i = %d\n\n", i);

    // --- Original Allocation Example ---
    printf("## Allocating three strings in the arena...\n");

    // Allocate and copy some strings into the arena.
    // Note that arena_alloc now takes the arena pointer `main_arena` directly.
    char s1[] = "Hello from the Arena!\n";
    char *p1 = arena_alloc(main_arena, strlen(s1) + 1);
    strcpy(p1, s1);

    char s2[] = "This is a standalone C program. ";
    char *p2 = arena_alloc(main_arena, strlen(s2) + 1);
    strcpy(p2, s2);

    char s3[] = "It works on WASM, Linux, macOS, and Windows.\n";
    char *p3 = arena_alloc(main_arena, strlen(s3) + 1);
    strcpy(p3, s3);

    printf("Strings allocated. Printing from the arena:\n");
    printf("%s%s%s\n", p1, p2, p3);

    // --- Reset Example ---
    printf("## Resetting the arena...\n");
    arena_reset(main_arena);
    printf("Arena has been reset. Pointers p1, p2, and p3 are now invalid.\n");
    printf("Allocating a new string to show that memory is being reused:\n");

    char s4[] = "This new string overwrites the old data after the reset!\n";
    char *p4 = arena_alloc(main_arena, strlen(s4) + 1);
    strcpy(p4, s4);

    printf("%s\n", p4);

    // --- Freeing Example ---
    printf("## Freeing the arena...\n");
    // This deallocates all chunks and the arena structure itself.
    arena_free(main_arena);
    printf("Arena has been completely deallocated and memory returned to the system.\n");

    // -----------------

    void* hb = wasi_heap_base();
    printf("heap_base = %p (%zu)\n", hb, (size_t)hb);
    size_t ms1 = wasi_heap_size();
    printf("heap_size = %zu\n", ms1);
    void* mg = wasi_heap_grow(4 * WASM_PAGE_SIZE);
    printf("heap_grow_return = %p (%zu)\n", mg, (size_t)mg);
    assert((size_t)hb + ms1 == (size_t)mg);

    size_t ms2 = wasi_heap_size();
    printf("heap_size = %zu\n", ms2);
    assert(ms1 + 4*WASM_PAGE_SIZE == ms2);

    mg = wasi_heap_grow(8 * WASM_PAGE_SIZE);
    printf("heap_grow_return = %p (%zu)\n", mg, (size_t)mg);
    assert((size_t)hb + ms2 == (size_t)mg);

    ms2 = wasi_heap_size();
    printf("heap_size = %zu\n", ms2);
    assert(ms1 + (4+8)*WASM_PAGE_SIZE == ms2);

    // Buddy allocator tests
    {
        buddy_init();

        // Allocate a small block (will round up to MIN_PAGE_SIZE)
        void* p1 = buddy_alloc(100);
        if (!p1) {
            printf("Allocation failed\n");
            exit(1);
        }
        printf("Allocated p1\n");

        // Allocate a larger block
        void* p2 = buddy_alloc(8192);
        if (!p2) {
            printf("Allocation failed\n");
            exit(1);
        }
        printf("Allocated p2\n");

        // Free the first block
        buddy_free(p1);
        printf("Freed p1\n");

        // Allocate again to demonstrate reuse
        void* p3 = buddy_alloc(200);
        if (!p3) {
            printf("Allocation failed\n");
            exit(1);
        }
        printf("Allocated p3\n");

        // Free remaining
        buddy_free(p2);
        buddy_free(p3);

        printf("All freed\n");
    }

    return 0; // Success
}
