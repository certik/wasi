#pragma once

#include <stdint.h>
#include <stddef.h>


#if defined(_WIN32) || defined(_WIN64)

typedef char * va_list;

#define _INTSIZEOF(n) ((sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1))
#define _ADDRESSOF(v) (&(v))

#define va_start(ap, v) ((void)((ap) = (va_list)_ADDRESSOF(v) + _INTSIZEOF(v)))
#define va_arg(ap, t) (*(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)))
#define va_end(ap) ((void)((ap) = (va_list)0))
#define va_copy(dest, src) ((void)((dest) = (src)))

#else

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start((ap), (last))
#define va_arg(ap, type) __builtin_va_arg((ap), type)
#define va_end(ap) __builtin_va_end((ap))
#define va_copy(dest, src) __builtin_va_copy((dest), (src))

#endif
