#pragma once

#include <stdint.h>
#include <stddef.h>

void *malloc(size_t);
void free(void*);
void exit(int);
void srand(int);
int rand();
