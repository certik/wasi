#pragma once

#include <base_types.h>

void buddy_init(void);

// Returns NULL on allocation failure
void *buddy_alloc(size_t size);

void buddy_free(void *ptr);
