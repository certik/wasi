#include <base/exit.h>
#include <base/wasi.h>
#include <base/base_io.h>

void exit(int status) {
    wasi_proc_exit(status);
}

void abort(void) {
    const char *msg = "abort() called\n";
    ciovec_t iov = {.buf = msg, .buf_len = 15};
    write_all(WASI_STDOUT_FD, &iov, 1);
    exit(1);
}
