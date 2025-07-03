/*
 * On WASM:
 * clang w2.c stdlib/string.c -target wasm32 -Istdlib/ -Iwasi/ --no-standard-libraries -Wl,--no-entry -Wl,--export-all -o w2.wasm
 * wasmtime --invoke __original_main w2.wasm
 * On Linux:
 * clang w2.c && ./a.out
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h> // For size_t
#include <string.h> // For strlen
#include <stdint.h> // For uintptr_t

// The page size in WebAssembly is fixed at 64KB.
#define WASM_PAGE_SIZE 65536

// The current size of the memory in pages, managed by the application logic.
size_t current_memory_pages;

// =============================================================================
// PLATFORM-SPECIFIC IMPLEMENTATION
// =============================================================================

#if defined(__wasm32__)
// =============================================================================
// == WEB_ASSEMBLY BACKEND
// =============================================================================

char* get_memory() {
    return (char*)0; // Linear memory starts at address 0 in WASM
}

// __builtin_wasm_memory_grow is provided by Clang for wasm32; no implementation needed.

#include <wasi.h>

void print_string(const char* str, uint32_t len) {
    ciovec_t iov = { (void *)str, len };
    size_t nwritten;
    fd_write(1, &iov, 1, &nwritten);
}

void log_message(const char *text) {
    size_t len = strlen(text);
    char newline = '\n';
    print_string(text, len);
    print_string(&newline, 1);
}

// Helper function to append a size_t value as a string to the buffer
void append_size_t(char **buf_ptr, size_t val) {
    char temp[20];  // Enough for 64-bit size_t (max 20 digits)
    int i = 0;
    if (val == 0) {
        temp[i++] = '0';
    } else {
        while (val > 0) {
            temp[i++] = '0' + (val % 10);
            val /= 10;
        }
        // Reverse the digits
        for (int j = 0; j < i / 2; j++) {
            char t = temp[j];
            temp[j] = temp[i - j - 1];
            temp[i - j - 1] = t;
        }
    }
    // Copy to buffer
    for (int j = 0; j < i; j++) {
        *(*buf_ptr)++ = temp[j];
    }
}

int printf(const char *format, ...) {
    char buffer[512];  // Fixed-size buffer, sufficient for the given use cases
    char *buf_ptr = buffer;
    const char *fmt_ptr = format;
    va_list args;
    va_start(args, format);

    // Parse the format string
    while (*fmt_ptr != '\0') {
        if (*fmt_ptr != '%') {
            *buf_ptr++ = *fmt_ptr++;  // Copy non-specifier characters
        } else {
            fmt_ptr++;  // Skip '%'
            if (*fmt_ptr == 'z') {
                fmt_ptr++;  // Skip 'z'
                if (*fmt_ptr == 'u') {  // Handle %zu
                    size_t val = va_arg(args, size_t);
                    append_size_t(&buf_ptr, val);
                    fmt_ptr++;  // Skip 'u'
                }
            } else if (*fmt_ptr == 'c') {  // Handle %c
                int c = va_arg(args, int);  // Char promoted to int in varargs
                *buf_ptr++ = (char)c;  // Explicit cast to char
                fmt_ptr++;
            } else if (*fmt_ptr == '%') {  // Handle %%
                *buf_ptr++ = '%';
                fmt_ptr++;
            } else {
                // Unsupported specifier, copy as is
                *buf_ptr++ = '%';
                *buf_ptr++ = *fmt_ptr++;
            }
        }
    }
    *buf_ptr = '\0';  // Null-terminate the string

    va_end(args);
    log_message(buffer);
    return (buf_ptr - buffer);  // Return number of characters written
}

#else
// =============================================================================
// == LINUX BACKEND
// =============================================================================

#include <sys/mman.h> // For mmap
#include <unistd.h>   // For sysconf, _SC_PAGESIZE

// Reserve 4GB, the maximum for a 32-bit WASM module.
const size_t MAX_MEMORY_BYTES = 4UL * 1024 * 1024 * 1024;

static char* memory;

size_t __builtin_wasm_memory_grow(size_t memory_index, size_t num_pages_to_add) {
    if (memory_index != 0 || !memory) {
        return (size_t)-1;
    }

    size_t old_pages = current_memory_pages;
    size_t new_total_pages = current_memory_pages + num_pages_to_add;
    size_t new_total_bytes = new_total_pages * WASM_PAGE_SIZE;

    if (new_total_bytes > MAX_MEMORY_BYTES) {
        fprintf(stderr, "Error: Cannot grow memory beyond 4GB reservation.\n");
        return (size_t)-1;
    }

    char* new_region_start = memory + (old_pages * WASM_PAGE_SIZE);
    size_t bytes_to_add = num_pages_to_add * WASM_PAGE_SIZE;

    void* result = mmap(
        new_region_start,
        bytes_to_add,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
        -1, 0
    );

    if (result == MAP_FAILED) {
        perror("mmap commit failed for growth");
        return (size_t)-1;
    }

    return old_pages;
}

__attribute__((constructor))
void init_linux_memory() {
    memory = (char*)mmap(NULL, MAX_MEMORY_BYTES, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        perror("mmap reserve failed");
        memory = NULL;
        return;
    }

    void* first_page = mmap(memory, WASM_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (first_page == MAP_FAILED) {
        perror("mmap commit failed for initial page");
        munmap(memory, MAX_MEMORY_BYTES);
        memory = NULL;
        return;
    }

    printf("[Linux Backend] Successfully reserved 4GB and committed 1 page (64KB).\n");
}

char* get_memory() {
    return memory;
}

#endif

// =============================================================================
// UNIFIED APPLICATION LOGIC
// =============================================================================

int main() {
    // Initialize current_memory_pages for WebAssembly
    #if defined(__wasm32__)
    current_memory_pages = __builtin_wasm_memory_size(0);
    // Ensure at least 1 page of memory
    if (current_memory_pages == 0) {
        size_t result = __builtin_wasm_memory_grow(0, 1);
        if (result == (size_t)-1) {
            printf("Failed to initialize memory with 1 page.\n");
            return 1;
        }
        current_memory_pages = 1;
    }
    #else
    current_memory_pages = 1; // Linux initializes 1 page in init_linux_memory
    #endif

    char* mem = get_memory();
    // Instead of checking for NULL, verify memory size
    if (current_memory_pages == 0) {
        printf("No memory available (0 pages).\n");
        return 1;
    }

    printf("--- Memory Test Initial State ---\n");
    printf("Initial memory size: %zu pages (%zu bytes).\n", current_memory_pages, current_memory_pages * WASM_PAGE_SIZE);

    size_t last_byte_initial = (current_memory_pages * WASM_PAGE_SIZE) - 1;
    mem[last_byte_initial] = 'A';
    printf("Successfully wrote '%c' to last byte of initial memory (offset %zu).\n", mem[last_byte_initial], last_byte_initial);

    printf("\n--- Growing Memory ---\n");
    size_t pages_to_add = 2;
    printf("Attempting to grow memory by %zu pages using the standard builtin...\n", pages_to_add);

    size_t old_page_count = __builtin_wasm_memory_grow(0, pages_to_add);
    if (old_page_count == (size_t)-1) {
        printf("Failed to grow memory.\n");
        return 1;
    }

    current_memory_pages += pages_to_add;

    printf("Memory grown successfully.\n");
    printf("Previous size: %zu pages. New size: %zu pages.\n", old_page_count, current_memory_pages);

    printf("\n--- Memory Test After Growth ---\n");

    size_t last_byte_new = (current_memory_pages * WASM_PAGE_SIZE) - 1;
    mem[last_byte_new] = 'Z';
    printf("Successfully wrote '%c' to last byte of new memory (offset %zu).\n", mem[last_byte_new], last_byte_new);

    printf("Verified old data is still present: mem[%zu] = '%c'\n", last_byte_initial, mem[last_byte_initial]);

    return 0;
}
