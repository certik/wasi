#pragma once

#include <stddef.h>
#include <stdint.h>


typedef struct {
    uint8_t* base;
    size_t capacity;
    size_t offset;
} Arena;

static void allocation_error();
void arena_init(Arena* arena);
void* arena_alloc(Arena* arena, size_t size);
void arena_reset(Arena* arena); 
