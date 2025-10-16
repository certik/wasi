#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <base_io.h>

// FILE I/O implementation
// We use system calls where available (macOS via libSystem)
// For WASM/WASI, we'd need WASI file APIs

#if defined(__APPLE__)
// On macOS, link with libSystem which provides these
#define FILE_DECLARED
struct __sFILE;
typedef struct __sFILE FILE;
extern FILE *fopen(const char *, const char *);
extern int fclose(FILE *);
extern int fseek(FILE *, long, int);
extern long ftell(FILE *);
extern size_t fread(void *, size_t, size_t, FILE *);
#elif defined(__linux__)
// On Linux, we need raw syscalls since we're -nostdlib
// For now, stub these out - they won't work on Linux
struct FILE_INTERNAL;
typedef struct FILE_INTERNAL FILE;
FILE *fopen(const char *filename, const char *mode) { return NULL; }
int fclose(FILE *stream) { return -1; }
int fseek(FILE *stream, long offset, int whence) { return -1; }
long ftell(FILE *stream) { return -1; }
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) { return 0; }
#elif defined(_WIN32)
// On Windows, stub for now
struct FILE_INTERNAL;
typedef struct FILE_INTERNAL FILE;
FILE *fopen(const char *filename, const char *mode) { return NULL; }
int fclose(FILE *stream) { return -1; }
int fseek(FILE *stream, long offset, int whence) { return -1; }
long ftell(FILE *stream) { return -1; }
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) { return 0; }
#else // __wasi__
// On WASI, stub for now
struct FILE_INTERNAL;
typedef struct FILE_INTERNAL FILE;
FILE *fopen(const char *filename, const char *mode) { return NULL; }
int fclose(FILE *stream) { return -1; }
int fseek(FILE *stream, long offset, int whence) { return -1; }
long ftell(FILE *stream) { return -1; }
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) { return 0; }
#endif

// stdlib/stdio.c now reuses printf/vprintf from base/printf.c
// This avoids code duplication and ensures consistent behavior

#include <base/printf.h>
