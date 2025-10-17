#include <base/base_types.h>
#include <base/io.h>
#include <base/exit.h>
#include <base/base_io.h>
#include <base/mem.h>
#include <base/scratch.h>
#include <base/wasi.h>

// Returns the file contents as a null-terminated string in `text`.
// Returns `true` on success, otherwise `false`.
// The size of `text` includes the null character, which is inserted
// to allow tokenizing the text and use a null character as a "file end"
// condition.
bool read_file(Arena *arena, const string filename, string *text) {
    Scratch scratch = scratch_begin_avoid_conflict(arena);
    // Open file using WASI interface
    wasi_fd_t fd = wasi_path_open(str_to_cstr_copy(scratch.arena, filename),
            filename.size, WASI_O_RDONLY);
    if (fd < 0) {
        scratch_end(scratch);
        return false;
    }

    // Get file size by seeking to end
    uint64_t filesize_u64;
    if (wasi_fd_seek(fd, 0, WASI_SEEK_END, &filesize_u64) != 0) {
        wasi_fd_close(fd);
        scratch_end(scratch);
        return false;
    }

    // Seek back to beginning
    uint64_t dummy;
    if (wasi_fd_seek(fd, 0, WASI_SEEK_SET, &dummy) != 0) {
        wasi_fd_close(fd);
        scratch_end(scratch);
        return false;
    }

    size_t filesize = (size_t)filesize_u64;

    // Allocate buffer
    char *bytes = arena_alloc_array(arena, char, filesize+1);

    // Read file contents using iovec
    iovec_t iov = { .iov_base = bytes, .iov_len = filesize };
    size_t nread;
    int ret = wasi_fd_read(fd, &iov, 1, &nread);
    wasi_fd_close(fd);

    if (ret != 0 || nread != filesize) {
        scratch_end(scratch);
        return false;
    }
    bytes[nread] = '\0';
    text->str = bytes;
    text->size = filesize+1;
    scratch_end(scratch);
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

void println_explicit(string fmt, size_t arg_count, ...) {
    Scratch scratch = scratch_begin();
    va_list varg;
    va_start(varg, arg_count);

    string text = format_explicit_varg(scratch.arena, fmt, arg_count, varg);
    va_end(varg);
    text = str_concat(scratch.arena, text, str_lit("\n"));
    ciovec_t iov = {.buf = text.str, .buf_len = text.size};
    write_all(1, &iov, 1);

    scratch_end(scratch);
}
