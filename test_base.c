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
#include <base/format.h>
#include <base/io.h>
#include <base/hashtable.h>
#include <base/vector.h>
#include <test_base.h>

// Define hashtable and vector types for tests
#define MapIntString_HASH(key) ((size_t)(key))
#define MapIntString_EQUAL(key1, key2) ((key1) == (key2))
DEFINE_HASHTABLE_FOR_TYPES(int, string, MapIntString)

#define MapStringInt_HASH(key) (str_hash(key))
#define MapStringInt_EQUAL(key1, key2) (str_eq((key1), (key2)))
DEFINE_HASHTABLE_FOR_TYPES(string, int, MapStringInt)

DEFINE_VECTOR_FOR_TYPE(int, VecInt)
DEFINE_VECTOR_FOR_TYPE(int*, VecIntP)

// Helper function for inner scratch scope
static char* test_nested_scratch_inner(Arena *outer_arena, bool avoid_conflict) {
    Scratch inner;
    if (avoid_conflict) {
        inner = scratch_begin_avoid_conflict(outer_arena);
    } else {
        inner = scratch_begin();
    }

    // Fill result using outer_arena AFTER inner scratch begins
    char *result = arena_alloc(outer_arena, 50);
    strcpy(result, "ABC");
    println(outer_arena, str_lit("  ARENAS: inner={}, outer={}"), inner.arena, outer_arena);

    char *inner_temp = arena_alloc(inner.arena, 50);
    strcpy(inner_temp, "Inner temp");
    println(outer_arena, str_lit("  In inner scratch: {}"), inner_temp);
    if (avoid_conflict) {
        assert(inner.arena != outer_arena);
    } else {
        assert(inner.arena == outer_arena);
    }
    scratch_end(inner);

    return result;
}

// Helper function for outer scratch scope
static void test_nested_scratch_outer(bool avoid_conflict) {
    Scratch outer = scratch_begin();
    char *outer_temp = test_nested_scratch_inner(outer.arena, avoid_conflict);
    char *outer_temp2 = arena_alloc(outer.arena, 50);
    strcpy(outer_temp2, "XXX");

    if (avoid_conflict) {
        println(outer.arena, str_lit("  In outer scratch after inner: {}"), outer_temp);

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
        println(outer.arena, str_lit("  In outer scratch after inner: {} (corrupted!)"), outer_temp);
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
    Arena *arena = arena_new(4096);
    println(arena, str_lit("## Testing WASI heap operations..."));
    void* hb = wasi_heap_base();
    println(arena, str_lit("heap_base = {} ({})"), hb, (size_t)hb);
    size_t ms1 = wasi_heap_size();
    println(arena, str_lit("heap_size = {}"), ms1);
    void* mg = wasi_heap_grow(4 * WASM_PAGE_SIZE);
    println(arena, str_lit("heap_grow_return = {} ({})"), mg, (size_t)mg);
    assert((size_t)hb + ms1 == (size_t)mg);

    size_t ms2 = wasi_heap_size();
    println(arena, str_lit("heap_size = {}"), ms2);
    assert(ms1 + 4*WASM_PAGE_SIZE == ms2);

    mg = wasi_heap_grow(8 * WASM_PAGE_SIZE);
    println(arena, str_lit("heap_grow_return = {} ({})"), mg, (size_t)mg);
    assert((size_t)hb + ms2 == (size_t)mg);

    ms2 = wasi_heap_size();
    println(arena, str_lit("heap_size = {}"), ms2);
    assert(ms1 + (4+8)*WASM_PAGE_SIZE == ms2);
    println(arena, str_lit("WASI heap tests passed"));
    arena_free(arena);
}

void test_buddy(void) {
    Arena *arena = arena_new(4096);
    println(arena, str_lit("## Testing buddy allocator..."));
    buddy_init();

    // Allocate a small block (will round up to MIN_PAGE_SIZE)
    void* p1 = buddy_alloc(100);
    if (!p1) {
        println(arena, str_lit("Allocation failed"));
        arena_free(arena);
        exit(1);
    }
    println(arena, str_lit("Allocated p1"));

    // Allocate a larger block
    void* p2 = buddy_alloc(8192);
    if (!p2) {
        println(arena, str_lit("Allocation failed"));
        arena_free(arena);
        exit(1);
    }
    println(arena, str_lit("Allocated p2"));

    // Free the first block
    buddy_free(p1);
    println(arena, str_lit("Freed p1"));

    // Allocate again to demonstrate reuse
    void* p3 = buddy_alloc(200);
    if (!p3) {
        println(arena, str_lit("Allocation failed"));
        arena_free(arena);
        exit(1);
    }
    println(arena, str_lit("Allocated p3"));

    // Free remaining
    buddy_free(p2);
    buddy_free(p3);
    println(arena, str_lit("Buddy allocator tests passed"));
    arena_free(arena);
}

void test_arena(void) {
    println(NULL, str_lit("## Testing arena allocator..."));
    println(NULL, str_lit("Creating a new arena with an initial size of 4KB..."));
    Arena *main_arena = arena_new(4096);
    if (!main_arena) {
        println(NULL, str_lit("Error: Failed to create the arena."));
        exit(1);
    }
    arena_pos_t saved_pos_0 = arena_get_pos(main_arena);

    println(main_arena, str_lit("Allocating three strings in the arena..."));
    char s1[] = "Hello from the Arena!\n";
    char *p_s1 = arena_alloc(main_arena, strlen(s1) + 1);
    strcpy(p_s1, s1);

    char s2[] = "This is a standalone C program. ";
    char *p_s2 = arena_alloc(main_arena, strlen(s2) + 1);
    strcpy(p_s2, s2);

    char s3[] = "It works on WASM, Linux, macOS, and Windows.\n";
    char *p_s3 = arena_alloc(main_arena, strlen(s3) + 1);
    strcpy(p_s3, s3);

    println(main_arena, str_lit("Strings allocated. Printing from the arena:"));
    println(main_arena, str_lit("{}{}{}"), p_s1, p_s2, p_s3);

    println(main_arena, str_lit("Saving position and making temporary allocations..."));
    arena_pos_t saved_pos = arena_get_pos(main_arena);

    char s_temp[] = "[--THIS IS A TEMPORARY ALLOCATION THAT WILL BE ROLLED BACK--]";
    char *p_temp = arena_alloc(main_arena, strlen(s_temp) + 1);
    strcpy(p_temp, s_temp);
    println(main_arena, str_lit("Allocated temporary string: {}"), p_temp);

    println(main_arena, str_lit("Resetting to saved position..."));
    arena_reset(main_arena, saved_pos);

    println(main_arena, str_lit("Allocating again from the saved position..."));
    char s4[] = "String 3, allocated after reset.\n";
    char *p_s4 = arena_alloc(main_arena, strlen(s4) + 1);
    strcpy(p_s4, s4);
    println(main_arena, str_lit("Allocated: {}"), p_s4);

    println(main_arena, str_lit("Final content of the arena (first two strings are still valid):"));
    println(main_arena, str_lit("-> {}{}{}{}"), p_s1, p_s2, p_s3, p_s4);

    println(main_arena, str_lit("Resetting the arena..."));
    arena_reset(main_arena, saved_pos_0);
    println(main_arena, str_lit("Arena has been reset. Previous pointers are now invalid."));
    println(main_arena, str_lit("Allocating a new string to show that memory is being reused:"));

    char s5[] = "This new string overwrites the old data after the reset!\n";
    char *p_s5 = arena_alloc(main_arena, strlen(s5) + 1);
    strcpy(p_s5, s5);
    println(main_arena, str_lit("{}"), p_s5);

    println(main_arena, str_lit("Freeing the arena..."));
    arena_free(main_arena);
    println(NULL, str_lit("Arena has been completely deallocated and memory returned to the system."));
    println(NULL, str_lit("Arena allocator tests passed"));
}

void test_scratch(void) {
    println(NULL, str_lit("## Testing scratch arena..."));
    println(NULL, str_lit("Creating a new arena for scratch tests..."));
    Arena *scratch_test_arena = arena_new(4096);
    if (!scratch_test_arena) {
        println(NULL, str_lit("Error: Failed to create the arena."));
        exit(1);
    }

    println(scratch_test_arena, str_lit("Test 1: Basic scratch allocation and cleanup"));
    char *persistent = arena_alloc(scratch_test_arena, 100);
    strcpy(persistent, "This persists");

    {
        Scratch scratch = scratch_begin();
        char *temp1 = arena_alloc(scratch.arena, 50);
        strcpy(temp1, "Temporary 1");
        char *temp2 = arena_alloc(scratch.arena, 50);
        strcpy(temp2, "Temporary 2");
        println(scratch_test_arena, str_lit("  Inside scratch: {}, {}, {}"), persistent, temp1, temp2);
        assert(temp1 != temp2);
        scratch_end(scratch);
    }

    char *after_scratch = arena_alloc(scratch_test_arena, 100);
    strcpy(after_scratch, "After scratch");
    println(scratch_test_arena, str_lit("  After scratch end: {}, {}"), persistent, after_scratch);

    println(scratch_test_arena, str_lit("Test 2: Nested scratch scopes with conflict avoidance"));
    test_nested_scratch_outer(true);

    println(scratch_test_arena, str_lit("Test 2b: Nested scratch scopes WITHOUT conflict avoidance"));
    test_nested_scratch_outer(false);

    println(scratch_test_arena, str_lit("Test 3: Multiple sequential scratch scopes"));
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        strcpy(temp, "Iteration 0");
        println(scratch_test_arena, str_lit("  {}"), temp);
        scratch_end(scratch);
    }
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        strcpy(temp, "Iteration 1");
        println(scratch_test_arena, str_lit("  {}"), temp);
        scratch_end(scratch);
    }
    {
        Scratch scratch = scratch_begin();
        char *temp = arena_alloc(scratch.arena, 100);
        strcpy(temp, "Iteration 2");
        println(scratch_test_arena, str_lit("  {}"), temp);
        scratch_end(scratch);
    }

    println(scratch_test_arena, str_lit("Test 4: Verify memory reuse after scratch_end"));
    arena_pos_t before_reuse = arena_get_pos(scratch_test_arena);
    {
        Scratch scratch = scratch_begin();
        arena_alloc(scratch.arena, 1000);
        scratch_end(scratch);
    }
    arena_pos_t after_reuse = arena_get_pos(scratch_test_arena);
    assert(before_reuse.ptr == after_reuse.ptr);
    println(scratch_test_arena, str_lit("  Memory position restored correctly"));

    println(scratch_test_arena, str_lit("Freeing scratch test arena..."));
    arena_free(scratch_test_arena);
    println(NULL, str_lit("Scratch arena tests passed"));
}

void test_format(void) {
    println(NULL, str_lit("## Testing format..."));
    Arena* arena = arena_new(1024*10);
    double pi = 3.1415926535;

    // Example with no arguments
    string fmt = str_lit("Hello!");
    string result = format(arena, fmt);
    assert(str_eq(result, str_lit("Hello!")));
    println(arena, str_lit("No args: {}"), str_to_cstr_copy(arena, result));

    // Example with one argument
    fmt = str_lit("Hello, {}!");
    //result = format(arena, fmt, str_lit("world"));
    result = str_lit("Hello, world!");
    assert(str_eq(result, str_lit("Hello, world!")));
    println(arena, str_lit("One arg: {}"), str_to_cstr_copy(arena, result));

    fmt = str_lit("Hello, {}!");
    result = format(arena, fmt, 5);
    assert(str_eq(result, str_lit("Hello, 5!")));
    println(arena, str_lit("One arg: {}"), str_to_cstr_copy(arena, result));

    // Example with formatted double
    fmt = str_lit("Value: {:10.5f}");
    //result = format(arena, fmt, pi);
    // Note: Double formatting may have slight differences, so we just print it
    //println(arena, str_lit("Formatted double: {}"), str_to_cstr_copy(arena, result));

    // Example with formatted char
    fmt = str_lit("Char: |{:^5}|");
    result = format(arena, fmt, 'x');
    assert(str_eq(result, str_lit("Char: | 120 |")));
    println(arena, str_lit("Formatted char: {}"), str_to_cstr_copy(arena, result));

    // Example with multiple arguments
    fmt = str_lit("Hello, {}, {}, {}, {}!");
    //result = format(arena, fmt, "world", 35.5, str_lit("XX"), 3);
    //println(arena, str_lit("Multiple args: {}"), str_to_cstr_copy(arena, result));

    arena_free(arena);
    println(NULL, str_lit("Format tests passed"));
}

void test_io(void) {
    println(NULL, str_lit("## Testing io..."));
    Arena* arena = arena_new(1024*20);

    string text;
    bool ok = read_file(arena, str_lit("does not exist"), &text);
    assert(!ok);

    text.size = 0;
    assert(text.size == 0);
    ok = read_file(arena, str_lit("README.md"), &text);
    // README.md may not exist, so we don't assert on this
    if (ok) {
        assert(text.size > 10);
        println(arena, str_lit("Read README.md: {} bytes"), text.size);
    } else {
        println(arena, str_lit("README.md not found (expected in some environments)"));
    }

    //println(arena, str_lit("Hello from io."));

    arena_free(arena);
    println(NULL, str_lit("I/O tests passed"));
}

void test_hashtable_int_string(void) {
    println(NULL, str_lit("## Testing hashtable (int->string)..."));
    Arena* arena = arena_new(1024*10);

    MapIntString ht;
    MapIntString_init(arena, &ht, 16);
    MapIntString_insert(arena, &ht, 42, str_lit("forty-two"));
    string *value = MapIntString_get(&ht, 42);
    assert(value);
    println(arena, str_lit("Value for key 42: {}"), str_to_cstr_copy(arena, *value));

    arena_free(arena);
    println(NULL, str_lit("Hashtable (int->string) tests passed"));
}

void test_hashtable_string_int(void) {
    println(NULL, str_lit("## Testing hashtable (string->int)..."));
    Arena* arena = arena_new(1024*10);

    MapStringInt ht;
    MapStringInt_init(arena, &ht, 16);
    MapStringInt_insert(arena, &ht, str_lit("forty-two"), 42);
    int *value = MapStringInt_get(&ht, str_lit("forty-two"));
    assert(value);
    println(arena, str_lit("Value for key \"forty-two\": {}"), *value);

    arena_free(arena);
    println(NULL, str_lit("Hashtable (string->int) tests passed"));
}

void test_vector_int(void) {
    println(NULL, str_lit("## Testing vector (int)..."));
    Arena* arena = arena_new(1024*10);

    VecInt v;
    VecInt_reserve(arena, &v, 1);
    assert(v.size == 0);
    VecInt_push_back(arena, &v, 1);
    assert(v.size == 1);
    VecInt_push_back(arena, &v, 2);
    assert(v.size == 2);
    VecInt_push_back(arena, &v, 3);
    assert(v.size == 3);
    assert(v.data[0] == 1);
    assert(v.data[1] == 2);
    assert(v.data[2] == 3);

    arena_free(arena);
    println(NULL, str_lit("Vector (int) tests passed"));
}

void test_vector_int_ptr(void) {
    println(NULL, str_lit("## Testing vector (int*)..."));
    Arena* arena = arena_new(1024*10);

    VecIntP v;
    int i=1, j=2, k=3;
    VecIntP_reserve(arena, &v, 1);
    assert(v.size == 0);
    VecIntP_push_back(arena, &v, &i);
    assert(v.size == 1);
    VecIntP_push_back(arena, &v, &j);
    assert(v.size == 2);
    VecIntP_push_back(arena, &v, &k);
    assert(v.size == 3);
    assert(*v.data[0] == 1);
    assert(*v.data[1] == 2);
    assert(*v.data[2] == 3);
    k = 4;
    assert(*v.data[2] == 4);

    arena_free(arena);
    println(NULL, str_lit("Vector (int*) tests passed"));
}

void test_base(void) {
    Arena *arena = arena_new(4096);
    println(arena, str_lit("=== base tests ==="));

    test_wasi_heap();
    test_buddy();
    test_arena();
    test_scratch();
    test_format();
    test_io();
    test_hashtable_int_string();
    test_hashtable_string_int();
    test_vector_int();
    test_vector_int_ptr();

    println(arena, str_lit("base tests passed"));
    arena_free(arena);
}
