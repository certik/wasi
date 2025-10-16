#include <wasi.h>
#include <base_types.h>
#include <buddy.h>

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
typedef long long LONGLONG;
typedef union {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    LONGLONG QuadPart;
} LARGE_INTEGER;

// Windows constants
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define PAGE_READWRITE 0x04
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2

// Windows API function declarations
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) int __stdcall WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) LPVOID __stdcall VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int uExitCode);
__declspec(dllimport) HANDLE __stdcall CreateFileA(const char* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, void* lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
__declspec(dllimport) int __stdcall CloseHandle(HANDLE hObject);
__declspec(dllimport) int __stdcall ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, void* lpOverlapped);
__declspec(dllimport) int __stdcall SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, LARGE_INTEGER* lpNewFilePointer, DWORD dwMoveMethod);

// Our emulated heap state for Windows
static uint8_t* windows_heap_base = NULL;
static size_t committed_pages = 0;
static const size_t RESERVED_SIZE = 1ULL << 32; // Reserve 4GB of virtual address space

// Emulation of `fd_write` using Windows WriteFile API
uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
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
            wasi_proc_exit(1); // Failed to reserve memory
        }
        
        // Commit the first page
        void* committed = VirtualAlloc(windows_heap_base, WASM_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
        if (committed == NULL) {
            wasi_proc_exit(1); // Failed to commit memory
        }
        committed_pages = 1;
    }
}

static inline uintptr_t align(uintptr_t val, uintptr_t alignment) {
  return (val + alignment - 1) & ~(alignment - 1);
}

// Windows wasi_heap_grow implementation using VirtualAlloc
void* wasi_heap_grow(size_t num_bytes) {
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

void* wasi_heap_base() {
    return windows_heap_base;
}

// Windows wasi_heap_size implementation
size_t wasi_heap_size() {
    return committed_pages * WASM_PAGE_SIZE;
}

// Stub for __chkstk which is normally provided by the C runtime
// Since we're using /kernel flag, we need to provide this ourselves
void __chkstk() {
    // Do nothing - we're not using large stack allocations
}

// _fltused is required by MSVC when using floating-point operations
// This symbol must be present when using floating-point without the CRT
int _fltused = 1;

// Process exit function
void wasi_proc_exit(int status) {
    ExitProcess((unsigned int)status);
}

// Forward declaration for main
int main();

// File I/O implementations
wasi_fd_t wasi_path_open(const char* path, int flags) {
    // Map WASI flags to Windows flags
    DWORD access = 0;
    DWORD creation = OPEN_EXISTING;

    if ((flags & WASI_O_RDWR) == WASI_O_RDWR) {
        access = GENERIC_READ | GENERIC_WRITE;
    } else if (flags & WASI_O_WRONLY) {
        access = GENERIC_WRITE;
    } else {
        access = GENERIC_READ;
    }

    if (flags & WASI_O_CREAT) {
        creation = CREATE_ALWAYS;
    }

    HANDLE handle = CreateFileA(
        path,
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        creation,
        0,
        NULL
    );

    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    // Return handle cast to int (wasi_fd_t)
    return (wasi_fd_t)(long long)handle;
}

int wasi_fd_close(wasi_fd_t fd) {
    HANDLE handle = (HANDLE)(long long)fd;
    return CloseHandle(handle) ? 0 : 1;  // Return 0 on success, non-zero on error
}

int wasi_fd_read(wasi_fd_t fd, const iovec_t* iovs, size_t iovs_len, size_t* nread) {
    HANDLE handle = (HANDLE)(long long)fd;
    size_t total_read = 0;

    // Windows doesn't have readv, so loop over iovecs
    for (size_t i = 0; i < iovs_len; i++) {
        DWORD bytes_read = 0;
        int result = ReadFile(handle, iovs[i].iov_base, (DWORD)iovs[i].iov_len, &bytes_read, NULL);
        if (!result) {
            *nread = total_read;
            return 1;  // Error
        }
        total_read += bytes_read;
        if (bytes_read < iovs[i].iov_len) {
            // Short read, stop here
            break;
        }
    }

    *nread = total_read;
    return 0;  // Success
}

int wasi_fd_seek(wasi_fd_t fd, int64_t offset, int whence, uint64_t* newoffset) {
    HANDLE handle = (HANDLE)(long long)fd;
    LARGE_INTEGER distance;
    LARGE_INTEGER new_position;
    distance.QuadPart = offset;

    DWORD method = FILE_BEGIN;
    if (whence == WASI_SEEK_CUR) method = FILE_CURRENT;
    else if (whence == WASI_SEEK_END) method = FILE_END;

    int result = SetFilePointerEx(handle, distance, &new_position, method);
    if (!result) {
        *newoffset = 0;
        return 1;  // Error
    }
    *newoffset = (uint64_t)new_position.QuadPart;
    return 0;  // Success
}

int wasi_fd_tell(wasi_fd_t fd, uint64_t* offset) {
    HANDLE handle = (HANDLE)(long long)fd;
    LARGE_INTEGER distance;
    LARGE_INTEGER position;
    distance.QuadPart = 0;

    int result = SetFilePointerEx(handle, distance, &position, FILE_CURRENT);
    if (!result) {
        *offset = 0;
        return 1;  // Error
    }
    *offset = (uint64_t)position.QuadPart;
    return 0;  // Success
}

// Entry point for Windows - MSVC uses _start but we need to set it up correctly
void _start() {
    ensure_heap_initialized();
    buddy_init();
    int status = main();
    wasi_proc_exit(status);
}
