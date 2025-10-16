#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    printf("Assertion failed: (%s) at '%s:%u' in function '%s'\n", assertion, file, line, function);
    exit(1);
}
