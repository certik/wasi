/**
 * @file standalone_arena.c
 * @brief A simple, standalone C program with an Arena allocator.
 *
 * This program does not link against the standard C library (`-nostdlib`).
 * It provides two implementations chosen at compile time:
 * 1. WebAssembly (WASM) using WASI system calls.
 * 2. Linux using raw syscalls.
 *
 * The program implements an Arena allocator on top of a heap that is managed
 * either by the WASM runtime or by `mmap` on Linux. It then allocates a few
 * strings onto the arena and prints them to stdout using the `fd_write` syscall.
 *
 * --- Compilation Instructions ---
 *
 * For WebAssembly (WASI):
 * clang --target=wasm32-wasi -nostdlib -Wl,--no-entry -Wl,--export=__heap_base -Wl,--initial-memory=131072 -o arena.wasm standalone_arena.c
 *
 * To Run with wasmtime:
 * wasmtime arena.wasm
 *
 * For Linux (x86_64):
 * gcc -nostdlib -o arena_linux standalone_arena.c
 *
 * To Run on Linux:
 * ./arena_linux
 */

// Basic type definitions to avoid including any standard headers.
typedef unsigned long size_t;
typedef signed long ssize_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

#define NULL ((void*)0)
#define WASM_PAGE_SIZE 65536 // 64KiB

// --- Platform-Agnostic Interface ---

// WASI-style iovec structure, used by fd_write on both platforms.
typedef struct ciovec_s {
    const void* buf;
    size_t buf_len;
} ciovec_t;

// Forward declarations for our platform-agnostic functions.
uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);
void* memory_grow(size_t num_pages);
size_t memory_size(void);
void exit_program(int status);

// --- Platform-Specific Implementation ---

#if defined(__wasm__) && defined(__wasm32__)
// =============================================================================
// == WebAssembly (WASI) Implementation
// =============================================================================

// __heap_base is a special symbol provided by the wasm-ld linker. It marks
// the end of the static data section and the beginning of the linear memory
// heap that we can manage. We declare it as an external variable.
extern uint8_t __heap_base;

// WASI syscall function for writing to a file descriptor.
// We use import_module and import_name to tell the compiler this function
// is provided by the WASI host environment.
__attribute__((
    import_module("wasi_snapshot_preview1"),
    import_name("fd_write")
))
uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);


__attribute__((
    import_module("wasi_snapshot_preview1"),
    import_name("proc_exit")
))
void exit_program(int status);


// Wrapper around the `memory.size` WASM instruction.
// The argument `0` is required for the current memory space.
// Returns the current memory size in units of WASM_PAGE_SIZE (64KiB).
size_t memory_size(void) {
    return __builtin_wasm_memory_size(0);
}

// Wrapper around the `memory.grow` WASM instruction.
// Attempts to grow the linear memory by `num_pages`.
// Returns the previous size in pages on success, or -1 on failure.
void* memory_grow(size_t num_pages) {
    size_t prev_size = __builtin_wasm_memory_grow(0, num_pages);
    if (prev_size == (size_t)-1) {
        return NULL;
    }
    return (void*)(prev_size * WASM_PAGE_SIZE);
}

// For WASI, the entry point is `_start`, which we define to call our `main` function.
void _start(void);
int main(void);

void _start(void) {
    int status = main();
    exit_program(status);
}

#else
// =============================================================================
// == Linux (x86_64) Implementation
// =============================================================================

// Syscall numbers for x86_64
#define SYS_WRITEV 20
#define SYS_MMAP 9
#define SYS_MPROTECT 10
#define SYS_EXIT 60

// mmap flags
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

// Our emulated heap state for Linux
static uint8_t* linux_heap_base = NULL;
static size_t committed_pages = 0;
static const size_t RESERVED_SIZE = 1UL << 32; // Reserve 4GB of virtual address space

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

// Emulation of `fd_write` using the `writev` syscall.
uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    ssize_t ret = syscall(SYS_WRITEV, (long)fd, (long)iovs, (long)iovs_len, 0, 0, 0);
    if (ret < 0) {
        *nwritten = 0;
        return (uint32_t)-ret; // Return errno-like value
    }
    *nwritten = ret;
    return 0; // Success
}

void exit_program(int status) {
    syscall(SYS_EXIT, (long)status, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

// Initializes the heap using mmap. We reserve a large chunk of virtual
// address space but don't commit any physical memory to it initially.
static void ensure_heap_initialized() {
    if (linux_heap_base == NULL) {
        linux_heap_base = (uint8_t*)syscall(
            SYS_MMAP,
            (long)NULL,          // address hint
            RESERVED_SIZE,       // size
            0,                   // protection (none)
            MAP_PRIVATE | MAP_ANONYMOUS, // flags
            -1,                  // file descriptor
            0                    // offset
        );
        // A simple check to see if mmap failed. A more robust solution
        // would check for specific error values.
        if ((long)linux_heap_base > -4096L) {
            linux_heap_base = NULL;
        }
    }
}

// Emulation of `__builtin_wasm_memory_size`. Returns committed page count.
size_t memory_size(void) {
    ensure_heap_initialized();
    return committed_pages;
}

// Emulation of `__builtin_wasm_memory_grow`. Commits pages using `mprotect`.
void* memory_grow(size_t num_pages) {
    ensure_heap_initialized();
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
        num_pages * WASM_PAGE_SIZE,
        PROT_READ | PROT_WRITE,
        0, 0, 0
    );

    if (ret != 0) {
        return NULL; // mprotect failed
    }

    void* old_top = linux_heap_base + (committed_pages * WASM_PAGE_SIZE);
    committed_pages = new_total_pages;
    return old_top;
}

// For Linux, we define `__heap_base` as our mmap'd region.
// This is a bit of a trick to make the Arena code more uniform.
#define __heap_base (*(uint8_t**)&linux_heap_base)

// The entry point for a -nostdlib Linux program is `_start`.
void _start(void) {
    ensure_heap_initialized();
    int status = main();
    exit_program(status);
}

#endif

// --- Arena Allocator Implementation ---

typedef struct {
    uint8_t* base;
    size_t capacity;
    size_t offset;
} Arena;

/**
 * @brief Initializes an Arena allocator.
 *
 * It sets up the arena to use the available memory from `__heap_base` up to
 * the current memory size. If no memory is available, it tries to grow it.
 *
 * @param arena Pointer to the Arena struct to initialize.
 */
void arena_init(Arena* arena) {
    // On Linux, __heap_base is NULL initially, so memory_size() will call
    // ensure_heap_initialized() to set it up.
    size_t current_pages = memory_size();
    if (current_pages == 0) {
        if (memory_grow(1) == NULL) { // Try to allocate one page
            arena->base = NULL;
            arena->capacity = 0;
            arena->offset = 0;
            return;
        }
        current_pages = 1;
    }

    arena->base = (uint8_t*)&__heap_base;
    arena->capacity = current_pages * WASM_PAGE_SIZE;
    arena->offset = 0;
}

/**
 * @brief Allocates a block of memory from the arena.
 *
 * This is a simple bump allocator. If the arena is out of space, it will
 * attempt to grow the underlying heap.
 *
 * @param arena Pointer to the Arena.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, or NULL on failure.
 */
void* arena_alloc(Arena* arena, size_t size) {
    if (arena->base == NULL) return NULL;

    // Simple alignment to 8 bytes
    size = (size + 7) & ~7;

    if (arena->offset + size > arena->capacity) {
        size_t needed_bytes = (arena->offset + size) - arena->capacity;
        size_t pages_to_grow = (needed_bytes + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;

        if (memory_grow(pages_to_grow) == NULL) {
            return NULL; // Failed to grow memory
        }
        arena->capacity += pages_to_grow * WASM_PAGE_SIZE;
    }

    void* ptr = arena->base + arena->offset;
    arena->offset += size;
    return ptr;
}

/**
 * @brief Resets the arena, effectively "freeing" all allocations.
 *
 * @param arena Pointer to the Arena.
 */
void arena_reset(Arena* arena) {
    arena->offset = 0;
}

// Custom strlen to avoid C library dependency
size_t my_strlen(const char* str) {
    const char* s;
    for (s = str; *s; ++s);
    return (s - str);
}

// Custom strcpy to avoid C library dependency
char* my_strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}


// --- Main Application Logic ---

int main(void) {
    Arena main_arena;
    arena_init(&main_arena);

    // Allocate and copy some strings onto the arena
    const char* s1 = "Hello from the Arena! ";
    char* p1 = arena_alloc(&main_arena, my_strlen(s1) + 1);
    my_strcpy(p1, s1);

    const char* s2 = "This is a standalone C program. ";
    char* p2 = arena_alloc(&main_arena, my_strlen(s2) + 1);
    my_strcpy(p2, s2);

    const char* s3 = "It works on both WASM and Linux.\n";
    char* p3 = arena_alloc(&main_arena, my_strlen(s3) + 1);
    my_strcpy(p3, s3);

    // Prepare iovecs for fd_write
    ciovec_t iovs[] = {
        { .buf = p1, .buf_len = my_strlen(p1) },
        { .buf = p2, .buf_len = my_strlen(p2) },
        { .buf = p3, .buf_len = my_strlen(p3) },
    };

    size_t nwritten;
    fd_write(1, iovs, 3, &nwritten); // 1 = stdout

    return 0; // Success
}
