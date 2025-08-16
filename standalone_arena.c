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
    Arena main_arena;
    arena_init(&main_arena);

    printf("Hello World!\n");
    int i = 10;
    printf("i = %d\n", i);

    // Allocate and copy some strings onto the arena
    char s1[] = "Hello from the Arena!\n";
    char* p1 = arena_alloc(&main_arena, strlen(s1) + 1);
    strcpy(p1, s1);

    char s2[] = "This is a standalone C program. ";
    char* p2 = arena_alloc(&main_arena, strlen(s2) + 1);
    strcpy(p2, s2);

    char s3[] = "It works on WASM, Linux, macOS, and Windows.\n";
    char* p3 = arena_alloc(&main_arena, strlen(s3) + 1);
    strcpy(p3, s3);

    printf("%s%s%s", p1, p2, p3);

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
