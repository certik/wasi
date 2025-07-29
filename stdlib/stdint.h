#pragma once

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

#if defined(_WIN32)
  #if defined(_WIN64)
    typedef uint64_t size_t;
    typedef int64_t ssize_t;
    typedef uint64_t uintptr_t;
  #else
    typedef uint32_t size_t;
    typedef int32_t ssize_t;
    typedef uint32_t uintptr_t;
  #endif
#else
  typedef unsigned long size_t;
  typedef signed long ssize_t;
  typedef unsigned long uintptr_t;
#endif
