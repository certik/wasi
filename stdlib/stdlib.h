#pragma once

#include <stdint.h>

#define NULL (void*)0

void *malloc(size_t);
void free(void*);
void exit(int);
void srand(int);
int rand();
