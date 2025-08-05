#include <stdlib.h>
#include <wasi.h>

__attribute__((
    __import_module__("wasi_snapshot_preview1"),
    __import_name__("fd_write")
))
uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);


__attribute__((
    __import_module__("wasi_snapshot_preview1"),
    __import_name__("proc_exit")
))
void proc_exit(int status);
