#include <base/scratch.h>
#include <base/wasi.h>
#include <base/exit.h>
#include <base/base_io.h>

Scratch scratch_begin_from_arena(Arena *arena) {
    // Reset arena to the beginning to ensure we reuse existing chunks
    // instead of accumulating new ones on every scratch_begin call
    arena_pos_t first_pos = arena_get_first_pos(arena);
    arena_reset(arena, first_pos);

    Scratch scratch = {.arena=arena, .saved_pos=first_pos};
    return scratch;
}

Scratch scratch_begin() {
    return scratch_begin_avoid_conflict(NULL);
}

Arena* scratch_arenas[2] = {NULL, NULL};

void init_scratch_arenas() {
    scratch_arenas[0] = arena_new(1024);
    scratch_arenas[1] = arena_new(1024);
}

Scratch scratch_begin_avoid_conflict(Arena *conflict) {
    if (scratch_arenas[0] == NULL) {
        init_scratch_arenas();
    }
    for (int i = 0; i < 2; i++) {
        if (scratch_arenas[i] != conflict) {
            return scratch_begin_from_arena(scratch_arenas[i]);
        }
    }
    FATAL_ERROR("Cannot find conflict-free arena.");
    return (Scratch){NULL,0};
}

void scratch_end(Scratch scratch) {
    arena_reset(scratch.arena, scratch.saved_pos);
}
