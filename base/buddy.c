#include <base/buddy.h>
#include <base/wasi.h>
#include <base/base_types.h>
#include <base/assert.h>
#include <base/base_io.h>

#define MIN_PAGE_SIZE 4096UL
#define MAX_ORDER 20 // 2^20 * 4KB = 4GB

/*
 * Header for each memory block (free or allocated).
 * This header is stored "inline" at the beginning of each block of memory.
 */
struct buddy_block {
    // The order of the block. Positive if free, negative if allocated.
    int order;
    struct buddy_block *prev;
    struct buddy_block *next;
};

struct list_head {
    struct buddy_block *first;
};

// free_lists[i] contains a doubly-linked list of free blocks of order i.
static struct list_head free_lists[MAX_ORDER + 1];
static void *heap_base;

static void list_add(struct list_head *lh, struct buddy_block *p) {
    p->next = lh->first;
    p->prev = NULL;
    if (lh->first) {
        lh->first->prev = p;
    }
    lh->first = p;
}

static void list_remove(struct list_head *lh, struct buddy_block *p) {
    if (p->prev) {
        p->prev->next = p->next;
    } else {
        lh->first = p->next;
    }
    if (p->next) {
        p->next->prev = p->prev;
    }
}

static void add_memory(void *mem, size_t bytes) {
    uintptr_t start = (uintptr_t)mem;
    uintptr_t end = start + bytes;

    // Align start address up to a MIN_PAGE_SIZE boundary
    uintptr_t mis = start % MIN_PAGE_SIZE;
    if (mis) {
        start += MIN_PAGE_SIZE - mis;
    }

    // Carve the memory region into the largest possible power-of-two blocks
    while (start + MIN_PAGE_SIZE <= end) {
        int order = 0;
        size_t block_size = MIN_PAGE_SIZE;
        while ((block_size << 1) <= (end - start) && (start % (block_size << 1) == 0) && order < MAX_ORDER) {
            block_size <<= 1;
            order++;
        }

        struct buddy_block *p = (struct buddy_block *)start;
        p->order = order;
        list_add(&free_lists[order], p);

        start += block_size;
    }
}

void buddy_init(void) {
    heap_base = wasi_heap_base();
    for (int o = 0; o <= MAX_ORDER; o++) {
        free_lists[o].first = NULL;
    }
    size_t initial_size = wasi_heap_size();
    if (initial_size > 0) {
        add_memory(heap_base, initial_size);
    }
}

static void *buddy_alloc_order(int order) {
    assert(order >= 0 && order <= MAX_ORDER);

    // Find the smallest available block that is large enough
    int current_order;
    for (current_order = order; current_order <= MAX_ORDER; current_order++) {
        if (free_lists[current_order].first) {
            break; // Found a suitable block
        }
    }

    // If no block is available, grow the heap
    if (current_order > MAX_ORDER) {
        size_t required_size = MIN_PAGE_SIZE << order;
        size_t grow_by = ((required_size + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE) * WASM_PAGE_SIZE;
        void *new_mem = wasi_heap_grow(grow_by);
        if (!new_mem) {
            writeln(WASI_STDERR_FD, "buddy_alloc: exit 1");
            wasi_proc_exit(1); // Or return NULL on failure
        }
        add_memory(new_mem, grow_by);
        return buddy_alloc_order(order); // Retry allocation
    }

    // We have a block of order 'current_order'. Remove it from its free list.
    struct buddy_block *p = free_lists[current_order].first;
    list_remove(&free_lists[current_order], p);

    // Split the block until it's the desired size
    while (current_order > order) {
        current_order--;
        size_t half_size = MIN_PAGE_SIZE << current_order;
        struct buddy_block *buddy = (struct buddy_block *)((uintptr_t)p + half_size);
        buddy->order = current_order;
        list_add(&free_lists[current_order], buddy);
    }

    // Mark the block as allocated by making its order negative
    p->order = -(order + 1);

    // Return the pointer to the memory *after* our inline header
    return (void *)(p + 1);
}

void *buddy_alloc(size_t size) {
    assert(size > 0);

    // Add space for our header to the requested size
    size += sizeof(struct buddy_block);

    // Calculate the order required for the allocation
    int order = 0;
    size_t block_size = MIN_PAGE_SIZE;
    while (block_size < size) {
        block_size <<= 1;
        order++;
        if (order > MAX_ORDER) {
            return NULL; // Request is too large
        }
    }
    return buddy_alloc_order(order);
}

void buddy_free(void *ptr) {
    assert(ptr);

    // Get the block header from the user-provided pointer
    struct buddy_block *p = ((struct buddy_block *)ptr) - 1;

    // Retrieve the original order and mark the block as free
    int order = -p->order - 1;
    if (order < 0 || order > MAX_ORDER) {
        return; // Invalid pointer or heap corruption
    }

    uintptr_t heap_end = (uintptr_t)heap_base + wasi_heap_size();

    // Coalesce with buddy if possible
    while (order < MAX_ORDER) {
        size_t block_size = MIN_PAGE_SIZE << order;
        uintptr_t p_addr = (uintptr_t)p;
        uintptr_t buddy_addr = p_addr ^ block_size;

        // Ensure the buddy is within the heap bounds before accessing it
        if (buddy_addr < (uintptr_t)heap_base || buddy_addr >= heap_end) {
            break;
        }

        struct buddy_block *buddy = (struct buddy_block *)buddy_addr;

        if (buddy->order != order) {
            break; // Buddy is not free or not the same size
        }

        // Buddy is free and of the same order, so merge them.
        list_remove(&free_lists[order], buddy);

        // The merged block starts at the lower of the two addresses
        if (buddy_addr < p_addr) {
            p = buddy;
        }

        order++;
    }

    p->order = order;
    list_add(&free_lists[order], p);
}
