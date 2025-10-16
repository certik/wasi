#include <stddef.h>
#include <stdint.h>

// Forward declare string functions
extern size_t strlen(const char *str);
extern char *strcpy(char *dest, const char *src);
extern void *memcpy(void *dest, const void *src, size_t n);

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <base/base_io.h>
#include <base/wasi.h>
#include <base/arena.h>
#include <base/scratch.h>
#include <base/buddy.h>
#include <base/base_string.h>
#include <test_base.h>

// Helper function for inner scratch scope
static char* test_nested_scratch_inner(Arena *outer_arena, int avoid_conflict) {
    Scratch inner;
    if (avoid_conflict) {
        inner = scratch_begin_avoid_conflict(outer_arena);
    } else {
        inner = scratch_begin();
    }

    // Fill result using outer_arena AFTER inner scratch begins
    char *result = arena_alloc(outer_arena, 50);
    strcpy(result, "ABC");
    printf("  ARENAS: inner=%p, outer=%p\n", inner.arena, outer_arena);

    char *inner_temp = arena_alloc(inner.arena, 50);
    strcpy(inner_temp, "Inner temp");
    printf("  In inner scratch: %s\n", inner_temp);
    if (avoid_conflict) {
        assert(inner.arena != outer_arena);
    } else {
        assert(inner.arena == outer_arena);
    }
    scratch_end(inner);

    return result;
}

// Helper function for outer scratch scope
static void test_nested_scratch_outer(int avoid_conflict) {
    Scratch outer = scratch_begin();
    char *outer_temp = test_nested_scratch_inner(outer.arena, avoid_conflict);
    char *outer_temp2 = arena_alloc(outer.arena, 50);
    strcpy(outer_temp2, "XXX");

    if (avoid_conflict) {
        printf("  In outer scratch after inner: %s\n", outer_temp);

        // Values are different (correct)
        assert(outer_temp[0] == 'A');
        assert(outer_temp[1] == 'B');
        assert(outer_temp[2] == 'C');

        assert(outer_temp2[0] == 'X');
        assert(outer_temp2[1] == 'X');
        assert(outer_temp2[2] == 'X');

        // and the pointers are different (correct)
        assert(outer_temp != outer_temp2);
    } else {
        printf("  In outer scratch after inner: %s (corrupted!)\n", outer_temp);
        // This demonstrates the bug: scratch_begin() without conflict avoidance allows
        // both scopes to share the same arena, and scratch_end(inner) invalidates outer_temp
        // The values are the same (bug)
        assert(outer_temp[0] == 'X');
        assert(outer_temp[1] == 'X');
        assert(outer_temp[2] == 'X');

        assert(outer_temp2[0] == 'X');
        assert(outer_temp2[1] == 'X');
        assert(outer_temp2[2] == 'X');

        // and the pointers are the same (bug)
        assert(outer_temp == outer_temp2);
    }
    scratch_end(outer);
}

void test_wasi_heap(void) {
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
    printf("WASI heap tests passed\n");
}

void test_buddy(void) {
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
    printf("Buddy allocator tests passed\n");
}

void test_arena(void) {
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
    printf("%s%s%s", p_s1, p_s2, p_s3);

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

    printf("Final content of the arena (first two strings are still valid):\n");
    printf("-> %s%s%s%s", p_s1, p_s2, p_s3, p_s4);

    printf("Resetting the arena...\n");
    arena_reset(main_arena, saved_pos_0);
    printf("Arena has been reset. Previous pointers are now invalid.\n");
    printf("Allocating a new string to show that memory is being reused:\n");

    char s5[] = "This new string overwrites the old data after the reset!\n";
    char *p_s5 = arena_alloc(main_arena, strlen(s5) + 1);
    strcpy(p_s5, s5);
    printf("%s", p_s5);

    printf("Freeing the arena...\n");
    arena_free(main_arena);
    printf("Arena has been completely deallocated and memory returned to the system.\n");
    printf("Arena allocator tests passed\n");
}

void test_scratch(void) {
    printf("## Testing scratch arena...\n");
    printf("Creating a new arena for scratch tests...\n");
    Arena *scratch_test_arena = arena_new(4096);
    if (!scratch_test_arena) {
        printf("Error: Failed to create the arena.\n");
        exit(1);
    }

    printf("Test 1: Basic scratch allocation and cleanup\n");
    char *persistent = arena_alloc(scratch_test_arena, 100);
    strcpy(persistent, "This persists");

    {
        Scratch scratch = scratch_begin();
        char *temp1 = arena_alloc(scratch.arena, 50);
        strcpy(temp1, "Temporary 1");
        char *temp2 = arena_alloc(scratch.arena, 50);
        strcpy(temp2, "Temporary 2");
        printf("  Inside scratch: %s, %s, %s\n", persistent, temp1, temp2);
        assert(temp1 != temp2);
        scratch_end(scratch);
    }

    char *after_scratch = arena_alloc(scratch_test_arena, 100);
    strcpy(after_scratch, "After scratch");
    printf("  After scratch end: %s, %s\n", persistent, after_scratch);

    printf("Test 2: Nested scratch scopes with conflict avoidance\n");
    test_nested_scratch_outer(1);

    printf("Test 2b: Nested scratch scopes WITHOUT conflict avoidance\n");
    test_nested_scratch_outer(0);

    printf("Test 3: Multiple sequential scratch scopes\n");
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        strcpy(temp, "Iteration 0");
        printf("  %s\n", temp);
        scratch_end(scratch);
    }
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        strcpy(temp, "Iteration 1");
        printf("  %s\n", temp);
        scratch_end(scratch);
    }
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        strcpy(temp, "Iteration 2");
        printf("  %s\n", temp);
        scratch_end(scratch);
    }

    printf("Test 4: Verify memory reuse after scratch_end\n");
    arena_pos_t before_reuse = arena_get_pos(scratch_test_arena);
    {
        Scratch scratch = scratch_begin();
        arena_alloc(scratch.arena, 1000);
        scratch_end(scratch);
    }
    arena_pos_t after_reuse = arena_get_pos(scratch_test_arena);
    assert(before_reuse.ptr == after_reuse.ptr);
    printf("  Memory position restored correctly\n");

    printf("Freeing scratch test arena...\n");
    arena_free(scratch_test_arena);
    printf("Scratch arena tests passed\n");
}

void test_string(void) {
    printf("## Testing base string functions...\n");
    Arena *arena = arena_new(4096);

    // Test str_from_cstr_view
    string s1 = str_from_cstr_view("hello");
    assert(s1.size == 5);
    assert(s1.str[0] == 'h');

    // Test str_lit macro
    string s2 = str_lit("world");
    assert(s2.size == 5);

    // Test str_eq
    string s3 = str_lit("hello");
    assert(str_eq(s1, s3));
    assert(!str_eq(s1, s2));

    // Test str_concat
    string s4 = str_concat(arena, s1, str_lit(" "));
    string s5 = str_concat(arena, s4, s2);
    assert(s5.size == 11);
    assert(str_eq(s5, str_lit("hello world")));

    // Test int_to_string
    string s6 = int_to_string(arena, 42);
    assert(str_eq(s6, str_lit("42")));

    string s7 = int_to_string(arena, -123);
    assert(str_eq(s7, str_lit("-123")));

    // Test char_to_string
    string s8 = char_to_string(arena, 'X');
    assert(s8.size == 1);
    assert(s8.str[0] == 'X');

    // Test str_to_cstr_copy
    char *cstr = str_to_cstr_copy(arena, s5);
    assert(cstr[11] == '\0');
    assert(strlen(cstr) == 11);

    printf("String function tests passed\n");
    arena_free(arena);
}

void test_base(void) {
    printf("=== base tests ===\n");

    test_wasi_heap();
    test_buddy();
    test_arena();
    test_scratch();
    test_string();

    printf("base tests passed\n\n");
}
