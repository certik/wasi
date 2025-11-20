#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void *malloc(size_t);
void free(void*);
void exit(int);
void abort(void);
void srand(int);
int rand();
int snprintf(char *str, size_t size, const char *format, ...);
int atoi(const char *str);
long long atoll(const char *str);
double atof(const char *str);
