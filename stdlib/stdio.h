#pragma once

#include <stdarg.h>
#include <stddef.h>

int printf(const char* format, ...);
int vprintf(const char* format, va_list ap);

// FILE I/O support (minimal)
// Forward declare - actual implementation is platform-specific
#if !defined(FILE_DECLARED)
#define FILE_DECLARED
typedef struct FILE FILE;
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
