#pragma once

#include <stddef.h>

// Memory and string manipulation functions for base/
// Self-contained implementations with no external dependencies

size_t strlen(const char *str);
char *strcpy(char *dest, const char *src);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *s, int c, size_t n);
void *memchr(const void *s, int c, size_t n);
