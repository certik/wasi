#include "fps_platform.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static char dummy_heap[1024 * 16];
static size_t dummy_heap_used = 0;

void* wasi_heap_base(void) {
    return dummy_heap;
}

size_t wasi_heap_size(void) {
    return sizeof(dummy_heap);
}

void* wasi_heap_grow(size_t num_bytes) {
    // Grow within dummy buffer for the mock; return NULL if out of space.
    if (dummy_heap_used + num_bytes > sizeof(dummy_heap)) {
        return NULL;
    }
    void *ptr = dummy_heap + dummy_heap_used;
    dummy_heap_used += num_bytes;
    return ptr;
}

uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    size_t total = 0;
    for (size_t i = 0; i < iovs_len; i++) {
        fwrite(iovs[i].buf, 1, iovs[i].buf_len, fd == WASI_STDERR_FD ? stderr : stdout);
        total += iovs[i].buf_len;
    }
    if (nwritten) *nwritten = total;
    return 0;
}

int wasi_fd_read(wasi_fd_t fd, const iovec_t* iovs, size_t iovs_len, size_t* nread) {
    size_t total = 0;
    for (size_t i = 0; i < iovs_len; i++) {
        size_t r = fread(iovs[i].iov_base, 1, iovs[i].iov_len, fd == WASI_STDIN_FD ? stdin : stdin);
        total += r;
    }
    if (nread) *nread = total;
    return 0;
}

int wasi_fd_seek(wasi_fd_t fd, int64_t offset, int whence, uint64_t* newoffset) {
    FILE *f = fd == WASI_STDERR_FD ? stderr : fd == WASI_STDOUT_FD ? stdout : stdin;
    if (fseek(f, (long)offset, whence) != 0) return -1;
    long pos = ftell(f);
    if (newoffset) *newoffset = (uint64_t)pos;
    return 0;
}

int wasi_fd_tell(wasi_fd_t fd, uint64_t* offset) {
    FILE *f = fd == WASI_STDERR_FD ? stderr : fd == WASI_STDOUT_FD ? stdout : stdin;
    long pos = ftell(f);
    if (pos < 0) return -1;
    if (offset) *offset = (uint64_t)pos;
    return 0;
}

int wasi_fd_close(wasi_fd_t fd) {
    (void)fd;
    return 0;
}

wasi_fd_t wasi_path_open(const char* path, size_t path_len, uint64_t rights, int oflags) {
    (void)path_len;
    (void)rights;
    (void)oflags;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    return fileno(f);
}

int wasi_args_sizes_get(size_t* argc, size_t* argv_buf_size) {
    if (argc) *argc = 0;
    if (argv_buf_size) *argv_buf_size = 0;
    return 0;
}

int wasi_args_get(char** argv, char* argv_buf) {
    (void)argv;
    (void)argv_buf;
    return 0;
}

void wasi_proc_exit(int status) {
    exit(status);
}

void platform_init(int argc, char** argv) {
    (void)argc;
    (void)argv;
}

double fast_sqrt(double x) {
    return sqrt(x);
}

float fast_sqrtf(float x) {
    return (float)sqrt((double)x);
}

bool platform_read_file_mmap(const char *filename, uint64_t *out_handle, void **out_data, size_t *out_size) {
    (void)filename;
    (void)out_handle;
    (void)out_data;
    (void)out_size;
    return false;
}

void platform_file_unmap(uint64_t handle) {
    (void)handle;
}
