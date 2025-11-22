#ifndef FPS_PLATFORM_H
#define FPS_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Memory Handling
void* wasi_heap_base(void);
size_t wasi_heap_size(void);
void* wasi_heap_grow(size_t num_bytes);

// I/O vector types
typedef struct ciovec_s {
    const void* buf;
    size_t buf_len;
} ciovec_t;

typedef struct iovec_s {
    void* iov_base;
    size_t iov_len;
} iovec_t;

// File descriptor type - opaque handle to an open file
typedef int wasi_fd_t;

// Rights and flags (simplified)
#define WASI_RIGHT_FD_READ   0x2
#define WASI_RIGHT_FD_WRITE  0x40
#define WASI_RIGHT_FD_SEEK   0x4
#define WASI_RIGHT_FD_TELL   0x20
#define WASI_RIGHTS_READ  (WASI_RIGHT_FD_READ | WASI_RIGHT_FD_SEEK | WASI_RIGHT_FD_TELL)
#define WASI_RIGHTS_WRITE (WASI_RIGHT_FD_WRITE | WASI_RIGHT_FD_SEEK | WASI_RIGHT_FD_TELL)
#define WASI_RIGHTS_RDWR  (WASI_RIGHTS_READ | WASI_RIGHTS_WRITE)
#define WASI_O_CREAT   0x1
#define WASI_O_TRUNC   0x8
#define WASI_SEEK_SET 0
#define WASI_SEEK_CUR 1
#define WASI_SEEK_END 2
#define WASI_STDIN_FD  0
#define WASI_STDOUT_FD 1
#define WASI_STDERR_FD 2

// FD operations (mock implementations provided)
uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);
int wasi_fd_read(wasi_fd_t fd, const iovec_t* iovs, size_t iovs_len, size_t* nread);
int wasi_fd_seek(wasi_fd_t fd, int64_t offset, int whence, uint64_t* newoffset);
int wasi_fd_tell(wasi_fd_t fd, uint64_t* offset);
int wasi_fd_close(wasi_fd_t fd);
wasi_fd_t wasi_path_open(const char* path, size_t path_len, uint64_t rights, int oflags);

// Args
int wasi_args_sizes_get(size_t* argc, size_t* argv_buf_size);
int wasi_args_get(char** argv, char* argv_buf);

// Process exit
void wasi_proc_exit(int status);

// Platform init
void platform_init(int argc, char** argv);

// Math
double fast_sqrt(double x);
float fast_sqrtf(float x);

// File mapping (mock)
bool platform_read_file_mmap(const char *filename, uint64_t *out_handle, void **out_data, size_t *out_size);
void platform_file_unmap(uint64_t handle);

#endif // FPS_PLATFORM_H
