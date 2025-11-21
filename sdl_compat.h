// SDL/glibc compat helpers for Clang builds on Linux
#ifndef SDL_COMPAT_H
#define SDL_COMPAT_H

// glibc's wchar.h expects __gnuc_va_list; define it when Clang doesn't provide it
#if (defined(__clang__) || defined(__GNUC__)) && !defined(__GNUC_VA_LIST)
#define __GNUC_VA_LIST
typedef __builtin_va_list __gnuc_va_list;
#endif

#endif // SDL_COMPAT_H
