#pragma once

#include <base_types.h>

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
