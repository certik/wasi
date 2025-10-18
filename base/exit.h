#pragma once

#include <base/base_io.h>

#define FATAL_ERROR(x) PRINT_ERR(x); abort();

// Process exit for base/
void exit(int status);
void abort(void);
