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

// stdlib tests
void test_stdlib(void) {
    printf("=== stdlib tests ===\n");

    printf("Hello World!\n");
    int i = 10;
    printf("i = %d\n\n", i);

    printf("stdlib tests passed\n\n");
}

// base tests
void test_base(void) {
    printf("=== base tests ===\n");

    // WASI heap tests
    printf("## Testing WASI heap operations...\n");
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
    printf("WASI heap tests passed\n\n");

    // Buddy allocator tests
    printf("## Testing buddy allocator...\n");
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
    printf("Buddy allocator tests passed\n\n");

    // Arena allocator tests
    printf("## Testing arena allocator...\n");
    printf("Creating a new arena with an initial size of 4KB...\n");
    Arena *main_arena = arena_new(4096);
    if (!main_arena) {
        printf("Error: Failed to create the arena.\n");
        exit(1);
    }
    arena_pos_t saved_pos_0 = arena_get_pos(main_arena);

    printf("Allocating three strings in the arena...\n");
    char s1[] = "Hello from the Arena!\n";
    char *p_s1 = arena_alloc(main_arena, strlen(s1) + 1);
    strcpy(p_s1, s1);

    char s2[] = "This is a standalone C program. ";
    char *p_s2 = arena_alloc(main_arena, strlen(s2) + 1);
    strcpy(p_s2, s2);

    char s3[] = "It works on WASM, Linux, macOS, and Windows.\n";
    char *p_s3 = arena_alloc(main_arena, strlen(s3) + 1);
    strcpy(p_s3, s3);

    printf("Strings allocated. Printing from the arena:\n");
    printf("%s%s%s\n", p_s1, p_s2, p_s3);

    printf("Saving position and making temporary allocations...\n");
    arena_pos_t saved_pos = arena_get_pos(main_arena);

    char s_temp[] = "[--THIS IS A TEMPORARY ALLOCATION THAT WILL BE ROLLED BACK--]";
    char *p_temp = arena_alloc(main_arena, strlen(s_temp) + 1);
    strcpy(p_temp, s_temp);
    printf("Allocated temporary string: %s\n", p_temp);

    printf("Resetting to saved position...\n");
    arena_reset(main_arena, saved_pos);

    printf("Allocating again from the saved position...\n");
    char s4[] = "String 3, allocated after reset.\n";
    char *p_s4 = arena_alloc(main_arena, strlen(s4) + 1);
    strcpy(p_s4, s4);
    printf("Allocated: %s", p_s4);

    printf("\nFinal content of the arena (first two strings are still valid):\n");
    printf("-> %s%s%s%s", p_s1, p_s2, p_s3, p_s4);

    printf("Resetting the arena...\n");
    arena_reset(main_arena, saved_pos_0);
    printf("Arena has been reset. Previous pointers are now invalid.\n");
    printf("Allocating a new string to show that memory is being reused:\n");

    char s5[] = "This new string overwrites the old data after the reset!\n";
    char *p_s5 = arena_alloc(main_arena, strlen(s5) + 1);
    strcpy(p_s5, s5);
    printf("%s\n", p_s5);

    printf("Freeing the arena...\n");
    arena_free(main_arena);
    printf("Arena has been completely deallocated and memory returned to the system.\n");
    printf("Arena allocator tests passed\n\n");

    printf("base tests passed\n\n");
}

int main(void) {
    test_stdlib();
    test_base();

    printf("=== All tests passed ===\n");
    return 0;
}
