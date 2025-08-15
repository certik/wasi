#include <buddy.h>
#include <wasi.h>

/*
 * Buddy allocator implementation.
 * Manages memory in power-of-two blocks starting from a minimum page size of 4096 bytes.
 * Allocations are rounded up to the nearest power-of-two multiple of the min page size.
 * Metadata is stored in a fixed-size array supporting up to 4GB of managed memory.
 */

#define MIN_PAGE_SIZE 4096UL
#define MAX_PAGES (1UL << 20)  // Supports up to 4GB (1M * 4KB)
#define MAX_ORDER 20  // 2^20 * 4KB = 4GB

struct page {
    struct page *next;
    struct page *prev;
    int order;
};

struct list_head {
    struct page *first;
};

static struct page pages[MAX_PAGES];
static struct list_head free_lists[MAX_ORDER + 1];
static void *heap_base;

static void list_add(struct list_head *lh, struct page *p) {
    p->next = lh->first;
    p->prev = (struct page *)0;
    if (lh->first) {
        lh->first->prev = p;
    }
    lh->first = p;
}

static void list_remove(struct list_head *lh, struct page *p) {
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
    if (bytes < MIN_PAGE_SIZE) {
        return;
    }
    unsigned long start = (unsigned long)mem;
    unsigned long mis = start % MIN_PAGE_SIZE;
    if (mis) {
        start += MIN_PAGE_SIZE - mis;
    }
    size_t remaining = bytes - (start - (unsigned long)mem);
    remaining -= remaining % MIN_PAGE_SIZE;
    while (remaining >= MIN_PAGE_SIZE) {
        int order = 0;
        size_t bs = MIN_PAGE_SIZE;
        while ((bs << 1) <= remaining && (start % (bs << 1) == 0) && order < MAX_ORDER) {
            bs <<= 1;
            order++;
        }
        size_t idx = (start - (unsigned long)heap_base) / MIN_PAGE_SIZE;
        if (idx + (1UL << order) > MAX_PAGES) {
            break;  // Exceeds supported max
        }
        struct page *p = &pages[idx];
        p->order = order;
        p->next = (struct page *)0;
        p->prev = (struct page *)0;
        list_add(&free_lists[order], p);
        start += bs;
        remaining -= bs;
    }
}

void buddy_init(void) {
    heap_base = wasi_heap_base();
    for (size_t i = 0; i < MAX_PAGES; i++) {
        pages[i].order = -1;
        pages[i].next = (struct page *)0;
        pages[i].prev = (struct page *)0;
    }
    for (int o = 0; o <= MAX_ORDER; o++) {
        free_lists[o].first = (struct page *)0;
    }
    size_t initial_size = wasi_heap_size();
    add_memory(heap_base, initial_size);
}

static void *buddy_alloc_order(int order) {
    if (order < 0 || order > MAX_ORDER) {
        return (void *)0;
    }
    int o;
    for (o = order; o <= MAX_ORDER; o++) {
        struct list_head *lh = &free_lists[o];
        if (lh->first) {
            struct page *p = lh->first;
            list_remove(lh, p);
            while (o > order) {
                o--;
                int num_min_pages = 1 << o;
                int buddy_idx = (int)((p - pages) + num_min_pages);
                struct page *buddy_p = &pages[buddy_idx];
                buddy_p->order = o;
                buddy_p->next = (struct page *)0;
                buddy_p->prev = (struct page *)0;
                list_add(&free_lists[o], buddy_p);
            }
            p->order = - (order + 1);
            int idx = (int)(p - pages);
            void *addr = (void *)((unsigned long)heap_base + (unsigned long)idx * MIN_PAGE_SIZE);
            return addr;
        }
    }
    // Grow heap
    size_t current_size = wasi_heap_size();
    unsigned long current_end = (unsigned long)heap_base + current_size;
    size_t bs = MIN_PAGE_SIZE << (size_t)order;
    unsigned long mis = current_end % bs;
    size_t pad = mis ? bs - mis : 0;
    size_t min_grow = pad + bs;
    size_t wp = WASM_PAGE_SIZE;
    size_t grow_by = ((min_grow + wp - 1) / wp) * wp;
    void *new_mem = wasi_heap_grow(grow_by);
    if (!new_mem) {
        wasi_proc_exit(1);
    }
    add_memory(new_mem, grow_by);
    return buddy_alloc_order(order);
}

void *buddy_alloc(size_t size) {
    if (size == 0) {
        return (void *)0;
    }
    int order = 0;
    size_t bs = MIN_PAGE_SIZE;
    while (bs < size) {
        bs <<= 1;
        order++;
        if (order > MAX_ORDER) {
            return (void *)0;
        }
    }
    return buddy_alloc_order(order);
}

void buddy_free(void *ptr) {
    if (ptr == (void *)0) {
        return;
    }
    unsigned long addr = (unsigned long)ptr;
    if (addr % MIN_PAGE_SIZE != 0) {
        return;  // Misaligned
    }
    size_t idx = (addr - (unsigned long)heap_base) / MIN_PAGE_SIZE;
    if (idx >= MAX_PAGES) {
        return;
    }
    struct page *p = &pages[idx];
    int order = -p->order - 1;
    if (order < 0 || order > MAX_ORDER) {
        return;
    }
    p->order = order;
    while (order < MAX_ORDER) {
        int num_min_pages = 1 << order;
        size_t buddy_idx = idx ^ (size_t)num_min_pages;
        if (buddy_idx >= MAX_PAGES) {
            break;
        }
        struct page *buddy_p = &pages[buddy_idx];
        if (buddy_p->order != order) {
            break;
        }
        list_remove(&free_lists[order], buddy_p);
        if (buddy_idx < idx) {
            idx = buddy_idx;
            p = buddy_p;
        }
        order++;
        p->order = order;
    }
    list_add(&free_lists[order], p);
}
