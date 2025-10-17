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
#define SYS_READV 19
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

// Command line arguments storage
static int stored_argc = 0;
static char** stored_argv = NULL;

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
    return (result < 0) ? (int)(-result) : 0;
}

int wasi_fd_read(wasi_fd_t fd, const iovec_t* iovs, size_t iovs_len, size_t* nread) {
    long result = syscall(SYS_READV, (long)fd, (long)iovs, (long)iovs_len, 0, 0, 0);
    if (result < 0) {
        *nread = 0;
        return (int)(-result);  // Return errno
    }
    *nread = (size_t)result;
    return 0;  // Success
}

int wasi_fd_seek(wasi_fd_t fd, int64_t offset, int whence, uint64_t* newoffset) {
    long result = syscall(SYS_LSEEK, (long)fd, (long)offset, (long)whence, 0, 0, 0);
    if (result < 0) {
        *newoffset = 0;
        return (int)(-result);  // Return errno
    }
    *newoffset = (uint64_t)result;
    return 0;  // Success
}

int wasi_fd_tell(wasi_fd_t fd, uint64_t* offset) {
    long result = syscall(SYS_LSEEK, (long)fd, 0, (long)WASI_SEEK_CUR, 0, 0, 0);
    if (result < 0) {
        *offset = 0;
        return (int)(-result);  // Return errno
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

// The entry point for a -nostdlib Linux program is `_start`.
// The kernel enters with RSP % 16 == 0, but the ABI requires RSP % 16 == 8
// before a call instruction (so after the call pushes return address, it's aligned).
// We need to ensure proper 16-byte stack alignment for functions using SSE instructions.
// On entry, the stack layout is:
//   rsp+0: argc
//   rsp+8: argv[0]
//   rsp+16: argv[1]
//   ...
__attribute__((naked))
void _start() {
    __asm__ volatile (
        "xor %rbp, %rbp\n"           // Clear frame pointer as per ABI
        "mov (%rsp), %rdi\n"         // argc from stack to first argument (rdi)
        "lea 8(%rsp), %rsi\n"        // argv from stack+8 to second argument (rsi)
        "andq $-16, %rsp\n"          // Align stack to 16 bytes
        "call _start_c\n"            // Call the C portion
        "mov %eax, %edi\n"           // Move return value to exit code
        "mov $60, %eax\n"            // SYS_EXIT
        "syscall\n"                  // Exit
        "hlt\n"                      // Should never reach here
    );
}

// The actual C entry point
int _start_c(int argc, char** argv) {
    stored_argc = argc;
    stored_argv = argv;
    ensure_heap_initialized();
    buddy_init();
    int status = main();
    return status;
}
