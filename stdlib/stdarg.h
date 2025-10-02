#pragma once

#include <stdint.h>
#include <stddef.h>

#if defined(_MSC_VER)
// MSVC provides its own intrinsics for varargs on x64
typedef char* va_list;

#if defined(_M_X64) || defined(_M_ARM64)
// x64 and ARM64 use different calling conventions
// MSVC provides __crt_va_start, __crt_va_arg, __crt_va_end as intrinsics
#define va_start __crt_va_start
#define va_arg __crt_va_arg
#define va_end __crt_va_end
#define va_copy(dest, src) ((dest) = (src))
#else
// x86 (32-bit) fallback
#define _INTSIZEOF(n) ((sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1))
#define va_start(ap, v) ((void)((ap) = (va_list)(&(v)) + _INTSIZEOF(v)))
#define va_arg(ap, t) (*(t*)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)))
#define va_end(ap) ((void)((ap) = (va_list)0))
#define va_copy(dest, src) ((dest) = (src))
#endif

#else
// Use compiler builtins for Clang/GCC
typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start((ap), (last))
#define va_arg(ap, type) __builtin_va_arg((ap), type)
#define va_end(ap) __builtin_va_end((ap))
#define va_copy(dest, src) __builtin_va_copy((dest), (src))

#endif
