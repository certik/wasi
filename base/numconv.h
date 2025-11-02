#pragma once

#include <base/base_types.h>
#include <base/stdarg.h>

// Number to string conversion functions for base/
// Self-contained implementations with no external dependencies

// Convert unsigned 64-bit integer to string
// Returns length of string written (not including null terminator)
size_t uint64_to_str(uint64_t val, char* buf);

// Convert signed 64-bit integer to string
// Returns length of string written (not including null terminator)
size_t int64_to_str(int64_t val, char* buf);

// Convert int to string (calls int64_to_str)
// Returns length of string written (not including null terminator)
size_t int_to_str(int val, char* buf);

// Convert double to string with specified precision
// Returns length of string written (not including null terminator)
// precision: number of decimal places (-1 for default of 6)
size_t double_to_str(double val, char* buf, int precision);

// Simple vsnprintf/snprintf implementations for base/ (only for nostdlib builds)
#if !defined(WASI_LINUX_SKIP_ENTRY) && !defined(WASI_MACOS_SKIP_ENTRY) && !defined(WASI_WINDOWS_SKIP_ENTRY)
// Supports: %d, %u, %f, %.Nf, %s
// Returns number of characters written (not including null terminator)
int vsnprintf(char *str, size_t size, const char *format, va_list args);

// Simple snprintf implementation for base/
// Supports: %d, %u, %f, %.Nf, %s
// Returns number of characters written (not including null terminator)
int snprintf(char *str, size_t size, const char *format, ...);
#else
// Use standard library snprintf/vsnprintf when building with SDL
#include <stdio.h>
#endif
