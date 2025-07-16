#pragma once

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
//                           ^ __heap_base             ^ memory_size()
//
// In WASM all of this is part of the samme linear contiguous memory. The
// __heap_base is created by wasm-ld (might not be at a page boundary?), and
// memory_size() starts at a page boundary.
//
// Linux with Clang:
// [reserved] [data] [...] [heap                    ]                 [stack]
//                         ^ __heap_base             ^ memory_size()
// The [...] are other ELF sections, and any possible shared libraries, maybe
// also some unused space. The [stack] starts at the highest virtual address
// and grows down. The [heap] is our own memory reserved to 4GB via mmap, and
// we commit to more pages when `memory_grow()` is called. The __heap_base is
// the initial pointer returned by mmap.
// There will be regions which are not reserved (will segfault) both before
// heap and after heap.
//
// macOS and Windows likely work in a similar way.
//
// On all platforms the heap size is thus computed as `memory_size() -
// __heap_base`. It is not guaranteed that all addresses below __heap_base are
// addressable.
// TODO: always define this on all platforms as a variable.
// extern void* __heap_base;
#define WASM_PAGE_SIZE 65536 // 64KiB, the page size used by memory_grow().
// memory.grow WASM instruction
void* memory_grow(size_t num_pages);

// memory.size WASM instruction
// Returns the total size of memory as a position (pointer) of the last
// allocated byte plus one. TODO: Might return it in page size, need to check.
// Should probably return in bytes, to make it platform agnostic.
// memory_size() is always at a page boundary at all times on all platforms.
// The __heap_base however is at a page boundary on native platforms, but maybe
// not in WASM, need to check.
size_t memory_size(void);


// WASI import functions

typedef struct ciovec_s {
    const void* buf;
    size_t buf_len;
} ciovec_t;

uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);

void proc_exit(int status);
