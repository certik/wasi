#include <test_base.h>
#include <base/wasi.h>
#include <base/buddy.h>
#include <base/mem.h>

// Test runner that only depends on base/ - no stdlib/ dependencies
// This proves that base/ is completely self-contained

int main() {
    // Check for --test-input flag and run stdin test if present
    if (check_test_input_flag()) {
        return 0;
    }

    // Normal test suite
    test_base();
    return 0;
}
