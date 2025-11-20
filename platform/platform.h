#pragma once

#include <base/base_types.h>

/*
 * The following functions provide a C interface to WASM / WASI.
 *
 * The C compiler compiles any general C code into WASM, but then we need a few
 * API calls that are not provided by C itself, such as WASM memory management
 * and WASI external calls, those are provided in this header file as
 * interfaces.
 *
 * This file is the only way a C program can communicate with the system.
 * This API is platform independent and everything else is built on top in a
 * platform-independent way.
 *
 * The implementation of these functions is then done in a platform-specific
 * way. For WASM we use Clang special intrinsic functions and attributes to
 * direct Clang to generate a proper WASM instruction or external function. For
 * native platforms we use the officially supported API of the system kernel
 * directly as follows: 
 *   * Linux: syscalls
 *   * macOS: the `libSystem.dylib` shared library
 *   * Windows: the `kernel32.dll` shared library
 */

// Memory Handling
//
// The memory is organized as follows:
// WASM with Clang:
// [reserved] [data] [stack] [ heap of wasi_heap_size() bytes ]
//                           ^ wasi_heap_base()
//
// In WASM all of this is part of the samme linear contiguous memory. The
// __heap_base is created by wasm-ld (not be at a page boundary), and
// wasi_heap_base()+wasi_heap_size() starts at a page boundary.
//
// Linux with Clang:
// [reserved] [data] [...] [ heap of wasi_heap_size() bytes ]         [stack]
//                         ^ wasi_heap_base()
// The [...] are other ELF sections, and any possible shared libraries, maybe
// also some unused space. The [stack] starts at the highest virtual address
// and grows down. The [heap] is our own memory reserved to 4GB via mmap, and
// we commit to more pages when `heap_grow()` is called. The wasi_heap_base()
// is the initial pointer returned by mmap.
// There will be regions which are not reserved (will segfault) both before
// heap and after heap.
//
// macOS and Windows work in a similar way.
//
// It is not guaranteed that all addresses below wasi_heap_base() are
// addressable. The wasi_heap_base() is at a page boundary on native platforms,
// but not in WASM.

#define WASM_PAGE_SIZE 65536 // 64 KiB

// Returns a pointer to the base of the heap. The base might not be at a system
// page boundary.
void* wasi_heap_base();

// Returns the size of the heap in bytes. The heap size is not in general a
// multiple of a page size, because the heap base might not lie on a page
// boundary.
size_t wasi_heap_size();

// Grows the heap by `num_bytes` bytes. If not multiple of a system-dependent
// page size (not necessarily equal to WASM_PAGE_SIZE, although on most systems
// WASM_PAGE_SIZE is usually a multiple of the system page size), it will round
// up to an even multiple of system page size.
// Returns the pointer to the new region (equal to the last
// `wasi_heap_base()+wasi_heap_size()`)
void* wasi_heap_grow(size_t num_bytes);



// Write multiple buffers to the file descriptor.
typedef struct ciovec_s {
    const void* buf;
    size_t buf_len;
} ciovec_t;
uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);


// Terminate the process with exit code `status`
void wasi_proc_exit(int status);


// File I/O
//
// File descriptor type - opaque handle to an open file
typedef int wasi_fd_t;

// I/O vector for scatter-gather operations (read buffers)
typedef struct iovec_s {
    void* iov_base;
    size_t iov_len;
} iovec_t;

// WASI rights flags (capabilities for file operations)
#define WASI_RIGHT_FD_READ   0x2   // __WASI_RIGHTS_FD_READ (1 << 1)
#define WASI_RIGHT_FD_WRITE  0x40  // __WASI_RIGHTS_FD_WRITE (1 << 6)
#define WASI_RIGHT_FD_SEEK   0x4   // __WASI_RIGHTS_FD_SEEK (1 << 2)
#define WASI_RIGHT_FD_TELL   0x20  // __WASI_RIGHTS_FD_TELL (1 << 5)

// Common rights combinations
#define WASI_RIGHTS_READ  (WASI_RIGHT_FD_READ | WASI_RIGHT_FD_SEEK | WASI_RIGHT_FD_TELL)
#define WASI_RIGHTS_WRITE (WASI_RIGHT_FD_WRITE | WASI_RIGHT_FD_SEEK | WASI_RIGHT_FD_TELL)
#define WASI_RIGHTS_RDWR  (WASI_RIGHTS_READ | WASI_RIGHTS_WRITE)

// File creation flags (WASI oflags - passed through directly)
#define WASI_O_CREAT   0x1  // __WASI_OFLAGS_CREAT (1 << 0)
#define WASI_O_TRUNC   0x8  // __WASI_OFLAGS_TRUNC (1 << 3)

// Seek whence values
#define WASI_SEEK_SET 0
#define WASI_SEEK_CUR 1
#define WASI_SEEK_END 2

// Special file descriptors
#define WASI_STDIN_FD  0
#define WASI_STDOUT_FD 1
#define WASI_STDERR_FD 2

// Open a file at the given path with the specified rights and creation flags.
// rights: combination of WASI_RIGHTS_READ, WASI_RIGHTS_WRITE, or WASI_RIGHTS_RDWR
// oflags: combination of WASI_O_CREAT, WASI_O_TRUNC (passed directly to WASI)
// Returns a file descriptor on success, or -1 on error.
wasi_fd_t wasi_path_open(const char* path, size_t path_len, uint64_t rights, int oflags);

// Close a file descriptor.
// Returns 0 on success, or errno on error.
int wasi_fd_close(wasi_fd_t fd);

// Read from a file descriptor using scatter-gather I/O.
// Returns 0 on success with bytes read in *nread, or errno on error.
int wasi_fd_read(wasi_fd_t fd, const iovec_t* iovs, size_t iovs_len, size_t* nread);

// Seek to a position in the file.
// Returns 0 on success with new position in *newoffset, or errno on error.
int wasi_fd_seek(wasi_fd_t fd, int64_t offset, int whence, uint64_t* newoffset);

// Get the current position in the file.
// Returns 0 on success with position in *offset, or errno on error.
int wasi_fd_tell(wasi_fd_t fd, uint64_t* offset);


// Command Line Arguments
//
// Get the sizes of the command line arguments.
// Returns 0 on success with:
//   *argc: number of arguments
//   *argv_buf_size: total size needed to store all argument strings (including null terminators)
// Returns errno on error.
int wasi_args_sizes_get(size_t* argc, size_t* argv_buf_size);

// Get the command line arguments.
// Parameters:
//   argv: array of pointers to be filled with argument string pointers
//   argv_buf: buffer to store the actual argument strings
// The caller must allocate:
//   - argv array of at least argc pointers
//   - argv_buf buffer of at least argv_buf_size bytes
// Returns 0 on success, or errno on error.
int wasi_args_get(char** argv, char* argv_buf);


//=============================================================================
// Platform Initialization
//=============================================================================
//
// Initialize the platform runtime (heap, buddy allocator, command line args).
//
// USAGE PATTERNS:
//
// 1. When PLATFORM_SKIP_ENTRY is DEFINED (platform skips entry point):
//    - The platform does NOT provide _start or any entry point implementation
//    - You MUST provide your own entry point (main, SDL_main, etc.)
//    - You MUST call platform_init(argc, argv) manually in your entry point
//    - Do NOT implement app_main()
//    - Example: SDL apps call this in SDL_AppInit()
//
// 2. When PLATFORM_SKIP_ENTRY is NOT defined (platform provides entry point):
//    - The platform provides _start which calls platform_init() then app_main()
//    - You MUST implement app_main()
//    - Do NOT call platform_init() - it's called automatically by _start
//
// Parameters:
//   argc: argument count (may be 0 for platforms without argc/argv)
//   argv: argument vector (may be NULL for platforms without argc/argv)
//
void platform_init(int argc, char** argv);


// Math Functions
//
// Square root functions using platform-specific builtins
double fast_sqrt(double x);
float fast_sqrtf(float x);
