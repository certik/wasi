#pragma once

#include <base_types.h>

void buddy_init(void);
void *buddy_alloc(size_t size);
void buddy_free(void *ptr);
