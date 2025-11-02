#pragma once

// Basic integer types - guard against conflicts with standard headers
#ifndef __uint8_t_defined
typedef unsigned char uint8_t;
#endif
#ifndef __uint16_t_defined
typedef unsigned short uint16_t;
#endif
#ifndef __uint32_t_defined
typedef unsigned int uint32_t;
#endif
#ifndef __uint64_t_defined
typedef unsigned long long uint64_t;
#endif

#ifndef __int8_t_defined
typedef signed char int8_t;
#endif
#ifndef __int16_t_defined
typedef signed short int16_t;
#endif
#ifndef __int32_t_defined
typedef signed int int32_t;
#endif
#ifndef __int64_t_defined
typedef signed long long int64_t;
#endif

// Pointer-sized integer type
#if defined(_WIN32) && defined(_WIN64)
    // For 64 bit Windows the long is 4 bytes, but pointer is 8 bytes
    typedef uint64_t uintptr_t;
#else
    // For 32 bit platforms and wasm64 the long and a pointer is 4 bytes, for
    // 64 bit macOS/Linux the long and pointer is 8 bytes
    typedef unsigned long uintptr_t;
#endif

// Size types
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

// NULL pointer
#ifndef NULL
#define NULL ((void*)0)
#endif

// Boolean type and constants
#ifndef bool
#define bool _Bool
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#ifndef INT8_C
#define INT8_C(value) value
#endif
#ifndef UINT8_C
#define UINT8_C(value) value##u
#endif
#ifndef INT16_C
#define INT16_C(value) value
#endif
#ifndef UINT16_C
#define UINT16_C(value) value##u
#endif
#ifndef INT32_C
#define INT32_C(value) value
#endif
#ifndef UINT32_C
#define UINT32_C(value) value##u
#endif
#ifndef INT64_C
#define INT64_C(value) value##ll
#endif
#ifndef UINT64_C
#define UINT64_C(value) value##ull
#endif
#ifndef INTMAX_C
#define INTMAX_C(value) INT64_C(value)
#endif
#ifndef UINTMAX_C
#define UINTMAX_C(value) UINT64_C(value)
#endif
#ifndef INT32_MAX
#define INT32_MAX ((int32_t)0x7FFFFFFF)
#endif
#ifndef UINT32_MAX
#define UINT32_MAX ((uint32_t)0xFFFFFFFFu)
#endif
#ifndef INT64_MAX
#define INT64_MAX ((int64_t)0x7FFFFFFFFFFFFFFFll)
#endif
#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFull)
#endif
