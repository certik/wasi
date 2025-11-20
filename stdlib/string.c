// stdlib string.c - wrappers around base/mem.h functions
// Provides standard C library names for nostdlib builds

#include <stdlib/string.h>
#include <base/mem.h>

size_t strlen(const char *str) {
    return base_strlen(str);
}

char *strcpy(char *dest, const char *src) {
    return base_strcpy(dest, src);
}

int strcmp(const char *s1, const char *s2) {
    return base_strcmp(s1, s2);
}

void *memcpy(void *dest, const void *src, size_t n) {
    return base_memcpy(dest, src, n);
}

void *memmove(void *dest, const void *src, size_t n) {
    return base_memmove(dest, src, n);
}

int memcmp(const void *s1, const void *s2, size_t n) {
    return base_memcmp(s1, s2, n);
}

// Note: memset() is defined in base/mem.c, not here, because the compiler
// can implicitly insert calls to it even with -fno-builtin.

void *memchr(const void *s, int c, size_t n) {
    return base_memchr(s, c, n);
}

char *strchr(const char *s, int c) {
    return base_strchr(s, c);
}

char *strrchr(const char *s, int c) {
    return base_strrchr(s, c);
}

char *strncpy(char *dest, const char *src, size_t n) {
    return base_strncpy(dest, src, n);
}

size_t strcspn(const char *s, const char *reject) {
    return base_strcspn(s, reject);
}

int strncmp(const char *s1, const char *s2, size_t n) {
    return base_strncmp(s1, s2, n);
}

char *strstr(const char *haystack, const char *needle) {
    return base_strstr(haystack, needle);
}
