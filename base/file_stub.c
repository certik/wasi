// Stub implementations of FILE I/O functions for base/ when building without stdlib
// These allow base/io.c to compile but the functions will return errors at runtime

#include <stddef.h>

typedef struct FILE_STUB FILE;

/*

FILE *fopen(const char *filename, const char *mode) {
    (void)filename;
    (void)mode;
    return (FILE*)0;  // NULL
}

int fclose(FILE *stream) {
    (void)stream;
    return -1;
}

int fseek(FILE *stream, long offset, int whence) {
    (void)stream;
    (void)offset;
    (void)whence;
    return -1;
}

long ftell(FILE *stream) {
    (void)stream;
    return -1;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;
}
*/
