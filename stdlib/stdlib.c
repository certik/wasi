#include <stdlib.h>

#include <wasi.h>

void exit(int status) {
    wasi_proc_exit(status);
}
