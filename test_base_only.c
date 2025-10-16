#include <test_base.h>

// Test runner that only depends on base/ - no stdlib/ dependencies
// This proves that base/ is completely self-contained

int main() {
    test_base();
    return 0;
}
