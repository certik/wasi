#pragma once

#include <base/arena.h>
#include <base/base_string.h>
#include <base/format.h>

void println_explicit(Arena *arena, string fmt, size_t arg_count, ...);

#define println(arena, fmt, ...) \
    println_explicit(arena, fmt, COUNT_ARGS(__VA_ARGS__) __VA_OPT__(,) APPLY_WITH_COUNT(COUNT_ARGS(__VA_ARGS__) __VA_OPT__(,) __VA_ARGS__))
