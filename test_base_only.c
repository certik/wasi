#include <stddef.h>
#include <stdint.h>

// Test runner that only depends on base/ - no stdlib/ dependencies
// This proves that base/ is completely self-contained

#include <base/assert.h>
#include <base/exit.h>
#include <base/mem.h>
#include <base/base_io.h>
#include <base/wasi.h>
#include <base/arena.h>
#include <base/scratch.h>
#include <base/buddy.h>
#include <base/format.h>
#include <base/io.h>
#include <base/hashtable.h>
#include <base/vector.h>

// Forward declare test function from test_base.c
extern void test_base(void);

int main() {
    test_base();
    return 0;
}
