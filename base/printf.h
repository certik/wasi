#pragma once

#include <base/stdarg.h>

// Simple printf implementation for base/
// Only supports basic format specifiers needed for tests

int printf(const char* format, ...);
int vprintf(const char* format, va_list ap);
