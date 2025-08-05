#include <wasi.h>

// =============================================================================
// == Windows Implementation (MSVC)
// =============================================================================

// Windows API declarations - we'll define only what we need
typedef void* HANDLE;
typedef unsigned long DWORD;
typedef DWORD* LPDWORD;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef unsigned long long SIZE_T;

// Windows constants
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define PAGE_READWRITE 0x04
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)

// Windows API function declarations
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) int __stdcall WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) LPVOID __stdcall VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int uExitCode);

// Our emulated heap state for Windows
static uint8_t* windows_heap_base = NULL;
static size_t committed_pages = 0;
static const size_t RESERVED_SIZE = 1ULL << 32; // Reserve 4GB of virtual address space

// Emulation of `fd_write` using Windows WriteFile API
uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    if (fd != 1) { // Only support stdout
        *nwritten = 0;
        return 1; // Error
    }
    
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) {
        *nwritten = 0;
        return 1; // Error
    }
    
    size_t total_written = 0;
    for (size_t i = 0; i < iovs_len; i++) {
        DWORD bytes_written = 0;
        int result = WriteFile(hStdOut, iovs[i].buf, (DWORD)iovs[i].buf_len, &bytes_written, NULL);
        if (!result) {
            *nwritten = total_written;
            return 1; // Error
        }
        total_written += bytes_written;
    }
    
    *nwritten = total_written;
    return 0; // Success
}

// Initialize the heap by reserving and committing initial memory
static void ensure_heap_initialized() {
    if (windows_heap_base == NULL) {
        // Reserve a large virtual address space
        windows_heap_base = (uint8_t*)VirtualAlloc(NULL, RESERVED_SIZE, MEM_RESERVE, PAGE_READWRITE);
        if (windows_heap_base == NULL) {
            proc_exit(1); // Failed to reserve memory
        }
        
        // Commit the first page
        void* committed = VirtualAlloc(windows_heap_base, WASM_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
        if (committed == NULL) {
            proc_exit(1); // Failed to commit memory
        }
        committed_pages = 1;
    }
}

// Windows heap_grow implementation using VirtualAlloc
void* heap_grow(size_t num_bytes) {
    size_t num_pages = align(num_bytes, WASM_PAGE_SIZE) / WASM_PAGE_SIZE;
    
    if (num_pages == 0) {
        // TODO: what should be returned here?
        return (void*)(committed_pages * WASM_PAGE_SIZE);
    }
    
    size_t bytes_to_commit = num_pages * WASM_PAGE_SIZE;
    void* new_memory = VirtualAlloc(
        windows_heap_base + (committed_pages * WASM_PAGE_SIZE),
        bytes_to_commit,
        MEM_COMMIT,
        PAGE_READWRITE
    );
    
    if (new_memory == NULL) {
        return (void*)-1; // Growth failed
    }
    
    size_t prev_size = committed_pages;
    committed_pages += num_pages;
    return (void*)(windows_heap_base + prev_size * WASM_PAGE_SIZE);
}

void* heap_base() {
    return windows_heap_base;
}

// Windows heap_size implementation
size_t heap_size() {
    return committed_pages * WASM_PAGE_SIZE;
}

// Stub for __chkstk which is normally provided by the C runtime
// Since we're using /kernel flag, we need to provide this ourselves
void __chkstk() {
    // Do nothing - we're not using large stack allocations
}

// Process exit function
void proc_exit(int status) {
    ExitProcess((unsigned int)status);
}

// Forward declaration for main
int main();

// Entry point for Windows - MSVC uses _start but we need to set it up correctly
void _start() {
    ensure_heap_initialized();
    int status = main();
    proc_exit(status);
}
