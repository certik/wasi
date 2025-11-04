#include <stdint.h>
#include <stddef.h>

#include <base/mem.h>
#include <base/exit.h>
#include <wasi.h>
#include <buddy.h>
#include <stdlib.h>
#include <string.h>

void* malloc(size_t size) {
    return buddy_alloc(size);
}

void free(void* ptr) {
    buddy_free(ptr);
}

void exit(int status) {
    base_exit(status);
}

void abort(void) {
    base_abort();
}

// Linear Congruential Generator (LCG)
// Parameters: a = 1103515245, c = 12345, m = 2^31
static uint32_t rand_state = 1;

void srand(int seed) {
    rand_state = (uint32_t)seed;
}

int rand() {
    rand_state = (rand_state * 1103515245u + 12345u) & 0x7FFFFFFF;
    return (int)rand_state;
}
