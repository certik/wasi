#pragma once

#include <stdarg.h>

// Printf implementation for stdlib
// Only supports basic format specifiers

int printf(const char* format, ...);
int vprintf(const char* format, va_list ap);
