#include <base/exit.h>
#include <platform/platform.h>

void base_exit(int status) {
    wasi_proc_exit(status);
}

void base_abort(void) {
    PRINT_ERR("Aborting...");
    base_exit(1);
}
