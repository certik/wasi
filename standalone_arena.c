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
    arena_pos_t saved_pos_0 = arena_get_pos(main_arena);

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

    // --- Part 2: Save Position and Make Temporary Allocations ---
    printf("\n## Part 2: Saving position and making temporary allocations\n");
    arena_pos_t saved_pos = arena_get_pos(main_arena);
    printf("Position saved.\n");

    char s_temp[] = "[--THIS IS A TEMPORARY ALLOCATION THAT WILL BE ROLLED BACK--]";
    char *p_temp = arena_alloc(main_arena, strlen(s_temp) + 1);
    strcpy(p_temp, s_temp);
    printf("Allocated temporary string: %s\n", p_temp);

    // --- Part 3: Reset to Saved Position ---
    printf("\n## Part 3: Resetting to saved position\n");
    arena_reset(main_arena, saved_pos);
    printf("Arena reset to saved position. The temporary allocation is now invalid.\n");

    // --- Part 4: Allocate Again ---
    printf("\n## Part 4: Allocating again from the saved position\n");
    char s4[] = "String 3, allocated after reset.\n";
    char *p4 = arena_alloc(main_arena, strlen(s3) + 1);
    strcpy(p4, s4);
    printf("Allocated: %s", p4);

    printf("\nFinal content of the arena (first two strings are still valid):\n");
    printf("-> %s%s%s%s", p1, p2, p3, p4);

    // --- Reset Example ---
    printf("## Resetting the arena...\n");
    arena_reset(main_arena, saved_pos_0);
    printf("Arena has been reset. Pointers p1, p2, and p3 are now invalid.\n");
    printf("Allocating a new string to show that memory is being reused:\n");

    char s5[] = "This new string overwrites the old data after the reset!\n";
    char *p5 = arena_alloc(main_arena, strlen(s5) + 1);
    strcpy(p5, s5);

    printf("%s\n", p5);

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
