#include <base/io.h>
#include <test_stdlib.h>
#include <test_base.h>

int app_main(void) {
    // Check for --test-input flag and run stdin test if present
    if (check_test_input_flag()) {
        return 0;
    }

    // Normal test suite
    test_stdlib();
    test_base();

    println(str_lit("=== All tests passed ==="));
    return 0;
}
