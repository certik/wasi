#include <base/exit.h>
#include <base/wasi.h>
#include <base/base_io.h>

void exit(int status) {
    wasi_proc_exit(status);
}

void abort(void) {
    PRINT_ERR("abort() called");
    exit(1);
}
