#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <base/base_io.h>
#include <base/wasi.h>

// FILE structure wrapping a WASI file descriptor
typedef struct FILE {
    wasi_fd_t fd;
    int eof;
    int error;
} FILE;

// Statically allocate a few FILE structures
static FILE file_pool[16];
static int file_pool_initialized = 0;

static void init_file_pool() {
    if (!file_pool_initialized) {
        for (int i = 0; i < 16; i++) {
            file_pool[i].fd = -1;
            file_pool[i].eof = 0;
            file_pool[i].error = 0;
        }
        file_pool_initialized = 1;
    }
}

static FILE* alloc_file(wasi_fd_t fd) {
    init_file_pool();
    for (int i = 0; i < 16; i++) {
        if (file_pool[i].fd == -1) {
            file_pool[i].fd = fd;
            file_pool[i].eof = 0;
            file_pool[i].error = 0;
            return &file_pool[i];
        }
    }
    return NULL;
}

static void free_file(FILE* file) {
    if (file) {
        file->fd = -1;
        file->eof = 0;
        file->error = 0;
    }
}

FILE *fopen(const char *filename, const char *mode) {
    // Parse mode string
    int flags = 0;
    if (mode[0] == 'r') {
        flags = WASI_O_RDONLY;
    } else if (mode[0] == 'w') {
        flags = WASI_O_WRONLY | WASI_O_CREAT | WASI_O_TRUNC;
    } else if (mode[0] == 'a') {
        flags = WASI_O_WRONLY | WASI_O_CREAT;
    } else {
        return NULL;
    }

    // Check for binary mode (ignored, all files are binary)
    // Check for + mode (read/write)
    for (int i = 1; mode[i]; i++) {
        if (mode[i] == '+') {
            flags = (flags & ~(WASI_O_RDONLY | WASI_O_WRONLY)) | WASI_O_RDWR;
        }
    }

    wasi_fd_t fd = wasi_path_open(filename, flags);
    if (fd < 0) {
        return NULL;
    }

    FILE* file = alloc_file(fd);
    if (!file) {
        wasi_fd_close(fd);
        return NULL;
    }

    return file;
}

int fclose(FILE *stream) {
    if (!stream || stream->fd < 0) {
        return -1;
    }

    int result = wasi_fd_close(stream->fd);
    free_file(stream);
    return result;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream || stream->fd < 0) {
        return -1;
    }

    int64_t result = wasi_fd_seek(stream->fd, (int64_t)offset, whence);
    if (result < 0) {
        stream->error = 1;
        return -1;
    }

    stream->eof = 0;
    return 0;
}

long ftell(FILE *stream) {
    if (!stream || stream->fd < 0) {
        return -1;
    }

    int64_t result = wasi_fd_tell(stream->fd);
    return (long)result;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || stream->fd < 0 || size == 0 || nmemb == 0) {
        return 0;
    }

    size_t total_bytes = size * nmemb;
    int64_t bytes_read = wasi_fd_read(stream->fd, ptr, total_bytes);

    if (bytes_read < 0) {
        stream->error = 1;
        return 0;
    }

    if (bytes_read == 0) {
        stream->eof = 1;
    }

    return (size_t)bytes_read / size;
}

// stdlib/stdio.c now reuses printf/vprintf from stdlib/printf.c
// This avoids code duplication and ensures consistent behavior

#include <printf.h>
