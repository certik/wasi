#pragma once

#include <base/base_types.h>

// Memory and string manipulation functions for base/
// Self-contained implementations with no external dependencies

// Only declare these if we're not using the standard library
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0 || defined(USE_CUSTOM_TYPES)
size_t strlen(const char *str);
char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *s, int c, size_t n);
void *memchr(const void *s, int c, size_t n);
#else
// Use system headers when building with standard library
#include <string.h>
#endif
