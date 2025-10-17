#include <test_base.h>
#include <base/wasi.h>
#include <base/buddy.h>
#include <base/mem.h>

// Test runner that only depends on base/ - no stdlib/ dependencies
// This proves that base/ is completely self-contained

int main() {
    // Get command line arguments to check for --test-input flag
    size_t argc, argv_buf_size;
    int ret = wasi_args_sizes_get(&argc, &argv_buf_size);

    if (ret == 0 && argc > 1) {
        char** argv = (char**)buddy_alloc(argc * sizeof(char*));
        char* argv_buf = (char*)buddy_alloc(argv_buf_size);

        if (argv && argv_buf) {
            wasi_args_get(argv, argv_buf);

            // Check if first argument is --test-input
            if (strcmp(argv[1], "--test-input") == 0) {
                test_stdin();
                buddy_free(argv);
                buddy_free(argv_buf);
                return 0;
            }

            buddy_free(argv);
            buddy_free(argv_buf);
        }
    }

    // Normal test suite
    test_base();
    return 0;
}
