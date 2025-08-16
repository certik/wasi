#include "arena.h"
#include "buddy.h"
#include <stdint.h> // For uintptr_t

// All allocations will be aligned to this boundary (must be a power of two).
#define ARENA_ALIGNMENT 16
// New chunks will be at least this large.
#define MIN_CHUNK_SIZE 4096

// Represents a single chunk of memory obtained from the buddy allocator.
struct arena_chunk {
    struct arena_chunk *next;
    // Total size of the block returned by buddy_alloc for this chunk.
    size_t size;
    // The data area for this chunk begins immediately after this struct.
};

// The main arena structure. Its definition is hidden from the public API.
struct arena_s {
    struct arena_chunk *first_chunk;
    struct arena_chunk *current_chunk;
    char *current_ptr;
    size_t remaining_in_chunk;
    size_t default_chunk_size;
};

// Aligns a value up to the nearest multiple of ARENA_ALIGNMENT.
static inline uintptr_t align_up(uintptr_t val) {
    return (val + ARENA_ALIGNMENT - 1) & ~(uintptr_t)(ARENA_ALIGNMENT - 1);
}

arena_t *arena_new(size_t initial_size) {
    // Allocate the arena controller struct itself.
    arena_t *arena = buddy_alloc(sizeof(arena_t));
    if (!arena) {
        return NULL;
    }

    if (initial_size < MIN_CHUNK_SIZE) {
        initial_size = MIN_CHUNK_SIZE;
    }
    arena->default_chunk_size = initial_size;
    arena->first_chunk = NULL;

    // Allocate the first chunk.
    size_t total_alloc_size = sizeof(struct arena_chunk) + initial_size;
    struct arena_chunk *first = buddy_alloc(total_alloc_size);
    if (!first) {
        buddy_free(arena);
        return NULL;
    }
    first->next = NULL;
    first->size = total_alloc_size;

    // Initialize arena state to point to the start of the first chunk.
    arena->first_chunk = first;
    arena->current_chunk = first;

    uintptr_t data_start = align_up((uintptr_t)(first + 1));
    uintptr_t chunk_end = (uintptr_t)first + total_alloc_size;

    arena->current_ptr = (char *)data_start;
    arena->remaining_in_chunk = (data_start < chunk_end) ? (chunk_end - data_start) : 0;

    return arena;
}

void *arena_alloc(arena_t *arena, size_t size) {
    if (!arena || size == 0) {
        return NULL;
    }

    size_t aligned_size = (size + ARENA_ALIGNMENT - 1) & ~(size_t)(ARENA_ALIGNMENT - 1);

try_alloc:
    // If the current chunk has enough space, perform a simple bump allocation.
    if (aligned_size <= arena->remaining_in_chunk) {
        void *ptr = arena->current_ptr;
        arena->current_ptr += aligned_size;
        arena->remaining_in_chunk -= aligned_size;
        return ptr;
    }

    // Not enough space. If a next chunk already exists (from previous use), move to it.
    if (arena->current_chunk && arena->current_chunk->next) {
        arena->current_chunk = arena->current_chunk->next;

        struct arena_chunk* chunk = arena->current_chunk;
        uintptr_t data_start = align_up((uintptr_t)(chunk + 1));
        uintptr_t chunk_end = (uintptr_t)chunk + chunk->size;

        arena->current_ptr = (char *)data_start;
        arena->remaining_in_chunk = (data_start < chunk_end) ? (chunk_end - data_start) : 0;

        goto try_alloc; // Retry allocation in the next chunk.
    }

    // No more reusable chunks are available, so allocate a new one.
    size_t new_chunk_data_size = arena->default_chunk_size;
    if (aligned_size > new_chunk_data_size) {
        new_chunk_data_size = aligned_size; // Ensure the new chunk is large enough.
    }

    size_t total_alloc_size = sizeof(struct arena_chunk) + new_chunk_data_size;
    struct arena_chunk *new_chunk = buddy_alloc(total_alloc_size);
    if (!new_chunk) {
        return NULL; // Buddy allocation failed.
    }

    new_chunk->next = NULL;
    new_chunk->size = total_alloc_size;

    // Link the new chunk to the end of the list.
    if (arena->current_chunk) {
        arena->current_chunk->next = new_chunk;
    } else {
        arena->first_chunk = new_chunk;
    }
    arena->current_chunk = new_chunk;

    // Set the allocation pointer to the start of the new chunk.
    uintptr_t data_start = align_up((uintptr_t)(new_chunk + 1));
    uintptr_t chunk_end = (uintptr_t)new_chunk + total_alloc_size;
    arena->current_ptr = (char *)data_start;
    arena->remaining_in_chunk = (data_start < chunk_end) ? (chunk_end - data_start) : 0;

    // Retry the allocation now that we have a new, sufficiently large chunk.
    goto try_alloc;
}

void arena_free(arena_t *arena) {
    if (!arena) return;
    struct arena_chunk *current = arena->first_chunk;
    while (current) {
        struct arena_chunk *next = current->next;
        buddy_free(current);
        current = next;
    }
    buddy_free(arena);
}

arena_pos_t arena_get_pos(arena_t *arena) {
    arena_pos_t pos;
    if (arena) {
        pos.chunk = arena->current_chunk;
        pos.ptr = arena->current_ptr;
    } else {
        pos.chunk = NULL;
        pos.ptr = NULL;
    }
    return pos;
}

void arena_reset_to(arena_t *arena, arena_pos_t pos) {
    if (!arena || !pos.chunk || !pos.ptr) {
        return;
    }

    // Restore the state from the saved position
    arena->current_chunk = pos.chunk;
    arena->current_ptr = pos.ptr;

    // Recalculate the remaining size in the restored chunk
    uintptr_t chunk_end = (uintptr_t)pos.chunk + pos.chunk->size;
    uintptr_t current_pos = (uintptr_t)pos.ptr;

    arena->remaining_in_chunk = (current_pos < chunk_end) ? (chunk_end - current_pos) : 0;
}
