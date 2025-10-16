#include <base/scratch.h>
#include <base/wasi.h>

Scratch scratch_begin_from_arena(Arena *arena) {
    Scratch scratch;
    scratch.arena = arena;
    scratch.saved_pos = arena_get_pos(arena);
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
    //assert(false);
    wasi_proc_exit(1);
    Scratch scratch;
    return scratch;
}

void scratch_end(Scratch scratch) {
    arena_reset(scratch.arena, scratch.saved_pos);
}
