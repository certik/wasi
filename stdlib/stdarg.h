#pragma once

#include <stdint.h>
#include <stddef.h>

#if defined(_MSC_VER)
// MSVC varargs implementation
#if defined(_M_X64) || defined(_M_ARM64)
// x64 and ARM64: All arguments are 8-byte aligned and passed in shadow space or stack
typedef char* va_list;

// On x64, arguments are passed via registers (RCX, RDX, R8, R9) but the caller
// allocates shadow space on the stack. The va_list points to the first arg after
// the last named parameter. All arguments are 8-byte aligned.
#define _VA_ALIGN 8
#define _VA_ROUNDED_SIZE(t) ((sizeof(t) + _VA_ALIGN - 1) & ~(_VA_ALIGN - 1))

#define va_start(ap, v) ((void)((ap) = (va_list)((char*)(&(v)) + _VA_ROUNDED_SIZE(v))))
#define va_arg(ap, t) (*(t*)((ap += _VA_ROUNDED_SIZE(t)) - _VA_ROUNDED_SIZE(t)))
#define va_end(ap) ((void)((ap) = (va_list)0))
#define va_copy(dest, src) ((dest) = (src))
#else
// x86 (32-bit): 4-byte alignment
typedef char* va_list;
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
