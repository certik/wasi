#pragma once

#include <base_types.h>
#include "arena.h"

typedef struct {
    Arena *arena;
    arena_pos_t saved_pos;
} Scratch;

Scratch scratch_begin();
Scratch scratch_begin_avoid_conflict(Arena *conflict);
Scratch scratch_begin_from_arena(Arena *arena);
void scratch_end(Scratch scratch);
