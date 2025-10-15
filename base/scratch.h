#pragma once

#include <base_types.h>
#include "arena.h"

typedef struct {
    Arena *arena;
    arena_pos_t saved_pos;
} Scratch;

// Internally there are 2 scratch arenas. You can use a scratch in the
// region marked by scratch_begin*() and scratch_end().

// Use if there are no other arenas that you allocate from in the scratch region.
// Always returns the first scratch arena.
Scratch scratch_begin();

// Use if there is another arena that you allocate from in the scratch region.
// Pass the other arena as an argument `conflict`.
// Returns a scratch arena that does not conflict with `conflict`.
Scratch scratch_begin_avoid_conflict(Arena *conflict);

// Use if there is another arena available in the region but you do not push
// into it in the scratch region. The scratch will be created in `arena`.
Scratch scratch_begin_from_arena(Arena *arena);

// Marks the end of the scratch region. Resets the arena that was used to
// create the scratch to the position before the scratch.
void scratch_end(Scratch scratch);
