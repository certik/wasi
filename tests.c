#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <test_stdlib.h>
#include <test_base.h>
#include <base/wasi.h>
#include <base/buddy.h>

int main(void) {
    // Check for --test-input flag and run stdin test if present
    if (check_test_input_flag()) {
        return 0;
    }

    // Normal test suite
    test_stdlib();
    test_base();

    printf("=== All tests passed ===\n");
    return 0;
}
