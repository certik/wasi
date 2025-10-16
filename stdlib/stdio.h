#pragma once

#include <stdarg.h>
#include <stddef.h>

int printf(const char* format, ...);
int vprintf(const char* format, va_list ap);
