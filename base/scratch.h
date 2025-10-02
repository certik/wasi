#pragma once

#include <base_types.h>
#include "arena.h"

/**
 * @brief A handle representing a scratch arena scope.
 *
 * Scratch arenas provide RAII-style temporary allocation within a function.
 * Use scratch_begin() to start a scope and scratch_end() to automatically
 * reset the arena to its previous state.
 */
typedef struct {
    Arena *arena;
    arena_pos_t saved_pos;
} Scratch;

/**
 * @brief Begins a scratch arena scope.
 *
 * Saves the current position of the arena and returns a Scratch handle.
 * All allocations made through scratch_alloc() will be deallocated when
 * scratch_end() is called.
 *
 * @param arena A pointer to the arena to use for scratch allocations.
 * @return A Scratch handle that must be passed to scratch_end().
 */
Scratch scratch_begin(Arena *arena);

/**
 * @brief Allocates memory from the scratch arena.
 *
 * This is equivalent to calling arena_alloc() on the underlying arena.
 *
 * @param scratch A pointer to the scratch arena handle.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, or NULL on failure.
 */
void *scratch_alloc(Scratch *scratch, size_t size);

/**
 * @brief Ends a scratch arena scope.
 *
 * Resets the arena to the position saved by scratch_begin(), effectively
 * deallocating all memory allocated through this scratch scope.
 *
 * @param scratch A pointer to the scratch arena handle.
 */
void scratch_end(Scratch *scratch);
