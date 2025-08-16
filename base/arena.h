#pragma once

#include <stddef.h>

// An opaque data type for the arena allocator.
typedef struct arena_s arena_t;

/**
 * @brief Creates a new arena allocator.
 *
 * This function initializes an arena and allocates an initial memory chunk
 * from the buddy allocator.
 * @param initial_size The suggested size for the first chunk. The actual size
 * may be larger to meet alignment and minimum size requirements.
 * @return A pointer to the newly created arena, or NULL on failure.
 */
arena_t *arena_new(size_t initial_size);

/**
 * @brief Allocates a block of memory from the arena.
 *
 * Memory is allocated using a bump pointer for high efficiency. If the current
 * chunk is full, the arena will advance to the next chunk (if available after a
 * reset) or allocate a new one from the buddy system.
 *
 * @param arena A pointer to the arena.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, aligned to 16 bytes.
 * Returns NULL if the arena is invalid or allocation fails.
 */
void *arena_alloc(arena_t *arena, size_t size);

/**
 * @brief Resets the arena for reuse.
 *
 * This function resets the allocation pointer back to the beginning of the
 * first chunk. It does not free any memory, allowing all previously allocated
 * chunks to be quickly and efficiently reused.
 *
 * @param arena A pointer to the arena.
 */
void arena_reset(arena_t *arena);

/**
 * @brief Deallocates all memory used by the arena.
 *
 * This function iterates through all chunks owned by the arena, frees them
 * using `buddy_free`, and finally frees the arena structure itself.
 * The arena pointer is invalid after this call.
 *
 * @param arena A pointer to the arena.
 */
void arena_free(arena_t *arena);
