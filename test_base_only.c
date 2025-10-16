#include <stddef.h>
#include <stdint.h>

// Test runner that only depends on base/ - no stdlib/ dependencies
// This proves that base/ is completely self-contained

// Forward declare string functions from base/mem
extern size_t strlen(const char *str);
extern char *strcpy(char *dest, const char *src);
extern void *memcpy(void *dest, const void *src, size_t n);

// Forward declare test function from test_base.c
extern void test_base(void);

int main() {
    test_base();
    return 0;
}
