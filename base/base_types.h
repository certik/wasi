#pragma once

// When building with SDL (which uses system headers), use standard headers
#if defined(WASI_LINUX_SKIP_ENTRY) || defined(WASI_MACOS_SKIP_ENTRY) || defined(WASI_WINDOWS_SKIP_ENTRY)
// Define __gnuc_va_list for wchar.h (needed on Linux when using conda clang, not needed for MSVC)
#if defined(__clang__) || defined(__GNUC__)
#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
typedef __builtin_va_list __gnuc_va_list;
#endif
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
// Basic integer types for nostdlib builds
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
#endif

// Pointer-sized integer type and size types (only for nostdlib builds)
#if !defined(WASI_LINUX_SKIP_ENTRY) && !defined(WASI_MACOS_SKIP_ENTRY) && !defined(WASI_WINDOWS_SKIP_ENTRY)

#if defined(_WIN32) && defined(_WIN64)
    // For 64 bit Windows the long is 4 bytes, but pointer is 8 bytes
    typedef uint64_t uintptr_t;
#else
    // For 32 bit platforms and wasm64 the long and a pointer is 4 bytes, for
    // 64 bit macOS/Linux the long and pointer is 8 bytes
    typedef unsigned long uintptr_t;
#endif

#if defined(_WIN32) && defined(_WIN64)
    // 64 bit Windows has 8 byte size_t (but 4 byte long)
    typedef uint64_t size_t;
    typedef int64_t ssize_t;
#else
    // All other platforms have long and size_t the same number of bytes (4 or
    // 8)
    typedef unsigned long size_t;
    typedef signed long ssize_t;
#endif

#else
// For SDL builds, ensure we have ssize_t if not provided by system
#if !defined(_SSIZE_T_DEFINED) && !defined(_SSIZE_T_) && !defined(__ssize_t_defined)
#if defined(_WIN64)
    // 64-bit Windows: long is 4 bytes, but ssize_t should be 8 bytes (pointer-sized)
    typedef long long ssize_t;
#else
    typedef long ssize_t;
#endif
#endif
#endif

// NULL, boolean types, and limits (only for nostdlib builds)
#if !defined(WASI_LINUX_SKIP_ENTRY) && !defined(WASI_MACOS_SKIP_ENTRY) && !defined(WASI_WINDOWS_SKIP_ENTRY)

#define NULL ((void*)0)

// Boolean type and constants
#define bool _Bool
#define true 1
#define false 0
#define SIZE_MAX ((size_t)-1)

#define INT8_C(value) value
#define UINT8_C(value) value##u
#define INT16_C(value) value
#define UINT16_C(value) value##u
#define INT32_C(value) value
#define UINT32_C(value) value##u
#define INT64_C(value) value##ll
#define UINT64_C(value) value##ull
#define INTMAX_C(value) INT64_C(value)
#define UINTMAX_C(value) UINT64_C(value)
#define INT32_MAX ((int32_t)0x7FFFFFFF)
#define UINT32_MAX ((uint32_t)0xFFFFFFFFu)
#define INT64_MAX ((int64_t)0x7FFFFFFFFFFFFFFFll)
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFull)

#endif
