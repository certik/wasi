#include <platform.h>
#include <base_types.h>
#include <buddy.h>

#define WASI(name) __attribute__((__import_module__("wasi_snapshot_preview1"), __import_name__(#name))) name

uint32_t WASI(fd_write)(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);
void WASI(proc_exit)(int status);
int WASI(path_open)(int dirfd, int dirflags, const char* path, size_t path_len, int oflags, uint64_t fs_rights_base, uint64_t fs_rights_inheriting, int fdflags, int* fd);
int WASI(fd_close)(int fd);
int WASI(fd_read)(int fd, const iovec_t* iovs, size_t iovs_len, size_t* nread);
int WASI(fd_seek)(int fd, int64_t offset, int whence, uint64_t* newoffset);
int WASI(fd_tell)(int fd, uint64_t* offset);
int WASI(args_sizes_get)(size_t* argc, size_t* argv_buf_size);
int WASI(args_get)(char** argv, char* argv_buf);

#undef WASI

// =============================================================================
// == WebAssembly (WASI) Implementation
// =============================================================================

// __heap_base is a special symbol provided by the wasm-ld linker. It marks
// the end of the static data section and the beginning of the linear memory
// heap that we can manage. It is declared as an external variable.
//extern uint8_t __heap_base;

// Wrapper around the `memory.size` WASM instruction.
// The argument `0` is required for the current memory space.
// Returns the pointer to the last allocated byte plus one.
size_t wasi_heap_size() {
    return WASM_PAGE_SIZE * __builtin_wasm_memory_size(0)
        - (size_t)wasi_heap_base();
}

static inline uintptr_t align(uintptr_t val, uintptr_t alignment) {
  return (val + alignment - 1) & ~(alignment - 1);
}

// Wrapper around the `memory.grow` WASM instruction.
// Attempts to grow the linear memory by `num_pages`.
// Returns the previous size in pages on success, or -1 on failure.
void* wasi_heap_grow(size_t num_bytes) {
    size_t num_pages = align(num_bytes, WASM_PAGE_SIZE) / WASM_PAGE_SIZE;
    size_t prev_size = __builtin_wasm_memory_grow(0, num_pages);
    if (prev_size == (size_t)(-1)) {
        return NULL;
    }
    return (void*)(prev_size * WASM_PAGE_SIZE);
}


extern uint8_t* __heap_base;

void* wasi_heap_base() {
    return &__heap_base;
}

void wasi_proc_exit(int status) {
    proc_exit(status);
}

uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    return fd_write(fd, iovs, iovs_len, nwritten);
}

// File I/O implementations
wasi_fd_t wasi_path_open(const char* path, size_t path_len, uint64_t rights, int oflags) {
    // WASI requires path_open to be called with a directory fd (use 3 for preopen)
    // We simplify by using the preopen directory
    int fd = -1;

    // Both rights and oflags are passed through directly (no translation needed)
    int ret = path_open(
        3,           // dirfd (preopen)
        0,           // dirflags
        path,
        path_len,
        oflags,      // passed through directly
        rights,      // passed through directly
        0,           // inheriting rights
        0,           // fdflags
        &fd
    );

    // Note: The WASI specification reserves file descriptors 0, 1, and 2 for
    // stdin, stdout, and stderr. The path_open function will never return these
    // values, so there's no collision risk with standard streams.
    // See: https://github.com/WebAssembly/WASI/blob/main/legacy/preview1/docs.md
    // "File descriptors 0, 1, and 2 are always reserved for stdin, stdout, and stderr."
    return (ret == 0) ? fd : -1;
}

int wasi_fd_close(wasi_fd_t fd) {
    return fd_close(fd);
}

int wasi_fd_read(wasi_fd_t fd, const iovec_t* iovs, size_t iovs_len, size_t* nread) {
    return fd_read(fd, iovs, iovs_len, nread);
}

int wasi_fd_seek(wasi_fd_t fd, int64_t offset, int whence, uint64_t* newoffset) {
    return fd_seek(fd, offset, whence, newoffset);
}

int wasi_fd_tell(wasi_fd_t fd, uint64_t* offset) {
    return fd_tell(fd, offset);
}

// Command line arguments implementation
int wasi_args_sizes_get(size_t* argc, size_t* argv_buf_size) {
    return args_sizes_get(argc, argv_buf_size);
}

int wasi_args_get(char** argv, char* argv_buf) {
    return args_get(argv, argv_buf);
}

// Forward declaration for application entry point
int app_main();

void ensure_heap_initialized() {
}

// Initialize the platform and call the application
static void platform_init_and_run() {
    buddy_init();
    int status = app_main();
    wasi_proc_exit(status);
}

#ifdef PLATFORM_USE_EXTERNAL_STDLIB
// When using external stdlib, define main() which will be called by libc
int main() {
    platform_init_and_run();
    return 0;  // Never reached
}
#else
// For WASI, the entry point is `_start`
void _start() {
    platform_init_and_run();
}
#endif
