#include "scratch.h"

Scratch scratch_begin_from_arena(Arena *arena) {
    Scratch scratch;
    scratch.arena = arena;
    scratch.saved_pos = arena_get_pos(arena);
    return scratch;
}

void scratch_end(Scratch *scratch) {
    arena_reset(scratch->arena, scratch->saved_pos);
}
