#pragma once

#include <stddef.h>
#include <stdint.h>

// Number to string conversion functions for base/
// Self-contained implementations with no external dependencies

// Convert unsigned 64-bit integer to string
// Returns length of string written (not including null terminator)
size_t uint64_to_str(uint64_t val, char* buf);

// Convert signed 64-bit integer to string
// Returns length of string written (not including null terminator)
size_t int64_to_str(int64_t val, char* buf);

// Convert int to string (calls int64_to_str)
// Returns length of string written (not including null terminator)
size_t int_to_str(int val, char* buf);

// Convert double to string with specified precision
// Returns length of string written (not including null terminator)
// precision: number of decimal places (-1 for default of 6)
size_t double_to_str(double val, char* buf, int precision);
