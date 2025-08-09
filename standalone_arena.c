#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <base_io.h>
#include <wasi.h>
#include <arena.h>
#include <string.h>



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

    void* hb = heap_base();
    printf("heap_base = %p\n", hb);
    size_t ms1 = heap_size();
    printf("heap_size = %zu\n", ms1);
    void* mg = heap_grow(4 * WASM_PAGE_SIZE);
    printf("heap_grow_return = %p\n", mg);
    size_t ms2 = heap_size();
    printf("heap_size = %zu\n", ms2);
    mg = heap_grow(4 * WASM_PAGE_SIZE);
    printf("heap_grow_return = %p\n", mg);
    ms2 = heap_size();
    printf("heap_size = %zu\n", ms2);

    return 0; // Success
}
