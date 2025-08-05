#pragma once

#include <stddef.h>

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

// Special WASM instructions for memory handling
// The memory is organized as follows:
// WASM with Clang:
// [reserved] [data] [stack] [heap                    ]
//                           ^ __heap_base             ^ heap_size()
//
// In WASM all of this is part of the samme linear contiguous memory. The
// __heap_base is created by wasm-ld (might not be at a page boundary?), and
// heap_size() starts at a page boundary.
//
// Linux with Clang:
// [reserved] [data] [...] [heap                    ]                 [stack]
//                         ^ __heap_base             ^ heap_size()
// The [...] are other ELF sections, and any possible shared libraries, maybe
// also some unused space. The [stack] starts at the highest virtual address
// and grows down. The [heap] is our own memory reserved to 4GB via mmap, and
// we commit to more pages when `heap_grow()` is called. The __heap_base is
// the initial pointer returned by mmap.
// There will be regions which are not reserved (will segfault) both before
// heap and after heap.
//
// macOS and Windows likely work in a similar way.
//
// On all platforms the heap size is thus computed as `heap_size() -
// __heap_base`. It is not guaranteed that all addresses below __heap_base are
// addressable.
// The __heap_base is at a page boundary on native platforms, but not in WASM.

void* heap_base();

#define WASM_PAGE_SIZE 65536 // 64KiB
// memory.grow WASM instruction
// Returns the pointer to the new region (equal to the last `heap_base()+heap_size()`)
// Accepts the number of bytes (not pages) to grow
void* heap_grow(size_t num_bytes);

// Returns the size of the heap in bytes
// Computed using the memory.size WASM instruction minus heap_base()
size_t heap_size();


// WASI import functions

typedef struct ciovec_s {
    const void* buf;
    size_t buf_len;
} ciovec_t;

uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);

void proc_exit(int status);
