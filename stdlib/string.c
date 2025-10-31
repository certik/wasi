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

int memcmp(const void *s1, const void *s2, size_t n) {
    return base_memcmp(s1, s2, n);
}

void *memset(void *s, int c, size_t n) {
    return base_memset(s, c, n);
}

void *memchr(const void *s, int c, size_t n) {
    return base_memchr(s, c, n);
}
