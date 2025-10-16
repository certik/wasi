#include <base/base_types.h>
#include <base/io.h>
#include <base/exit.h>
#include <base/base_io.h>
#include <base/mem.h>
#include <base/scratch.h>

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
