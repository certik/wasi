#pragma once

// Basic integer types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

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
#define NULL ((void*)0)

// Boolean type and constants
#define bool _Bool
#define true 1
#define false 0
