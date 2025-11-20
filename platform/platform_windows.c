#include <platform.h>
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
typedef unsigned short wchar_t;
typedef union {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    LONGLONG QuadPart;
} LARGE_INTEGER;

// Windows constants
#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define PAGE_READWRITE 0x04
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
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
__declspec(dllimport) wchar_t* __stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t** __stdcall CommandLineToArgvW(const wchar_t* lpCmdLine, int* pNumArgs);
__declspec(dllimport) HANDLE __stdcall LocalFree(HANDLE hMem);

// Our emulated heap state for Windows
static uint8_t* windows_heap_base = NULL;
static size_t committed_pages = 0;
static const size_t RESERVED_SIZE = 1ULL << 32; // Reserve 4GB of virtual address space

// Command line arguments storage (UTF-8 converted)
static int stored_argc = 0;
static char** stored_argv = NULL;
static char* stored_argv_buf = NULL;

// Emulation of `fd_write` using Windows WriteFile API
uint32_t wasi_fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten) {
    HANDLE hOutput;

    // Handle standard streams specially
    if (fd == WASI_STDOUT_FD) {
        hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    } else if (fd == WASI_STDERR_FD) {
        hOutput = GetStdHandle(STD_ERROR_HANDLE);
    } else {
        // Treat as a file handle returned from wasi_path_open
        hOutput = (HANDLE)(long long)fd;
    }

    if (hOutput == INVALID_HANDLE_VALUE) {
        *nwritten = 0;
        return 1; // Error
    }

    size_t total_written = 0;
    for (size_t i = 0; i < iovs_len; i++) {
        DWORD bytes_written = 0;
        int result = WriteFile(hOutput, iovs[i].buf, (DWORD)iovs[i].buf_len, &bytes_written, NULL);
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
#ifdef WASI_WINDOWS_SKIP_ENTRY
void ensure_heap_initialized() {
#else
static void ensure_heap_initialized() {
#endif
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

// Forward declaration for command line argument initialization
static void init_args();

// Public initialization function for manual use (e.g., SDL apps using external stdlib)
void platform_init(int argc, char** argv) {
    init_args();
    ensure_heap_initialized();
    buddy_init();
}

// File I/O implementations
wasi_fd_t wasi_path_open(const char* path, size_t path_len, uint64_t rights, int oflags) {
    // Extract access mode from rights
    DWORD access = 0;
    DWORD creation = OPEN_EXISTING;
    int has_read = (rights & WASI_RIGHT_FD_READ) != 0;
    int has_write = (rights & WASI_RIGHT_FD_WRITE) != 0;

    if (has_read && has_write) {
        access = GENERIC_READ | GENERIC_WRITE;
    } else if (has_write) {
        access = GENERIC_WRITE;
    } else {
        access = GENERIC_READ;
    }

    // Map oflags to Windows creation disposition
    if ((oflags & WASI_O_CREAT) && (oflags & WASI_O_TRUNC)) {
        creation = CREATE_ALWAYS;  // Create new or truncate existing
    } else if (oflags & WASI_O_CREAT) {
        creation = OPEN_ALWAYS;    // Open existing or create new
    } else if (oflags & WASI_O_TRUNC) {
        creation = TRUNCATE_EXISTING;  // Truncate existing file (fails if doesn't exist)
    } else {
        creation = OPEN_EXISTING;  // Open existing file
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
    // Note: Windows HANDLEs are pointers (typically large values) and will never
    // collide with the special file descriptor values 0, 1, or 2 (stdin/stdout/stderr).
    // See: https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
    // HANDLEs are kernel object handles, not POSIX-style small integer file descriptors.
    return (wasi_fd_t)(long long)handle;
}

int wasi_fd_close(wasi_fd_t fd) {
    HANDLE handle = (HANDLE)(long long)fd;
    return CloseHandle(handle) ? 0 : 1;  // Return 0 on success, non-zero on error
}

int wasi_fd_read(wasi_fd_t fd, const iovec_t* iovs, size_t iovs_len, size_t* nread) {
    HANDLE handle;

    // Handle standard input specially
    if (fd == WASI_STDIN_FD) {
        handle = GetStdHandle(STD_INPUT_HANDLE);
    } else {
        // Treat as a file handle returned from wasi_path_open
        handle = (HANDLE)(long long)fd;
    }

    if (handle == INVALID_HANDLE_VALUE) {
        *nread = 0;
        return 1;  // Error
    }

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

// Helper: Convert UTF-16 wide char to UTF-8
// Returns number of bytes written (1-3 for BMP), or 0 on error
// Note: This handles Basic Multilingual Plane only (wchar_t is 16-bit on Windows)
static int wchar_to_utf8(wchar_t wc, char* out) {
    if (wc < 0x80) {
        out[0] = (char)wc;
        return 1;
    } else if (wc < 0x800) {
        out[0] = (char)(0xC0 | (wc >> 6));
        out[1] = (char)(0x80 | (wc & 0x3F));
        return 2;
    } else {
        // BMP character (up to 0xFFFF)
        out[0] = (char)(0xE0 | (wc >> 12));
        out[1] = (char)(0x80 | ((wc >> 6) & 0x3F));
        out[2] = (char)(0x80 | (wc & 0x3F));
        return 3;
    }
}

// Helper: Get length of wide string
static size_t wcslen(const wchar_t* str) {
    size_t len = 0;
    while (*str++) len++;
    return len;
}

// Helper: Convert wide string to UTF-8
// Returns number of bytes written (excluding null terminator)
static size_t widestr_to_utf8(const wchar_t* wstr, char* out, size_t out_size) {
    size_t written = 0;
    while (*wstr && written + 4 < out_size) {
        int bytes = wchar_to_utf8(*wstr, out + written);
        if (bytes == 0) break;
        written += bytes;
        wstr++;
    }
    if (written < out_size) {
        out[written] = '\0';
    }
    return written;
}

// Initialize command line arguments from Windows API
static void init_args() {
    if (stored_argv != NULL) return;  // Already initialized

    wchar_t* cmd_line = GetCommandLineW();
    int argc = 0;
    wchar_t** wargv = CommandLineToArgvW(cmd_line, &argc);

    if (wargv == NULL || argc == 0) {
        stored_argc = 0;
        stored_argv = NULL;
        return;
    }

    // Calculate total buffer size needed for UTF-8 conversion
    size_t total_size = 0;
    for (int i = 0; i < argc; i++) {
        // Worst case: each wide char becomes 3 UTF-8 bytes (BMP only)
        total_size += wcslen(wargv[i]) * 3 + 1;
    }

    // Allocate storage using VirtualAlloc
    stored_argv = (char**)VirtualAlloc(NULL, argc * sizeof(char*), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    stored_argv_buf = (char*)VirtualAlloc(NULL, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!stored_argv || !stored_argv_buf) {
        stored_argc = 0;
        LocalFree((HANDLE)wargv);
        return;
    }

    // Convert each argument to UTF-8
    char* buf_ptr = stored_argv_buf;
    for (int i = 0; i < argc; i++) {
        stored_argv[i] = buf_ptr;
        size_t bytes = widestr_to_utf8(wargv[i], buf_ptr, total_size - (buf_ptr - stored_argv_buf));
        buf_ptr += bytes + 1;  // +1 for null terminator
    }

    stored_argc = argc;
    LocalFree((HANDLE)wargv);
}

// Command line arguments implementation
int wasi_args_sizes_get(size_t* argc, size_t* argv_buf_size) {
    init_args();

    *argc = (size_t)stored_argc;

    // Calculate total buffer size needed
    size_t total_size = 0;
    for (int i = 0; i < stored_argc; i++) {
        const char* arg = stored_argv[i];
        while (*arg++) total_size++;  // strlen
        total_size++;  // null terminator
    }
    *argv_buf_size = total_size;
    return 0;
}

int wasi_args_get(char** argv, char* argv_buf) {
    init_args();

    char* buf_ptr = argv_buf;
    for (int i = 0; i < stored_argc; i++) {
        argv[i] = buf_ptr;
        const char* src = stored_argv[i];
        while (*src) {
            *buf_ptr++ = *src++;
        }
        *buf_ptr++ = '\0';
    }
    return 0;
}

#ifndef PLATFORM_USE_EXTERNAL_STDLIB
// Forward declaration for application entry point (only for nostdlib builds)
int app_main();

// Initialize the platform and call the application
static int platform_init_and_run() {
    platform_init(0, NULL);
    int status = app_main();
    return status;
}

// Entry point for Windows - MSVC uses _start but we need to set it up correctly
void _start() {
    int status = platform_init_and_run();
    wasi_proc_exit(status);
}
#endif
