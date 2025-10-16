#include <base/exit.h>
#include <base/wasi.h>
#include <base/printf.h>

void exit(int status) {
    wasi_proc_exit(status);
}

void abort(void) {
    printf("abort() called\n");
    exit(1);
}
