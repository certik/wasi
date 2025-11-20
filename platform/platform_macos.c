#include <platform.h>
#include <base_types.h>
#include <buddy.h>

// =============================================================================
// == macOS Implementation
// =============================================================================

// Define off_t as long long (macOS uses 64-bit off_t).
typedef long long off_t;

// Define struct iovec since we can't include headers
struct iovec {
    void *iov_base;
    size_t iov_len;
};

// Extern declarations for libSystem functions.
extern void* mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
extern int mprotect(void *addr, size_t len, int prot);
extern ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
extern void _exit(int status);
extern int * __error(); // Returns pointer to errno
extern int open(const char *path, int flags, ...);
extern int close(int fd);
extern int dup(int fd);
extern int dup2(int oldfd, int newfd);
extern int fcntl(int fd, int cmd, ...);
extern ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
extern off_t lseek(int fd, off_t offset, int whence);

// Protection and mapping flags (macOS-specific values)
#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02

#define MAP_PRIVATE 0x0002
#define MAP_ANONYMOUS 0x1000  // Different from Linux

// fcntl commands
#define F_DUPFD 0

// Emulated heap state for macOS.
static uint8_t* linux_heap_base = NULL; // Reuse name for consistency
static size_t committed_pages = 0;
static const size_t RESERVED_SIZE = 1ULL << 32; // 4GB virtual space

// Command line arguments storage
static int stored_argc = 0;
static char** stored_argv = NULL;

void ensure_heap_initialized() {
    if (linux_heap_base == NULL) {
        linux_heap_base = (uint8_t*)mmap(
            NULL,
            RESERVED_SIZE,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );
        if (linux_heap_base == (void*)-1) {
            // TODO: abort here if we cannot reserve the memory
            linux_heap_base = NULL;
        }
    }
}

// Implementation of fd_write using writev.
uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    ssize_t ret = writev(fd, (const struct iovec *)iovs, (int)iovs_len);
    if (ret < 0) {
        *nwritten = 0;
        return (uint32_t)*__error(); // Get errno value
    }
    *nwritten = (size_t)ret;
    return 0;
}

void wasi_proc_exit(int status) {
    _exit(status);
}

void* wasi_heap_base() {
    return linux_heap_base;
}

size_t wasi_heap_size() {
    return committed_pages * WASM_PAGE_SIZE;
}

static inline uintptr_t align(uintptr_t val, uintptr_t alignment) {
  return (val + alignment - 1) & ~(alignment - 1);
}

// Implementation of wasi_heap_grow using mprotect to commit pages.
void* wasi_heap_grow(size_t num_bytes) {
    size_t num_pages = align(num_bytes, WASM_PAGE_SIZE) / WASM_PAGE_SIZE;
    if (linux_heap_base == NULL) {
        return NULL;
    }

    size_t new_total_pages = committed_pages + num_pages;
    if ((new_total_pages * WASM_PAGE_SIZE) > RESERVED_SIZE) {
        return NULL;
    }

    int ret = mprotect(
        (void*)(linux_heap_base + (committed_pages * WASM_PAGE_SIZE)),
        num_pages * WASM_PAGE_SIZE,
        PROT_READ | PROT_WRITE
    );

    if (ret != 0) {
        return NULL;
    }

    void* old_top = (void*)(linux_heap_base + (committed_pages * WASM_PAGE_SIZE));
    committed_pages = new_total_pages;
    return old_top;
}

// Forward declaration for application entry point
int app_main();

// macOS open() flags
#define O_RDONLY   0x0000
#define O_WRONLY   0x0001
#define O_RDWR     0x0002
#define O_CREAT    0x0200
#define O_TRUNC    0x0400

// File I/O implementations
wasi_fd_t wasi_path_open(const char* path, size_t path_len, uint64_t rights, int oflags) {
    // Extract access mode from rights
    int os_flags = 0;
    int has_read = (rights & WASI_RIGHT_FD_READ) != 0;
    int has_write = (rights & WASI_RIGHT_FD_WRITE) != 0;

    if (has_read && has_write) {
        os_flags |= O_RDWR;
    } else if (has_write) {
        os_flags |= O_WRONLY;
    } else {
        os_flags |= O_RDONLY;
    }

    // Map oflags to macOS creation flags
    if (oflags & WASI_O_CREAT) os_flags |= O_CREAT;
    if (oflags & WASI_O_TRUNC) os_flags |= O_TRUNC;

    // Default mode for created files (0644)
    int fd = open(path, os_flags, 0644);

    // On macOS, open() can return 0, 1, or 2 if those file descriptors were closed.
    // This would collide with stdin/stdout/stderr. To prevent this, we use fcntl()
    // with F_DUPFD to find the lowest available FD >= 3, then close the original low FD.
    // See: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/open.2.html
    // "The file descriptor returned by a successful call will be the lowest-numbered
    //  file descriptor not currently open for the process."
    // See: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fcntl.2.html
    // "F_DUPFD: Find the lowest numbered available file descriptor greater than or
    //  equal to arg and make it be a copy of fd."
    if (fd >= 0 && fd <= WASI_STDERR_FD) {
        // Use fcntl with F_DUPFD to get lowest available FD >= 3
        int new_fd = fcntl(fd, F_DUPFD, 3);
        if (new_fd < 0) {
            // fcntl failed, close the original FD and return error
            close(fd);
            return -1;
        }
        // Verify new_fd is not a reserved FD (should be > 2)
        if (new_fd <= WASI_STDERR_FD) {
            // This should never happen, but if it does, close both and fail
            close(new_fd);
            close(fd);
            return -1;
        }
        close(fd);
        fd = new_fd;
    }

    return fd;
}

int wasi_fd_close(wasi_fd_t fd) {
    int result = close(fd);
    return (result < 0) ? *__error() : 0;
}

int wasi_fd_read(wasi_fd_t fd, const iovec_t* iovs, size_t iovs_len, size_t* nread) {
    ssize_t result = readv(fd, (const struct iovec*)iovs, (int)iovs_len);
    if (result < 0) {
        *nread = 0;
        return *__error();  // Return errno
    }
    *nread = (size_t)result;
    return 0;  // Success
}

int wasi_fd_seek(wasi_fd_t fd, int64_t offset, int whence, uint64_t* newoffset) {
    off_t result = lseek(fd, (off_t)offset, whence);
    if (result < 0) {
        *newoffset = 0;
        return *__error();  // Return errno
    }
    *newoffset = (uint64_t)result;
    return 0;  // Success
}

int wasi_fd_tell(wasi_fd_t fd, uint64_t* offset) {
    off_t result = lseek(fd, 0, WASI_SEEK_CUR);
    if (result < 0) {
        *offset = 0;
        return *__error();  // Return errno
    }
    *offset = (uint64_t)result;
    return 0;  // Success
}

// Command line arguments implementation
int wasi_args_sizes_get(size_t* argc, size_t* argv_buf_size) {
    *argc = (size_t)stored_argc;

    // Calculate total buffer size needed
    size_t total_size = 0;
    for (int i = 0; i < stored_argc; i++) {
        const char* arg = stored_argv[i];
        while (*arg++) total_size++;  // strlen
        total_size++;  // null terminator
    }
    *argv_buf_size = total_size;
    return 0;
}

int wasi_args_get(char** argv, char* argv_buf) {
    char* buf_ptr = argv_buf;
    for (int i = 0; i < stored_argc; i++) {
        argv[i] = buf_ptr;
        const char* src = stored_argv[i];
        while (*src) {
            *buf_ptr++ = *src++;
        }
        *buf_ptr++ = '\0';
    }
    return 0;
}

// Initialize the platform and call the application
static int platform_init_and_run(int argc, char** argv) {
    stored_argc = argc;
    stored_argv = argv;
    ensure_heap_initialized();
    buddy_init();
    int status = app_main();
    return status;
}

#ifdef PLATFORM_USE_EXTERNAL_STDLIB
// When using external stdlib, define main() which will be called by libc
int main(int argc, char** argv) {
    return platform_init_and_run(argc, argv);
}
#else
// Entry point for macOS.
// macOS passes argc and argv to the entry point (unlike raw Linux)
void _start(int argc, char** argv) {
    int status = platform_init_and_run(argc, argv);
    wasi_proc_exit(status);
}
#endif
