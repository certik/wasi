#include "scratch.h"

Scratch scratch_begin(Arena *arena) {
    Scratch scratch;
    scratch.arena = arena;
    scratch.saved_pos = arena_get_pos(arena);
    return scratch;
}

void *scratch_alloc(Scratch *scratch, size_t size) {
    if (!scratch || !scratch->arena) {
        return NULL;
    }
    return arena_alloc(scratch->arena, size);
}

void scratch_end(Scratch *scratch) {
    if (!scratch || !scratch->arena) {
        return;
    }
    arena_reset(scratch->arena, scratch->saved_pos);
}
