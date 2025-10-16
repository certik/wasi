#include <base/base_types.h>
#include <base/io.h>
#include <base/exit.h>
#include <base/base_io.h>
#include <base/mem.h>
#include <base/scratch.h>
#include <base/wasi.h>

// Returns the file contents as a null-terminated string in `text`.
// Returns `true` on success, otherwise `false`.
bool read_file(Arena *arena, const string filename, string *text) {
    char *cfilename = str_to_cstr_copy(arena, filename);
    if (cfilename == NULL || *cfilename == '\0') return false;

    // Open file using WASI interface
    wasi_fd_t fd = wasi_path_open(cfilename, WASI_O_RDONLY);
    if (fd < 0) return false;

    // Get file size by seeking to end
    int64_t filesize = wasi_fd_seek(fd, 0, WASI_SEEK_END);
    if (filesize < 0) {
        wasi_fd_close(fd);
        return false;
    }

    // Seek back to beginning
    if (wasi_fd_seek(fd, 0, WASI_SEEK_SET) < 0) {
        wasi_fd_close(fd);
        return false;
    }

    // Allocate buffer
    char *bytes = arena_alloc_array(arena, char, filesize+1);
    if (bytes == NULL) {
        wasi_fd_close(fd);
        return false;
    }

    // Read file contents
    int64_t readsize = wasi_fd_read(fd, bytes, (size_t)filesize);
    wasi_fd_close(fd);

    if (readsize != filesize) return false;
    bytes[readsize] = '\0';
    text->str = bytes;
    text->size = filesize+1;
    return true;
}


string read_file_ok(Arena *arena, const string filename) {
    string text;
    if (read_file(arena, filename, &text)) {
        return text;
    } else {
        const char *msg = "File cannot be opened.\n";
        ciovec_t iov = {.buf = msg, .buf_len = strlen(msg)};
        write_all(1, &iov, 1);
        abort();
        return text;
    }
}

void println_explicit(Arena *arena, string fmt, size_t arg_count, ...) {
    va_list varg;
    va_start(varg, arg_count);

    // If NULL arena is passed, use a scratch arena
    Scratch scratch;
    bool use_scratch = (arena == NULL);
    if (use_scratch) {
        scratch = scratch_begin();
        arena = scratch.arena;
    }

    string text = format_explicit_varg(arena, fmt, arg_count, varg);
    va_end(varg);
    text = str_concat(arena, text, str_lit("\n"));
    ciovec_t iov = {.buf = text.str, .buf_len = text.size};
    write_all(1, &iov, 1);

    if (use_scratch) {
        scratch_end(scratch);
    }
}
