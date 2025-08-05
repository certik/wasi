#include <stdlib.h>
#include <wasi.h>

#define WASI(name) __attribute__((__import_module__("wasi_snapshot_preview1"), __import_name__(#name))) name

uint32_t WASI(fd_write)(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);
void WASI(proc_exit)(int status);

#undef WASI
