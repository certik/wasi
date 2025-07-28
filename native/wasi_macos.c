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

// Protection and mapping flags (macOS-specific values)
#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02

#define MAP_PRIVATE 0x0002
#define MAP_ANONYMOUS 0x1000  // Different from Linux

// Emulated heap state for macOS.
static uint8_t* linux_heap_base = NULL; // Reuse name for consistency
static size_t committed_pages = 0;
static const size_t RESERVED_SIZE = 1ULL << 32; // 4GB virtual space

static void ensure_heap_initialized() {
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

// Emulation of fd_write using writev.
uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    ssize_t ret = writev(fd, (const struct iovec *)iovs, (int)iovs_len);
    if (ret < 0) {
        *nwritten = 0;
        return (uint32_t)*__error(); // Get errno value
    }
    *nwritten = (size_t)ret;
    return 0;
}

void proc_exit(int status) {
    _exit(status);
}

void* heap_base() {
    return linux_heap_base;
}

// Emulation of memory_size.
void* memory_size() {
    return (void*)(linux_heap_base + (committed_pages * WASM_PAGE_SIZE));
}

// Emulation of memory_grow using mprotect to commit pages.
void* memory_grow(size_t num_pages) {
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

// Forward declaration for main
int main();

// Entry point for macOS.
void _start() {
    ensure_heap_initialized();
    int status = main();
    proc_exit(status);
}
