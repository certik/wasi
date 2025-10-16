#include <base/base_types.h>
#include <base/io.h>
#include <base/exit.h>
#include <base/base_io.h>
#include <base/mem.h>
#include <base/scratch.h>

// Forward declarations for stdio functions (FILE I/O)
typedef struct FILE FILE;
extern FILE *fopen(const char *filename, const char *mode);
extern int fclose(FILE *stream);
extern int fseek(FILE *stream, long offset, int whence);
extern long ftell(FILE *stream);
extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2


// Returns the file contents as a null-terminated string in `text`.
// Returns `true` on success, otherwise `false`.
bool read_file(Arena *arena, const string filename, string *text) {
    char *cfilename = str_to_cstr_copy(arena, filename);
    if (cfilename == NULL || *cfilename == '\0') return false;
    FILE *file = fopen(cfilename, "rb");
    if (file == NULL) return false;
    fseek(file, 0, SEEK_END);
    uint64_t filesize = ftell(file);
    if (filesize < 0) {
        fclose(file);
        return false;
    }
    fseek(file, 0, SEEK_SET);
    char *bytes = arena_alloc_array(arena, char, filesize+1);
    if (bytes == NULL) {
        fclose(file);
        return false;
    }
    size_t readsize = fread(bytes, 1, filesize, file);
    fclose(file);
    if (readsize != filesize) return false;
    bytes[readsize] = '\0';
    text->str=bytes;
    text->size=filesize+1;
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
