#pragma once

#include <base/base_types.h>

// Memory and string manipulation functions for base/
// Self-contained implementations with no external dependencies
// Prefixed with base_ to avoid conflicts with system headers

size_t base_strlen(const char *str);
char *base_strcpy(char *dest, const char *src);
int base_strcmp(const char *s1, const char *s2);
void *base_memcpy(void *dest, const void *src, size_t n);
int base_memcmp(const void *s1, const void *s2, size_t n);
void *base_memset(void *s, int c, size_t n);
void *base_memchr(const void *s, int c, size_t n);
