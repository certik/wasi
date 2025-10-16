#include <wasi.h>
#include <base_types.h>
#include <buddy.h>

// =============================================================================
// == Linux (x86_64) Implementation
// =============================================================================

// Syscall numbers for x86_64
#define SYS_READ 0
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_LSEEK 8
#define SYS_MMAP 9
#define SYS_MPROTECT 10
#define SYS_WRITEV 20
#define SYS_EXIT 60

// mmap flags
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

// Our emulated heap state for Linux
static uint8_t* linux_heap_base = NULL;
static size_t committed_pages = 0;
static const size_t RESERVED_SIZE = 1ULL << 32; // Reserve 4GB of virtual address space

// Helper function to make a raw syscall.
static inline long syscall(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// Implementation of `fd_write` using the `writev` syscall.
uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    ssize_t ret = syscall(SYS_WRITEV, (long)fd, (long)iovs, (long)iovs_len, 0, 0, 0);
    if (ret < 0) {
        *nwritten = 0;
        return (uint32_t)-ret; // Return errno-like value
    }
    *nwritten = ret;
    return 0; // Success
}

void wasi_proc_exit(int status) {
    syscall(SYS_EXIT, (long)status, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

// Initializes the heap using mmap. We reserve large chunk of virtual
// address space but don't commit any physical memory to it initially.
static void ensure_heap_initialized() {
    if (linux_heap_base == NULL) {
        long mmap_ret = syscall(
            SYS_MMAP,
            (long)NULL,          // address hint
            (long)RESERVED_SIZE,       // size
            (long)0,                   // protection (none)
            (long)(MAP_PRIVATE | MAP_ANONYMOUS), // flags
            (long)-1,                  // file descriptor
            (long)0                    // offset
        );
        if (mmap_ret < 0) {
            linux_heap_base = NULL;
        } else {
            linux_heap_base = (uint8_t*)mmap_ret;
        }
    }
}

void* wasi_heap_base() {
    return linux_heap_base;
}


// Implementation of wasi_heap_size(). Returns committed page count.
size_t wasi_heap_size() {
    return committed_pages * WASM_PAGE_SIZE;
}

static inline uintptr_t align(uintptr_t val, uintptr_t alignment) {
  return (val + alignment - 1) & ~(alignment - 1);
}

// Implementation of wasi_heap_grow(). Commits pages using `mprotect`.
void* wasi_heap_grow(size_t num_bytes) {
    size_t num_pages = align(num_bytes, WASM_PAGE_SIZE) / WASM_PAGE_SIZE;
    if (linux_heap_base == NULL) {
        return NULL;
    }

    size_t new_total_pages = committed_pages + num_pages;
    if ((new_total_pages * WASM_PAGE_SIZE) > RESERVED_SIZE) {
        return NULL; // Cannot grow beyond reserved size
    }

    // Use mprotect to make the pages readable and writable, which commits them.
    long ret = syscall(
        SYS_MPROTECT,
        (long)(linux_heap_base + (committed_pages * WASM_PAGE_SIZE)),
        (long)(num_pages * WASM_PAGE_SIZE),
        (long)(PROT_READ | PROT_WRITE),
        0, 0, 0
    );

    if (ret != 0) {
        return NULL; // mprotect failed
    }

    void* old_top = linux_heap_base + (committed_pages * WASM_PAGE_SIZE);
    committed_pages = new_total_pages;
    return old_top;
}

// Forward declaration for main
int main();

// Linux open() flags
#define O_RDONLY   0x0000
#define O_WRONLY   0x0001
#define O_RDWR     0x0002
#define O_CREAT    0x0100
#define O_TRUNC    0x0200

// File I/O implementations
wasi_fd_t wasi_path_open(const char* path, int flags) {
    // Map WASI flags to Linux flags
    int os_flags = 0;
    if ((flags & WASI_O_RDWR) == WASI_O_RDWR) {
        os_flags |= O_RDWR;
    } else if (flags & WASI_O_WRONLY) {
        os_flags |= O_WRONLY;
    } else {
        os_flags |= O_RDONLY;
    }

    if (flags & WASI_O_CREAT) os_flags |= O_CREAT;
    if (flags & WASI_O_TRUNC) os_flags |= O_TRUNC;

    // Default mode for created files (0644)
    long result = syscall(SYS_OPEN, (long)path, (long)os_flags, (long)0644, 0, 0, 0);
    return (wasi_fd_t)result;
}

int wasi_fd_close(wasi_fd_t fd) {
    long result = syscall(SYS_CLOSE, (long)fd, 0, 0, 0, 0, 0);
    return (int)result;
}

int64_t wasi_fd_read(wasi_fd_t fd, void* buf, size_t len) {
    long result = syscall(SYS_READ, (long)fd, (long)buf, (long)len, 0, 0, 0);
    return (int64_t)result;
}

int64_t wasi_fd_seek(wasi_fd_t fd, int64_t offset, int whence) {
    long result = syscall(SYS_LSEEK, (long)fd, (long)offset, (long)whence, 0, 0, 0);
    return (int64_t)result;
}

int64_t wasi_fd_tell(wasi_fd_t fd) {
    long result = syscall(SYS_LSEEK, (long)fd, 0, (long)WASI_SEEK_CUR, 0, 0, 0);
    return (int64_t)result;
}

// The entry point for a -nostdlib Linux program is `_start`.
void _start() {
    ensure_heap_initialized();
    buddy_init();
    int status = main();
    wasi_proc_exit(status);
}
